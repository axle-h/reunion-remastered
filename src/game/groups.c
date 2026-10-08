// Unit Groups (2b8d): group lists, the group detail screen (0x16) and the New Group
// screen (0x15).
#include "game/groups.h"

#include "game/blit.h"
#include "game/gfx.h"
#include "game/gfxpath.h"
#include "game/mouse.h"
#include "game/satellite.h"
#include "game/sound.h"
#include "game/system.h"
#include "game/textfile.h"

// Routines of units that are not ported yet (stubs in stubs.c).
void PlanetInfo_SelectCurBody(void);                                            // 357b:3253
void PlanetInfo_GetBodyLocationName(PString *result, int16_t sys, int16_t planet, int16_t moon); // 357b:355e
void Intel_DisbandGroup(int16_t list, int16_t group);                           // 29b9:0b93

// Unnamed globals.
#define ICONS_PTR (*(FarPtr *)ds_at(0x78A6))   // grafika\benyil slot icons (0x2300 bytes)
#define PF_PTR (*(FarPtr *)ds_at(0x789C))      // planet force group at the shown group's location
// GroupsScreen's working variables, globals in the original (2b8d:02bb..0605).
#define SAVED_CAT (*(int16_t *)ds_at(0x7892))     // SlotCategory before page up / down
#define SLOT_S (*(int16_t *)ds_at(0x7894))        // ship slot of the clicked button
#define SLOT_W (*(int16_t *)ds_at(0x7896))        // weapon column
#define SLOT_AVAIL (*(int16_t *)ds_at(0x7898))    // stock the unit would come from
#define SLOT_RETURNED (*(int16_t *)ds_at(0x78A2)) // weapons sent back to stock

// Slot layout blobs (Groups::SlotLayouts, found through TypeSlots[kind].Layout[cat]):
//   byte 0: number of ship slots (rows)     byte 1: number of weapon columns
//   from byte 2: weapon items of 14 bytes:  product, stock index, string[5], x:word, y:byte, x2:word, y2:byte
//   then ship items of 25 bytes:            product, stock index, string[8], x:word, y:byte, ..., 4 capacity bytes
// so item w is at blob + w * 14 - 12 and ship s at blob + s * 25 + 0x21, with the
// weapon capacity multiplier of (s, w) at blob + s * 25 + w + 0x35.
static const uint8_t *weapon_item(const uint8_t *layout, int w) { return layout + w * 14 - 12; }
static const uint8_t *ship_item(const uint8_t *layout, int s) { return layout + s * 25 + 0x21; }
static uint8_t capacity(const uint8_t *layout, int s, int w) { return layout[s * 25 + w + 0x35]; }

#define PRODUCT(a) (&ds.Research_Products[(a) - 1])

// Byte tables addressed by Kind at DS:0BE5 .. 0BF9 (rows of Units / slot counts for the
// empty tests).
static uint8_t kind_table(uint16_t base, uint8_t kind) { return *(uint8_t *)ds_at(base + kind); }

static bool units_empty(const TGroup *g, uint16_t count_tbl, uint16_t row_a, uint16_t row_b)
{
    bool empty = true;
    unsigned count = kind_table(count_tbl, g->Kind);
    for (int pass = 0; pass < 2; pass++) {
        unsigned row = kind_table(pass == 0 ? row_a : row_b, g->Kind);
        if (count == 0)
            continue;
        for (unsigned t = 1;; t++) {
            // Units[row][t].ShipType, addressed as in the original: g + 40 * row + 10 * t - 21
            const int16_t *ship_type = (const int16_t *)((const uint8_t *)g + row * 40 + t * 10 - 21);
            empty = empty && *ship_type == 0;
            if (t == count)
                break;
        }
    }
    return empty;
}

// 2b8d:0002 screen 0x16
static void group_changed(void)
{
    Groups_FirstSlotCategory();
    Groups_SetupGroupsHotspots();
    Groups_DrawGroupPanel(true);
    if (ds.Game_GroupList == 0)
        ds.Game_SelGroup0 = ds.Game_SelGroup;
    else
        ds.Game_SelGroup1 = ds.Game_SelGroup;
    Satellite_UpdateGroupsMenu();
}

