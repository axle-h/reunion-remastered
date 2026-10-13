#include "game/shipinfo.h"

#include <SDL3/SDL.h>

#include "game/blit.h"
#include "game/gfx.h"
#include "game/groups.h"
#include "game/le.h"
#include "game/minmax.h"
#include "game/mouse.h"
#include "game/satellite.h"
#include "game/sound.h"
#include "game/system.h"

// 357b:332c PlanetInfo::GetBodyName (result is a 255-character temporary)
void PlanetInfo_GetBodyName(char *dst, size_t cap, int16_t system, int16_t planet, int16_t moon);
// 357b:3253
void PlanetInfo_SelectCurBody(void);


// Offset of the info panel text lines and of the grid on screen, from the code.
enum : uint16_t {
    ROW49 = 0x3D40, // screen row 49 (the views start there) in a 320-wide image
    MARKER_ON = 0x1C55,   // 7x6 selection markers inside the ship3 sprite sheet
    MARKER_OFF = 0x1C45,
};

static uint16_t slot_col(int16_t slot) { return (uint16_t)(slot - 1) >> 3; }
static uint16_t slot_row(int16_t slot) { return (uint16_t)(slot - 1) % 8; }

// 26fe:001b
void ShipInfo_ShipInfoScreen(void)
{
    if (ds.Game_ScreenEntered) {
        ds.Game_ShipInfoButtons = -1;
        Satellite_UpdateShipInfoMenu();
        ShipInfo_SetupShipInfoHotspots();
        ShipInfo_InitShipInfoScreen();
    }

    uint16_t hit = (uint16_t)ds.Game_HotspotHit;
    // The icon hotspots are 0x17.. (group = hit - 0x16), the name hotspots 0x37..
    // (group = hit - 0x36). Select when a different group's hotspot is hit
    // (names only on a fresh press).
    bool check_names;
    if (hit < 0x17)
        check_names = true;
    else
        check_names = MinMax_Min(ds.Game_ShownGroupCount, 32) + 0x16 < (int32_t)hit
                      || (int16_t)(hit - 0x16) == ds.Game_SelGroup;
    bool select = false;
    if (check_names)
        select = !(hit < 0x37 || ds.Mouse_ButtonsChanged != 0
                   || (int16_t)(hit - 0x36) == ds.Game_SelGroup);
    else
        select = true;
    if (select && ds.Mouse_Buttons != 0) {
        if (ds.Game_SelGroup != 0)
            ShipInfo_EraseSelectionMarker();
        if (hit < 0x37)
            ds.Game_SelGroup = (int16_t)(ds.Game_HotspotHit - 0x16);
        else
            ds.Game_SelGroup = (int16_t)(ds.Game_HotspotHit - 0x36);
        if (ds.Game_GroupList == 0)
            ds.Game_SelGroup0 = ds.Game_SelGroup;
        else
            ds.Game_SelGroup1 = ds.Game_SelGroup;
        ShipInfo_DrawSelectionMarker();
        Satellite_UpdateShipInfoMenu();
        ShipInfo_DrawGroupInfo();
    }

    hit = (uint16_t)ds.Game_HotspotHit;
    if (hit > 0x36 && ds.Mouse_Buttons == 0) {
        // Released over a name: open that group.
        const TGroup *g = Groups_At(ds.Game_GroupList, (int16_t)(ds.Game_HotspotHit - 0x36));
        ds.Game_CurSystem = g->System;
        ds.Game_CurPlanet[ds.Game_CurSystem - 1] = g->Planet;
        ds.Game_CurMoon[ds.Game_CurSystem - 1] = g->Moon;
        PlanetInfo_SelectCurBody();
        if (ds.Mouse_HeldLeft != 0 && ds.Game_GroupList == 0) {
            Sound_PlayButtonSfx("controll");
            if (ds.Game_SelGroup != 0)
                ShipInfo_EraseSelectionMarker();
            ds.Game_SelGroup = (int16_t)(ds.Game_HotspotHit - 0x36);
            ds.Game_SelGroup0 = ds.Game_SelGroup;
            ShipInfo_DrawSelectionMarker();
            ShipInfo_DrawGroupInfo();
            ds.Game_ScreenSwitch = 1;
            ds.Game_CurrentScreen = 0x11;
        }
        if (ds.Mouse_HeldLeft == 0 || ds.Game_GroupList == 1) {
            Sound_PlayButtonSfx("group");
            if (ds.Game_SelGroup != 0)
                ShipInfo_EraseSelectionMarker();
            ds.Game_SelGroup = (int16_t)(ds.Game_HotspotHit - 0x36);
            if (ds.Game_GroupList == 0)
                ds.Game_SelGroup0 = ds.Game_SelGroup;
            else
                ds.Game_SelGroup1 = ds.Game_SelGroup;
            ShipInfo_DrawSelectionMarker();
            ShipInfo_DrawGroupInfo();
            ds.Game_ScreenSwitch = 1;
            ds.Game_CurrentScreen = 0x16;
        }
    }

    // "Change mode". On list 0 the first test plays the error sound and the
    // second one then switches to list 1 anyway (the first test clears HotspotHit
    // only when it switches); the "change" sound cuts the error sound off at once.
    if (ds.Game_HotspotHit == 0x15) {
        if (ds.Game_GroupList == 0) {
            Sound_PlayButtonSfx("hiba");
        } else {
            Sound_PlayButtonSfx("change");
            ds.Game_GroupList = 0;
            ds.Game_ShownGroupCount = ds.Game_GroupCount;
            ds.Game_SelGroup = ds.Game_SelGroup0;
            Satellite_UpdateShipInfoMenu();
            ShipInfo_SetupShipInfoHotspots();
            ShipInfo_DrawGroupInfo();
            ShipInfo_SwitchGroupList();
            ds.Game_HotspotHit = 0;
        }
    }
    if (ds.Game_HotspotHit == 0x15) {
        if (ds.Game_GroupList == 1) {
            Sound_PlayButtonSfx("hiba");
        } else {
            Sound_PlayButtonSfx("change");
            ds.Game_GroupList = 1;
            ds.Game_ShownGroupCount = ds.Game_PlanetForceCount;
            ds.Game_SelGroup = ds.Game_SelGroup1;
            Satellite_UpdateShipInfoMenu();
            ShipInfo_SetupShipInfoHotspots();
            ShipInfo_DrawGroupInfo();
            ShipInfo_SwitchGroupList();
            ds.Game_HotspotHit = 0;
        }
    }

    // Menu command 0xF: New group
    if (ds.Game_MenuCommand == 0xF && ds.Game_GroupCount < 32) {
        if (ds.Game_SelGroup != 0)
            ShipInfo_EraseSelectionMarker();
        ds.Game_GroupList = 0;
        ds.Game_ShownGroupCount = (int16_t)(ds.Game_GroupCount + 1);
        ds.Game_GroupCount = ds.Game_ShownGroupCount;
        ds.Game_SelGroup = ds.Game_ShownGroupCount;
        ds.Game_SelGroup0 = ds.Game_ShownGroupCount;
        Groups_ClearGroup();
        ds.Game_ScreenSwitch = 1;
        ds.Game_CurrentScreen = 0x15;
    }
}

