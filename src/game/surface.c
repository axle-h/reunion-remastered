// Unit Surface (2ef2): see surface.h.
#include "game/surface.h"

#include <stdio.h>
#include <string.h>

#include "assets/map.h"
#include "game/blit.h"
#include "game/dseg.h"
#include "game/fixes.h"
#include "game/gfx.h"
#include "game/minmax.h"
#include "game/mouse.h"
#include "game/planetinfo.h"
#include "game/sound.h"
#include "game/system.h"
#include "game/textfile.h"

#define P(lit) ((const uint8_t *)PSTR(lit))

static int16_t cur_planet(void)
{
    return ds.Game_CurPlanet[ds.Game_CurSystem - 1];
}

static int16_t cur_moon(void)
{
    return ds.Game_CurMoon[ds.Game_CurSystem - 1];
}

// Cell (x, y) of a map layer; `size` is 1 for the byte layers and 2 for the
// word layer. The offset wraps like the original's 16-bit arithmetic (x or
// y 0 is below the layer), inside the layer's segment (System_GetSegment).
static void *cell(uint8_t *layer, int16_t x, int16_t y, int size)
{
    return layer + (uint16_t)(x * 60 * size + y * size - 61 * size);
}

static uint8_t *tile_at(int16_t x, int16_t y)
{
    return cell(ds.Surface_TileLayer, x, y, 1);
}

static uint8_t *occupy_at(int16_t x, int16_t y)
{
    return cell(ds.Surface_OccupyLayer, x, y, 1);
}

static uint16_t *building_at(int16_t x, int16_t y)
{
    return cell(ds.Surface_BuildingLayer, x, y, 2);
}

// Record `index` (1-based) of Game_Buildings.
static TBuilding *bldg(int16_t index)
{
    return ds.Game_Buildings + (index - 1);
}

static const TBuildingType *btype(int16_t kind)
{
    return (const TBuildingType *)((const uint8_t *)ds.Surface_BuildingTypes
                                   + (uint16_t)(kind * 0x3F) - 0x3F);
}

// Footprint tile of kind at (column i, row j), 0-based; FFh = no tile.
static uint8_t foot(const TBuildingType *bt, int i, int j)
{
    return bt->Tiles[j][i];
}

static TBody *body_of(int16_t system, int16_t planet, int16_t moon)
{
    int16_t idx = planet;
    if (moon != 0)
        idx = ((const uint8_t *)ds.Galaxy_MoonLists[system - 1])[planet * 9 + moon - 9];
    return ds.Galaxy_Bodies[system - 1] + (idx - 1);
}

// The test that guards the map drawing routines: the body is a colony, has a
// satellite or a base, as 357b:3253 and the screen code see it.
static bool body_shown(const TBody *b)
{
    return (b->Owner == 1 && (b->Colony != 0 || b->_0A != 0)) || (int8_t)b->Satellite > 0
        || b->_09 != 0;
}

static void screen_blit_rect(const uint8_t *src, uint16_t dest_ofs, int16_t w, int16_t h,
                             int16_t src_stride)
{
    Blit_CopyRectStrided(src, gfx_screen() + dest_ofs, w, h, src_stride, 0x140);
}

// ---------------------------------------------------------------------------
// Screen setup
// ---------------------------------------------------------------------------

// 2ef2:004a
void Surface_EnterSurface(void)
{
    PString s, tmp, prefix;

    uint8_t *pic = System_GetMem(0xBCC0);
    Gfx_LoadPic(P("grafika\\designer"), pic, 1, 0x97, 0, 0, 0x140, true, Gfx_PalettePtr(),
                0, 0x6F, 0x40, 0, 0xBE);
    Blit_CopyRect(pic, gfx_screen() + 0x3D40, 0x140, 0x97);
    Mouse_HideCursor();
    Blit_FillRect(0x5D, 0x35, 0xE0, 0x90, 0x40);
    Mouse_ShowCursor();
    ds.Surface_BufA = System_GetMem(0x24FF);
    Blit_CopyRectStrided(pic + 0x69E, ds.Surface_BufA, 0x7B, 0x4D, 0x140, 0x7B);
    System_FreeMem(pic, 0xBCC0);

    System_StrLong(ds.Surface_TileSet, 0, &s, 0xFF);
    ds.Surface_RadarColors = System_GetMem(0x820);
    System_StrLoad(&tmp, PSTR("planets\\radar"));
    System_StrConcat(&tmp, &s);
    Gfx_LoadPic((const uint8_t *)&tmp, ds.Surface_RadarColors, 1, 0x1A, 0, 0, 0x50, false,
                Gfx_PalettePtr(), 0, 0x3F, 0xB0, 0, 0x3F);

    System_StrLong(ds.Surface_TileSet, 0, &s, 0xFF);
    prefix.len = 0;
    if (ds.Game_CurBody->Owner > 1)
        System_StrStore(PSTR("t"), &prefix, 0xFF);
    ds.Surface_BufC = System_GetMem(0xF000);
    System_StrLoad(&tmp, PSTR("planets\\epul"));
    System_StrConcat(&tmp, &prefix);
    System_StrConcat(&tmp, &s);
    Gfx_LoadPic((const uint8_t *)&tmp, ds.Surface_BufC, 1, 0xC0, 0, 0, 0x140, false,
                Gfx_PalettePtr(), 0, 0x3F, 0xB0, 0, 0x3F);

    int16_t fanim = ds.Surface_FanimHeight[ds.Surface_TileSet - 1];
    if (fanim != 0) {
        ds.Surface_BufD = System_GetMem((uint16_t)(fanim * 0x140));
        System_StrLoad(&tmp, PSTR("planets\\fanim"));
        System_StrConcat(&tmp, &s);
        // 0C0h rows whatever the sheet height; LoadPic clips to the picture.
        Gfx_LoadPic((const uint8_t *)&tmp, ds.Surface_BufD, 1, 0xC0, 0, 0, 0x140, false,
                    Gfx_PalettePtr(), 0, 0x3F, 0xB0, 0, 0x3F);
    }

    int16_t felsz = ds.Surface_FelszHeight[ds.Surface_TileSet - 1];
    ds.Surface_BufB = System_GetMem((uint16_t)(felsz * 0x140));
    System_StrLoad(&tmp, PSTR("planets\\felsz"));
    System_StrConcat(&tmp, &s);
    Gfx_LoadPic((const uint8_t *)&tmp, ds.Surface_BufB, 1, (uint16_t)felsz, 0, 0, 0x140,
                false, Gfx_PalettePtr(), 0, 0x3F, 0xB0, 0, 0x3F);

    ds.Surface_TileLayer = System_GetSegment();  // GetMem(0x1518)
    ds.Surface_OccupyLayer = System_GetSegment();  // GetMem(0x1518)
    ds.Surface_BuildingLayer = System_GetSegment();  // GetMem(0x2A30)
    Surface_LoadMap();

    ds.Surface_RadarDouble = ds.Surface_MapWidth < 0x2E && ds.Surface_MapHeight < 0x1F;
    int16_t w = (int16_t)ds.Surface_MapWidth;
    int16_t h = (int16_t)ds.Surface_MapHeight;
    int16_t dx = ds.Surface_RadarDouble ? (int16_t)(w * -2) : (int16_t)-w;
    ds.Surface_RadarX = (int16_t)((dx + 0x5A) / 2 + 1);
    int16_t dy = ds.Surface_RadarDouble ? (int16_t)(h * -2) : (int16_t)-h;
    ds.Surface_RadarY = (int16_t)((dy + 0x3C) / 2 + 0x8B);
    ds.Surface_RadarOfs = (uint16_t)(ds.Surface_RadarY * 0x140 + ds.Surface_RadarX);
    if (!ds.Surface_RadarDouble)
        ds.Surface_RadarBuf = System_GetMem((uint16_t)(w * h));
    else
        ds.Surface_RadarBuf = System_GetMem((uint16_t)(w * 4 * h));
    ds.Surface_RadarStale = 0xFFFF;
}