static int16_t *planet_force_item(uint8_t b)
{
    return (int16_t *)((uint8_t *)far_host(PF_PTR) + 0x83 + b * 2);
}

// The stock a slot draws from: the home planet's product stock or the planet force's item.
static int16_t *slot_stock(const uint8_t *item)
{
    return ds.Groups_AtHomePlanet ? &PRODUCT(item[0])->Stock : planet_force_item(item[1]);
}

// 2b8d:0002 screen 0x16 handler: group detail
void Groups_GroupsScreen(void)
{
    if (ds.Game_ScreenEntered) {
        ds.Groups_PlanetForceMode = ds.Game_PrevScreen == 0x14;
        if (ds.Groups_PlanetForceMode) {
            ds.Game_ShownGroupCount = ds.Game_PlanetForceCount;
            ds.Game_SelGroup = ds.Game_SelGroup1;
            ds.Game_GroupList = 1;
            for (int16_t i = 1; i <= ds.Game_PlanetForceCount; i++) {
                const TGroup *g = Groups_At(1, i);
                if (g->System == ds.Game_CurSystem
                    && g->Planet == ds.Game_CurPlanet[ds.Game_CurSystem - 1]
                    && g->Moon == ds.Game_CurMoon[ds.Game_CurSystem - 1]) {
                    ds.Game_SelGroup = i;
                    ds.Game_SelGroup1 = i;
                }
            }
        }
        ds.Game_SatButtons = -1;
        Satellite_UpdateGroupsMenu();
        Groups_FirstSlotCategory();
        GfxPath_ShowScreenBackdrop();
        Groups_LoadGroupsGfx();
        Groups_SetupGroupsHotspots();
        Groups_DrawGroupPanel(true);
        Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
    }

    if (ds.Game_MenuCommand == 0x2E) {
        const TGroup *g = Groups_Selected();
        ds.Game_CurSystem = g->System;
        ds.Game_CurPlanet[ds.Game_CurSystem - 1] = g->Planet;
        ds.Game_CurMoon[ds.Game_CurSystem - 1] = g->Moon;
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = 7;
    }
    if (ds.Game_MenuCommand == 0x25) {
        Intel_DisbandGroup(ds.Game_GroupList, ds.Game_SelGroup);
        ds.Game_CurrentScreen = 0x10;
        ds.Game_ScreenSwitch = true;
    }

    if (ds.Game_HotspotHit == 0x15) {
        Sound_PlayButtonSfx((const uint8_t *)PSTR("x"));
        TextFile_BeginInput((uint8_t *)&Groups_Selected()->Name, 0x18, 0x38, 0x11);
    }
    // Previous group
    if (ds.Game_HotspotHit == 0x16 && !ds.Groups_PlanetForceMode) {
        Sound_PlayButtonSfx((const uint8_t *)PSTR("x"));
        if ((uint16_t)ds.Game_SelGroup > 1 || (ds.Game_GroupList == 1 && ds.Game_GroupCount > 0)) {
            TextFile_EndInput();
            if (ds.Game_SelGroup == 1) {
                ds.Game_GroupList = 0;
                ds.Game_ShownGroupCount = ds.Game_GroupCount;
                ds.Game_SelGroup = ds.Game_GroupCount + 1;
            }
            ds.Game_SelGroup--;
            group_changed();
        }
    }
    // Next group
    if (!ds.Groups_PlanetForceMode && ds.Game_HotspotHit == 0x17) {
        Sound_PlayButtonSfx((const uint8_t *)PSTR("x"));
        // The original compares the word SelGroup with the sign-extended Shown count
        if ((int32_t)(uint16_t)ds.Game_SelGroup < (int32_t)ds.Game_ShownGroupCount
            || ds.Game_GroupList == 0) {
            TextFile_EndInput();
            if ((int32_t)ds.Game_ShownGroupCount == (int32_t)(uint16_t)ds.Game_SelGroup) {
                ds.Game_GroupList = 1;
                ds.Game_ShownGroupCount = ds.Game_PlanetForceCount;
                ds.Game_SelGroup = 0;
            }
            ds.Game_SelGroup++;
            group_changed();
        }
    }
    // Page up / down through the slot categories
    if (ds.Game_HotspotHit == 0x19) {
        Sound_PlayButtonSfx((const uint8_t *)PSTR("x"));
        SAVED_CAT = ds.Groups_SlotCategory;
        Groups_PrevSlotCategory();
        if (ds.Groups_SlotCategory < SAVED_CAT) {
            Groups_SetupGroupsHotspots();
            Groups_DrawGroupPanel(true);
        } else {
            ds.Groups_SlotCategory = SAVED_CAT;
        }
    }
    if (ds.Game_HotspotHit == 0x1A) {
        Sound_PlayButtonSfx((const uint8_t *)PSTR("x"));
        SAVED_CAT = ds.Groups_SlotCategory;
        Groups_NextSlotCategory();
        if (SAVED_CAT < ds.Groups_SlotCategory) {
            Groups_SetupGroupsHotspots();
            Groups_DrawGroupPanel(true);
        } else {
            ds.Groups_SlotCategory = SAVED_CAT;
        }
    }

    // Ship slot buttons: the button moves units between the group and the planet's stock
    if ((uint16_t)ds.Game_HotspotHit > 0x1B && (uint16_t)ds.Game_HotspotHit < 0x21) {
        TGroup *g = Groups_Selected();
        const uint8_t *layout = far_host(Groups_SlotLayout(g->Kind, ds.Groups_SlotCategory));
        SLOT_S = ds.Game_HotspotHit - 0x1B;
        // Units[row][j]; row from SlotA
        uint8_t row = *(uint8_t *)ds_at(0x0BB9 + g->Kind * 4 + ds.Groups_SlotCategory);
        int16_t *unit = (int16_t *)((uint8_t *)g + row * 0x28 + SLOT_S * 10 - 21);
        const uint8_t *ship = ship_item(layout, SLOT_S);
        if (!ds.Mouse_HeldLeft) {
            if (*unit > 0) {
                (*unit)--;
                (*slot_stock(ship))++;
                int16_t weapons = layout[1];
                if (weapons >= 1) {
                    for (SLOT_W = 1;; SLOT_W++) {
                        int16_t w = SLOT_W;
                        if (unit[w] > 0) {
                            // Weapons beyond the new capacity go back to stock
                            SLOT_RETURNED = 0;
                            while ((int16_t)(capacity(layout, SLOT_S, w) * *unit) < (int16_t)(unit[w] - SLOT_RETURNED))
                                SLOT_RETURNED++;
                            unit[w] -= SLOT_RETURNED;
                            *slot_stock(weapon_item(layout, w)) += SLOT_RETURNED;
                        }
                        if (SLOT_W == weapons)
                            break;
                    }
                }
            }
        } else {
            SLOT_AVAIL = *slot_stock(ship);
            if (SLOT_AVAIL > 0 && *unit < 1000) {
                (*unit)++;
                (*slot_stock(ship))--;
            }
        }
        Groups_DrawGroupPanel(false);
        Satellite_UpdateGroupsMenu();
    }

    // Weapon buttons
    if ((uint16_t)ds.Game_HotspotHit > 0x20 && (uint16_t)ds.Game_HotspotHit < 0x35) {
        TGroup *g = Groups_Selected();
        const uint8_t *layout = far_host(Groups_SlotLayout(g->Kind, ds.Groups_SlotCategory));
        uint8_t row = *(uint8_t *)ds_at(0x0BB9 + g->Kind * 4 + ds.Groups_SlotCategory);
        uint16_t k = (uint16_t)(ds.Game_HotspotHit - 0x21);
        SLOT_S = (int16_t)(k / 5 + 1);
        SLOT_W = (int16_t)(k % 5 + 1);
        int16_t s = SLOT_S, w = SLOT_W;
        int16_t *unit = (int16_t *)((uint8_t *)g + row * 0x28 + s * 10 - 21);
        const uint8_t *item = weapon_item(layout, w);
        if (!ds.Mouse_HeldLeft) {
            if (unit[w] > 0) {
                unit[w]--;
                (*slot_stock(item))++;
            }
        } else {
            int16_t sum = (int16_t)(unit[1] + unit[2] + unit[3] + unit[4]);
            SLOT_AVAIL = *slot_stock(item);
            if (SLOT_AVAIL > 0 && unit[w] < (int16_t)(capacity(layout, s, w) * unit[0])
                && (g->Kind != 4 || sum < unit[0])) {
                unit[w]++;
                (*slot_stock(item))--;
            }
        }
        Groups_DrawGroupPanel(false);
    }
}