// grafika\ship<list+1>
static const char *ship_backdrop_name(char name[32])
{
    SDL_snprintf(name, 32, "grafika\\ship%d", ds.Game_GroupList + 1);
    return name;
}

// 26fe:02e6
void ShipInfo_InitShipInfoScreen(void)
{
    char name[32];
    ds.ShipInfo_Backdrop = game_alloc(0xBCC0);
    Gfx_LoadPicScreen(ship_backdrop_name(name), ds.ShipInfo_Backdrop, 0x97, Gfx_PalettePtr());
    Blit_CopyRect(ds.ShipInfo_Backdrop, gfx_screen() + ROW49, 320, 0x97);
    ds.ShipInfo_Sprites = game_alloc(0x5000);
    Gfx_LoadPic("grafika\\ship3", ds.ShipInfo_Sprites, 1, 0x40, 0, 0, 320, false,
                Gfx_PalettePtr(), 0, 0xB0, 0x40, 0, 0xBE);
    ShipInfo_DrawAllGroupIcons();
    if (ds.Game_SelGroup != 0) {
        ShipInfo_DrawGroupInfo();
        ShipInfo_DrawSelectionMarker();
    }
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
}

// Shared by the two icon routines: the backdrop piece behind the slot is copied
// to dest first (so a previous icon vanishes), then the icon is blitted over it.
static void draw_icon(int16_t icon, int16_t slot, uint8_t *seg, uint16_t base_ofs,
                      uint16_t copy_dy, uint16_t blit_dy, uint16_t blit_dx)
{
    uint16_t col = slot_col(slot), row = slot_row(slot);
    uint16_t slot_x = (uint16_t)(col * 0x30);
    uint8_t *src = ds.ShipInfo_Backdrop + (uint16_t)(slot_x + (row * 0x10 + 0xB) * 320 + 0x10);
    Blit_CopyRect(src, seg + (uint16_t)(base_ofs + (row * 0x10 + copy_dy) * 320 + slot_x + 0x10),
                  0x20, 0xF);
    Mouse_HideCursor();
    if (icon != 0) {
        uint8_t *sprite = ds.ShipInfo_Sprites + (uint16_t)((icon - 1) * 0x20 + 0x141);
        Blit_BlitTransparent(
            sprite, seg + (uint16_t)(base_ofs + (row * 0x10 + blit_dy) * 320 + slot_x + blit_dx),
            0x1F, 0xE, 0x40);
    }
    Mouse_ShowCursor();
}