// 2ef2:03e7
void Surface_LoadMap(void)
{
    PString kind, variant, name;
    const TBody *body = ds.Game_CurBody;
    System_StrLong((int8_t)body->SurfaceType, 0, &kind, 0xFF);
    System_StrLong(ds.Surface_MapVariant, 0, &variant, 0xFF);
    System_StrLoad(&name, PSTR("map\\map"));
    System_StrConcat(&name, &kind);
    System_StrConcat(&name, PSTR("_"));
    System_StrConcat(&name, &variant);
    System_StrConcat(&name, PSTR(".map"));
    char path[256];
    snprintf(path, sizeof path, "%.*s", name.len, name.s);

    struct map m;
    // The original stops with a run-time error when the file can't be read.
    if (!map_load(path, &m))
        return;
    // The original reads the cells into a GetMem'd buffer and frees it at
    // the end (the heap keeps the trace).
    uint16_t size = (uint16_t)(m.width * m.height);
    uint8_t *cells = System_GetMem(size);
    memcpy(cells, m.cells, size);
    for (uint16_t row = 1; row <= (uint16_t)m.height; row++) {
        for (uint16_t col = 1; col <= (uint16_t)m.width; col++) {
            uint16_t c = cells[(uint16_t)(m.width * (row - 1) + col - 1)];
            *tile_at((int16_t)col, (int16_t)row) = (uint8_t)c;
            // MapTemplates[TileSet][c / 20 + 1][c % 20]: a nonzero entry is blocked
            // terrain. Addressed flat, so tiles 240 and up read the next template.
            const uint8_t *templates = &ds.Surface_MapTemplates[0][0][0];
            uint8_t blocked = templates[(uint16_t)(ds.Surface_TileSet - 1) * 0xF0 + c];
            *occupy_at((int16_t)col, (int16_t)row) = blocked == 0 ? 0 : 0xFF;
            *building_at((int16_t)col, (int16_t)row) = 0;
        }
    }
    System_FreeMem(cells, size);
    ds.Surface_MapWidth = (uint16_t)m.width;
    ds.Surface_MapHeight = (uint16_t)m.height;
    map_free(&m);
}

// 2ef2:062f
void Surface_RegisterBuildingAnims(int16_t system, int16_t planet, int16_t moon)
{
    int16_t count = ds.Game_BuildingCount;
    for (int16_t i = 1; i <= count; i++) {
        TBuilding *b = bldg(i);
        if (b->System == system && b->Planet == planet && b->Moon == moon) {
            // The decompile calls these two routines in SpWar; the near calls
            // really go to PlaceBuilding (2a64) and MarkFootprint (25fd).
            if (b->X == 0xFF && b->Y == 0xFF)
                Surface_PlaceBuilding(i);
            if (b->X < 0xFF && b->Y < 0xFF) {
                int16_t stage = (int16_t)((b->Health + 0x27) / 0x28);
                if (stage > 3)
                    stage = 3;
                Surface_MarkFootprint(i, b->Kind, b->X, b->Y, stage);
            }
        }
    }
    if (ds.Game_CurBody->Owner > 1) {
        int16_t x = 0, y = 0;
        bool found = false;
        Surface_FindBuildSpot(0x19, system, planet, moon, &x, &y, &found);
        // BUG? The original marks the footprint even when no spot was found
        // (x and y are then uninitialised); skipped here.
        if (found)
            Surface_MarkFootprint(1, 0x19, x, y, 0);
    }
}

// ---------------------------------------------------------------------------
// Hotspots
// ---------------------------------------------------------------------------

// 2ef2:0791
void Surface_SetupSurfaceHotspots(void)
{
    static const uint8_t empty[1] = {0};
    bool cb = ds.Surface_CanBuild != 0;
    ds.Mouse_HotspotCount = 0xC;
    Mouse_SetHotspot(1, 0x5D, 0x35, 0xE0, 0x90, P("Terrain"), 0, 0);
    Mouse_SetHotspot(2, 0, 0x8A, 0x59, 0x3E, P("Radar"), 0, 0);
    Mouse_SetHotspot(3, 0, 0x40, 0xB, 0x20, cb ? P("Invention up ") : P("No effect"), 1, 0);
    Mouse_SetHotspot(4, 0, 0x61, 0xB, 0x20, cb ? P("Invention down") : P("No effect"), 1, 0);
    Mouse_SetHotspot(5, 0xC, 0x42, 0x4D, 0x3B, cb ? P("Invention Info") : P("No effect"), 1, 0);
    Mouse_SetHotspot(6, 0x59, 0x35, 4, 0x90, P("Left"), 0, 1);
    Mouse_SetHotspot(7, 0x13D, 0x35, 3, 0x90, P("Right"), 0, 2);
    Mouse_SetHotspot(8, 0x5D, 0x31, 0xE0, 4, P("Up"), 0, 3);
    Mouse_SetHotspot(9, 0x5D, 0xC5, 0xE0, 3, P("Down"), 0, 4);
    Mouse_SetHotspot(10, 0, 0x32, 0x2C, 0xD, cb ? P("Build") : empty, 1, 0);
    Mouse_SetHotspot(11, 0x2D, 0x32, 0x2C, 0xD, P("Destroy"), 1, 0);
}

// 2ef2:1441 (the Ghidra fragment at 1472 is its second half)
void Surface_SetupInfoHotspots(void)
{
    ds.Mouse_HotspotCount = 4;
    Mouse_SetHotspot(1, 0x5A, 0x31, 0xE6, 0x97, P("See surface"), 1, 0);
    Mouse_SetHotspot(2, 0xC, 0x42, 0x4D, 0x3B, P("See surface"), 1, 0);
    Mouse_SetHotspot(3, 0, 0x40, 0xB, 0x20, P("Invention up"), 1, 0);
    Mouse_SetHotspot(4, 0, 0x61, 0xB, 0x20, P("Invention down"), 1, 0);
}