// 2b8d:0773 screen 0x15 handler: New Group
void Groups_NewGroupScreen(void)
{
    TGroup *g = Groups_At(0, ds.Game_SelGroup);
    if (ds.Game_ScreenEntered) {
        GfxPath_ShowScreenBackdrop();
        while (ds.Game_GroupKindAllowed[g->Kind - 1] == 0)
            g->Kind = g->Kind % 4 + 1;
        Groups_DrawNewGroupNameAndType();
        Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
        Groups_SetupNewGroupHotspots();
        TextFile_BeginInput((uint8_t *)&g->Name, 0x87, 0x5E, 0xE);
    }
    if (ds.Game_HotspotHit == 0x15) {
        Sound_PlayButtonSfx((const uint8_t *)PSTR("x"));
        TextFile_BeginInput((uint8_t *)&g->Name, 0x87, 0x5E, 0xE);
    }
    if (ds.Game_HotspotHit == 0x16) {
        Sound_PlayButtonSfx((const uint8_t *)PSTR("x"));
        g->Kind = g->Kind % 4 + 1;
        while (ds.Game_GroupKindAllowed[g->Kind - 1] == 0)
            g->Kind = g->Kind % 4 + 1;
        Groups_DrawNewGroupNameAndType();
    }
    if (ds.Game_MenuCommand == 0x3C) {
        Sound_PlayButtonSfx((const uint8_t *)PSTR("okay"));
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = 0x16;
    }
    if (ds.Game_MenuCommand == 0x3A) {
        Sound_PlayButtonSfx((const uint8_t *)PSTR("hiba"));
        ds.Game_ShownGroupCount--;
        ds.Game_SelGroup = 0;
        ds.Game_SelGroup0 = 0;
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = 0x10;
        ds.Game_GroupCount = ds.Game_ShownGroupCount;
    }
}