// 26fe:03c3: draws one group slot icon and refreshes its selection marker
void ShipInfo_DrawGroupIcon(int16_t icon, int16_t slot)
{
    draw_icon(icon, slot, gfx_screen(), 0, 0x3C, 0x3D, 0x11);
    int16_t saved = ds.Game_SelGroup;
    ds.Game_SelGroup = slot;
    if (saved < 0 || slot != saved)
        ShipInfo_EraseSelectionMarker();
    else
        ShipInfo_DrawSelectionMarker();
    ds.Game_SelGroup = saved;
}

static int16_t icon_of(const TGroup *g)
{
    // Statuses below 7 have 160 icons per Kind in the sheet; 7 (planet force) is its own.
    if (g->Status < 7)
        return (int16_t)(g->Status + (g->Kind - 1) * 0xA0);
    return g->Status;
}

// 26fe:04d4
void ShipInfo_DrawAllGroupIcons(void)
{
    int16_t n = ds.Game_ShownGroupCount;
    for (int16_t i = 1; i <= n; i++)
        ShipInfo_DrawGroupIcon(icon_of(Groups_At(ds.Game_GroupList, i)), i);
    for (int16_t i = (int16_t)(ds.Game_ShownGroupCount + 1); i < 0x21; i++) {
        ShipInfo_DrawGroupIcon(0, i);
        if (i == 0x20)
            break;
    }
}

// 26fe:056d: as DrawGroupIcon into the off-screen copy (a full 320x200 image)
void ShipInfo_DrawGroupIconBuffered(int16_t icon, int16_t slot)
{
    draw_icon(icon, slot, ds.ShipInfo_Buffer, ROW49, 0xB, 0xC, 0x11);
    int16_t saved = ds.Game_SelGroup;
    ds.Game_SelGroup = slot;
    if (saved < 0 || slot != saved)
        ShipInfo_EraseSelectionMarkerBuffered();
    else
        ShipInfo_DrawSelectionMarkerBuffered();
    ds.Game_SelGroup = saved;
}