// 2ef2:151b
void Surface_SetupSimpleInfoHotspots(void)
{
    ds.Mouse_HotspotCount = 2;
    Mouse_SetHotspot(1, 0x5A, 0x31, 0xE6, 0x97, P("See surface"), 1, 0);
    Mouse_SetHotspot(2, 0xC, 0x42, 0x4D, 0x3B, P("See surface"), 1, 0);
}

// ---------------------------------------------------------------------------
// Info panel and frame
// ---------------------------------------------------------------------------

// Draws "<label><value><suffix>" in the info panel at row `row`.
static void info_line(const PString *label, const PString *value, const PString *suffix,
                      const PString *suffix2, int16_t row)
{
    PString tmp;
    System_StrLoad(&tmp, label);
    System_StrConcat(&tmp, value);
    if (suffix)
        System_StrConcat(&tmp, suffix);
    if (suffix2)
        System_StrConcat(&tmp, suffix2);
    Gfx_DrawTextFC((const uint8_t *)&tmp, 0x62, (int16_t)(row * 9 + 0x46), 0x15);
}

// 2ef2:0a9d (+ fragment 0e00)
void Surface_DrawBuildingInfo(void)
{
    PString s, s2, tmp;
    const TBuildingType *bt = btype(ds.Surface_SelKind);
    int8_t cat = (int8_t)bt->Category;
    // Category 0 reads the three bytes before the table, as the original does.
    const int8_t *rows = &ds.Surface_CategoryRows[0][0] + (cat - 1) * 3;

    Gfx_DrawTextF9((const uint8_t *)bt, 0x75, 0x3B, 0x12);
    System_StrLong(bt->Cost, 0, &s, 0xFF);

    if (ds.Surface_Mode == 3) {
        System_StrLoad(&tmp, PSTR("Cost       : "));
        System_StrConcat(&tmp, &s);
        Gfx_DrawTextFC((const uint8_t *)&tmp, 0x62, 0x46, 0x14);
        for (int16_t i = 1; i <= 3; i++)
            Gfx_DrawTextFC(P(""), 0x62, (int16_t)(i * 9 + 0x46), 0x15);

        System_StrLong(bt->Production, 0, &s, 0xFF);
        if (cat == 2 || cat == 9) {
            System_StrStore(PSTR("Production : mine"), &s, 0xFF);
        } else {
            System_StrLoad(&tmp, PSTR("Production : "));
            System_StrConcat(&tmp, &s);
            System_StrConcat(&tmp, PSTR(" kwh"));
            System_StrStore(&tmp, &s, 0xFF);
        }
        int16_t row = rows[0];
        if (row > 0)
            Gfx_DrawTextFC((const uint8_t *)&s, 0x62, (int16_t)(row * 9 + 0x46), 0x15);

        System_StrLong(bt->Workers, 0, &s, 0xFF);
        System_StrLoad(&tmp, PSTR("Workers    : "));
        System_StrConcat(&tmp, &s);
        System_StrStore(&tmp, &s, 0xFF);
        row = rows[1];
        if (row > 0)
            Gfx_DrawTextFC((const uint8_t *)&s, 0x62, (int16_t)(row * 9 + 0x46), 0x15);

        System_StrLong(bt->Energy, 0, &s, 0xFF);
        System_StrLoad(&tmp, PSTR("Energy     : "));
        System_StrConcat(&tmp, &s);
        System_StrConcat(&tmp, PSTR(" kwh"));
        System_StrStore(&tmp, &s, 0xFF);
        row = rows[2];
        if (row > 0)
            Gfx_DrawTextFC((const uint8_t *)&s, 0x62, (int16_t)(row * 9 + 0x46), 0x15);
    }

    if (ds.Surface_Mode == 4) {
        const TBuilding *b = bldg(ds.Surface_SelBuilding);
        if (b->Flag != 0)
            System_StrStore(PSTR("Active"), &s, 0xFF);
        else
            System_StrStore(PSTR("Passive"), &s, 0xFF);
        info_line(PSTR("Status     : "), &s, nullptr, nullptr, 0);

        int16_t row = rows[0];
        if (row > 0) {
            // Output% of the nominal production, scaled by the power efficiency BD%.
            int32_t v = (int32_t)b->Output * (int32_t)bt->Production;
            v = v / 100;
            v = (int32_t)((uint32_t)v * b->BD);
            v = v / 100;
            System_StrLong(v, 0, &s, 0xFF);
            System_StrLoad(&tmp, &s);
            System_StrConcat(&tmp, PSTR(" kwh"));
            System_StrStore(&tmp, &s, 0xFF);
            if (cat == 2) {
                const TBody *body = ds.Game_CurBody;
                System_StrLong(body->Deposits[0] / 10, 0, &s, 0xFF);
                System_StrLoad(&tmp, &s);
                System_StrConcat(&tmp, PSTR(" t/ptp"));
                System_StrStore(&tmp, &s, 0xFF);
                if (body->Deposits[0] < 10)
                    System_StrStore(PSTR("NONE"), &s, 0xFF);
            }
            if (b->Flag == 0)
                System_StrStore(PSTR("NONE"), &s, 0xFF);
            info_line(PSTR("Production : "), &s, nullptr, nullptr, row);
        }

        row = rows[1];
        if (row > 0) {
            System_StrLong((uint16_t)b->W9, 0, &s, 0xFF);
            System_StrLong(bt->Workers, 0, &s2, 0xFF);
            if (b->Flag == 0)
                System_StrStore(PSTR("0"), &s, 0xFF);
            info_line(PSTR("Workers    : "), &s, PSTR("/"), &s2, row);
        }

        row = rows[2];
        if (row > 0) {
            System_StrLong((uint16_t)b->WB, 0, &s, 0xFF);
            if (b->Flag == 0)
                System_StrStore(PSTR("0"), &s, 0xFF);
            info_line(PSTR("Energy     : "), &s, PSTR(" kwh"), nullptr, row);
        }

        row = rows[2];
        if (row < 0)
            row = rows[1];
        if (row < 0)
            row = rows[0];
        if (row < 0)
            row = 0;
        System_StrLong(b->BD, 0, &s, 0xFF);
        if (b->Flag == 0)
            System_StrStore(PSTR("0"), &s, 0xFF);
        info_line(PSTR("Working    : "), &s, PSTR("%"), nullptr, (int16_t)(row + 1));
    }
    TextFile_ShowBuildingInfo((int16_t)((ds.Surface_SelKind - 1) * 6));
}

