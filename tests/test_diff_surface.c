// Differential test of unit Surface (2ef2): the planet surface screen (map,
// radar, info panel, hotspots) and the building bookkeeping (add, place,
// find a spot, count, delete), original (under Unicorn) against the port,
// from a new game's surface screen of the home planet with the map, the
// buildings and the selection perturbed. See docs/difftest.md. Needs the
// game data.
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/surface.h"
#include "game/system.h"

DIFF_PROC(Surface_EnterSurface, Surface_EnterSurface());
DIFF_PROC(Surface_LoadMap, Surface_LoadMap());
DIFF_PROC(Surface_RegisterBuildingAnims,
          Surface_RegisterBuildingAnims((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_PROC(Surface_SetupSurfaceHotspots, Surface_SetupSurfaceHotspots());
DIFF_PROC(Surface_DrawBuildingInfo, Surface_DrawBuildingInfo());
DIFF_PROC(Surface_ShowBuildingInfoView, Surface_ShowBuildingInfoView());
DIFF_PROC(Surface_ShowBuildingPicture, Surface_ShowBuildingPicture());
DIFF_PROC(Surface_DrawSurfaceFrame, Surface_DrawSurfaceFrame());
DIFF_PROC(Surface_SetupInfoHotspots, Surface_SetupInfoHotspots());
DIFF_PROC(Surface_SetupSimpleInfoHotspots, Surface_SetupSimpleInfoHotspots());
DIFF_PROC(Surface_DrawModeButtons, Surface_DrawModeButtons());
DIFF_PROC(Surface_BlinkModeButton, Surface_BlinkModeButton());
DIFF_PROC(Surface_DrawKindArrows, Surface_DrawKindArrows());
DIFF_PROC(Surface_BlitTile, Surface_BlitTile((FarPtr)a[0], (uint16_t)a[1], (int16_t)a[2], (int16_t)a[3]));
DIFF_PROC(Surface_BlitTileInner,
          Surface_BlitTileInner((FarPtr)a[0], (uint16_t)a[1], (int16_t)a[2], (int16_t)a[3]));
DIFF_PROC(Surface_DrawBuildingPreview, Surface_DrawBuildingPreview((int16_t)a[0]));
DIFF_PROC(Surface_DrawBuildingTiles, Surface_DrawBuildingTiles((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_PROC(Surface_DrawFootprint, Surface_DrawFootprint((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_PROC(Surface_DrawMapView, Surface_DrawMapView());
DIFF_PROC(Surface_UpdateModeForCell, Surface_UpdateModeForCell((int16_t)a[0], (int16_t)a[1]));
DIFF_PROC(Surface_AddBuilding, Surface_AddBuilding((uint8_t)a[0], (uint8_t)a[1], (uint8_t)a[2], (uint8_t)a[3],
                                                   (uint8_t)a[4], (uint8_t)a[5]));
DIFF_PROC(Surface_BuildOnCurrentPlanet,
          Surface_BuildOnCurrentPlanet((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_PROC(Surface_MarkFootprint, Surface_MarkFootprint((int16_t)a[0], (int16_t)a[1], (int16_t)a[2],
                                                       (int16_t)a[3], (int16_t)a[4]));
DIFF_PROC(Surface_FindBuildSpot,
          Surface_FindBuildSpot((int16_t)a[0], (int16_t)a[1], (int16_t)a[2], (int16_t)a[3],
                                far_host((FarPtr)a[4]), far_host((FarPtr)a[5]), far_host((FarPtr)a[6])));
DIFF_PROC(Surface_AutoPlaceBuilding,
          Surface_AutoPlaceBuilding((int16_t)a[0], (int16_t)a[1], (int16_t)a[2], (int16_t)a[3]));
DIFF_PROC(Surface_PlaceBuilding, Surface_PlaceBuilding((int16_t)a[0]));
DIFF_PROC(Surface_DeleteBuilding, Surface_DeleteBuilding((int16_t)a[0]));
DIFF_FUNC(Surface_CountBuildings, Surface_CountBuildings((int16_t)a[0], (int16_t)a[1], (int16_t)a[2],
                                                         (int16_t)a[3], a[4] != 0));
DIFF_PROC(Surface_CountBuildingsByKind,
          Surface_CountBuildingsByKind((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_PROC(Surface_RebuildRadar, Surface_RebuildRadar());
DIFF_PROC(Surface_DrawRadar, Surface_DrawRadar());
DIFF_PROC(Surface_Init, Surface_Init());

static int16_t *w16(uint16_t off) { return (int16_t *)ds_at(off); }

static void seed(DiffRng *r)
{
    ds.System_RandSeed = (int32_t)diff_rand(r, 0xFFFFFFFFu);
}

static TBuilding *bldg(int i) { return (TBuilding *)far_host(ds.Game_Buildings) + (i - 1); }
static TBody *body(void) { return far_host(ds.Game_CurBody); }
static int cur_system(void) { return ds.Game_CurSystem; }
static int cur_planet(void) { return ds.Game_CurPlanet[ds.Game_CurSystem - 1]; }
static int cur_moon(void) { return ds.Game_CurMoon[ds.Game_CurSystem - 1]; }
static const TBuildingType *btype(int kind) { return &ds.Surface_BuildingTypes[kind - 1]; }
static uint8_t *occupy(int x, int y)
{
    return (uint8_t *)far_host(ds.Surface_OccupyLayer) + (uint16_t)(x * 60 + y - 61);
}
static uint16_t *building_at(int x, int y)
{
    return (uint16_t *)((uint8_t *)far_host(ds.Surface_BuildingLayer) + (uint16_t)(x * 120 + y * 2 - 122));
}
static int map_w(void) { return ds.Surface_MapWidth; }
static int map_h(void) { return ds.Surface_MapHeight; }

// --- Perturbations --------------------------------------------------------------------

// Buildings on the current body: finished or not, powered or not, figures.
static void perturb_buildings(DiffRng *r)
{
    for (int i = 1; i <= ds.Game_BuildingCount; i++) {
        TBuilding *b = bldg(i);
        if (!diff_chance(r, 50))
            continue;
        b->Health = (uint8_t)diff_pick(r, 0, 0, 0, 1, 40, 41, 80, 81, 120, 159);
        b->Flag = (uint8_t)diff_range(r, 0, 1);
        b->Output = (uint8_t)diff_range(r, 0, 255);
        b->W9 = (int16_t)diff_range(r, 0, 300);
        b->WB = (int16_t)diff_range(r, 0, 300);
        b->BD = (uint8_t)diff_pick(r, 0, 50, 100, diff_range(r, 0, 255));
    }
}

// Appends n buildings of random kinds on the current body (unplaced or at a
// random cell).
static void add_buildings(DiffRng *r, int n)
{
    for (int k = 0; k < n && ds.Game_BuildingCount < 1000; k++) {
        TBuilding *b = bldg(++ds.Game_BuildingCount);
        b->Kind = (uint8_t)diff_pick(r, 0x11, diff_range(r, 1, 25));
        b->System = (uint8_t)cur_system();
        b->Planet = (uint8_t)cur_planet();
        b->Moon = (uint8_t)cur_moon();
        if (diff_chance(r, 50)) {
            b->X = b->Y = 0xFF;
        } else {
            b->X = (uint8_t)diff_range(r, 1, map_w() - 4);
            b->Y = (uint8_t)diff_range(r, 1, map_h() - 4);
        }
        b->Health = (uint8_t)diff_pick(r, 0, 0, 1, 50, 120);
        b->Flag = (uint8_t)diff_range(r, 0, 1);
        b->Output = (uint8_t)diff_range(r, 0, 200);
        b->W9 = b->WB = 0;
        b->BD = (uint8_t)diff_range(r, 0, 100);
    }
}

// The body shown or not (Owner/Colony/Satellite/base decide whether the map
// routines draw).
static void perturb_body(DiffRng *r)
{
    TBody *b = body();
    if (diff_chance(r, 15)) {
        b->Owner = (uint8_t)diff_pick(r, 0, 1, 2);
        b->Colony = (uint8_t)diff_range(r, 0, 1);
        b->_0A = (uint8_t)diff_pick(r, 0, 0, 1);
        b->Satellite = (uint8_t)diff_pick(r, 0, 0, 1, 0xF6);
        b->_09 = (uint8_t)diff_pick(r, 0, 0, 1);
    }
}

// Cells of the occupancy and building layers: free, blocked, building cells.
static void perturb_layers(DiffRng *r, int n)
{
    for (int k = 0; k < n; k++) {
        int x = diff_range(r, 1, map_w()), y = diff_range(r, 1, map_h());
        *occupy(x, y) = (uint8_t)diff_pick(r, 0, 0, 0xFF, diff_range(r, 1, 100), 0xDD, 0xDF);
        int bi = ds.Game_BuildingCount ? diff_range(r, 0, ds.Game_BuildingCount) : 0;
        *building_at(x, y) = (uint16_t)(*occupy(x, y) == 0 || *occupy(x, y) == 0xFF ? 0 : bi);
    }
}

static void scroll(DiffRng *r)
{
    ds.Surface_ScrollX = (int16_t)diff_pick(r, 0, map_w() - 14, diff_range(r, 0, map_w() - 14));
    ds.Surface_ScrollY = (int16_t)diff_pick(r, 0, map_h() - 9, diff_range(r, 0, map_h() - 9));
    if (ds.Surface_ScrollX < 0)
        ds.Surface_ScrollX = 0;
    if (ds.Surface_ScrollY < 0)
        ds.Surface_ScrollY = 0;
}

static int kind(DiffRng *r) { return diff_pick(r, 1, 25, 0x11, 0x19, diff_range(r, 1, 25)); }

// --- Screen set-up ----------------------------------------------------------------------

// Map variants per surface type (MAP\MAP<type>_<n>.MAP files).
static const int variants[12] = {0, 10, 2, 10, 2, 9, 3, 1, 2, 2, 2, 4};

// What leaving the surface screen frees (Main, screen 20).
static void free_surface(void)
{
    int t = ds.Surface_TileSet;
    System_FreeMem(ds.Surface_RadarColors, 0x820);
    System_FreeMem(ds.Surface_RadarBuf, (uint16_t)(map_w() * map_h() * (ds.Surface_RadarDouble ? 4 : 1)));
    System_FreeMem(ds.Surface_BufA, 0x24FF);
    System_FreeMem(ds.Surface_BufC, 0xF000);
    if (ds.Surface_FanimHeight[t - 1] != 0)
        System_FreeMem(ds.Surface_BufD, (uint16_t)(ds.Surface_FanimHeight[t - 1] * 0x140));
    System_FreeMem(ds.Surface_BufB, (uint16_t)(ds.Surface_FelszHeight[t - 1] * 0x140));
    System_FreeMem(ds.Surface_BuildingLayer, 0x2A30);
    System_FreeMem(ds.Surface_OccupyLayer, 0x1518);
    System_FreeMem(ds.Surface_TileLayer, 0x1518);
}

// Another surface type and map: small ones (radar at 2x) often.
static void other_map(DiffRng *r)
{
    int t = diff_pick(r, 2, 3, 4, 7, 11, diff_range(r, 1, 11));
    body()->SurfaceType = (uint8_t)t;
    ds.Surface_MapVariant = (int16_t)diff_range(r, 0, variants[t] - 1);
    ds.Surface_TileSet = ds.Surface_Tileset[t - 1];
}

static void setup_enter(DiffRng *r, int32_t *a)
{
    (void)a;
    free_surface();
    if (diff_chance(r, 60))
        other_map(r);
    perturb_body(r);
    if (diff_chance(r, 30))
        body()->Owner = (uint8_t)diff_pick(r, 1, 2, 3);
}

static void setup_load_map(DiffRng *r, int32_t *a)
{
    (void)a;
    int t = diff_pick(r, body()->SurfaceType, diff_range(r, 1, 11));
    body()->SurfaceType = (uint8_t)t;
    ds.Surface_MapVariant = (int16_t)diff_range(r, 0, variants[t] - 1);
    ds.Surface_TileSet = ds.Surface_Tileset[t - 1];
}

static void setup_register(DiffRng *r, int32_t *a)
{
    seed(r);
    perturb_buildings(r);
    add_buildings(r, diff_range(r, 0, 6));
    // Unplaced buildings get a spot.
    for (int i = 1; i <= ds.Game_BuildingCount; i++)
        if (diff_chance(r, 10))
            bldg(i)->X = bldg(i)->Y = 0xFF;
    if (diff_chance(r, 30))
        body()->Owner = (uint8_t)diff_pick(r, 2, 3);
    // Not a full map: with no spot for the enemy base the original marks
    // its footprint at uninitialised (stack garbage) x, y; the port skips it.
    a[0] = cur_system();
    a[1] = cur_planet();
    a[2] = cur_moon();
}

static void setup_can_build(DiffRng *r, int32_t *a)
{
    (void)a;
    ds.Surface_CanBuild = (uint8_t)diff_pick(r, 0, 1, 2);
}

// Info panel: type info (mode 3) or an existing building (mode 4).
static void setup_info(DiffRng *r, int32_t *a)
{
    (void)a;
    perturb_buildings(r);
    add_buildings(r, 1);
    ds.Surface_Mode = (int16_t)diff_pick(r, 3, 4, 4, 1, 2);
    ds.Surface_SelKind = (int16_t)kind(r);
    if (ds.Surface_Mode == 4) {
        ds.Surface_SelBuilding = (int16_t)diff_range(r, 1, ds.Game_BuildingCount);
        ds.Surface_SelKind = bldg(ds.Surface_SelBuilding)->Kind;
        if (diff_chance(r, 30))
            body()->Deposits[0] = (uint8_t)diff_pick(r, 0, 9, 10, 99, 255);
    }
}

static void setup_mode(DiffRng *r, int32_t *a)
{
    (void)a;
    *w16(0x791C) = (int16_t)diff_pick(r, 0, 1, 10, 11, 19, diff_range(r, 0, 19));
    ds.Surface_Mode = (int16_t)diff_pick(r, 1, 2, 2, 3, 4, 6, 6);
    ds.Surface_SelKind = (int16_t)kind(r);
}

// --- Tiles ------------------------------------------------------------------------------

static void setup_blit_tile(DiffRng *r, int32_t *a)
{
    a[0] = (int32_t)(diff_chance(r, 50) ? ds.Surface_BufB : ds.Surface_BufC);
    a[1] = diff_pick(r, 0, 19, 20, diff_range(r, 0, 100));
    a[2] = diff_range(r, 1, 14);
    a[3] = diff_range(r, 1, 9);
}

static void setup_preview(DiffRng *r, int32_t *a)
{
    perturb_body(r);
    ds.Surface_CanBuild = (uint8_t)diff_pick(r, 0, 1, 1);
    ds.Surface_SelKind = (int16_t)kind(r);
    ds.Surface_TileSet = (int16_t)diff_pick(r, ds.Surface_TileSet, ds.Surface_TileSet, 1, 7, 8);
    a[0] = kind(r);
}

static void setup_building_tiles(DiffRng *r, int32_t *a)
{
    a[0] = kind(r);
    a[1] = diff_range(r, 0, 250);
    a[2] = diff_range(r, 50, 130);
}

static void setup_footprint(DiffRng *r, int32_t *a)
{
    scroll(r);
    perturb_layers(r, 40);
    a[0] = kind(r);
    a[1] = ds.Surface_ScrollX + diff_pick(r, 1, 12, 13, 14, diff_range(r, 1, 14));
    a[2] = ds.Surface_ScrollY + diff_pick(r, 1, 7, 8, 9, diff_range(r, 1, 9));
}

static void setup_map_view(DiffRng *r, int32_t *a)
{
    (void)a;
    perturb_body(r);
    perturb_buildings(r);
    scroll(r);
    perturb_layers(r, 60);
    ds.Surface_Mode = (int16_t)diff_pick(r, 1, 2, 2, 2, 4);
    ds.Surface_SelKind = (int16_t)kind(r);
    ds.Mouse_ClickX = (int16_t)diff_pick(r, 0, 0x5D, 100, 200, 300, diff_range(r, 0, 319));
    ds.Mouse_ClickY = (int16_t)diff_pick(r, 0, 0x35, 60, 100, 190, diff_range(r, 0, 199));
    *(uint8_t *)ds_at(0x791E) = (uint8_t)diff_range(r, 0, 1);  // blink
    // Finished, unpowered buildings blink.
    for (int i = 1; i <= ds.Game_BuildingCount; i++)
        if (diff_chance(r, 30)) {
            bldg(i)->Health = 0;
            bldg(i)->Flag = 0;
        }
}

// --- Placing --------------------------------------------------------------------------------

static void setup_update_mode(DiffRng *r, int32_t *a)
{
    perturb_layers(r, 60);
    if (diff_chance(r, 20))
        body()->Owner = (uint8_t)diff_pick(r, 0, 2);
    ds.Surface_Mode = (int16_t)diff_pick(r, 1, 1, 2, 2, 2, 3, 6, 6);
    ds.Surface_SelKind = (int16_t)kind(r);
    ds.Surface_SelBuilding = (int16_t)diff_range(r, 0, 3);
    a[0] = diff_pick(r, 1, map_w(), map_w() - 1, diff_range(r, 1, map_w()));
    a[1] = diff_pick(r, 1, map_h(), map_h() - 1, diff_range(r, 1, map_h()));
    if (diff_chance(r, 40)) {  // on a building's cell
        TBuilding *b = bldg(diff_range(r, 1, ds.Game_BuildingCount));
        if (b->X != 0xFF) {
            a[0] = b->X;
            a[1] = b->Y;
        }
    }
}

static void setup_add(DiffRng *r, int32_t *a)
{
    seed(r);
    if (diff_chance(r, 15))
        ds.Game_BuildingCount = (int16_t)diff_pick(r, 998, 999, 1000);
    a[0] = kind(r);
    a[1] = cur_system();
    a[2] = cur_planet();
    a[3] = cur_moon();
    if (diff_chance(r, 30)) {  // another body of the system (or a moon)
        a[2] = diff_range(r, 1, ds.Galaxy_PlanetCount[a[1] - 1]);
        a[3] = diff_pick(r, 0, 0, 1);
    }
    a[4] = diff_pick(r, 0xFF, diff_range(r, 1, 40));
    a[5] = diff_pick(r, 0xFF, diff_range(r, 1, 40));
}

static void setup_build_here(DiffRng *r, int32_t *a)
{
    seed(r);
    scroll(r);
    perturb_body(r);
    if (diff_chance(r, 15))
        ds.Game_BuildingCount = (int16_t)diff_pick(r, 999, 1000);
    if (diff_chance(r, 30))
        add_buildings(r, 1);
    a[0] = kind(r);
    const TBuildingType *bt = btype(a[0]);
    a[1] = ds.Surface_ScrollX + diff_range(r, 1, 14 - (int8_t)bt->Width + 1);
    a[2] = ds.Surface_ScrollY + diff_range(r, 1, 9 - (int8_t)bt->Height + 1);
}

static void setup_mark(DiffRng *r, int32_t *a)
{
    a[0] = diff_range(r, 0, ds.Game_BuildingCount + 1);
    a[1] = kind(r);
    const TBuildingType *bt = btype(a[1]);
    a[2] = diff_range(r, 1, map_w() - (int8_t)bt->Width + 1);
    a[3] = diff_range(r, 1, map_h() - (int8_t)bt->Height + 1);
    a[4] = diff_pick(r, 0, 0, 1, 2, 3);
}

static void setup_find_spot(DiffRng *r, int32_t *a)
{
    perturb_layers(r, diff_pick(r, 0, 50, 400));
    if (diff_chance(r, 15))  // nearly full
        for (int x = 1; x <= map_w(); x++)
            for (int y = 1; y <= map_h(); y++)
                if (!diff_chance(r, 3))
                    *occupy(x, y) = 0xFF;
    a[0] = kind(r);
    a[1] = cur_system();
    a[2] = cur_planet();
    a[3] = cur_moon();
    if (diff_chance(r, 20)) {  // another body: its surface type picks the tile set
        a[2] = diff_range(r, 1, ds.Galaxy_PlanetCount[a[1] - 1]);
        a[3] = diff_pick(r, 0, 1);
    }
    a[4] = (int32_t)DIFF_SCRATCH;
    a[5] = (int32_t)(DIFF_SCRATCH + 2);
    a[6] = (int32_t)(DIFF_SCRATCH + 4);
    memset(far_host(DIFF_SCRATCH), 0x55, 6);
}

static void setup_auto_place(DiffRng *r, int32_t *a)
{
    seed(r);
    perturb_layers(r, 30);
    if (diff_chance(r, 30))
        ds.Game_CurrentScreen = (int16_t)diff_pick(r, 1, 7, 0x14);
    a[0] = kind(r);
    a[1] = cur_system();
    a[2] = cur_planet();
    a[3] = cur_moon();
    if (diff_chance(r, 25)) {
        a[2] = diff_range(r, 1, ds.Galaxy_PlanetCount[a[1] - 1]);
        a[3] = 0;
    }
}

static void setup_place(DiffRng *r, int32_t *a)
{
    add_buildings(r, diff_range(r, 1, 3));
    perturb_layers(r, 30);
    a[0] = diff_pick(r, ds.Game_BuildingCount, diff_range(r, 1, ds.Game_BuildingCount));
}

static void setup_delete(DiffRng *r, int32_t *a)
{
    add_buildings(r, diff_range(r, 0, 3));
    a[0] = diff_pick(r, 1, ds.Game_BuildingCount, diff_range(r, 1, ds.Game_BuildingCount));
}

static void setup_count(DiffRng *r, int32_t *a)
{
    perturb_buildings(r);
    add_buildings(r, diff_range(r, 0, 8));
    a[0] = diff_pick(r, 0x11, diff_range(r, 1, 25));
    a[1] = cur_system();
    a[2] = cur_planet();
    a[3] = cur_moon();
    if (diff_chance(r, 20))
        a[2] = diff_range(r, 1, 8);
    a[4] = diff_range(r, 0, 1);
}

static void setup_count_kind(DiffRng *r, int32_t *a)
{
    perturb_buildings(r);
    add_buildings(r, diff_range(r, 0, 8));
    a[0] = cur_system();
    a[1] = cur_planet();
    a[2] = cur_moon();
    if (diff_chance(r, 20))
        a[1] = diff_range(r, 1, 8);
}

// --- Radar ---------------------------------------------------------------------------------------

static void setup_radar(DiffRng *r, int32_t *a)
{
    (void)a;
    if (diff_chance(r, 40)) {  // another map, often a small one: the radar at 2x
        free_surface();
        other_map(r);
        Surface_EnterSurface();
    }
    perturb_body(r);
    perturb_buildings(r);
    perturb_layers(r, 60);
    scroll(r);
    if (diff_chance(r, 60)) {  // a working radar station
        add_buildings(r, 1);
        TBuilding *b = bldg(ds.Game_BuildingCount);
        b->Kind = 0x11;
        b->Health = 0;
        b->Flag = (uint8_t)diff_pick(r, 1, 1, 0);
    }
    // A single-scale radar for a big map (the buffer is big enough).
    if (ds.Surface_RadarDouble && diff_chance(r, 30))
        ds.Surface_RadarDouble = 0;
}

static void setup_none(DiffRng *r, int32_t *a)
{
    (void)r;
    (void)a;
}

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    if (!diff_boot("900:click:216,16;1300:quit")) {
        printf("FAILED to reach the surface screen\n");
        return 1;
    }
    DiffState *base = diff_save();
    printf("surface %dx%d, tileset %d, %d buildings\n", map_w(), map_h(), ds.Surface_TileSet,
           ds.Game_BuildingCount);

    diff_Surface_BlitTile.decl = "(src: pointer; tile: word; x, y: integer)";
    diff_Surface_DrawFootprint.decl = "(kind, x, y: integer)";
    diff_Surface_DrawFootprint.near = true;

    const struct {
        DiffFn *fn;
        DiffSetup setup;
        int cases;
    } fns[] = {
        {&diff_Surface_EnterSurface, setup_enter, 6},
        {&diff_Surface_LoadMap, setup_load_map, 20},
        {&diff_Surface_RegisterBuildingAnims, setup_register, 30},
        {&diff_Surface_SetupSurfaceHotspots, setup_can_build, 6},
        {&diff_Surface_DrawBuildingInfo, setup_info, 40},
        {&diff_Surface_ShowBuildingInfoView, setup_info, 10},
        {&diff_Surface_ShowBuildingPicture, setup_info, 10},
        {&diff_Surface_DrawSurfaceFrame, setup_none, 2},
        {&diff_Surface_SetupInfoHotspots, setup_none, 2},
        {&diff_Surface_SetupSimpleInfoHotspots, setup_none, 2},
        {&diff_Surface_DrawModeButtons, setup_none, 2},
        {&diff_Surface_BlinkModeButton, setup_mode, 30},
        {&diff_Surface_DrawKindArrows, setup_mode, 10},
        {&diff_Surface_BlitTile, setup_blit_tile, 20},
        {&diff_Surface_BlitTileInner, setup_blit_tile, 20},
        {&diff_Surface_DrawBuildingPreview, setup_preview, 30},
        {&diff_Surface_DrawBuildingTiles, setup_building_tiles, 20},
        {&diff_Surface_DrawFootprint, setup_footprint, 30},
        {&diff_Surface_DrawMapView, setup_map_view, 40},
        {&diff_Surface_UpdateModeForCell, setup_update_mode, 50},
        {&diff_Surface_AddBuilding, setup_add, 40},
        {&diff_Surface_BuildOnCurrentPlanet, setup_build_here, 30},
        {&diff_Surface_MarkFootprint, setup_mark, 30},
        {&diff_Surface_FindBuildSpot, setup_find_spot, 40},
        {&diff_Surface_AutoPlaceBuilding, setup_auto_place, 30},
        {&diff_Surface_PlaceBuilding, setup_place, 20},
        {&diff_Surface_DeleteBuilding, setup_delete, 20},
        {&diff_Surface_CountBuildings, setup_count, 30},
        {&diff_Surface_CountBuildingsByKind, setup_count_kind, 20},
        {&diff_Surface_RebuildRadar, setup_radar, 30},
        {&diff_Surface_DrawRadar, setup_radar, 20},
        {&diff_Surface_Init, setup_none, 1},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, &base, 1, fns[i].setup, diff_cases(fns[i].cases));
    diff_free(base);
    return diff_finish();
}