// 26fe:0693: loads the other list's backdrop, draws its icons off-screen and
// wipes them in along a rotating line.
void ShipInfo_SwitchGroupList(void)
{
    char name[32];
    Gfx_LoadPic(ship_backdrop_name(name), ds.ShipInfo_Backdrop, 1, 0x97, 0, 0, 320, false,
                Gfx_PalettePtr(), 0, 0xB0, 0x40, 0, 0xBE);
    // 26fe:0710: a 320-wide image of the view from row 49. The icons and
    // markers (slots 1..32) and the wipe's lines stay inside it.
    ds.ShipInfo_Buffer = game_alloc(0xFB40);
    Blit_CopyBytes(ds.ShipInfo_Backdrop, ds.ShipInfo_Buffer + ROW49, 0xBCC0);
    int16_t n = ds.Game_ShownGroupCount;
    for (int16_t i = 1; i <= n; i++)
        ShipInfo_DrawGroupIconBuffered(icon_of(Groups_At(ds.Game_GroupList, i)), i);
    for (int16_t i = (int16_t)(ds.Game_ShownGroupCount + 1); i < 0x21; i++) {
        ShipInfo_DrawGroupIconBuffered(0, i);
        if (i == 0x20)
            break;
    }

    uint8_t *bseg = ds.ShipInfo_Buffer;
    Mouse_HideCursor();
    // Lines from the fixed point (95,125) around the edge of the view (rows 49..199).
    for (int16_t x = 0x5F; x <= 0xBE; x++) {
        if (x % 0xC == 0)
            Gfx_WaitVRetrace();
        Gfx_DrawLineCopy(gfx_screen(), x, 0x31, 0x5F, 0x7D, bseg);
    }
    for (int16_t y = 0x31; y <= 199; y++) {
        if (y % 0xC == 0)
            Gfx_WaitVRetrace();
        Gfx_DrawLineCopy(gfx_screen(), 0xBE, y, 0x5F, 0x7D, bseg);
    }
    for (int16_t x = 0xBE; x >= 0; x--) {
        if (x % 0xC == 0)
            Gfx_WaitVRetrace();
        Gfx_DrawLineCopy(gfx_screen(), x, 199, 0x5F, 0x7D, bseg);
    }
    for (int16_t y = 199; y >= 0x31; y--) {
        if (y % 0xC == 0)
            Gfx_WaitVRetrace();
        Gfx_DrawLineCopy(gfx_screen(), 0, y, 0x5F, 0x7D, bseg);
    }
    for (int16_t x = 0; x <= 0x5F; x++) {
        if (x % 0xC == 0)
            Gfx_WaitVRetrace();
        Gfx_DrawLineCopy(gfx_screen(), x, 0x31, 0x5F, 0x7D, bseg);
    }
    Mouse_ShowCursor();
    game_free(ds.ShipInfo_Buffer);
}

// The panel's strings are string[30]s.
enum { S30 = 31 };

// "Time remaining: n": (pilot experience + legs) / (experience + 1) + jump
static void draw_time_remaining(const TGroup *g, int16_t y)
{
    char s30[S30];
    int16_t legs = (int16_t)(g->Leg1 + g->Leg2);
    uint16_t exp = ds.Game_StaffExp[0];
    int32_t dividend = (int32_t)exp + legs;
    uint16_t divisor = (uint16_t)(exp + 1);
    if (divisor == 0)
        System_RunError(200, nullptr);
    int32_t q = dividend / divisor;
    SDL_snprintf(s30, sizeof s30, "Time remaining:%d", (int)(q + g->Jump));
    Gfx_DrawTextF9(s30, 0xCA, y, 0x12);
}