// 2ef2:112c
void Surface_ShowBuildingInfoView(void)
{
    PString s, name;
    uint8_t *buf = System_GetMem(0x1950);
    System_StrLong(ds.Surface_SelKind, 0, &s, 0xFF);
    System_StrLoad(&name, PSTR("epulet\\epulet"));
    System_StrConcat(&name, &s);
    Gfx_LoadPic((const uint8_t *)&name, buf, 1, 0x48, 0, 0, 0x5A, false, Gfx_PalettePtr(),
                0, 0x6F, 0x40, 0, 0xBE);
    Mouse_HideCursor();
    Blit_FillRect(0x5D, 0x35, 0xE0, 0x90, 0x40);
    Mouse_ShowCursor();
    Surface_DrawSurfaceFrame();
    Mouse_HideCursor();
    screen_blit_rect(buf + 0xB6, 0x46E4, 0x56, 0x44, 0x5A);
    Mouse_ShowCursor();
    System_FreeMem(buf, 0x1950);
    Surface_DrawBuildingInfo();
}

// 2ef2:1204: like 112c but only replaces the building picture.
void Surface_ShowBuildingPicture(void)
{
    PString s, name;
    uint8_t *buf = System_GetMem(0x1950);
    System_StrLong(ds.Surface_SelKind, 0, &s, 0xFF);
    System_StrLoad(&name, PSTR("epulet\\epulet"));
    System_StrConcat(&name, &s);
    Gfx_LoadPic((const uint8_t *)&name, buf, 1, 0x48, 0, 0, 0x5A, false, Gfx_PalettePtr(),
                0, 0x6F, 0x40, 0, 0xBE);
    Mouse_HideCursor();
    screen_blit_rect(buf + 0xB6, 0x46E4, 0x56, 0x44, 0x5A);
    Mouse_ShowCursor();
    System_FreeMem(buf, 0x1950);
    Surface_DrawBuildingInfo();
}

// 2ef2:12af (+ fragment 1404 = the last blit)
void Surface_DrawSurfaceFrame(void)
{
    static const struct {
        uint16_t src, dest;
        int16_t w, h;
    } pieces[10] = {
        {0x000, 0x46E2, 2, 0x44},  {0x003, 0x473A, 2, 0x44},  {0x00C, 0x4462, 0x5A, 2},
        {0x17D, 0x9BE2, 0x5A, 2},  {0x006, 0xA19E, 2, 0x41},  {0x009, 0xA27A, 2, 0x41},
        {0x2EE, 0x9F1E, 0x6F, 2},  {0x45F, 0x9F8D, 0x6F, 2},  {0x5D0, 0xF2DE, 0x6F, 2},
        {0x741, 0xF34D, 0x6F, 2},
    };
    Mouse_HideCursor();
    for (int i = 0; i < 10; i++)
        screen_blit_rect(ds.Surface_BufA + pieces[i].src, pieces[i].dest, pieces[i].w,
                         pieces[i].h, 0x7B);
    Mouse_ShowCursor();
}

// 2ef2:1589
void Surface_DrawModeButtons(void)
{
    Mouse_HideCursor();
    screen_blit_rect(ds.Surface_BufA + 0x8B2, 0x3E80, 0x2C, 0xD, 0x7B);
    screen_blit_rect(ds.Surface_BufA + 0xEF1, 0x3EAD, 0x2C, 0xD, 0x7B);
    Mouse_ShowCursor();
}

// 2ef2:15de: per frame; the Build / Destroy button alternates at phases 1 and 11.
void Surface_BlinkModeButton(void)
{
    ds.Surface_BlinkCounter = (uint16_t)((ds.Surface_BlinkCounter + 1) % 0x14);
    if (ds.Surface_Mode == 2) {
        int16_t sel = -1;
        if (ds.Surface_BlinkCounter == 1)
            sel = 0x2C;
        if (ds.Surface_BlinkCounter == 0xB)
            sel = 0;
        if (sel == -1)
            return;
        Mouse_HideCursor();
        screen_blit_rect(ds.Surface_BufA + (uint16_t)(sel + 0x8B2), 0x3E80, 0x2C, 0xD, 0x7B);
        Mouse_ShowCursor();
    }
    if (ds.Surface_Mode == 6) {
        int16_t sel = -1;
        if (ds.Surface_BlinkCounter == 1)
            sel = 0x2C;
        if (ds.Surface_BlinkCounter == 0xB)
            sel = 0;
        if (sel != -1) {
            Mouse_HideCursor();
            screen_blit_rect(ds.Surface_BufA + (uint16_t)(sel + 0xEF1), 0x3EAD, 0x2C, 0xD, 0x7B);
            Mouse_ShowCursor();
        }
    }
}

// 2ef2:16c3: the up / down arrows, greyed out at the first / last kind.
void Surface_DrawKindArrows(void)
{
    Mouse_HideCursor();
    int16_t n = ds.Surface_SelKind == 1 ? 1 : 2;
    screen_blit_rect(ds.Surface_BufA + (uint16_t)((n - 1) * 0xC + 0x15AB), 0x5000, 0xC, 0x20, 0x7B);
    n = ds.Surface_SelKind == 0x19 ? 3 : 4;
    screen_blit_rect(ds.Surface_BufA + (uint16_t)((n - 1) * 0xC + 0x15AB), 0x7940, 0xC, 0x20, 0x7B);
    Mouse_ShowCursor();
}

// ---------------------------------------------------------------------------
// Tile drawing
// ---------------------------------------------------------------------------

// 2ef2:1765 (near): one 16x16 piece of the terrain backdrop at (x, y) of the
// 77x64 preview window; the tile argument is unused, the pattern depends on
// the surface type.
static void draw_backdrop_tile(uint16_t x, uint16_t y)
{
    uint16_t ofs;
    if (ds.Surface_TileSet == 1 || ds.Surface_TileSet == 7)
        ofs = 0;
    else if (ds.Surface_TileSet == 8)
        ofs = (uint16_t)(((x >> 4) % 4) * 0x10 + ((y >> 5) % 2) * 0x40 + ((y >> 4) % 2) * 0x1400);
    else
        ofs = (uint16_t)(((x >> 4) % 2) * 0x10 + ((y >> 4) % 2) * 0x1400);
    int16_t w = 0x10, h = 0x10;
    if ((uint16_t)(x + 0x10) > 0x4D)
        w = (int16_t)(0x4D - x);
    if ((uint16_t)(y + 0x10) > 0x40)
        h = (int16_t)(0x40 - y);
    Blit_CopyRect(ds.Surface_BufB + ofs, gfx_screen() + (uint16_t)(x + 0x500C + y * 0x140),
                  w, h);
}

// 2ef2:18cd (near): tile number `tile` (1-based, 20 per row) of BufC to screen (x, y).
static void draw_building_tile(uint16_t tile, int16_t x, int16_t y)
{
    uint16_t t = (uint16_t)(tile - 1);
    Blit_CopyRect(ds.Surface_BufC + (uint16_t)((t % 0x14) * 0x10 + (t / 0x14) * 0x1400),
                  gfx_screen() + (uint16_t)(y * 0x140 + x), 0x10, 0x10);
}

