#include "game/planetlist.h"

#include "game/blit.h"
#include "game/gfx.h"
#include "game/minmax.h"
#include "game/mouse.h"
#include "game/system.h"

// Not ported yet: see stubs.c or planetinfo.h.
#include "game/planetinfo.h"

static PlanetListEntry *entry_at(int16_t i)
{
    return (PlanetListEntry *)ds.PlanetList_Entries + (i - 1);
}

// 1390:000b
void PlanetList_SetupScrollBarHotspot(void)
{
    ds.Mouse_HotspotCount = 1;
    Mouse_SetHotspot(1, 0x134, 0x37, 0xC, 0x8C, (const uint8_t *)PSTR("Scroll bar"), 0, 0);
}

// 1390:0047
void PlanetList_AddListEntry(int16_t sys, int16_t planet, int16_t moon, int16_t bodyIdx)
{
    const TBody *b = ds.Galaxy_Bodies[sys - 1] + (bodyIdx - 1);

    // The original's flags, in the order it computes them.
    bool colony = b->Owner == 1 && b->Colony != 0;
    bool miner = b->Owner == 1 && b->Colony == 0 && b->_0A != 0;
    // Surface::Tileset[SurfaceType] indexes SurfaceColonisable (both 1-based).
    int16_t tileset = ds.Surface_Tileset[(int8_t)b->SurfaceType - 1];
    bool accomodable = b->Colony == 0 && b->Miner == 0 && b->InfoLevel > 0x1D && b->Surveyed != 0
                       && b->Owner < 2 && ds.Galaxy_SurfaceColonisable[tileset - 1] != 0;
    bool mineable = b->Owner == 0 && b->Colony == 0 && b->Miner == 0 && b->_0A == 0
                    && b->InfoLevel > 9 && b->_03 != 0;
    if (accomodable)
        mineable = false;
    bool notAlien = b->Owner < 2;
    // The original also reads Hostile and Status for owners 0 and 1 (below
    // the arrays), but uses them only for aliens.
    bool known = false;
    int8_t status = 0;
    if (!notAlien) {
        known = ds.Races_Hostile[b->Owner - 1] != 0 || b->InfoLevel >= 0x28;
        status = ds.Races_Races[b->Owner - 2].Status;
    }
    bool aliens = !notAlien && ((known && status == 4) || (!known && b->InfoLevel >= 0x1E));
    bool enemy = !notAlien && status == 2 && known;
    bool friendly = !notAlien && status == 6 && known;

    int16_t kind = 0;
    if (ds.PlanetList_Mode == 1) {
        kind = colony;
        if (miner)
            kind = 2;
    } else if (ds.PlanetList_Mode == 2) {
        kind = accomodable;
        if (mineable)
            kind = 2;
    } else if (ds.PlanetList_Mode == 3) {
        kind = aliens;
        if (enemy)
            kind = 2;
        if (friendly)
            kind = 3;
    }
    if (kind != 0) {
        PlanetListEntry *e = entry_at(++ds.PlanetList_Count);
        e->Kind = kind;
        e->System = sys;
        e->Planet = planet;
        e->Moon = moon;
        if (moon > 0) {
            e = entry_at(++ds.PlanetList_Count);
            e->Kind = 4;
            e->System = sys;
            e->Planet = planet;
            e->Moon = moon;
        }
    }
}

// 1390:02b6
void PlanetList_BuildList(void)
{
    ds.PlanetList_Count = 0;
    for (int16_t sys = 1;; sys++) {
        int16_t planets;
        if (ds.Galaxy_SystemStatus[sys - 1] == 1 && (planets = ds.Galaxy_PlanetCount[sys - 1]) > 0) {
            for (int16_t p = 1;; p++) {
                const TMoonList *ml = ds.Galaxy_MoonLists[sys - 1] + (p - 1);
                PlanetList_AddListEntry(sys, p, 0, p);
                uint8_t moons = ml->Count;
                if (moons != 0) {
                    for (uint16_t m = 1;; m++) {
                        PlanetList_AddListEntry(sys, p, (int16_t)m, ml->Moons[m - 1]);
                        if (m == moons)
                            break;
                    }
                }
                if (p == planets)
                    break;
            }
        }
        if (sys == 8)
            break;
    }
}

// 1390:03e9
void PlanetList_ListKindCaption(PString *result, int16_t mode, int16_t kind)
{
    result->len = 0;
#define CAP(text) System_StrStore(PSTR(text), result, 0xFF)
    if (mode == 1) {
        if (kind == 1)
            CAP("Colony on ");
        if (kind == 2)
            CAP("Miner station on ");
    } else if (mode == 2) {
        if (kind == 1)
            CAP("Accomodable ");
        if (kind == 2)
            CAP("Mineable   ");
    } else if (mode == 3) {
        if (kind == 1)
            CAP("Aliens on       ");
        if (kind == 2)
            CAP("Enemy colony on ");
        if (kind == 3)
            CAP("Friendly colony on ");
    }
#undef CAP
}