// 2b8d:08bc loads the slot icons
void Groups_LoadGroupsGfx(void)
{
    ICONS_PTR = System_GetMem(0x2300);
    Gfx_LoadPic((const uint8_t *)PSTR("grafika\\benyil"), ICONS_PTR, 1, 0x1C, 0, 0, 0x140, false,
                Gfx_PalettePtr(), 0, 0xB0, 0x40, 0, 0xBE);
}

static FarPtr screen_at(uint16_t offset) { return far_ptr(ds.Gfx_ScreenSeg, offset); }

static void prefix_location(PString *loc, const char *prefix, uint8_t len)
{
    PString t;
    PString p = {len, {0}};
    for (uint8_t i = 0; i < len; i++)
        p.s[i] = prefix[i];
    System_StrLoad(&t, &p);
    System_StrConcat(&t, loc);
    System_StrStore(&t, loc, 255);
}

// 2b8d:092b group name, location and (when full) the slot grid
void Groups_DrawGroupPanel(bool full)
{
    TGroup *g = Groups_Selected();
    PString loc, tmp, num;

    Gfx_DrawTextF9((const uint8_t *)ds_at(0x591A + g->Kind * 14), 0xDF, 0x38, 0xD);
    Gfx_DrawTextFC((const uint8_t *)&g->Name, 0x18, 0x38, 0x11);
    PlanetInfo_GetBodyLocationName(&tmp, g->System, g->Planet, g->Moon);
    System_StrStore(&tmp, &loc, 255);
    if (g->Status == 6) prefix_location(&loc, "Fly to ", 7);
    if (g->Status == 5) prefix_location(&loc, "Fly to ", 7);
    if (g->Status == 4) prefix_location(&loc, "Fly to ", 7);
    if (g->Status == 2) prefix_location(&loc, "In orbit ", 9);
    if (g->Status == 1) prefix_location(&loc, "In the dock of ", 15);
    Gfx_DrawTextF9((const uint8_t *)&loc, 0xC, 0x43, 0x31);

    if (full) {
        Mouse_HideCursor();
        // Previous / next group arrows, blanked when there is nowhere to go
        bool no_prev = ds.Groups_PlanetForceMode
            || (ds.Game_SelGroup == 1 && (ds.Game_GroupList == 0 || ds.Game_GroupCount == 0));
        if (no_prev)
            Blit_FillRect(9, 0x35, 10, 12, 0x40);
        else
            Blit_CopyRect(ICONS_PTR + 0x24, screen_at(0x4249), 10, 12);
        bool no_next = ds.Groups_PlanetForceMode
            || (ds.Game_GroupList == 1
                && (int32_t)ds.Game_ShownGroupCount == (int32_t)(uint16_t)ds.Game_SelGroup);
        if (no_next)
            Blit_FillRect(0x83, 0x35, 10, 12, 0x40);
        else
            Blit_CopyRect(ICONS_PTR + 0x2F, screen_at(0x42C3), 10, 12);
        // Page up / down arrows for the slot categories (CatRange at DS:0BB4, 2 bytes per kind)
        if (*(uint8_t *)ds_at(0x0BB2 + g->Kind * 2) < ds.Groups_SlotCategory)
            Blit_CopyRect(ICONS_PTR, screen_at(0x6DE6), 0x11, 0x1C);
        else
            Blit_FillRect(0x126, 0x57, 0x11, 0x1C, 0x40);
        if (*(uint8_t *)ds_at(0x0BB3 + g->Kind * 2) > ds.Groups_SlotCategory)
            Blit_CopyRect(ICONS_PTR + 0x12, screen_at(0x9C26), 0x11, 0x1C);
        else
            Blit_FillRect(0x126, 0x7C, 0x11, 0x1C, 0x40);
        Mouse_ShowCursor();
    }

    uint8_t row = *(uint8_t *)ds_at(0x0BB9 + g->Kind * 4 + ds.Groups_SlotCategory);
    const uint8_t *layout = far_host(Groups_SlotLayout(g->Kind, ds.Groups_SlotCategory));
    int ships = layout[0];
    int weapons = *(uint8_t *)ds_at(0x0BCD + g->Kind * 4 + ds.Groups_SlotCategory);

    if (full) {
        // Clear the table cells (column headers, ship counts, weapon counts)
        for (int w = 1; w <= 4; w++)
            Gfx_DrawTextF9((const uint8_t *)PSTR(""), (w - 1) * 0x26 + 0x90, 0x63, 5);
        for (int s = 1; s <= 4; s++) {
            Blit_FillRect(0x15, (s - 1) * 10 + 0x6D, 0x42, 8, 0x40);
            Blit_FillRect(0x6D, (s - 1) * 10 + 0x6D, 0x12, 8, 0x40);
        }
        for (int s = 1; s <= 4; s++)
            for (int w = 1; w <= 4; w++)
                Blit_FillRect((w - 1) * 0x26 + 0x92, (s - 1) * 10 + 0x6D, 0x18, 8, 0x40);
    }

    // Unit counts. FC (editable colour) when the slot's stock can be used and the colony allows it.
    for (int s = 1; s <= ships; s++) {
        // Units[row][s]: ShipType, then the weapon counts
        const int16_t *unit = (const int16_t *)((uint8_t *)g + row * 0x28 + s * 10 - 21);
        const uint8_t *ship = ship_item(layout, s);
        if (PRODUCT(ship[0])->State <= 0)
            continue;
        System_StrLong(unit[0], 3, &num, 255);
        bool editable = (ds.Groups_AtHomePlanet || ship[1] != 0) && ds.Groups_CanChangeSlots;
        if (editable)
            Gfx_DrawTextFC((const uint8_t *)&num, 0x6D, (s - 1) * 10 + 0x6D, 3);
        else
            Gfx_DrawTextF9((const uint8_t *)&num, 0x6D, (s - 1) * 10 + 0x6D, 3);
        for (int w = 1; w <= weapons; w++) {
            const uint8_t *item = weapon_item(layout, w);
            if (PRODUCT(item[0])->State <= 0)
                continue;
            if (capacity(layout, s, w) == 0)
                System_StrStore(PSTR("   -"), &num, 255);
            else
                System_StrLong(unit[w], 4, &num, 255);
            editable = (ds.Groups_AtHomePlanet || item[1] != 0) && ds.Groups_CanChangeSlots;
            if (editable)
                Gfx_DrawTextFC((const uint8_t *)&num, (w - 1) * 0x26 + 0x92, (s - 1) * 10 + 0x6D, 4);
            else
                Gfx_DrawTextF9((const uint8_t *)&num, (w - 1) * 0x26 + 0x92, (s - 1) * 10 + 0x6D, 4);
        }
    }

    if (full) {
        Blit_FillRect(8, 0xAF, 0x12F, 9, 0x40);
        Blit_FillRect(8, 0xB9, 0x12F, 9, 0x40);
    }
    // Weapon column headers and the stock of each weapon below the grid
    // (redrawn on every call: the stocks change with the counts)
    for (int w = 1; w <= weapons; w++) {
        const uint8_t *item = weapon_item(layout, w);
        if (PRODUCT(item[0])->State <= 0)
            continue;
        Gfx_DrawTextF9(item + 2, (w - 1) * 0x26 + 0x90, 0x63, 5);
        if (ds.Groups_CanChangeSlots)
            Gfx_DrawTextF9(item + 2, *(int16_t *)(item + 8), item[10], 5);
        System_StrLong(*slot_stock(item), 0, &num, 255);
        if (ds.Groups_CanChangeSlots && (ds.Groups_AtHomePlanet || item[1] != 0))
            Gfx_DrawTextF9((const uint8_t *)&num, *(int16_t *)(item + 11), item[13], 4);
    }
    // Ship names and the stock of each ship type
    for (int s = 1; s <= ships; s++) {
        const uint8_t *ship = ship_item(layout, s);
        if (PRODUCT(ship[0])->State <= 0)
            continue;
        Gfx_DrawTextF9(ship + 2, 0x15, (s - 1) * 10 + 0x6D, 0xB);
        if (ds.Groups_CanChangeSlots)
            Gfx_DrawTextF9(ship + 2, *(int16_t *)(ship + 14), ship[16], ship[20]);
        System_StrLong(*slot_stock(ship), 0, &num, 255);
        if (ds.Groups_CanChangeSlots && (ds.Groups_AtHomePlanet || ship[1] != 0))
            Gfx_DrawTextF9((const uint8_t *)&num, *(int16_t *)(ship + 17), ship[19], 4);
    }
}