// 26fe:093b: right-hand info panel for the selected group
void ShipInfo_DrawGroupInfo(void)
{
    if (ds.Game_SelGroup == 0) {
        Blit_FillRect(199, 0x37, 0x72, 0x89, 0x40);
        return;
    }
    const TGroup *g = Groups_Selected();
    char loc1[S30], loc2[S30] = "", num[S30], name[256], system[256];
    PlanetInfo_GetBodyName(loc1, sizeof loc1, g->System, g->Planet, 0);
    if (g->Moon != 0)
        PlanetInfo_GetBodyName(loc2, sizeof loc2, g->System, g->Planet, g->Moon);
    pstr_get(name, sizeof name, &g->Name);

    if (g->Status == 7) {
        Gfx_DrawTextFC(name, 0xCA, 0x3C, 0x12);
        Gfx_DrawTextF9("Base on: ", 0xCA, 0x46, 0x12);
        pstr_get(system, sizeof system, &ds.Names_Systems[g->System - 1]);
        Gfx_DrawTextF9(system, 0x102, 0x46, 8);
        Gfx_DrawTextF9(loc1, 0xD3, 0x4F, 0xF);
        Gfx_DrawTextF9(loc2, 0xDC, 0x59, 0xE);
        Gfx_DrawTextF9("", 0xCA, 0x64, 0x12);
    }
    if (g->Status >= 4 && g->Status <= 6) {
        Gfx_DrawTextFC(name, 0xCA, 0x3C, 0x12);
        Gfx_DrawTextF9("Destination:", 0xCA, 0x46, 0x12);
        pstr_get(system, sizeof system, &ds.Names_SystemsShort[g->System - 1]);
        Gfx_DrawTextF9(system, 0x114, 0x46, 5);
        Gfx_DrawTextF9(loc1, 0xD3, 0x4F, 0xF);
        Gfx_DrawTextF9(loc2, 0xDC, 0x59, 0xE);
        draw_time_remaining(g, 100);
    }
    if (g->Status == 1 || g->Status == 2) {
        Gfx_DrawTextFC(name, 0xCA, 0x3C, 0x12);
        Gfx_DrawTextF9("Currently on:", 0xCA, 0x46, 0x12);
        pstr_get(system, sizeof system, &ds.Names_SystemsShort[g->System - 1]);
        Gfx_DrawTextF9(system, 0x11A, 0x46, 5);
        Gfx_DrawTextF9(loc1, 0xD3, 0x4F, 0xF);
        Gfx_DrawTextF9(loc2, 0xDC, 0x59, 0xE);
        Gfx_DrawTextF9("", 0xCA, 0x64, 0x12);
    }

    // Clear the unit list area, then one line per non-empty slot of each category.
    Mouse_HideCursor();
    Blit_FillRect(199, 0x73, 0x72, 0x4D, 0x40);
    Mouse_ShowCursor();
    int16_t y = 0x74;
    // The original uses the global SlotCategory as its loop counter; it ends at 4.
    for (ds.Groups_SlotCategory = 1;; ds.Groups_SlotCategory++) {
        int16_t cat = ds.Groups_SlotCategory;
        const TTypeSlots *ts = &ds.Groups_TypeSlots[g->Kind - 1];
        if (ts->Used[cat - 1] != 0) {
            uint8_t row = ds.Groups_SlotA[g->Kind - 1][cat - 1];
            const TGroupSlot *units = &g->Units[row - 1][0];
            const uint8_t *layout = ts->Layout[cat - 1];
            int n = layout[0];
            for (int j = 1; j <= n; j++) {
                const uint8_t *entry = layout + j * 0x19 + 0x21;
                int16_t count = units[j - 1].ShipType;
                if (count > 0) {
                    Gfx_DrawTextF9(pstr_get(name, sizeof name, entry + 2), 0xCC, y, 0xB);
                    Gfx_DrawTextF9(":", 0x112, y, 1);
                    SDL_snprintf(num, sizeof num, "%3d", count);
                    Gfx_DrawTextFC(num, 0x118, y, 4);
                    y = (int16_t)(y + 9);
                }
            }
            y = (int16_t)(y + 3);
        }
        if (ds.Groups_SlotCategory == 4)
            break;
    }

    if (g->Kind == 4) {
        // Carrier: satellites in the first unit slot's weapon words.
        const uint8_t *words = (const uint8_t *)&g->Units[0][0];
        const uint8_t *layout = ds.Groups_TypeSlots[g->Kind - 1].Layout[3];
        int n = layout[1];
        for (int j = 1; j <= n; j++) {
            int16_t count = rd16le(words + j * 2);
            if (count > 0) {
                Gfx_DrawTextF9(pstr_get(name, sizeof name, &ds.Names_Satellites[j - 1]), 0xD0, y,
                               0xB);
                Gfx_DrawTextF9(":", 0x112, y, 1);
                SDL_snprintf(num, sizeof num, "%3d", count);
                Gfx_DrawTextFC(num, 0x118, y, 4);
                y = (int16_t)(y + 9);
            }
        }
    }
    // (26fe:0d76: the gap is added for every kind, not only carriers.)
    y = (int16_t)(y + 3);

    int16_t total = 0;
    if (g->Kind == 2) {
        // Miner stations: the fourth weapon word of the first row's slots.
        const uint8_t *layout = ds.Groups_TypeSlots[g->Kind - 1].Layout[1];
        int n = layout[0];
        for (int j = 1; j <= n; j++)
            total = (int16_t)(total + g->Units[0][j - 1].Weapons[3]);
    }
    if (g->Kind == 2 && total > 0) {
        SDL_snprintf(num, sizeof num, "%3d", total);
        Gfx_DrawTextF9("Miner stat", 0xD0, y, 0xB);
        Gfx_DrawTextF9(":", 0x112, y, 1);
        Gfx_DrawTextFC(num, 0x118, y, 4);
    }
}

