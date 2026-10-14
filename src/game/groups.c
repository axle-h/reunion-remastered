// Unit Groups (2b8d): group lists, the group detail screen (0x16) and the New Group
// screen (0x15).
#include "game/groups.h"

#include <SDL3/SDL.h>

#include "game/blit.h"
#include "game/fixes.h"
#include "game/gfx.h"
#include "game/gfxpath.h"
#include "game/improvements.h"
#include "game/lang.h"
#include "game/le.h"
#include "game/mouse.h"
#include "game/planetinfo.h"
#include "game/satellite.h"
#include "game/sound.h"
#include "game/system.h"
#include "game/textfile.h"

// Routines missing from their units' headers.
void PlanetInfo_SelectCurBody(void);                                            // 357b:3253
void PlanetInfo_GetBodyLocationName(char *dst, size_t cap, int16_t sys, int16_t planet,
                                    int16_t moon); // 357b:355e
void Intel_DisbandGroup(int16_t list, int16_t group);                           // 29b9:0b93

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

// Groups::EmptyTests[table][kind]: rows of Units / slot counts for the empty tests. The
// original adds Kind to the table's base, so Kind 5 (planet forces) reads the next
// table's first entry, and for the last table the byte after it (DS:0BFE).
static uint8_t kind_table(int table, uint8_t kind)
{
    return (&ds.Groups_EmptyTests[0][0])[(table - 1) * 4 + kind - 1];
}

// Units[row][t] of g, addressed as the original does: g + 40 * row + 10 * t
// - 21, five words (Count, then Weapons[1..4]) at odd offsets in the
// packed record. Word w of it (0 = Count):
static uint8_t *unit_at(const TGroup *g, int row, int t)
{
    return (uint8_t *)g + (row * 40 + t * 10 - 21);
}

static int16_t unit_word(const uint8_t *unit, int w)
{
    return rd16le(unit + w * 2);
}

static void set_unit_word(uint8_t *unit, int w, int16_t v)
{
    wr16le(unit + w * 2, v);
}