// 2b8d:11bf
void Groups_SetupGroupsHotspots(void)
{
    const TGroup *g = Groups_Selected();
    ds.Groups_AtHomePlanet = g->System == 1 && g->Planet == 5 && g->Moon == 0;
    int16_t pf = Groups_FindPlanetForce(g->System, g->Planet, g->Moon);
    PF_PTR = ds.Game_Groups + (uint16_t)((32 + pf - 1) * (int)sizeof(TGroup));
    ds.Game_CurSystem = g->System;
    ds.Game_CurPlanet[ds.Game_CurSystem - 1] = g->Planet;
    ds.Game_CurMoon[ds.Game_CurSystem - 1] = g->Moon;
    PlanetInfo_SelectCurBody();
    ds.Groups_CanChangeSlots = (uint8_t)ds.Game_CurBodyIsColony != 0 && (g->Status == 1 || g->Status == 7);

    ds.Mouse_HotspotCount = 0x20;
    Mouse_SetHotspot(1, 0x15, 0x35, 0x6C, 0xC, (const uint8_t *)PSTR("Change name"), 1, 0);
    Mouse_SetHotspot(2, 7, 0x35, 0xE, 0xC,
                     ds.Groups_PlanetForceMode ? (const uint8_t *)PSTR("") : (const uint8_t *)PSTR("Previous group"),
                     1, 0);
    Mouse_SetHotspot(3, 0x81, 0x35, 0xD, 0xC,
                     ds.Groups_PlanetForceMode ? (const uint8_t *)PSTR("") : (const uint8_t *)PSTR("Next group"),
                     1, 0);
    Mouse_SetHotspot(4, 0, 0, 0, 0, (const uint8_t *)PSTR(""), 1, 0);
    Mouse_SetHotspot(5, 0x123, 0x54, 0x1C, 0x23, (const uint8_t *)PSTR("Page up"), 1, 0);
    Mouse_SetHotspot(6, 0x123, 0x79, 0x1C, 0x23, (const uint8_t *)PSTR("Page down"), 1, 0);
    Mouse_ClearHotspots(7, 7);

    const uint8_t kind = g->Kind;
    const uint8_t *layout = far_host(Groups_SlotLayout(kind, ds.Groups_SlotCategory));
    Mouse_ClearHotspots(8, 0x20);
    int ships = layout[0];
    int weapons = *(uint8_t *)ds_at(0x0BCD + kind * 4 + ds.Groups_SlotCategory);
    for (int s = 1; s <= ships; s++) {
        for (int w = 1; w <= weapons; w++) {
            const uint8_t *item = weapon_item(layout, w);
            if (PRODUCT(item[0])->State <= 0)
                continue;
            int16_t index = (int16_t)(w + 0xC + (s - 1) * 5);
            if ((!ds.Groups_AtHomePlanet && item[1] == 0) || !ds.Groups_CanChangeSlots)
                Mouse_ClearHotspots(index, index);
            else
                Mouse_SetHotspot(index, (int16_t)((w - 1) * 0x26 + 0x8F), (uint8_t)((s - 1) * 10 + 0x6C),
                                 0x21, 9, (const uint8_t *)&PRODUCT(item[0])->Name, 0, 0);
        }
    }
    for (int s = 1; s <= ships; s++) {
        const uint8_t *ship = ship_item(layout, s);
        if (PRODUCT(ship[0])->State <= 0)
            continue;
        int16_t index = (int16_t)(s + 7);
        if ((!ds.Groups_AtHomePlanet && ship[1] == 0) || !ds.Groups_CanChangeSlots)
            Mouse_ClearHotspots(index, index);
        else
            Mouse_SetHotspot(index, 0x6A, (uint8_t)((s - 1) * 10 + 0x6C), 0x1A, 9,
                             (const uint8_t *)&PRODUCT(ship[0])->Name, 0, 0);
    }
}

