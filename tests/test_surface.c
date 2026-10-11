// surface: map loading into the three layers, footprint placement checks,
// building counts / deletion and the tile blits, on hand-made state.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/map.h"
#include "game/blit.h"
#include "game/dseg.h"
#include "game/fixes.h"
#include "game/mem.h"
#include "game/mouse.h"
#include "game/surface.h"
#include "game/system.h"
#include "platform/fs.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static TBody bodies[8];
static TBuilding buildings[1000];

static TBody *body(int idx) { return &bodies[idx - 1]; }

static TBuilding *bld(int i) { return &buildings[i - 1]; }

static uint8_t *layer(uint8_t *p, int x, int y) { return p + (x - 1) * 60 + (y - 1); }

// System 1 with one planet (body 1) of the given surface type, as the
// current body, plus the three layers.
static void setup(int surface, int variant)
{
    memset(bodies, 0, sizeof bodies);
    memset(buildings, 0, sizeof buildings);
    ds.Galaxy_Bodies[0] = bodies;
    body(1)->SurfaceType = (uint8_t)surface;
    body(1)->Owner = 1;
    body(1)->Colony = 1;
    ds.Game_CurSystem = 1;
    ds.Game_CurPlanet[0] = 1;
    ds.Game_CurMoon[0] = 0;
    ds.Game_CurBody = bodies;
    ds.Game_Buildings = buildings;
    ds.Game_BuildingCount = 0;
    ds.Surface_TileSet = ds.Surface_Tileset[surface - 1];
    ds.Surface_MapVariant = (int16_t)variant;
    if (ds.Surface_TileLayer) {
        System_FreeMem(ds.Surface_TileLayer, 0x1518);
        System_FreeMem(ds.Surface_OccupyLayer, 0x1518);
        System_FreeMem(ds.Surface_BuildingLayer, 0x2A30);
    }
    ds.Surface_TileLayer = System_GetMem(0x1518);
    ds.Surface_OccupyLayer = System_GetMem(0x1518);
    ds.Surface_BuildingLayer = System_GetMem(0x2A30);
}

static void load_map(void)
{
    for (int surface = 1; surface <= 10; surface++) {
        setup(surface, 0);
        char path[64];
        snprintf(path, sizeof path, "MAP\\MAP%d_0.MAP", surface);
        struct map m;
        if (!map_load(path, &m))
            continue;
        Surface_LoadMap();
        CHECK(ds.Surface_MapWidth == (uint16_t)m.width && ds.Surface_MapHeight == (uint16_t)m.height);
        int bad = 0, blocked = 0;
        for (int y = 1; y <= m.height; y++) {
            for (int x = 1; x <= m.width; x++) {
                uint8_t t = m.cells[(y - 1) * m.width + x - 1];
                // Flat, as the game reads it: tiles >= 240 run into the next set.
                const uint8_t *tpl = &ds.Surface_MapTemplates[0][0][0];
                bool wall = tpl[(ds.Surface_TileSet - 1) * 0xF0 + t] != 0;
                if (*layer(ds.Surface_TileLayer, x, y) != t)
                    bad++;
                if (*layer(ds.Surface_OccupyLayer, x, y) != (wall ? 0xFF : 0))
                    bad++;
                if (*(uint16_t *)(ds.Surface_BuildingLayer + ((x - 1) * 60 + (y - 1)) * 2) != 0)
                    bad++;
                blocked += wall;
            }
        }
        CHECK(bad == 0);
        if (surface == 1)
            printf("map1_0: %dx%d, %d blocked cells\n", m.width, m.height, blocked);
        map_free(&m);
    }
}

// Footprint of kind 4 or whatever fits: the first type whose footprint is
// at least 2x2 and allowed on the surface.
static int pick_kind(int surface)
{
    for (int k = 1; k <= 25; k++) {
        const TBuildingType *bt = &ds.Surface_BuildingTypes[k - 1];
        if (bt->Width >= 2 && bt->Height >= 2 && ((const uint8_t *)bt)[0x13 + surface] != 0)
            return k;
    }
    return 0;
}

