// Unit GrWarUnits (1430): ground battle unit assignment screen.
#include "game/grwarunits.h"

#include <string.h>

#include "game/blit.h"
#include "game/gfx.h"
#include "game/gfxpath.h"
#include "game/groups.h"
#include "game/minmax.h"
#include "game/mouse.h"
#include "game/music.h"
#include "game/planetinfo.h"
#include "game/sound.h"
#include "game/system.h"

// Adds to a longint table entry (GrWarUnits::Totals, Pool, ...) with the
// original's 32-bit wrap. A macro: the packed entries can't be pointed to.
#define ADD32(entry, v) ((entry) = (int32_t)((uint32_t)(entry) + (uint32_t)(int32_t)(v)))

static GrUnit *unit_at(FarPtr list, int i) { return (GrUnit *)far_host(list) + (i - 1); }

static const uint8_t *ps(const char *lit_with_len) { return (const uint8_t *)lit_with_len; }

// 1430:000f
void GrWarUnits_UnitAssignScreen(void)
{
    if (ds.Game_ScreenEntered) {
        ds.Game_CurSystem = ds.SpWar_BattleSystem;
        ds.Game_CurPlanet[ds.SpWar_BattleSystem - 1] = ds.SpWar_BattlePlanet;
        ds.Game_CurMoon[ds.Game_CurSystem - 1] = ds.SpWar_BattleMoon;
        PlanetInfo_SelectCurBody();
        Music_PlayTrack(ps("\x05" "earth"));
        GrWarUnits_CollectAvailableForces();
        GrWarUnits_UnitAssignDrawScreen();
        GrWarUnits_SetupUnitHotspots();
    }
    if (ds.Game_MenuCommand == 0xB) {
        ds.Game_CurrentScreen = 0x20;
        ds.Game_ScreenSwitch = true;
    }
    if (ds.Game_MenuCommand == 0x2A) {
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = 7;
        ds.Game_CurSystem = ds.SpWar_BattleSystem;
        ds.Game_CurPlanet[ds.SpWar_BattleSystem - 1] = ds.SpWar_BattlePlanet;
        ds.Game_CurMoon[ds.Game_CurSystem - 1] = ds.SpWar_BattleMoon;
    }

    // The toolbar icons for unit types 1..4 add a new slot of that type.
    uint16_t t = ds.Game_MenuCommand == 0xC;
    if (ds.Game_MenuCommand == 0x13)
        t = 2;
    if (ds.Game_MenuCommand == 0x2C)
        t = 3;
    if (ds.Game_MenuCommand == 0x24)
        t = 4;
    if (t != 0 && (uint16_t)ds.GrWarUnits_AttackerSlotCount < 0x14) {
        GrUnit *u = unit_at(ds.GrWarUnits_UnitLists[0], ds.GrWarUnits_AttackerSlotCount + 1);
        uint16_t n = (uint16_t)MinMax_Min(ds.GrWarUnits_Pool[t - 1], ds.GrWarUnits_SlotCap[t - 1]);
        ADD32(ds.GrWarUnits_Pool[t - 1], -(int32_t)(int16_t)n);
        u->type = (uint8_t)t;
        u->count = (int16_t)n;
        ds.GrWarUnits_AttackerSlotCount++;
        GrWarUnits_DrawUnitList();
    }

    uint16_t hit = (uint16_t)ds.Game_HotspotHit;
    // "Remove unit": return the slot's units to the pool and close the gap.
    if (hit > 0x14 && hit < 0x29 && (uint16_t)(hit - 0x14) <= (uint16_t)ds.GrWarUnits_AttackerSlotCount) {
        GrUnit *u = unit_at(ds.GrWarUnits_UnitLists[0], hit - 0x14);
        ADD32(ds.GrWarUnits_Pool[u->type - 1], u->count);
        int16_t last = ds.GrWarUnits_AttackerSlotCount - 1;
        for (int16_t i = hit - 0x14; i <= last; i++)
            System_MoveFwd(unit_at(ds.GrWarUnits_UnitLists[0], i + 1),
                           unit_at(ds.GrWarUnits_UnitLists[0], i), 0x27);
        ds.GrWarUnits_AttackerSlotCount--;
        GrWarUnits_DrawUnitList();
        Sound_PlayButtonSfx(ps("\x08" "click-ok"));
    }
    // "Add/sub": left button adds one unit from the pool, the other removes one.
    if (hit > 0x28 && hit < 0x3D && (uint16_t)(hit - 0x28) <= (uint16_t)ds.GrWarUnits_AttackerSlotCount) {
        GrUnit *u = unit_at(ds.GrWarUnits_UnitLists[0], hit - 0x28);
        int16_t cap = ds.GrWarUnits_SlotCap[u->type - 1];
        if (ds.Mouse_HeldLeft != 0) {
            if (ds.GrWarUnits_Pool[u->type - 1] > 0 && u->count < cap) {
                int16_t one = (int16_t)MinMax_Min(1, ds.GrWarUnits_Pool[u->type - 1]);
                int16_t n = (int16_t)MinMax_Min(one, cap - u->count);
                u->count = (int16_t)(u->count + n);
                ADD32(ds.GrWarUnits_Pool[u->type - 1], -(int32_t)n);
            }
        }
        if (ds.Mouse_HeldLeft == 0 && u->count > 0) {
            int16_t n = (int16_t)MinMax_Min(1, u->count);
            u->count = (int16_t)(u->count - n);
            ADD32(ds.GrWarUnits_Pool[u->type - 1], n);
        }
        GrWarUnits_DrawUnitList();
    }
}