// 2b8d:15ec
void Groups_DrawNewGroupNameAndType(void)
{
    const TGroup *g = Groups_Selected();
    Gfx_DrawTextFC((const uint8_t *)&g->Name, 0x87, 0x5E, 0xE);
    Gfx_DrawTextFC((const uint8_t *)ds_at(0x591A + g->Kind * 14), 0x87, 0x77, 0xD);
}

// 2b8d:1659
void Groups_SetupNewGroupHotspots(void)
{
    ds.Mouse_HotspotCount = 2;
    Mouse_SetHotspot(1, 0x84, 0x5B, 0x5D, 0xD, (const uint8_t *)PSTR("Change name"), 1, 0);
    Mouse_SetHotspot(2, 0x84, 0x75, 0x5D, 0xB, (const uint8_t *)PSTR("Change type"), 1, 0);
}

// 2b8d:16d0 resets the selected group
void Groups_ClearGroup(void)
{
    TGroup *g = Groups_Selected();
    g->Kind = 1;
    System_StrStore(PSTR("New Group"), (PString *)&g->Name, 17);
    g->System = 1;
    g->Planet = 5;
    g->Moon = 0;
    g->Status = 1;
    g->Leg1 = 0;
    g->Jump = 0;
    g->Leg2 = 0;
    for (int r = 0; r < 2; r++) {
        for (int s = 0; s < 4; s++) {
            g->Units[r][s].ShipType = 0;
            for (int w = 0; w < 4; w++)
                g->Units[r][s].Weapons[w] = 0;
        }
    }
    for (int i = 0; i < 6; i++) {
        g->Cargo[i].A = 0;
        g->Cargo[i].B = 0;
    }
    for (int i = 0; i < 13; i++)
        g->Items[i] = 0;
}