static void placement(void)
{
    setup(1, 0);
    Surface_LoadMap();
    int kind = pick_kind(1);
    CHECK(kind != 0);
    if (!kind)
        return;
    const TBuildingType *bt = &ds.Surface_BuildingTypes[kind - 1];

    // The first spot is the one nearest the centre; marking it makes the next one differ.
    int16_t x1, y1, x2, y2;
    bool f1, f2;
    Surface_FindBuildSpot(kind, 1, 1, 0, &x1, &y1, &f1);
    CHECK(f1);
    int16_t cx = (int16_t)((ds.Surface_MapWidth + 1 - bt->Width) / 2);
    int16_t cy = (int16_t)((ds.Surface_MapHeight + 1 - bt->Height) / 2);
    CHECK(abs(x1 - cx) <= 3 && abs(y1 - cy) <= 3);

    // AddBuilding records a new, unfinished building and flags it by the surface type.
    Surface_AddBuilding((uint8_t)kind, 1, 1, 0, (uint8_t)x1, (uint8_t)y1);
    CHECK(ds.Game_BuildingCount == 1);
    CHECK(bld(1)->Kind == kind && bld(1)->X == x1 && bld(1)->Y == y1);
    CHECK(bld(1)->Health >= 100 && bld(1)->Health < 160);

    Surface_MarkFootprint(1, kind, x1, y1, 0);
    for (int j = 0; j < bt->Height; j++) {
        for (int i = 0; i < bt->Width; i++) {
            uint8_t t = bt->Tiles[j][i];
            CHECK(*layer(ds.Surface_OccupyLayer, x1 + i, y1 + j) == (t == 0xFF ? 0 : t + 1));
            CHECK(*(uint16_t *)(ds.Surface_BuildingLayer
                                + ((x1 + i - 1) * 60 + (y1 + j - 1)) * 2) == (t == 0xFF ? 0 : 1));
        }
    }
    Surface_FindBuildSpot(kind, 1, 1, 0, &x2, &y2, &f2);
    CHECK(f2 && (x2 != x1 || y2 != y1));

    // Construction markers.
    Surface_MarkFootprint(1, kind, x1, y1, 2);
    CHECK(*layer(ds.Surface_OccupyLayer, x1, y1) == 0xDE || bt->Tiles[0][0] == 0xFF);

    // Mode 2 (placing) on the occupied spot falls back to browsing; on the
    // free one it stays.
    ds.Surface_SelKind = (int16_t)kind;
    ds.Surface_Mode = 2;
    Surface_UpdateModeForCell(x1, y1);
    CHECK(ds.Surface_Mode == 1);
    ds.Surface_Mode = 2;
    Surface_UpdateModeForCell(x2, y2);
    CHECK(ds.Surface_Mode == 2);
    // Off the map edge.
    ds.Surface_Mode = 2;
    Surface_UpdateModeForCell((int16_t)ds.Surface_MapWidth, (int16_t)ds.Surface_MapHeight);
    CHECK(ds.Surface_Mode == 1);

    // Browsing a cell of an own building selects it.
    ds.Surface_Mode = 1;
    Surface_UpdateModeForCell(x1, y1);
    CHECK(ds.Surface_Mode == 4 && ds.Surface_SelBuilding == 1 && ds.Surface_SelKind == kind);
    ds.Surface_Mode = 6;
    Surface_UpdateModeForCell(x2, y2);
    CHECK(ds.Surface_Mode == 1);

    // A kind that isn't allowed on this surface is not added.
    int bad = 0;
    for (int k = 1; k <= 25; k++)
        if (((const uint8_t *)&ds.Surface_BuildingTypes[k - 1])[0x13 + 1] == 0) {
            bad = k;
            break;
        }
    if (bad) {
        Surface_AddBuilding((uint8_t)bad, 1, 1, 0, 0xFF, 0xFF);
        CHECK(ds.Game_BuildingCount == 1);
    }

    // Unplaced building: PlaceBuilding gives it a spot (free of the first one).
    Surface_AddBuilding((uint8_t)kind, 1, 1, 0, 0xFF, 0xFF);
    CHECK(ds.Game_BuildingCount == 2 && bld(2)->X == 0xFF);
    Surface_PlaceBuilding(2);
    CHECK(bld(2)->X != 0xFF && bld(2)->Y != 0xFF);
}