// 26fe:0f2a: short variant for a travelling group, used on screen 0x11
void ShipInfo_DrawGroupFlightInfo(void)
{
    if (ds.Game_SelGroup == 0) {
        Blit_FillRect(199, 0x37, 0x72, 0x89, 0x40);
        return;
    }
    const TGroup *g = Groups_Selected();
    char loc1[S30], loc2[S30] = "", name[256];
    PlanetInfo_GetBodyName(loc1, sizeof loc1, g->System, g->Planet, 0);
    if (g->Moon != 0)
        PlanetInfo_GetBodyName(loc2, sizeof loc2, g->System, g->Planet, g->Moon);
    if (g->Status == 4 || g->Status == 5 || g->Status == 6) {
        Gfx_DrawTextF9("Destination:", 0xCA, 0x46, 0x12);
        Gfx_DrawTextF9(pstr_get(name, sizeof name, &ds.Names_SystemsShort[g->System - 1]), 0x114,
                       0x46, 5);
        Gfx_DrawTextF9(loc1, 0xD3, 0x4F, 0xF);
        if (g->Moon != 0)
            Gfx_DrawTextF9(loc2, 0xDC, 0x59, 0xE);
        draw_time_remaining(g, 100);
    }
}

static void marker(uint16_t sprite_ofs, uint8_t *seg, uint16_t base)
{
    Mouse_HideCursor();
    if (ds.Game_SelGroup != 0) {
        uint16_t row = slot_row(ds.Game_SelGroup), col = slot_col(ds.Game_SelGroup);
        Blit_CopyRect(ds.ShipInfo_Sprites + sprite_ofs,
                      seg + (uint16_t)(base + (row * 0x10) * 320 + 5 + col * 0x30), 7, 6);
    }
    Mouse_ShowCursor();
}

// 26fe:10e8 / 1147 (screen) and 11a6 / 1209 (off-screen copy)
void ShipInfo_DrawSelectionMarker(void)
{
    marker(MARKER_ON, gfx_screen(), 0x41 * 320);
}

void ShipInfo_EraseSelectionMarker(void)
{
    marker(MARKER_OFF, gfx_screen(), 0x41 * 320);
}

void ShipInfo_DrawSelectionMarkerBuffered(void)
{
    marker(MARKER_ON, ds.ShipInfo_Buffer, ROW49 + 0x10 * 320);
}

void ShipInfo_EraseSelectionMarkerBuffered(void)
{
    marker(MARKER_OFF, ds.ShipInfo_Buffer, ROW49 + 0x10 * 320);
}

// 26fe:1278: hotspot 1 = Change mode, 2 = unused, 3..34 = icons, 35.. = names
void ShipInfo_SetupShipInfoHotspots(void)
{
    ds.Mouse_HotspotCount = (int16_t)(ds.Game_ShownGroupCount + 0x22);
    Mouse_SetHotspot(1, 0, 0xBC, 200, 12, "Change mode", 1, 0);
    Mouse_SetHotspot(2, 0, 0, 0, 0, "", 1, 0);
    for (int16_t i = 1; i <= 0x20; i++) {
        THotspot *h = Mouse_Hotspot((int16_t)(i + 2));
        h->X = (int16_t)(((i - 1) / 8) * 0x30);
        h->Y = (uint8_t)(((i - 1) % 8 << 4) + 0x3C);
        h->W = 0x10;
        h->H = 0x11;
        char name[256] = "";
        if (ds.Game_ShownGroupCount >= i)
            pstr_get(name, sizeof name, &Groups_At(ds.Game_GroupList, i)->Name);
        pstr_set(&h->Caption, 0x12, name);
        h->Repeat_ = 0;
        h->_1A = 0;
    }
    for (int16_t i = 1; i <= ds.Game_ShownGroupCount; i++) {
        THotspot *h = Mouse_Hotspot((int16_t)(i + 0x22));
        h->X = (int16_t)(((i - 1) / 8) * 0x30 + 0x11);
        h->Y = (uint8_t)(((i - 1) % 8 << 4) + 0x3C);
        h->W = 0x20;
        h->H = 0x11;
        char name[256];
        pstr_get(name, sizeof name, &Groups_At(ds.Game_GroupList, i)->Name);
        pstr_set(&h->Caption, 0x12, name);
        h->Repeat_ = 0;
        h->_1A = 0;
    }
}