// 1390:04cc
void PlanetList_DrawList(void)
{
    int16_t rows = (int16_t)MinMax_Min(15, MinMax_Max(ds.PlanetList_Count - ds.PlanetList_Top + 1, 0));
    int16_t shown = rows;
    for (int16_t row = 1; rows > 0; row++) {
        int16_t i = row + ds.PlanetList_Top - 1;
        const PlanetListEntry *e = entry_at(i);
        PString tmp, cap, line, name;
        int16_t y = (int16_t)(row * 9 + 0x30);
        if (e->Kind < 4) {
            PlanetList_ListKindCaption(&tmp, ds.PlanetList_Mode, e->Kind);
            System_StrStore(&tmp, &cap, 0xFF);
            if (cap.len != 0) {
                Gfx_DrawTextFC((const uint8_t *)&cap, 10, y, cap.len);
                int16_t x = (int16_t)(cap.len * 6 + 10);
                if (e->Moon == 0) {
                    PlanetInfo_GetBodyLocationName(&tmp, e->System, e->Planet, e->Moon);
                    Gfx_DrawTextF9((const uint8_t *)&tmp, x, y, (int16_t)((0x133 - x) / 6));
                } else {
                    System_StrLoad(&line, (const PString *)&ds.Names_Systems[e->System - 1]);
                    System_StrConcat(&line, PSTR(" system planet "));
                    PlanetInfo_GetBodyName(&name, e->System, e->Planet, 0);
                    System_StrConcat(&line, &name);
                    Gfx_DrawTextF9((const uint8_t *)&line, x, y, (int16_t)((0x133 - x) / 6));
                }
            }
        } else {
            // A moon row: blank the caption of the planet row above and write
            // "moon <name>" after it.
            PlanetList_ListKindCaption(&tmp, ds.PlanetList_Mode, entry_at(i - 1)->Kind);
            System_StrStore(&tmp, &cap, 0xFF);
            int16_t x = (int16_t)(cap.len * 6 + 10);
            Blit_FillRect(10, y, (int16_t)(cap.len * 6 - 1), 8, 0x40);
            System_StrLoad(&line, PSTR("moon "));
            PlanetInfo_GetBodyName(&name, e->System, e->Planet, e->Moon);
            System_StrConcat(&line, &name);
            Gfx_DrawTextF9((const uint8_t *)&line, x, y, (int16_t)((0x133 - x) / 6));
        }
        if (row == shown)
            break;
    }
    Mouse_HideCursor();
    Blit_FillRect(7, (int16_t)((shown + 1) * 9 + 0x30), 0x120, (int16_t)((shown + 1) * -9 + 0x93), 0x40);
    Mouse_ShowCursor();
    PlanetList_DrawScrollBarThumb();
}

// 1390:0735
void PlanetList_RefreshList(void)
{
    PlanetList_BuildList();
    PlanetList_DrawList();
}

// 1390:0741
void PlanetList_DrawScrollBarThumb(void)
{
    int16_t y, h;
    if (ds.PlanetList_Count < 0x10) {
        y = 0;
        h = 0x89;
    } else {
        y = (int16_t)(((ds.PlanetList_Top - 1) * 0x87) / (ds.PlanetList_Count - 0xC));
        h = (int16_t)((0x834 / (int32_t)(ds.PlanetList_Count - 0xC)) / 5);
    }
    Mouse_HideCursor();
    Blit_FillRect(0x137, 0x38, 3, 0x8B, 0x40);
    Blit_FillRect(0x137, (int16_t)(y + 0x38), 3, h, 0x54);
    Mouse_ShowCursor();
}

// 1390:07c0
void PlanetList_DragScrollBar(void)
{
    while (ds.Mouse_Buttons != 0) {
        Mouse_Poll();
        int16_t old = ds.PlanetList_Top;
        ds.PlanetList_Top = (int16_t)(((ds.Mouse_ClickY - 0x37) * (ds.PlanetList_Count - 0xC)) / 0x8C + 1);
        if (ds.PlanetList_Count - 0xE < ds.PlanetList_Top)
            ds.PlanetList_Top = (int16_t)(ds.PlanetList_Count - 0xE);
        if (ds.PlanetList_Top < 1)
            ds.PlanetList_Top = 1;
        if (ds.PlanetList_Top != old)
            PlanetList_DrawList();
    }
}