// 2ef2:1924
void Surface_BlitTile(const uint8_t *src, uint16_t tile, int16_t x, int16_t y)
{
    int16_t sy = (int16_t)((y - 1) * 16 + 0x35);
    int16_t sx = (int16_t)((x - 1) * 16 + 0x5D);
    uint16_t ofs = (uint16_t)((tile % 0x14) * 0x10 + (tile / 0x14) * 0x1400);
    Blit_Copy16x16(src + ofs, gfx_screen() + (uint16_t)(sy * 0x140 + sx));
}

// 2ef2:198a
void Surface_BlitTileInner(const uint8_t *src, uint16_t tile, int16_t x, int16_t y)
{
    int16_t sy = (int16_t)((y - 1) * 16 + 0x36);
    int16_t sx = (int16_t)((x - 1) * 16 + 0x5E);
    uint16_t ofs = (uint16_t)((tile % 0x14) * 0x10 + (tile / 0x14) * 0x1400 + 0x141);
    Blit_CopyRect(src + ofs, gfx_screen() + (uint16_t)(sy * 0x140 + sx), 0xE, 0xE);
}

// 2ef2:19f7
void Surface_DrawBuildingPreview(int16_t kind)
{
    if (!body_shown(ds.Game_CurBody))
        return;
    if (ds.Surface_CanBuild != 0)
        Gfx_DrawTextF9((const uint8_t *)btype(ds.Surface_SelKind), 3, 0x82, 0xE);
    const TBuildingType *bt = btype(kind);
    int16_t ox = (int16_t)((0x4D - (int16_t)((int8_t)bt->Width << 4)) / 2 + 0xC);
    int16_t oy = (int16_t)((0x40 - (int16_t)((int8_t)bt->Height << 4)) / 2 + 0x40);
    for (int16_t i = 0; i <= 4; i++) {
        for (int16_t j = 0; j <= 3; j++) {
            // The original also passes the terrain tile at map cell (i + 6, j + 14);
            // the routine ignores it.
            draw_backdrop_tile((uint16_t)(i << 4), (uint16_t)(j << 4));
        }
    }
    if (ds.Surface_CanBuild == 0)
        return;
    int16_t wmax = (int16_t)((int8_t)bt->Width - 1);
    if (wmax < 0)
        return;
    for (int16_t i = 0; i <= wmax; i++) {
        int16_t hmax = (int16_t)((int8_t)bt->Height - 1);
        for (int16_t j = 0; j <= hmax; j++) {
            if (foot(bt, i, j) != 0xFF)
                draw_building_tile((uint16_t)(foot(bt, i, j) + 1), (int16_t)(i * 16 + ox),
                                   (int16_t)(j * 16 + oy));
        }
    }
}

// 2ef2:1b98
void Surface_DrawBuildingTiles(int16_t kind, int16_t x, int16_t y)
{
    const TBuildingType *bt = btype(kind);
    int16_t wmax = (int16_t)((int8_t)bt->Width - 1);
    for (int16_t i = 0; i <= wmax; i++) {
        int16_t hmax = (int16_t)((int8_t)bt->Height - 1);
        for (int16_t j = 0; j <= hmax; j++)
            draw_building_tile((uint16_t)(foot(bt, i, j) + 1), (int16_t)(x + i * 16),
                               (int16_t)(y + j * 16));
    }
}

// 2ef2:1c3c: redraws the cells of one footprint with the terrain or building tile now on the map.
void Surface_DrawFootprint(int16_t kind, int16_t x, int16_t y)
{
    const TBuildingType *bt = btype(kind);
    int16_t dx = (int16_t)(x - ds.Surface_ScrollX);
    int16_t dy = (int16_t)(y - ds.Surface_ScrollY);
    int16_t lw = (int16_t)((int8_t)bt->Width - 1);
    int16_t lh = (int16_t)((int8_t)bt->Height - 1);
    if ((int8_t)bt->Width + dx > 0xE)
        lw = (int16_t)(0xE - dx);
    if ((int8_t)bt->Height + dy > 9)
        lh = (int16_t)(9 - dy);
    for (int16_t i = 0; i <= lw; i++) {
        for (int16_t j = 0; j <= lh; j++) {
            if (foot(bt, i, j) == 0xFF)
                continue;
            int16_t cx = (int16_t)(i + x - ds.Surface_ScrollX);
            int16_t cy = (int16_t)(j + y - ds.Surface_ScrollY);
            uint8_t occ = *occupy_at((int16_t)(x + i), (int16_t)(y + j));
            if (occ == 0)
                Surface_BlitTile(ds.Surface_BufB,
                                 *tile_at((int16_t)(x + i), (int16_t)(y + j)), cx, cy);
            else
                Surface_BlitTile(ds.Surface_BufC, (uint16_t)(occ - 1), cx, cy);
        }
    }
}