// 1430:033c
void GrWarUnits_UnitAssignDrawScreen(void)
{
    GfxPath_SetToolbar(0x13);
    if (ds.Sim_PlayerAttacks != 0)
        GfxPath_AddToolbarIcon(0x2A);
    if (ds.GrWarUnits_Totals[0][0] > 0)
        GfxPath_AddToolbarIcon(0xC);
    if (ds.GrWarUnits_Totals[0][1] > 0)
        GfxPath_AddToolbarIcon(0x13);
    if (ds.GrWarUnits_Totals[0][2] > 0)
        GfxPath_AddToolbarIcon(0x2C);
    if (ds.GrWarUnits_Totals[0][3] > 0)
        GfxPath_AddToolbarIcon(0x24);
    GfxPath_ShowToolbar();
    ds.GrWar_UnitIcons = System_GetMem(0x3C00);
    FarPtr buf = System_GetMem(0xBCC0);
    Gfx_LoadPic(ps("\x0C" "war\\grbeoszt"), far_host(buf), 1, 0x97, 0, 0, 0x140, true,
                Gfx_PalettePtr(), 0, 0xB0, 0x40, 0, 0xBE);
    Blit_CopyRect(far_host(buf), gfx_screen() + 0x3D40, 0x140, 0x97);
    Gfx_LoadPic(ps("\x0A" "war\\gricon"), far_host(ds.GrWar_UnitIcons), 1, 0x30, 0, 0, 0x140, false,
                Gfx_PalettePtr(), 0, 0x6F, 0x40, 0, 0x6F);
    GrWarUnits_DrawUnitList();
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
    System_FreeMem(buf, 0xBCC0);
}

// Fills up to 20 slots of `list` from GrWarUnits::Scratch (types 1..4), at
// most SlotCap[t] units per slot; returns the slot count.
static int16_t fill_slots(FarPtr list, bool defenders)
{
    int16_t count = 0;
    for (int t = 1; t <= 4; t++) {
        while (ds.GrWarUnits_Scratch[t - 1] > 0 && count < 0x14) {
            int16_t n =
                (int16_t)MinMax_Min(ds.GrWarUnits_Scratch[t - 1], ds.GrWarUnits_SlotCap[t - 1]);
            ADD32(ds.GrWarUnits_Scratch[t - 1], -(int32_t)n);
            GrUnit *u = unit_at(list, count + 1);
            u->type = (uint8_t)t;
            u->count = n;
            if (defenders) {
                uint8_t *r = (uint8_t *)u;
                r[9] = 0;
                r[8] = 3;
                r[0xA] = 1;
                r[0xB] = 1;
                r[0xF] = 1;
                r[0x12] = 0;
            }
            count++;
        }
    }
    return count;
}

