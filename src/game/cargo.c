#include "game/cargo.h"

#include <SDL3/SDL.h>

#include "game/fixes.h"
#include "game/gfx.h"
#include "game/groups.h"
#include "game/lang.h"
#include "game/le.h"
#include "game/mouse.h"
#include "game/system.h"

// Routines missing from their units' headers.
void Message_ShowMessageNoLog(const char *text);                                   // 34b0:0197
int16_t Surface_CountBuildings(int16_t kind, int16_t system, int16_t planet,
                               int16_t moon, bool activeOnly);                        // 2ef2:2b1a
void Surface_CountBuildingsByKind(int16_t system, int16_t planet, int16_t moon);      // 2ef2:2b9e
void PlanetInfo_SelectCurBody(void);                                                  // 357b:3253

#define group_at Groups_At
#define sel_group Groups_Selected

// The values the screen moves between ship and planet, in packed records:
// the planet's ore stock i (a longint of Game::HomeStock or the body's
// Stores, through Cargo::Stores), item i's store (an integer: the product's
// Stock or the planet force's Items[i]) and the ship's Cargo[i].
static int32_t store_of(int16_t i)
{
    return rd32le(ds.Cargo_Stores + (i - 1) * 4);
}

static void add_store(int16_t i, int32_t amount)
{
    wr32le(ds.Cargo_Stores + (i - 1) * 4, (int32_t)((uint32_t)store_of(i) + (uint32_t)amount));
}

static int16_t item_store(int16_t i)
{
    return rd16le(ds.Cargo_ItemStore[i - 1]);
}

static void add_item_store(int16_t i, int16_t amount)
{
    wr16le(ds.Cargo_ItemStore[i - 1], (int16_t)(item_store(i) + amount));
}

static int32_t cargo_of(const TGroup *g, int i)
{
    return g->Cargo[i - 1];
}

static void add_cargo(TGroup *g, int i, int32_t amount)
{
    g->Cargo[i - 1] = (int32_t)((uint32_t)cargo_of(g, i) + (uint32_t)amount);
}

int32_t Cargo_ShipCapacity(const TGroup *g)
{
    uint32_t sum = 0;
    for (int i = 1; i <= 4; i++)
        sum += (uint32_t)((int32_t)g->Units[0][i - 1].Count * ds.Cargo_UnitSpace[i - 1]);
    return (int32_t)sum;
}

int32_t Cargo_ItemsSpace(const TGroup *g)
{
    uint32_t sum = 0;
    for (int i = 1; i <= 13; i++)
        sum += (uint32_t)((int32_t)g->Items[i - 1] * ds.Cargo_ItemSpace[i - 1]);
    return (int32_t)sum;
}

// True when the ship is at a colony of ours whose store can be shown/used.
static bool at_own_colony(const TGroup *g)
{
    const TBody *b = ds.Game_CurBody;
    return g->Status == 1 && b->Owner == 1 && (b->Colony != 0 || b->Droids != 0);
}