// 2ef2:1da3: redraws the 14x9 window; the build cursor's footprint is drawn
// with the inner 14x14 pixels and a coloured outline.
void Surface_DrawMapView(void)
{
    uint16_t mx = (uint16_t)((ds.Mouse_ClickX + 3) / 0x10);
    uint16_t my = (uint16_t)((ds.Mouse_ClickY + 0xB) / 0x10);
    uint16_t x0, y0, x1, y1;
    if (ds.Surface_Mode == 2 && mx > 5 && mx < 0x14 && my > 3 && my < 0xD) {
        const TBuildingType *bt = btype(ds.Surface_SelKind);
        x0 = (uint16_t)(mx - 5);
        y0 = (uint16_t)(my - 3);
        x1 = (uint16_t)(x0 + (uint16_t)MinMax_Min((int8_t)bt->Width - 1, (uint16_t)(0x13 - mx)));
        y1 = (uint16_t)(y0 + (uint16_t)MinMax_Min((int8_t)bt->Height - 1, (uint16_t)(0xC - my)));
    } else {
        x0 = 100;
        y0 = 100;
        x1 = 0;
        y1 = 0;
    }
    if (!body_shown(ds.Game_CurBody))
        return;

    int16_t frame_ofs = (int16_t)((ds.Surface_AnimTileFrame - 1) * 0x100);
    ds.Surface_AnimTileOffset =
        (uint16_t)((int32_t)frame_ofs * ds.Surface_FanimTileCount[ds.Surface_TileSet - 1]);
    const TBuildingType *bt = btype(ds.Surface_SelKind);
    for (uint16_t x = 1; x <= 14; x++) {
        for (uint16_t y = 1; y <= 9; y++) {
            int16_t ax = (int16_t)(x + ds.Surface_ScrollX);
            int16_t ay = (int16_t)(y + ds.Surface_ScrollY);
            uint8_t occ = *occupy_at(ax, ay);
            bool inside = !(x < x0 || x > x1) && !(y < y0 || y > y1);
            int16_t sx = (int16_t)x, sy = (int16_t)y;
            if (!inside) {
                if (occ == 0 || occ == 0xFF) {
                    uint16_t t = *tile_at(ax, ay);
                    if (t < ds.Surface_FelszTiles)
                        Surface_BlitTile(ds.Surface_BufB, t, sx, sy);
                    else
                        Surface_BlitTile(ds.Surface_BufD + ds.Surface_AnimTileOffset,
                                         (uint16_t)(t - ds.Surface_FelszTiles), sx, sy);
                } else {
                    int16_t bi = (int16_t)*building_at(ax, ay);
                    const TBuilding *b = bldg(bi);
                    Surface_BlitTile(ds.Surface_BufC, (uint16_t)(occ - 1), sx, sy);
                    if (b->Flag == 0 && b->Health == 0 && ds.Surface_BlinkOn != 0) {
                        // Three nested outlines flash on finished but unpowered buildings.
                        Gfx_DrawRectOutline((int16_t)((x - 1) * 16 + 0x61), (int16_t)((y - 1) * 16 + 0x39),
                                            (int16_t)(x * 16 + 0x5A), (int16_t)(y * 16 + 0x32), 0x11);
                        Gfx_DrawRectOutline((int16_t)((x - 1) * 16 + 0x62), (int16_t)((y - 1) * 16 + 0x3A),
                                            (int16_t)(x * 16 + 0x59), (int16_t)(y * 16 + 0x31), 0x14);
                        Gfx_DrawRectOutline((int16_t)((x - 1) * 16 + 0x63), (int16_t)((y - 1) * 16 + 0x3B),
                                            (int16_t)(x * 16 + 0x58), (int16_t)(y * 16 + 0x30), 0x11);
                    }
                }
            } else if (occ == 0) {
                // Cursor over free ground: the building tile itself, green outline.
                Surface_BlitTileInner(ds.Surface_BufC, foot(bt, x - x0, y - y0), sx, sy);
                Gfx_DrawRectOutline((int16_t)((x - 1) * 16 + 0x5E), (int16_t)((y - 1) * 16 + 0x36),
                                    (int16_t)(x * 16 + 0x5D), (int16_t)(y * 16 + 0x35), 0x0A);
            } else {
                // Cursor over something in the way: that cell, red outline.
                if (occ == 0xFF) {
                    uint16_t t = *tile_at(ax, ay);
                    if (t < ds.Surface_FelszTiles)
                        Surface_BlitTileInner(ds.Surface_BufB, t, sx, sy);
                    else
                        Surface_BlitTileInner(ds.Surface_BufD + ds.Surface_AnimTileOffset,
                                              (uint16_t)(t - ds.Surface_FelszTiles), sx, sy);
                } else {
                    Surface_BlitTileInner(ds.Surface_BufC, (uint16_t)(occ - 1), sx, sy);
                }
                Gfx_DrawRectOutline((int16_t)((x - 1) * 16 + 0x5E), (int16_t)((y - 1) * 16 + 0x36),
                                    (int16_t)(x * 16 + 0x5D), (int16_t)(y * 16 + 0x35), 0x14);
            }
        }
    }
    ds.Surface_BlinkOn = ds.Surface_BlinkOn == 0;
}

// ---------------------------------------------------------------------------
// Placing and removing buildings
// ---------------------------------------------------------------------------

// 2ef2:22df: x, y = the cell under the mouse; adjusts Mode and SelBuilding / SelKind.
void Surface_UpdateModeForCell(int16_t x, int16_t y)
{
    const TBody *body = ds.Game_CurBody;
    if (ds.Surface_Mode == 1 && body->Owner == 1) {
        ds.Surface_SelBuilding = (int16_t)*building_at(x, y);
        if (ds.Surface_SelBuilding != 0) {
            ds.Surface_SelKind = bldg(ds.Surface_SelBuilding)->Kind;
            ds.Surface_Mode = 4;
        }
    }
    if (ds.Surface_Mode == 2) {
        const TBuildingType *bt = btype(ds.Surface_SelKind);
        int16_t w = (int8_t)bt->Width, h = (int8_t)bt->Height;
        bool ok = (int16_t)(h + y - 1) <= (int16_t)ds.Surface_MapHeight
               && (int16_t)(w + x - 1) <= (int16_t)ds.Surface_MapWidth;
        if (ok && (uint16_t)(w - 1) < 0x8000) {
            for (uint16_t i = 0;; i++) {
                if ((uint16_t)(h - 1) < 0x8000) {
                    for (uint16_t j = 0;; j++) {
                        if (foot(bt, i, j) != 0xFF
                            && *occupy_at((int16_t)(x + i), (int16_t)(y + j)) != 0)
                            ok = false;
                        if (j == (uint16_t)(h - 1))
                            break;
                    }
                }
                if (i == (uint16_t)(w - 1))
                    break;
            }
        }
        if (!ok)
            ds.Surface_Mode = 1;
    }
    if (ds.Surface_Mode == 6 && body->Owner == 1) {
        ds.Surface_SelBuilding = (int16_t)*building_at(x, y);
        if (ds.Surface_SelBuilding == 0)
            ds.Surface_Mode = 1;
        else
            ds.Surface_SelKind = bldg(ds.Surface_SelBuilding)->Kind;
    }
}

// 2ef2:2463
void Surface_AddBuilding(uint8_t kind, uint8_t system, uint8_t planet, uint8_t moon, uint8_t x,
                         uint8_t y)
{
    int16_t idx = ds.Game_BuildingCount;
    int8_t surface = (int8_t)body_of(system, planet, moon)->SurfaceType;
    // TBuildingType._12 flags (+12h..+1Eh), indexed from +13h by surface type:
    // nonzero = allowed there, and the value scales the Output.
    int8_t factor = (int8_t)((const uint8_t *)btype(kind))[0x13 + surface];
    if (factor != 0 && ds.Game_BuildingCount < 1000) {
        TBuilding *b = ds.Game_Buildings + idx;
        ds.Game_BuildingCount++;
        b->Kind = kind;
        b->System = system;
        b->Planet = planet;
        b->Moon = moon;
        b->Health = (uint8_t)(System_Random(0x3C) + 100);
        b->X = x;
        b->Y = y;
        b->Output = (uint8_t)(factor * 40 + System_Random(0x50));
        b->Flag = 0;
        b->W9 = 0;
        b->WB = 0;
        b->BD = 0;
        PlanetInfo_BalanceBuildingPower(system, planet, moon);
    }
}