static bool units_empty(const TGroup *g, int count_tbl, int row_a, int row_b)
{
    bool empty = true;
    unsigned count = kind_table(count_tbl, g->Kind);
    for (int pass = 0; pass < 2; pass++) {
        unsigned row = kind_table(pass == 0 ? row_a : row_b, g->Kind);
        if (count == 0)
            continue;
        for (unsigned t = 1;; t++) {
            empty = empty && unit_word(unit_at(g, (int)row, (int)t), 0) == 0;
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

// The stock a slot draws from: the home planet's product stock or the planet
// force's item, a word in a packed record.
static uint8_t *slot_stock(const uint8_t *item)
{
    return ds.Groups_AtHomePlanet ? (uint8_t *)&PRODUCT(item[0])->Stock
                                  : (uint8_t *)ds.Groups_PlanetForceList + 0x83 + item[1] * 2;
}

static int16_t stock_of(const uint8_t *item)
{
    return rd16le(slot_stock(item));
}

static void add_stock(const uint8_t *item, int16_t n)
{
    wr16le(slot_stock(item), (int16_t)(stock_of(item) + n));
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
        Sound_PlayButtonSfx("x");
        TextFile_BeginInput((uint8_t *)&Groups_Selected()->Name, 0x18, 0x38, 0x11);
    }
    // Previous group
    if (ds.Game_HotspotHit == 0x16 && !ds.Groups_PlanetForceMode) {
        Sound_PlayButtonSfx("x");
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
        Sound_PlayButtonSfx("x");
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
        Sound_PlayButtonSfx("x");
        ds.Groups_SavedCategory = ds.Groups_SlotCategory;
        Groups_PrevSlotCategory();
        if (ds.Groups_SlotCategory < ds.Groups_SavedCategory) {
            Groups_SetupGroupsHotspots();
            Groups_DrawGroupPanel(true);
        } else {
            ds.Groups_SlotCategory = ds.Groups_SavedCategory;
        }
    }
    if (ds.Game_HotspotHit == 0x1A) {
        Sound_PlayButtonSfx("x");
        ds.Groups_SavedCategory = ds.Groups_SlotCategory;
        Groups_NextSlotCategory();
        if (ds.Groups_SavedCategory < ds.Groups_SlotCategory) {
            Groups_SetupGroupsHotspots();
            Groups_DrawGroupPanel(true);
        } else {
            ds.Groups_SlotCategory = ds.Groups_SavedCategory;
        }
    }

    // Ship slot buttons: the button moves units between the group and the planet's stock
    if ((uint16_t)ds.Game_HotspotHit > 0x1B && (uint16_t)ds.Game_HotspotHit < 0x21) {
        TGroup *g = Groups_Selected();
        const uint8_t *layout = Groups_SlotLayout(g->Kind, ds.Groups_SlotCategory);
        ds.Groups_SlotShip = ds.Game_HotspotHit - 0x1B;
        // Units[row][j]; row from SlotA
        uint8_t row = ds.Groups_SlotA[g->Kind - 1][ds.Groups_SlotCategory - 1];
        uint8_t *unit = unit_at(g, row, ds.Groups_SlotShip);
        const uint8_t *ship = ship_item(layout, ds.Groups_SlotShip);
        if (!ds.Mouse_HeldLeft) {
            if (unit_word(unit, 0) > 0) {
                set_unit_word(unit, 0, (int16_t)(unit_word(unit, 0) - 1));
                add_stock(ship, 1);
                int16_t weapons = layout[1];
                if (weapons >= 1) {
                    for (ds.Groups_SlotWeapon = 1;; ds.Groups_SlotWeapon++) {
                        int16_t w = ds.Groups_SlotWeapon;
                        if (unit_word(unit, w) > 0) {
                            // Weapons beyond the new capacity go back to stock
                            ds.Groups_SlotReturned = 0;
                            while ((int16_t)(capacity(layout, ds.Groups_SlotShip, w)
                                             * unit_word(unit, 0))
                                   < (int16_t)(unit_word(unit, w) - ds.Groups_SlotReturned))
                                ds.Groups_SlotReturned++;
                            set_unit_word(unit, w,
                                          (int16_t)(unit_word(unit, w) - ds.Groups_SlotReturned));
                            add_stock(weapon_item(layout, w), ds.Groups_SlotReturned);
                        }
                        if (ds.Groups_SlotWeapon == weapons)
                            break;
                    }
                }
            }
        } else {
            ds.Groups_SlotAvail = stock_of(ship);
            if (ds.Groups_SlotAvail > 0 && unit_word(unit, 0) < 1000) {
                set_unit_word(unit, 0, (int16_t)(unit_word(unit, 0) + 1));
                add_stock(ship, -1);
            }
        }
        Groups_DrawGroupPanel(false);
        Satellite_UpdateGroupsMenu();
    }

    // Weapon buttons
    if ((uint16_t)ds.Game_HotspotHit > 0x20 && (uint16_t)ds.Game_HotspotHit < 0x35) {
        TGroup *g = Groups_Selected();
        const uint8_t *layout = Groups_SlotLayout(g->Kind, ds.Groups_SlotCategory);
        uint8_t row = ds.Groups_SlotA[g->Kind - 1][ds.Groups_SlotCategory - 1];
        uint16_t k = (uint16_t)(ds.Game_HotspotHit - 0x21);
        ds.Groups_SlotShip = (int16_t)(k / 5 + 1);
        ds.Groups_SlotWeapon = (int16_t)(k % 5 + 1);
        int16_t s = ds.Groups_SlotShip, w = ds.Groups_SlotWeapon;
        uint8_t *unit = unit_at(g, row, s);
        const uint8_t *item = weapon_item(layout, w);
        if (!ds.Mouse_HeldLeft) {
            if (unit_word(unit, w) > 0) {
                set_unit_word(unit, w, (int16_t)(unit_word(unit, w) - 1));
                add_stock(item, 1);
            }
        } else {
            int16_t sum = (int16_t)(unit_word(unit, 1) + unit_word(unit, 2) + unit_word(unit, 3)
                                    + unit_word(unit, 4));
            ds.Groups_SlotAvail = stock_of(item);
            if (ds.Groups_SlotAvail > 0
                && unit_word(unit, w) < (int16_t)(capacity(layout, s, w) * unit_word(unit, 0))
                && (g->Kind != 4 || sum < unit_word(unit, 0))) {
                set_unit_word(unit, w, (int16_t)(unit_word(unit, w) + 1));
                add_stock(item, -1);
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
        Sound_PlayButtonSfx("x");
        TextFile_BeginInput((uint8_t *)&g->Name, 0x87, 0x5E, 0xE);
    }
    if (ds.Game_HotspotHit == 0x16) {
        Sound_PlayButtonSfx("x");
        g->Kind = g->Kind % 4 + 1;
        while (ds.Game_GroupKindAllowed[g->Kind - 1] == 0)
            g->Kind = g->Kind % 4 + 1;
        Groups_DrawNewGroupNameAndType();
    }
    if (ds.Game_MenuCommand == 0x3C) {
        Sound_PlayButtonSfx("okay");
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = 0x16;
    }
    if (ds.Game_MenuCommand == 0x3A) {
        Sound_PlayButtonSfx("hiba");
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
    ds.Groups_IconBuffer = game_alloc(0x2300);
    Gfx_LoadPic("grafika\\benyil", ds.Groups_IconBuffer, 1, 0x1C,
                0, 0, 0x140, false, Gfx_PalettePtr(), 0, 0xB0, 0x40, 0, 0xBE);
}

static uint8_t *screen_at(uint16_t offset) { return gfx_screen() + offset; }
static const uint8_t *icon_at(uint16_t offset)
{
    return ds.Groups_IconBuffer + offset;
}

// 2b8d:092b group name, location and (when full) the slot grid
void Groups_DrawGroupPanel(bool full)
{
    TGroup *g = Groups_Selected();
    char t[256], loc[256], num[16];

    Gfx_DrawTextF9(pstr_get(t, sizeof t, Lang_Slot(&ds.Groups_TypeNames[g->Kind - 1])), 0xDF, 0x38,
                   0xD);
    Gfx_DrawTextFC(pstr_get(t, sizeof t, &g->Name), 0x18, 0x38, 0x11);
    PlanetInfo_GetBodyLocationName(t, sizeof t, g->System, g->Planet, g->Moon);
    const char *prefix = g->Status == 4 || g->Status == 5 || g->Status == 6 ? Lang_Text("Fly to ")
                         : g->Status == 2                                ? Lang_Text("In orbit ")
                         : g->Status == 1                                ? Lang_Text("In the dock of ")
                                                                         : "";
    SDL_snprintf(loc, sizeof loc, "%s%s", prefix, t);
    Gfx_DrawTextF9(loc, 0xC, 0x43, 0x31);

    if (full) {
        Mouse_HideCursor();
        // Previous / next group arrows, blanked when there is nowhere to go
        bool no_prev = ds.Groups_PlanetForceMode
            || (ds.Game_SelGroup == 1 && (ds.Game_GroupList == 0 || ds.Game_GroupCount == 0));
        if (no_prev)
            Blit_FillRect(9, 0x35, 10, 12, 0x40);
        else
            Blit_CopyRect(icon_at(0x24), screen_at(0x4249), 10, 12);
        bool no_next = ds.Groups_PlanetForceMode
            || (ds.Game_GroupList == 1
                && (int32_t)ds.Game_ShownGroupCount == (int32_t)(uint16_t)ds.Game_SelGroup);
        if (no_next)
            Blit_FillRect(0x83, 0x35, 10, 12, 0x40);
        else
            Blit_CopyRect(icon_at(0x2F), screen_at(0x42C3), 10, 12);
        // Page up / down arrows for the slot categories (CatRange[kind])
        if (ds.Groups_CatRange[g->Kind - 1].Lo < ds.Groups_SlotCategory)
            Blit_CopyRect(icon_at(0), screen_at(0x6DE6), 0x11, 0x1C);
        else
            Blit_FillRect(0x126, 0x57, 0x11, 0x1C, 0x40);
        if (ds.Groups_CatRange[g->Kind - 1].Hi > ds.Groups_SlotCategory)
            Blit_CopyRect(icon_at(0x12), screen_at(0x9C26), 0x11, 0x1C);
        else
            Blit_FillRect(0x126, 0x7C, 0x11, 0x1C, 0x40);
        Mouse_ShowCursor();
    }

    uint8_t row = ds.Groups_SlotA[g->Kind - 1][ds.Groups_SlotCategory - 1];
    const uint8_t *layout = Groups_SlotLayout(g->Kind, ds.Groups_SlotCategory);
    int ships = layout[0];
    int weapons = ds.Groups_SlotB[g->Kind - 1][ds.Groups_SlotCategory - 1];

    if (full) {
        // Clear the table cells (column headers, ship counts, weapon counts)
        for (int w = 1; w <= 4; w++)
            Gfx_DrawTextF9("", (w - 1) * 0x26 + 0x90, 0x63, 5);
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
        // Units[row][s]: Count, then the weapon counts
        const uint8_t *unit = unit_at(g, row, s);
        const uint8_t *ship = ship_item(layout, s);
        if (PRODUCT(ship[0])->State <= 0)
            continue;
        SDL_snprintf(num, sizeof num, "%3d", unit_word(unit, 0));
        bool editable = (ds.Groups_AtHomePlanet || ship[1] != 0) && ds.Groups_CanChangeSlots;
        if (editable)
            Gfx_DrawTextFC(num, 0x6D, (s - 1) * 10 + 0x6D, 3);
        else
            Gfx_DrawTextF9(num, 0x6D, (s - 1) * 10 + 0x6D, 3);
        for (int w = 1; w <= weapons; w++) {
            const uint8_t *item = weapon_item(layout, w);
            if (PRODUCT(item[0])->State <= 0)
                continue;
            if (capacity(layout, s, w) == 0)
                SDL_strlcpy(num, "   -", sizeof num);
            else
                SDL_snprintf(num, sizeof num, "%4d", unit_word(unit, w));
            editable = (ds.Groups_AtHomePlanet || item[1] != 0) && ds.Groups_CanChangeSlots;
            if (editable)
                Gfx_DrawTextFC(num, (w - 1) * 0x26 + 0x92, (s - 1) * 10 + 0x6D, 4);
            else
                Gfx_DrawTextF9(num, (w - 1) * 0x26 + 0x92, (s - 1) * 10 + 0x6D, 4);
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
        pstr_get(t, sizeof t, item + 2);
        Gfx_DrawTextF9(t, (w - 1) * 0x26 + 0x90, 0x63, 5);
        if (ds.Groups_CanChangeSlots)
            Gfx_DrawTextF9(t, rd16le(item + 8), item[10], 5);
        SDL_snprintf(num, sizeof num, "%d", stock_of(item));
        if (ds.Groups_CanChangeSlots && (ds.Groups_AtHomePlanet || item[1] != 0))
            Gfx_DrawTextF9(num, rd16le(item + 11), item[13], 4);
    }
    // Ship names and the stock of each ship type
    for (int s = 1; s <= ships; s++) {
        const uint8_t *ship = ship_item(layout, s);
        if (PRODUCT(ship[0])->State <= 0)
            continue;
        pstr_get(t, sizeof t, ship + 2);
        Gfx_DrawTextF9(t, 0x15, (s - 1) * 10 + 0x6D, 0xB);
        if (ds.Groups_CanChangeSlots)
            Gfx_DrawTextF9(t, rd16le(ship + 14), ship[16], ship[20]);
        SDL_snprintf(num, sizeof num, "%d", stock_of(ship));
        if (ds.Groups_CanChangeSlots && (ds.Groups_AtHomePlanet || ship[1] != 0))
            Gfx_DrawTextF9(num, rd16le(ship + 17), ship[19], 4);
    }
}

// 2b8d:11bf
void Groups_SetupGroupsHotspots(void)
{
    const TGroup *g = Groups_Selected();
    char name[256];
    ds.Groups_AtHomePlanet = g->System == 1 && g->Planet == 5 && g->Moon == 0;
    int16_t pf = Groups_FindPlanetForce(g->System, g->Planet, g->Moon);
    ds.Groups_PlanetForceList = (TGroup *)ds.Game_Groups + (32 + pf - 1); // Groups^[1][pf]
    ds.Game_CurSystem = g->System;
    ds.Game_CurPlanet[ds.Game_CurSystem - 1] = g->Planet;
    ds.Game_CurMoon[ds.Game_CurSystem - 1] = g->Moon;
    PlanetInfo_SelectCurBody();
    ds.Groups_CanChangeSlots = (uint8_t)ds.Game_CurBodyIsColony != 0 && (g->Status == 1 || g->Status == 7);

    ds.Mouse_HotspotCount = 0x20;
    Mouse_SetHotspot(1, 0x15, 0x35, 0x6C, 0xC, Lang_Text("Change name"), 1, 0);
    Mouse_SetHotspot(2, 7, 0x35, 0xE, 0xC,
                     ds.Groups_PlanetForceMode ? "" : Lang_Text("Previous group"),
                     1, 0);
    Mouse_SetHotspot(3, 0x81, 0x35, 0xD, 0xC,
                     ds.Groups_PlanetForceMode ? "" : Lang_Text("Next group"),
                     1, 0);
    Mouse_SetHotspot(4, 0, 0, 0, 0, "", 1, 0);
    Mouse_SetHotspot(5, 0x123, 0x54, 0x1C, 0x23, Lang_Text("Page up"), 1, 0);
    Mouse_SetHotspot(6, 0x123, 0x79, 0x1C, 0x23, Lang_Text("Page down"), 1, 0);
    Mouse_ClearHotspots(7, 7);

    const uint8_t kind = g->Kind;
    const uint8_t *layout = Groups_SlotLayout(kind, ds.Groups_SlotCategory);
    Mouse_ClearHotspots(8, 0x20);
    int ships = layout[0];
    int weapons = ds.Groups_SlotB[kind - 1][ds.Groups_SlotCategory - 1];
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
                                 0x21, 9, pstr_get(name, sizeof name, &PRODUCT(item[0])->Name), 0,
                                 0);
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
                             pstr_get(name, sizeof name, &PRODUCT(ship[0])->Name), 0, 0);
    }
}

// 2b8d:15ec
void Groups_DrawNewGroupNameAndType(void)
{
    const TGroup *g = Groups_Selected();
    char t[256];
    Gfx_DrawTextFC(pstr_get(t, sizeof t, &g->Name), 0x87, 0x5E, 0xE);
    Gfx_DrawTextFC(pstr_get(t, sizeof t, Lang_Slot(&ds.Groups_TypeNames[g->Kind - 1])), 0x87, 0x77,
                   0xD);
}

// 2b8d:1659
void Groups_SetupNewGroupHotspots(void)
{
    ds.Mouse_HotspotCount = 2;
    Mouse_SetHotspot(1, 0x84, 0x5B, 0x5D, 0xD, Lang_Text("Change name"), 1, 0);
    Mouse_SetHotspot(2, 0x84, 0x75, 0x5D, 0xB, Lang_Text("Change type"), 1, 0);
}

// 2b8d:16d0 resets the selected group
void Groups_ClearGroup(void)
{
    TGroup *g = Groups_Selected();
    g->Kind = 1;
    pstr_set(&g->Name, 17, Lang_Text("New Group"));
    g->System = 1;
    g->Planet = 5;
    g->Moon = 0;
    g->Status = 1;
    g->Leg1 = 0;
    g->Jump = 0;
    g->Leg2 = 0;
    for (int r = 0; r < 2; r++) {
        for (int s = 0; s < 4; s++) {
            g->Units[r][s].Count = 0;
            for (int w = 0; w < 4; w++)
                g->Units[r][s].Weapons[w] = 0;
        }
    }
    for (int i = 0; i < 6; i++)
        g->Cargo[i] = 0;
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
    char name[256];
    pstr_set(&g->Name, 17, pstr_get(name, sizeof name, Lang_Slot(&ds.Groups_DefaultNames[g->Kind - 1])));
    if (g->Kind == 5)
        g->Status = 7;
    for (int cat = 1; cat <= 4; cat++) {
        if (!Groups_SlotUsed(g->Kind, cat))
            continue;
        uint8_t row = ds.Groups_SlotA[g->Kind - 1][cat - 1];
        const uint8_t *layout = Groups_SlotLayout(g->Kind, cat);
        for (int s = 1; s <= layout[0]; s++) {
            uint8_t *unit = unit_at(g, row, s);
            set_unit_word(unit, 0, 0);
            // The original multiplies each weapon's capacity by the (just zeroed) ship count
            for (int w = 1; w <= layout[1]; w++)
                set_unit_word(unit, w, (int16_t)(capacity(layout, s, w) * unit_word(unit, 0)));
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

// 2b8d:1abb no units in the two ship categories (EmptyTests 1..3)
bool Groups_IsGroupEmpty(int16_t list, int16_t group)
{
    return units_empty(Groups_At(list, group), 3, 1, 2);
}

// 2b8d:1bad the same over the other categories (EmptyTests 4..6)
bool Groups_IsGroupEmptyOfCargo(int16_t list, int16_t group)
{
    return units_empty(Groups_At(list, group), 6, 4, 5);
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

int16_t Groups_PlanetStorage(int16_t system, int16_t planet, int16_t moon)
{
    int16_t force = Groups_FindPlanetForce(system, planet, moon);
    // Each Miner station's 10,000 t, as building kind 19h gives, also while
    // a colony bought there waits to be set up (its planet force has none).
    const TBody *b = PlanetInfo_BodyAt(system, planet, moon);
    if (improvements.more_stations && b->Colony == 0 && (force == 0 || b->ColonyTimer > 0))
        return (int16_t)(10 * SDL_max(b->Droids, 1));
    if (force == 0 && fixes.station_storage)
        return 10;
    return Groups_At(1, force)->Storage;
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
    ds.Game_FindPlanetForceProc = Groups_FindPlanetForce;
}