// 2b8d:17fd ClearGroup, then the Kind and an empty slot grid for it
void Groups_InitGroupOfKind(void)
{
    Groups_ClearGroup();
    TGroup *g = Groups_Selected();
    if (ds.Game_GroupList == 0)
        g->Kind = (uint8_t)((uint16_t)(ds.Game_SelGroup - 1U) % 4 + 1);
    else
        g->Kind = 5;
    if (g->Kind == 3)
        g->Kind = 4;
    System_StrStore((const PString *)&ds.Groups_DefaultNames[g->Kind - 1], (PString *)&g->Name, 17);
    if (g->Kind == 5)
        g->Status = 7;
    for (int cat = 1; cat <= 4; cat++) {
        if (!Groups_SlotUsed(g->Kind, cat))
            continue;
        uint8_t row = *(uint8_t *)ds_at(0x0BB9 + g->Kind * 4 + cat);
        const uint8_t *layout = far_host(Groups_SlotLayout(g->Kind, cat));
        for (int s = 1; s <= layout[0]; s++) {
            int16_t *unit = (int16_t *)((uint8_t *)g + row * 0x28 + s * 10 - 21);
            unit[0] = 0;
            // The original multiplies each weapon's capacity by the (just zeroed) ship count
            for (int w = 1; w <= layout[1]; w++)
                unit[w] = (int16_t)(capacity(layout, s, w) * unit[0]);
        }
    }
}