// 2ef2:2581
void Surface_BuildOnCurrentPlanet(int16_t kind, int16_t x, int16_t y)
{
    // BUG? A full building table plays the 'x' sound; the 'buildin' branch is skipped.
    if (ds.Game_BuildingCount > 999)
        Sound_PlayButtonSfx(P("x"));
    if (ds.Game_BuildingCount < 1000) {
        Sound_PlayButtonSfx(P("buildin"));
        Surface_AddBuilding((uint8_t)kind, (uint8_t)ds.Game_CurSystem, (uint8_t)cur_planet(),
                            (uint8_t)cur_moon(), (uint8_t)x, (uint8_t)y);
        ds.Surface_SelBuilding = ds.Game_BuildingCount;
        Surface_MarkFootprint(ds.Game_BuildingCount, kind, x, y, 3);
        Surface_DrawFootprint(kind, x, y);
        Surface_RebuildRadar();
    }
}

// 2ef2:25fd
void Surface_MarkFootprint(int16_t index, int16_t kind, int16_t x, int16_t y, int16_t mode)
{
    const TBuildingType *bt = btype(kind);
    int16_t wmax = (int16_t)((int8_t)bt->Width - 1);
    for (int16_t i = 0; i <= wmax; i++) {
        int16_t hmax = (int16_t)((int8_t)bt->Height - 1);
        for (int16_t j = 0; j <= hmax; j++) {
            if (foot(bt, i, j) == 0xFF)
                continue;
            if (mode == 0)
                *occupy_at((int16_t)(x + i), (int16_t)(y + j)) = (uint8_t)(foot(bt, i, j) + 1);
            else
                *occupy_at((int16_t)(x + i), (int16_t)(y + j)) = (uint8_t)(mode + 0xDC);
            *building_at((int16_t)(x + i), (int16_t)(y + j)) = (uint16_t)index;
        }
    }
}

// 2ef2:270b (nested in FindBuildSpot): are all cells of the footprint free?
// The original also tests a local of FindBuildSpot that aliases the result
// variable it has just set, so that test always passes.
static bool footprint_fits(int16_t kind, int16_t x, int16_t y)
{
    const TBuildingType *bt = btype(kind);
    bool ok = true;
    uint16_t w = (uint16_t)((int8_t)bt->Width - 1);
    if (w < 0x8000) {
        for (uint16_t i = 0;; i++) {
            uint16_t h = (uint16_t)((int8_t)bt->Height - 1);
            if (h < 0x8000) {
                for (uint16_t j = 0;; j++) {
                    if (*occupy_at((int16_t)(x + i), (int16_t)(y + j)) != 0)
                        ok = false;
                    if (j == h)
                        break;
                }
            }
            if (i == w)
                break;
        }
    }
    return ok;
}

// 2ef2:27a5
void Surface_FindBuildSpot(int16_t kind, int16_t system, int16_t planet, int16_t moon, int16_t *x,
                           int16_t *y, bool *found)
{
    const TBody *body = body_of(system, planet, moon);
    ds.Surface_TileSet = ds.Surface_Tileset[(int8_t)body->SurfaceType - 1];
    const TBuildingType *bt = btype(kind);
    int16_t cx = (int16_t)(((int16_t)ds.Surface_MapWidth + 1 - (int8_t)bt->Width) / 2);
    int16_t cy = (int16_t)(((int16_t)ds.Surface_MapHeight + 1 - (int8_t)bt->Height) / 2);
    if (cx - 1 >= 0) {
        for (int16_t e = 0;; e++) {
            // Ring e around the centre: the top and bottom rows, then the left and right columns.
            for (int16_t a = (int16_t)(cx - e); a <= cx + e; a++) {
                if (footprint_fits(kind, a, (int16_t)(cy - e))) {
                    *found = true;
                    *x = a;
                    *y = (int16_t)(cy - e);
                    return;
                }
                if (footprint_fits(kind, a, (int16_t)(cy + e))) {
                    *found = true;
                    *x = a;
                    *y = (int16_t)(cy + e);
                    return;
                }
            }
            for (int16_t c = (int16_t)(cy - e); c <= cy + e; c++) {
                if (footprint_fits(kind, (int16_t)(cx - e), c)) {
                    *found = true;
                    *x = (int16_t)(cx - e);
                    *y = c;
                    return;
                }
                if (footprint_fits(kind, (int16_t)(cx + e), c)) {
                    *found = true;
                    *x = (int16_t)(cx + e);
                    *y = c;
                    return;
                }
            }
            if (e == cx - 1)
                break;
        }
    }
    *found = false;
}

// 2ef2:29b1
void Surface_AutoPlaceBuilding(int16_t kind, int16_t system, int16_t planet, int16_t moon)
{
    if (ds.Game_CurrentScreen == 0x14 && system == ds.Game_CurSystem && planet == cur_planet()
        && moon == cur_moon()) {
        int16_t x = 0, y = 0;
        bool found = false;
        Surface_FindBuildSpot(kind, system, planet, moon, &x, &y, &found);
        if (found) {
            Surface_AddBuilding((uint8_t)kind, (uint8_t)system, (uint8_t)planet, (uint8_t)moon,
                                (uint8_t)x, (uint8_t)y);
            Surface_RegisterBuildingAnims(ds.Game_CurSystem, cur_planet(), cur_moon());
            Surface_DrawMapView();
            Surface_RebuildRadar();
        } else if (fixes.autoplace_when_full) {
            // The original drops the building. Port fix (game/fixes.h): it
            // waits unplaced, as on a body that is not shown.
            Surface_AddBuilding((uint8_t)kind, (uint8_t)system, (uint8_t)planet, (uint8_t)moon, 0xFF, 0xFF);
        }
    } else {
        Surface_AddBuilding((uint8_t)kind, (uint8_t)system, (uint8_t)planet, (uint8_t)moon, 0xFF, 0xFF);
    }
}

// 2ef2:2a64
void Surface_PlaceBuilding(int16_t index)
{
    TBuilding *b = bldg(index);
    int16_t x = 0, y = 0;
    bool found = false;
    Surface_FindBuildSpot(b->Kind, b->System, b->Planet, b->Moon, &x, &y, &found);
    if (found) {
        b->X = (uint8_t)x;
        b->Y = (uint8_t)y;
    }
}

// 2ef2:2ac5: shifts the later records down (14 bytes each).
void Surface_DeleteBuilding(int16_t index)
{
    int16_t last = (int16_t)(ds.Game_BuildingCount - 1);
    for (int16_t i = index; i <= last; i++)
        System_MoveFwd(bldg((int16_t)(i + 1)), bldg(i), 14);
    ds.Game_BuildingCount--;
}

// 2ef2:2b1a
int16_t Surface_CountBuildings(int16_t kind, int16_t system, int16_t planet, int16_t moon,
                               bool activeOnly)
{
    int16_t n = 0;
    for (int16_t i = 1; i <= ds.Game_BuildingCount; i++) {
        const TBuilding *b = bldg(i);
        if (b->System == system && b->Planet == planet && b->Moon == moon && b->Kind == kind
            && b->Health == 0 && (!activeOnly || b->Flag != 0))
            n++;
    }
    return n;
}