static void counting(void)
{
    setup(1, 0);
    // Three buildings of kind 4 (one unfinished, one powered), one of kind 17 on another planet.
    static const struct { uint8_t kind, planet, health, flag; } rec[] = {
        {4, 1, 0, 0}, {4, 1, 0, 1}, {4, 1, 40, 1}, {17, 2, 0, 1}, {17, 1, 0, 1},
    };
    for (int i = 0; i < 5; i++) {
        *bld(i + 1) = (TBuilding){.Kind = rec[i].kind, .System = 1, .Planet = rec[i].planet,
                                  .Moon = 0, .Health = rec[i].health, .Flag = rec[i].flag};
    }
    ds.Game_BuildingCount = 5;
    CHECK(Surface_CountBuildings(4, 1, 1, 0, false) == 2);
    CHECK(Surface_CountBuildings(4, 1, 1, 0, true) == 1);
    CHECK(Surface_CountBuildings(17, 1, 1, 0, true) == 1);
    CHECK(Surface_CountBuildings(17, 1, 2, 0, false) == 1);
    CHECK(Surface_CountBuildings(4, 1, 2, 0, false) == 0);

    Surface_CountBuildingsByKind(1, 1, 0);
    CHECK(ds.Surface_KindCounts[0][4 - 1] == 1);
    CHECK(ds.Surface_KindCounts[1][4 - 1] == 1);
    CHECK(ds.Surface_KindCounts[1][17 - 1] == 1);
    CHECK(ds.Surface_KindCounts[0][17 - 1] == 0);

    Surface_DeleteBuilding(2);
    CHECK(ds.Game_BuildingCount == 4);
    CHECK(bld(2)->Kind == 4 && bld(2)->Health == 40 && bld(3)->Kind == 17 && bld(3)->Planet == 2);
    CHECK(Surface_CountBuildings(4, 1, 1, 0, false) == 1);
    Surface_DeleteBuilding(4);
    CHECK(ds.Game_BuildingCount == 3);

    Surface_Init();
    CHECK(ds.Game_FindBuildingProc == far_ptr(0x2EF2, 0x2B1A));
}

static void hotspots_and_blits(void)
{
    setup(1, 0);
    ds.Surface_CanBuild = 0;
    Surface_SetupSurfaceHotspots();
    CHECK(ds.Mouse_HotspotCount == 12);
    CHECK(Mouse_Hotspot(1)->X == 0x5D && Mouse_Hotspot(1)->W == 0xE0);
    CHECK(memcmp(Mouse_Hotspot(3)->Caption.s, "No effect", 9) == 0);
    CHECK(Mouse_Hotspot(10)->Caption.len == 0 && Mouse_Hotspot(11)->Repeat_ == 1);
    ds.Surface_CanBuild = 1;
    Surface_SetupSurfaceHotspots();
    CHECK(memcmp(Mouse_Hotspot(10)->Caption.s, "Build", 5) == 0);
    Surface_SetupSimpleInfoHotspots();
    CHECK(ds.Mouse_HotspotCount == 2);

    // BlitTile: tile 21 (row 1, column 1) of a sheet to window cell (2, 3).
    ds.Mouse_CursorState = 1;
    uint8_t *sheet = System_GetMem(0x4000);
    memset(sheet, 0, 0x4000);
    for (int r = 0; r < 16; r++)
        memset(sheet + 0x1400 + 16 + r * 320, 0x77, 16);
    Surface_BlitTile(sheet, 21, 2, 3);
    uint8_t *scr = &vid_screen[0][0];
    CHECK(scr[(0x35 + 32) * 320 + 0x5D + 16] == 0x77);
    CHECK(scr[(0x35 + 47) * 320 + 0x5D + 31] == 0x77);
    CHECK(scr[(0x35 + 31) * 320 + 0x5D + 16] == 0);
    CHECK(scr[(0x35 + 32) * 320 + 0x5D + 32] == 0);
    Surface_BlitTileInner(sheet, 21, 2, 3);
    CHECK(scr[(0x36 + 32) * 320 + 0x5E + 16] == 0x77);
}

// Fix (game/fixes.h): a building placed by the simulation on the colony
// being watched, with no free spot, is added unplaced; the original drops it.
static void autoplace_when_full(void)
{
    for (int fix = 0; fix <= 1; fix++) {
        setup(1, 0);
        Surface_LoadMap();
        memset(ds.Surface_OccupyLayer, 0xFF, 0x1518);
        ds.Game_CurrentScreen = 0x14;
        fixes.autoplace_when_full = fix;
        Surface_AutoPlaceBuilding(2, 1, 1, 0);
        CHECK(ds.Game_BuildingCount == fix);
        if (fix)
            CHECK(bld(1)->Kind == 2 && bld(1)->X == 0xFF && bld(1)->Y == 0xFF);
    }
    ds.Game_CurrentScreen = 0;
    fixes.autoplace_when_full = true;
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    mem_init();
    if (!dseg_init()) {
        printf("no REUNION.PRG, skipping\n");
        return 77;
    }
    system_init();
    // The charset lives in the game data block, which this test doesn't
    // load; blank glyphs instead.
    static uint8_t charset[0x2880];
    ds.Gfx_Charset = charset;
    static TGroup groups[2][32];
    ds.Game_Groups = groups;
    load_map();
    placement();
    counting();
    autoplace_when_full();
    hotspots_and_blits();
    printf(ok ? "surface: ok\n" : "surface: FAILED\n");
    return ok ? 0 : 1;
}