// 1430:0479: sums troops/attack/defence per unit type from groups, race
// forces and the planet's stores, then builds the two slot lists.
void GrWarUnits_CollectAvailableForces(void)
{
    for (int t = 1; t <= 4; t++) {
        ds.GrWarUnits_Attack[0][t - 1] = 0;
        ds.GrWarUnits_Defence[0][t - 1] = 0;
        ds.GrWarUnits_Totals[0][t - 1] = 0;
        ds.GrWarUnits_Attack[1][t - 1] = 0;
        ds.GrWarUnits_Defence[1][t - 1] = 0;
        ds.GrWarUnits_Totals[1][t - 1] = 0;
    }
    ds.GrWarUnits_UnitLists[0] = System_GetMem(0x333);
    ds.GrWarUnits_UnitLists[1] = System_GetMem(0x333);
    ds.GrWarUnits_AttackerSlotCount = 0;
    ds.GrWarUnits_DefenderSlotCount = 0;

    // The player's fleets and planet forces at the battle location.
    int16_t counts[2] = {ds.Game_GroupCount, ds.Game_PlanetForceCount};
    for (int list = 0; list < 2; list++) {
        for (int i = 1; i <= counts[list]; i++) {
            const TGroup *g = Groups_At((int16_t)list, (int16_t)i);
            if (g->System == ds.SpWar_BattleSystem && g->Planet == ds.SpWar_BattlePlanet
                && g->Moon == ds.SpWar_BattleMoon
                && (g->Status == 1 || g->Status == 2 || g->Status == 7)
                && (g->Kind == 1 || (g->Kind == 5 && g->Moon == ds.SpWar_BattleMoon))) {
                for (int t = 1; t <= 4; t++) {
                    if ((uint16_t)ds.GrWarUnits_AttackerSlotCount < 0x14) {
                        // Ground troops are the second row of the group's units.
                        const TGroupSlot *s = &g->Units[1][t - 1];
                        int32_t atk = s->Weapons[1] * ds.GrWarUnits_WeaponWeight[1]
                                      + s->Weapons[0] * ds.GrWarUnits_WeaponWeight[0];
                        ADD32(ds.GrWarUnits_Attack[0][t - 1], atk);
                        ADD32(ds.GrWarUnits_Defence[0][t - 1],
                              s->Weapons[2] * ds.GrWarUnits_WeaponWeight[2]);
                        ADD32(ds.GrWarUnits_Totals[0][t - 1], s->ShipType);
                    }
                }
            }
        }
    }

    // Race forces: status 6 races attack, status 2 races defend. The unit
    // counts of a force are its units 5..8; products are 16-bit as in the original.
    for (int r = 2; r <= 12; r++) {
        const TRace *race = &ds.Races_Races[r - 2];
        for (int pass = 0; pass < 2; pass++) {
            int8_t want = pass == 0 ? 6 : 2;
            if (race->ForceCount <= 0 || race->Status != want)
                continue;
            for (int k = 1; k <= race->ForceCount; k++) {
                const TRaceForce *f = &race->Forces[k - 1];
                if (f->System == ds.SpWar_BattleSystem && f->Body == ds.SpWar_BattlePlanet
                    && f->Moon == ds.SpWar_BattleMoon && f->Active == 1) {
                    for (int t = 1; t <= 4; t++) {
                        uint16_t u = (uint16_t)f->Units[t + 3];
                        ADD32(ds.GrWarUnits_Totals[pass][t - 1], u);
                        ADD32(ds.GrWarUnits_Attack[pass][t - 1],
                              (uint16_t)(u * ds.GrWarUnits_TypeAttack[t - 1]));
                        ADD32(ds.GrWarUnits_Defence[pass][t - 1],
                              (uint16_t)(u * ds.GrWarUnits_TypeDefence[t - 1]));
                    }
                }
            }
        }
    }

    // The planet's own stores belong to its owner's side.
    const TBody *body = (const TBody *)far_host(ds.Game_CurBody);
    if ((int8_t)body->Owner > 1 && ds.Races_Races[(int8_t)body->Owner - 2].Status == 6) {
        for (int t = 1; t <= 4; t++) {
            int32_t s = body->Stores[t + 3];
            ADD32(ds.GrWarUnits_Totals[0][t - 1], s);
            ADD32(ds.GrWarUnits_Attack[0][t - 1],
                  (uint32_t)ds.GrWarUnits_TypeAttack[t - 1] * (uint32_t)s);
        }
    }
    if ((int8_t)body->Owner > 1 && ds.Races_Races[(int8_t)body->Owner - 2].Status == 2) {
        for (int t = 1; t <= 4; t++) {
            int32_t s = body->Stores[t + 3];
            ADD32(ds.GrWarUnits_Totals[1][t - 1], s);
            ADD32(ds.GrWarUnits_Attack[1][t - 1],
                  (uint32_t)ds.GrWarUnits_TypeAttack[t - 1] * (uint32_t)s);
            ADD32(ds.GrWarUnits_Defence[1][t - 1],
                  (uint32_t)ds.GrWarUnits_TypeDefence[t - 1] * (uint32_t)s);
        }
    }

    // Attackers: slots from the totals; what is left over is the pool.
    System_MoveFwd(ds.GrWarUnits_Totals[0], ds.GrWarUnits_Scratch, 0x10);
    ds.GrWarUnits_AttackerSlotCount = fill_slots(ds.GrWarUnits_UnitLists[0], false);
    System_MoveFwd(ds.GrWarUnits_Scratch, ds.GrWarUnits_Pool, 0x10);
    // Defenders: every unit goes to a slot.
    System_MoveFwd(ds.GrWarUnits_Totals[1], ds.GrWarUnits_Scratch, 0x10);
    ds.GrWarUnits_DefenderSlotCount = fill_slots(ds.GrWarUnits_UnitLists[1], true);
}