// 2583:0029
void Cargo_CargoScreen(void)
{
    if (ds.Game_ScreenEntered) {
        Cargo_InitCargoScreen();
        Cargo_SetupCargoHotspots();
        Cargo_DrawCargoNames(0);
        Cargo_DrawCargoAmounts();
    }
    int16_t list = ds.Game_GroupList;
    int16_t sel = ds.Game_SelGroup;
    if (ds.Game_MenuCommand == 0x10) {
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = 0x11;
    }

    // Ship -> planet (hotspots 1..19, button column at x = 44h).
    ds.Cargo_Row = (int16_t)(ds.Game_HotspotHit - 0x14);
    if (ds.Cargo_Row > 0 && ds.Cargo_Row < 0x14) {
        TGroup *g = group_at(ds.Game_GroupList, ds.Game_SelGroup);
        if (ds.Cargo_Row < 7) {
            int slot = ds.Game_HotspotHit - 0x14;
            int32_t have = cargo_of(g, slot);
            if (have > 0) {
                int32_t amount = have;
                if (amount > 100 && ds.Mouse_HeldLeft)
                    amount = 100;
                uint32_t after = (uint32_t)store_of(ds.Cargo_Row) + (uint32_t)amount;
                if ((int32_t)after > ds.Cargo_PlanetCap)
                    amount = (int32_t)((uint32_t)ds.Cargo_PlanetCap
                                       - (uint32_t)store_of(ds.Cargo_Row));
                add_cargo(g, slot, (int32_t)(0u - (uint32_t)amount));
                add_store(ds.Cargo_Row, amount);
                ds.Cargo_ShipUsed -= (uint32_t)amount;
                Cargo_DrawCargoAmounts();
            }
            Cargo_FlashCargoRow(ds.Cargo_Row);
        } else if (Surface_CountBuildings(0xC, g->System, g->Planet, g->Moon, false) < 1) {
            Message_ShowMessageNoLog(Lang_Text(" You need a spaceport to transfer items "));
        } else {
            ds.Cargo_Row -= 6;
            TGroup *gl = group_at(list, sel);
            int16_t idx = (int16_t)ds.Cargo_ItemList[ds.Cargo_Row - 1];
            if (gl->Items[idx - 1] > 0) {
                int32_t space = ds.Cargo_ItemSpace[idx - 1];
                gl->Items[idx - 1]--;
                add_item_store(idx, 1);
                ds.Cargo_ShipUsed -= (uint32_t)space;
                Cargo_DrawCargoAmounts();
            }
            Cargo_FlashCargoRow(ds.Cargo_Row + 6);
        }
    }

    // Planet -> ship (hotspots 20..38, button column at x = EBh).
    list = ds.Game_GroupList;
    sel = ds.Game_SelGroup;
    int16_t hit = ds.Game_HotspotHit;
    ds.Cargo_Row = (int16_t)(hit - 0x27);
    if (ds.Cargo_Row > 0 && ds.Cargo_Row < 0x14) {
        TGroup *g = group_at(ds.Game_GroupList, ds.Game_SelGroup);
        if (ds.Cargo_Row < 7) {
            int32_t have = store_of(ds.Cargo_Row);
            if (have > 0) {
                int32_t amount = have;
                if (amount > 100 && ds.Mouse_HeldLeft)
                    amount = 100;
                int32_t room = (int32_t)((uint32_t)ds.Cargo_ShipCap - (uint32_t)ds.Cargo_ShipUsed);
                if (room < amount)
                    amount = room;
                // The original then caps this ore's cargo slot at the planet's
                // storage (Cargo::PlanetCap). Port fix (game/fixes.h): the ship's
                // room alone.
                int32_t slot = cargo_of(g, ds.Cargo_Row);
                if (!fixes.cargo_planet_cap
                    && (int32_t)((uint32_t)slot + (uint32_t)amount) > ds.Cargo_PlanetCap)
                    amount = (int32_t)((uint32_t)ds.Cargo_PlanetCap - (uint32_t)slot);
                add_cargo(g, ds.Cargo_Row, amount);
                add_store(ds.Cargo_Row, (int32_t)(0u - (uint32_t)amount));
                ds.Cargo_ShipUsed += (uint32_t)amount;
                Cargo_DrawCargoAmounts();
            }
            Cargo_FlashCargoRow(ds.Cargo_Row);
        } else if (Surface_CountBuildings(0xC, g->System, g->Planet, g->Moon, false) < 1) {
            Message_ShowMessageNoLog(Lang_Text(" You need a spaceport to transfer items "));
        } else {
            ds.Cargo_Row -= 6;
            int16_t idx = (int16_t)ds.Cargo_ItemList[ds.Cargo_Row - 1];
            int32_t space = ds.Cargo_ItemSpace[idx - 1];
            int32_t room = (int32_t)((uint32_t)ds.Cargo_ShipCap - (uint32_t)ds.Cargo_ShipUsed);
            if (item_store(idx) > 0 && space <= room) {
                group_at(list, sel)->Items[idx - 1]++;
                add_item_store(idx, -1);
                ds.Cargo_ShipUsed += (uint32_t)space;
                Cargo_DrawCargoAmounts();
            }
            Cargo_FlashCargoRow(ds.Cargo_Row + 6);
        }
    }
}

// 2583:0494
void Cargo_InitCargoScreen(void)
{
    TGroup *g = sel_group();
    if (g->Status == 1 || g->Status == 2) {
        ds.Game_CurSystem = g->System;
        ds.Game_CurPlanet[ds.Game_CurSystem - 1] = g->Planet;
        ds.Game_CurMoon[ds.Game_CurSystem - 1] = g->Moon;
        PlanetInfo_SelectCurBody();
    }
    g = sel_group();
    bool home = g->System == 1 && g->Planet == 5 && g->Moon == 0;
    Groups_FindPlanetForce(g->System, g->Planet, g->Moon);

    // Items this game can store: product ready for production (State 5).
    ds.Cargo_ItemCount = 0;
    for (int i = 1; i <= 13; i++) {
        int8_t prod = ds.Cargo_Items[i - 1];
        if (prod > 0 && ds.Research_Products[prod - 1].State == 5) {
            ds.Cargo_ItemCount++;
            ds.Cargo_ItemList[ds.Cargo_ItemCount - 1] = (uint16_t)i;
        }
    }

    // Ship capacity, then the space used by its items and ore cargo.
    ds.Cargo_ShipUsed = 0;
    ds.Cargo_ShipCap = Cargo_ShipCapacity(g);
    ds.Cargo_ShipUsed += (uint32_t)Cargo_ItemsSpace(g);
    for (int i = 1; i <= 6; i++)
        ds.Cargo_ShipUsed += (uint32_t)cargo_of(g, i);

    Surface_CountBuildingsByKind(g->System, g->Planet, g->Moon);
    int16_t pf = Groups_FindPlanetForce(g->System, g->Planet, g->Moon);
    TGroup *pforce = group_at(1, pf);
    ds.Cargo_PlanetCap = (int32_t)(uint16_t)Groups_PlanetStorage(g->System, g->Planet, g->Moon) * 1000;
    for (int i = 1; i <= 13; i++) {
        int8_t prod = ds.Cargo_Items[i - 1];
        if (prod > 0) {
            if (home)
                ds.Cargo_ItemStore[i - 1] = (uint8_t *)&ds.Research_Products[prod - 1].Stock;
            else
                ds.Cargo_ItemStore[i - 1] = (uint8_t *)&pforce->Items[i - 1];
        }
    }
    if (home) {
        ds.Cargo_Stores = (uint8_t *)ds.Game_HomeStock;
    } else {
        ds.Cargo_Stores = (uint8_t *)ds.Game_CurBody->Stores;
    }
}