// 2ef2:2b9e
void Surface_CountBuildingsByKind(int16_t system, int16_t planet, int16_t moon)
{
    for (int flag = 0; flag <= 1; flag++)
        for (int kind = 1; kind <= 25; kind++)
            ds.Surface_KindCounts[flag][kind - 1] = 0;
    for (int16_t i = 1; i <= ds.Game_BuildingCount; i++) {
        const TBuilding *b = bldg(i);
        if (b->System == system && b->Planet == planet && b->Moon == moon && b->Health == 0)
            ds.Surface_KindCounts[(int8_t)b->Flag][b->Kind - 1]++;
    }
}

// ---------------------------------------------------------------------------
// Radar
// ---------------------------------------------------------------------------

// Colour source of cell (x, y) in RadarColors: the building tile, or the
// terrain tile (one row further down in the sheet) when nothing stands there.
static uint8_t *radar_source(int16_t x, int16_t y)
{
    uint16_t v = (uint16_t)(*occupy_at((int16_t)(x + 1), (int16_t)(y + 1)) - 1);
    uint16_t ofs;
    if (v == 0xFFFF || v == 0xFE) {
        uint16_t t = *tile_at((int16_t)(x + 1), (int16_t)(y + 1));
        ofs = (uint16_t)((t / 0x14) * 0xA0 + (t % 0x14) * 2 + 0x28);
    } else {
        ofs = (uint16_t)((v / 0x14) * 0xA0 + (v % 0x14) * 2);
    }
    return ds.Surface_RadarColors + ofs;
}

static void radar_viewport(void)
{
    if (ds.Surface_RadarDouble) {
        Gfx_DrawRectOutline((int16_t)(ds.Surface_RadarX + 1 + ds.Surface_ScrollX * 2),
                            (int16_t)(ds.Surface_RadarY + 1 + ds.Surface_ScrollY * 2),
                            (int16_t)(ds.Surface_RadarX + ds.Surface_ScrollX * 2 + 0x1C),
                            (int16_t)(ds.Surface_RadarY + ds.Surface_ScrollY * 2 + 0x12), 0x70);
    } else {
        Gfx_DrawRectOutline((int16_t)(ds.Surface_RadarX + 1 + ds.Surface_ScrollX),
                            (int16_t)(ds.Surface_RadarY + 1 + ds.Surface_ScrollY),
                            (int16_t)(ds.Surface_RadarX + ds.Surface_ScrollX + 0xE),
                            (int16_t)(ds.Surface_RadarY + ds.Surface_ScrollY + 9), 0x70);
    }
}

// 2ef2:2c52: needs an active radar building (kind 11h); without one the
// area is blanked. The bitmap is drawn on the screen, then saved to RadarBuf.
void Surface_RebuildRadar(void)
{
    if (!body_shown(ds.Game_CurBody))
        return;
    bool radar = Surface_CountBuildings(0x11, ds.Game_CurSystem, cur_planet(), cur_moon(), true) > 0;
    Gfx_WaitVRetrace();
    Mouse_HideCursor();
    int16_t w = (int16_t)ds.Surface_MapWidth;
    int16_t h = (int16_t)ds.Surface_MapHeight;
    int16_t rx = ds.Surface_RadarX, ry = ds.Surface_RadarY;
    uint8_t *screen = gfx_screen();
    if (ds.Surface_RadarDouble) {
        if (radar) {
            for (uint16_t x = 0; x <= (uint16_t)(w - 1); x++) {
                for (uint16_t y = 0; y <= (uint16_t)(h - 1); y++) {
                    const uint8_t *src = radar_source((int16_t)x, (int16_t)y);
                    uint16_t dst = (uint16_t)(x * 2 + y * 0x280 + ds.Surface_RadarOfs);
                    screen[dst] = src[0];
                    screen[(uint16_t)(dst + 1)] = src[1];
                    screen[(uint16_t)(dst + 0x140)] = src[0x50];
                    screen[(uint16_t)(dst + 0x141)] = src[0x51];
                }
            }
        }
        if (!radar)
            Blit_FillRect(rx, ry, (int16_t)(w << 1), (int16_t)(h << 1), 0x40);
        Gfx_DrawRectOutline(rx, ry, (int16_t)(rx + w * 2 + 1), (int16_t)(ry + h * 2 + 1), 0xA0);
        Mouse_HideCursor();
        Blit_CopyRectStrided(screen + ds.Surface_RadarOfs, ds.Surface_RadarBuf,
                             (int16_t)(w << 1), (int16_t)(h << 1), 0x140, (int16_t)(w << 1));
        radar_viewport();
    }
    if (!ds.Surface_RadarDouble) {
        if (radar) {
            for (uint16_t x = 0; x <= (uint16_t)(w - 1); x++) {
                for (uint16_t y = 0; y <= (uint16_t)(h - 1); y++) {
                    const uint8_t *src = radar_source((int16_t)x, (int16_t)y);
                    screen[(uint16_t)(y * 0x140 + x + ds.Surface_RadarOfs)] = src[0];
                }
            }
        }
        if (!radar)
            Blit_FillRect(rx, ry, w, h, 0x40);
        Gfx_DrawRectOutline(rx, ry, (int16_t)(rx + w + 1), (int16_t)(ry + h + 1), 0xA0);
        Mouse_HideCursor();
        Blit_CopyRectStrided(screen + ds.Surface_RadarOfs, ds.Surface_RadarBuf, w, h,
                             0x140, w);
        radar_viewport();
    }
    Mouse_ShowCursor();
}

// 2ef2:312f: the saved bitmap and the viewport rectangle.
void Surface_DrawRadar(void)
{
    if (!body_shown(ds.Game_CurBody))
        return;
    Gfx_WaitVRetrace();
    Mouse_HideCursor();
    int16_t w = (int16_t)ds.Surface_MapWidth;
    int16_t h = (int16_t)ds.Surface_MapHeight;
    if (ds.Surface_RadarDouble) {
        Blit_CopyRectStrided(ds.Surface_RadarBuf, gfx_screen() + ds.Surface_RadarOfs,
                             (int16_t)(w << 1), (int16_t)(h << 1), (int16_t)(w << 1), 0x140);
        radar_viewport();
    }
    if (!ds.Surface_RadarDouble) {
        Blit_CopyRectStrided(ds.Surface_RadarBuf, gfx_screen() + ds.Surface_RadarOfs, w,
                             h, w, 0x140);
        radar_viewport();
    }
    Mouse_ShowCursor();
}

// 2ef2:329e
void Surface_Init(void)
{
    ds.Game_FindBuildingProc = Surface_CountBuildings;
}