// 2b8d:1996
void Groups_NextSlotCategory(void)
{
    do {
        ds.Groups_SlotCategory = ds.Groups_SlotCategory % 4 + 1;
    } while (!Groups_SlotUsed(Groups_Selected()->Kind, ds.Groups_SlotCategory));
}

// 2b8d:19f7
void Groups_PrevSlotCategory(void)
{
    do {
        ds.Groups_SlotCategory = (ds.Groups_SlotCategory + 2) % 4 + 1;
    } while (!Groups_SlotUsed(Groups_Selected()->Kind, ds.Groups_SlotCategory));
}

// 2b8d:1a62
void Groups_FirstSlotCategory(void)
{
    ds.Groups_SlotCategory = 1;
    while (!Groups_SlotUsed(Groups_Selected()->Kind, ds.Groups_SlotCategory))
        ds.Groups_SlotCategory = ds.Groups_SlotCategory % 4 + 1;
}

// 2b8d:1abb no units in the two ship categories (tables at DS:0BE5 / 0BE9 / 0BED)
bool Groups_IsGroupEmpty(int16_t list, int16_t group)
{
    return units_empty(Groups_At(list, group), 0x0BED, 0x0BE5, 0x0BE9);
}

// 2b8d:1bad the same over the other categories (tables at DS:0BF1 / 0BF5 / 0BF9)
bool Groups_IsGroupEmptyOfCargo(int16_t list, int16_t group)
{
    return units_empty(Groups_At(list, group), 0x0BF9, 0x0BF1, 0x0BF5);
}

// 2b8d:1c9f planet-force index at the location (the last match wins), 0 if none
int16_t Groups_FindPlanetForce(int16_t system, int16_t planet, int16_t moon)
{
    int16_t found = 0;
    for (int16_t i = 1; i <= ds.Game_PlanetForceCount; i++) {
        const TGroup *g = Groups_At(1, i);
        if (g->System == system && g->Planet == planet && g->Moon == moon)
            found = i;
    }
    return found;
}

// 2b8d:1d09
void Groups_InitPlanetForce(void)
{
    ds.Game_PlanetForceCount = 1;
    ds.Game_GroupCount = 0;
    ds.Game_ShownGroupCount = 0;
    ds.Game_GroupList = 1;
    ds.Game_SelGroup = 1;
    Groups_InitGroupOfKind();
}

// 2b8d:1d6f
void Groups_Init(void)
{
    ds.Game_FindPlanetForceProc = far_ptr(0x2B8D, 0x1C9F);
}