// 2583:07a6
void Cargo_SetupCargoHotspots(void)
{
    ds.Mouse_HotspotCount = 0x26;
    Mouse_ClearHotspots(1, 0x26);
    if (at_own_colony(sel_group())) {
        int16_t n = (int16_t)(ds.Cargo_ItemCount + 6);
        for (int16_t i = 1; i <= n; i++)
            Mouse_SetHotspot(i, 0x44, (uint8_t)(i * 8 + 0x35), 0x10, 7,
                             Lang_Text("Transfer to planet"), 0, 0);
        for (int16_t i = 1; i <= n; i++)
            Mouse_SetHotspot((int16_t)(i + 0x13), 0xEB, (uint8_t)(i * 8 + 0x35), 0x10, 7,
                             Lang_Text("Transfer to ship"), 0, 0);
    }
}

// 2583:0894
void Cargo_FlashCargoRow(int16_t row)
{
    Cargo_DrawCargoNames(row);
    Gfx_WaitFrames(4);
    Cargo_DrawCargoNames(0);
}

// 2583:08b1
void Cargo_DrawCargoNames(int16_t row)
{
    for (int16_t i = 1; i <= 6; i++) {
        char name[256];
        pstr_get(name, sizeof name, &ds.Names_Ores[i - 1]);
        if (i == row)
            Gfx_DrawTextFC(name, 0x70, (int16_t)(i * 8 + 0x35), 0x10);
        else
            Gfx_DrawTextF9(name, 0x70, (int16_t)(i * 8 + 0x35), 0x10);
    }
    int16_t n = (int16_t)ds.Cargo_ItemCount;
    for (int16_t i = 1; i <= n; i++) {
        int8_t prod = ds.Cargo_Items[ds.Cargo_ItemList[i - 1] - 1];
        char name[256];
        pstr_get(name, sizeof name, &ds.Research_Products[prod - 1].Name);
        if (i + 6 == row)
            Gfx_DrawTextFC(name, 0x70, (int16_t)(i * 8 + 0x65), 0x10);
        else
            Gfx_DrawTextF9(name, 0x70, (int16_t)(i * 8 + 0x65), 0x10);
    }
}

// 2583:09c6
void Cargo_DrawCargoAmounts(void)
{
    TGroup *g = sel_group();
    TBody *body = ds.Game_CurBody;
    char s[64];

    if (at_own_colony(g)) {
        for (int16_t i = 1; i <= 6; i++) {
            SDL_snprintf(s, sizeof s, "%6d", (int)store_of(i));
            Gfx_DrawTextF9(s, 0xC, (int16_t)(i * 8 + 0x35), 6);
        }
    }
    if (g->Status == 1) {
        if (body->Owner == 1 && body->Colony != 0
            && Surface_CountBuildings(0xC, g->System, g->Planet, g->Moon, false) > 0
            && (int16_t)ds.Cargo_ItemCount > 0) {
            int16_t n = (int16_t)ds.Cargo_ItemCount;
            for (int16_t i = 1; i <= n; i++) {
                SDL_snprintf(s, sizeof s, "%6d", item_store((int16_t)ds.Cargo_ItemList[i - 1]));
                Gfx_DrawTextF9(s, 0xC, (int16_t)(i * 8 + 0x65), 6);
            }
        }
    }
    for (int16_t i = 1; i <= 6; i++) {
        SDL_snprintf(s, sizeof s, "%6d", (int)cargo_of(g, i));
        Gfx_DrawTextF9(s, 0x10E, (int16_t)(i * 8 + 0x35), 6);
    }
    int16_t n = (int16_t)ds.Cargo_ItemCount;
    for (int16_t i = 1; i <= n; i++) {
        SDL_snprintf(s, sizeof s, "%6d", g->Items[ds.Cargo_ItemList[i - 1] - 1]);
        Gfx_DrawTextF9(s, 0x10E, (int16_t)(i * 8 + 0x65), 6);
    }
    SDL_snprintf(s, sizeof s, "%s%6d/%d", Lang_Text("Space: "), (int)ds.Cargo_ShipUsed,
                 (int)ds.Cargo_ShipCap);
    Gfx_DrawTextF9(s, 5, 0xC1, 0x19);
    SDL_snprintf(s, sizeof s, "%s%d", Lang_Text("Storage space on planet: "), (int)ds.Cargo_PlanetCap);
    Gfx_DrawTextF9(s, 0x80, 0xC1, 0x1F);
}