// 1430:0c14: 20 "Remove unit" and 20 "Add/sub ??" buttons, four per row.
void GrWarUnits_SetupUnitHotspots(void)
{
    ds.Mouse_HotspotCount = 0x28;
    for (int j = 1; j <= 0x28; j++) {
        THotspot *h = Mouse_Hotspot((int16_t)j);
        bool remove = j <= 0x14;
        int k = remove ? j - 1 : j - 0x15;
        h->X = (int16_t)((k % 4) * 0x4D + (remove ? 10 : 0x33));
        h->Y = (uint8_t)(k / 4 * 0x17 + 0x38);
        h->W = remove ? 0x12 : 0x1E;
        h->H = 0x10;
        System_StrStore(remove ? PSTR("Remove unit") : PSTR("Add/sub ??"), (PString *)&h->Caption, 0x12);
        h->Repeat_ = remove;
        h->_1A = 0;
    }
}

// 1430:0d3c: each slot's count, strength (attack average x count) and icon,
// emptied slots blanked, then the pool of each type and the force totals.
void GrWarUnits_DrawUnitList(void)
{
    PString s;
    int16_t n = ds.GrWarUnits_AttackerSlotCount;
    for (int i = 1; i <= n; i++) {
        const GrUnit *u = unit_at(ds.GrWarUnits_UnitLists[0], i);
        int16_t col = (int16_t)((i - 1) % 4 * 0x4D);
        int16_t row = (int16_t)((i - 1) / 4 * 0x17 + 0x31);
        uint16_t ofs = (uint16_t)(row * 0x140 + col);
        System_StrLong(u->count, 3, &s, 0x32);
        Gfx_DrawTextF9((uint8_t *)&s, (int16_t)(col + 0x3D), (int16_t)(row + 7), 3);
        int32_t total = ds.GrWarUnits_Totals[0][u->type - 1];
        int32_t str = 0;
        if (total > 0)
            str = (int32_t)((uint32_t)(int32_t)u->count
                            * (uint32_t)ds.GrWarUnits_Attack[0][u->type - 1]) / total;
        System_StrLong(str, 4, &s, 0x32);
        Gfx_DrawTextF9((uint8_t *)&s, (int16_t)(col + 0x37), (int16_t)(row + 0xF), 4);
        const uint8_t *icon =
            (const uint8_t *)far_host(ds.GrWar_UnitIcons) + (uint16_t)((u->type - 1) * 0x50);
        Blit_CopyRect(icon, gfx_screen() + (uint16_t)(ofs + 10 + 0x8C0), 0x10, 0x10);
    }
    for (int i = n + 1; i <= 0x14; i++) {
        int16_t col = (int16_t)((i - 1) % 4 * 0x4D);
        int16_t row = (int16_t)((i - 1) / 4 * 0x17 + 0x31);
        Gfx_DrawTextF9((const uint8_t *)PSTR(""), (int16_t)(col + 0x3D), (int16_t)(row + 7), 3);
        Gfx_DrawTextF9((const uint8_t *)PSTR(""), (int16_t)(col + 0x37), (int16_t)(row + 0xF), 4);
        Mouse_HideCursor();
        Blit_FillRect((int16_t)(col + 10), (int16_t)(row + 7), 0x10, 0x10, 0x40);
        Mouse_ShowCursor();
    }

    // Unit types still unassigned (pool) with their names, two per row.
    static const char *names[4] = {"Trooper", "Battle tank", "Aircraft", "Rocket launcher"};
    static const int16_t nx[4] = {0x3C, 0x3C, 0xA0, 0xA0}, ny[4] = {0xB0, 0xBA, 0xB0, 0xBA};
    static const int16_t vx[4] = {0x82, 0x82, 0x104, 0x104};
    static const int16_t nw[4] = {0xC, 0xC, 0x10, 0x10};
    for (int t = 1; t <= 4; t++) {
        // Test the attackers' total, print the unassigned pool.
        if (ds.GrWarUnits_Totals[0][t - 1] > 0) {
            System_StrLong(ds.GrWarUnits_Pool[t - 1], 0, &s, 0x32);
            PString name;
            name.len = (uint8_t)strlen(names[t - 1]);
            memcpy(name.s, names[t - 1], name.len);
            Gfx_DrawTextF9((uint8_t *)&name, nx[t - 1], ny[t - 1], nw[t - 1]);
            Gfx_DrawTextF9((uint8_t *)&s, vx[t - 1], ny[t - 1], 5);
        }
    }
}
