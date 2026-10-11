#include "game/cargo.h"

#include "game/gfx.h"
#include "game/groups.h"
#include "game/mouse.h"
#include "game/system.h"

// Not ported yet: see stubs.c.
void Message_ShowMessageNoLog(const uint8_t *text);                                   // 34b0:0197
int16_t Surface_CountBuildings(int16_t kind, int16_t system, int16_t planet,
                               int16_t moon, bool activeOnly);                        // 2ef2:2b1a
void Surface_CountBuildingsByKind(int16_t system, int16_t planet, int16_t moon);      // 2ef2:2b9e
void PlanetInfo_SelectCurBody(void);                                                  // 357b:3253

enum { GROUP_SIZE = sizeof(TGroup) };

// Groups is array[0..1] of array[1..32] of TGroup.
static FarPtr group_far(int16_t list, int16_t idx)
{
    uint16_t seg = (uint16_t)(ds.Game_Groups >> 16);
    uint16_t off = (uint16_t)ds.Game_Groups;
    return far_ptr(seg, (uint16_t)(off + (list * 32 + idx - 1) * GROUP_SIZE));
}

#define group_at Groups_At
#define sel_group Groups_Selected

static FarPtr ds_far(uint16_t off)
{
    return far_ptr(MEM_DS_SEG, off);
}

static int32_t *store_at(int16_t i)
{
    return (int32_t *)((uint8_t *)far_host(ds.Cargo_Stores) + (i - 1) * 4);
}

static int16_t *item_store(int16_t i)
{
    return far_host(ds.Cargo_ItemStore[i - 1]);
}

int32_t Cargo_ShipCapacity(const TGroup *g)
{
    uint32_t sum = 0;
    for (int i = 1; i <= 4; i++)
        sum += (uint32_t)((int32_t)g->Units[0][i - 1].ShipType * ds.Cargo_UnitSpace[i - 1]);
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
    const TBody *b = far_host(ds.Game_CurBody);
    return g->Status == 1 && b->Owner == 1 && (b->Colony != 0 || b->_0A != 0);
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
            TCargoSlot *slot = &g->Cargo[ds.Game_HotspotHit - 1 - 0x14];
            int32_t have = *(int32_t *)slot;
            if (have > 0) {
                int32_t amount = have;
                if (amount > 100 && ds.Mouse_HeldLeft)
                    amount = 100;
                uint32_t after = (uint32_t)*store_at(ds.Cargo_Row) + (uint32_t)amount;
                if ((int32_t)after > ds.Cargo_PlanetCap)
                    amount = (int32_t)((uint32_t)ds.Cargo_PlanetCap
                                       - (uint32_t)*store_at(ds.Cargo_Row));
                int32_t *cargo = (int32_t *)slot;
                *cargo = (int32_t)((uint32_t)*cargo - (uint32_t)amount);
                int32_t *store = store_at(ds.Cargo_Row);
                *store = (int32_t)((uint32_t)*store + (uint32_t)amount);
                ds.Cargo_ShipUsed -= (uint32_t)amount;
                Cargo_DrawCargoAmounts();
            }
            Cargo_FlashCargoRow(ds.Cargo_Row);
        } else if (Surface_CountBuildings(0xC, g->System, g->Planet, g->Moon, false) < 1) {
            Message_ShowMessageNoLog((const uint8_t *)PSTR(" You need a spaceport to transfer items "));
        } else {
            ds.Cargo_Row -= 6;
            TGroup *gl = group_at(list, sel);
            int16_t idx = (int16_t)ds.Cargo_ItemList[ds.Cargo_Row - 1];
            if (gl->Items[idx - 1] > 0) {
                int32_t space = ds.Cargo_ItemSpace[idx - 1];
                gl->Items[idx - 1]--;
                (*item_store(idx))++;
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
            int32_t have = *store_at(ds.Cargo_Row);
            if (have > 0) {
                int32_t amount = have;
                if (amount > 100 && ds.Mouse_HeldLeft)
                    amount = 100;
                int32_t room = (int32_t)((uint32_t)ds.Cargo_ShipCap - (uint32_t)ds.Cargo_ShipUsed);
                if (room < amount)
                    amount = room;
                // BUG? the ship's room is checked above, but this ore's cargo slot is
                // then capped by the planet's storage limit (Cargo::PlanetCap), as in the original.
                int32_t *slot = (int32_t *)&g->Cargo[ds.Cargo_Row - 1];
                if ((int32_t)((uint32_t)*slot + (uint32_t)amount) > ds.Cargo_PlanetCap)
                    amount = (int32_t)((uint32_t)ds.Cargo_PlanetCap - (uint32_t)*slot);
                *slot = (int32_t)((uint32_t)*slot + (uint32_t)amount);
                int32_t *store = store_at(ds.Cargo_Row);
                *store = (int32_t)((uint32_t)*store - (uint32_t)amount);
                ds.Cargo_ShipUsed += (uint32_t)amount;
                Cargo_DrawCargoAmounts();
            }
            Cargo_FlashCargoRow(ds.Cargo_Row);
        } else if (Surface_CountBuildings(0xC, g->System, g->Planet, g->Moon, false) < 1) {
            Message_ShowMessageNoLog((const uint8_t *)PSTR(" You need a spaceport to transfer items "));
        } else {
            ds.Cargo_Row -= 6;
            int16_t idx = (int16_t)ds.Cargo_ItemList[ds.Cargo_Row - 1];
            int32_t space = ds.Cargo_ItemSpace[idx - 1];
            int32_t room = (int32_t)((uint32_t)ds.Cargo_ShipCap - (uint32_t)ds.Cargo_ShipUsed);
            if (*item_store(idx) > 0 && space <= room) {
                group_at(list, sel)->Items[idx - 1]++;
                (*item_store(idx))--;
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
        ds.Cargo_ShipUsed += (uint32_t)*(int32_t *)&g->Cargo[i - 1];

    Surface_CountBuildingsByKind(g->System, g->Planet, g->Moon);
    int16_t pf = Groups_FindPlanetForce(g->System, g->Planet, g->Moon);
    ds.Cargo_PlanetCap = (int32_t)(uint16_t)group_at(1, pf)->Storage * 1000;
    FarPtr pfar = group_far(1, pf);
    for (int i = 1; i <= 13; i++) {
        int8_t prod = ds.Cargo_Items[i - 1];
        if (prod > 0) {
            if (home)
                ds.Cargo_ItemStore[i - 1] = ds_far((uint16_t)(offsetof(DSeg, Research_Products)
                                                  + (prod - 1) * sizeof(TProduct)
                                                  + offsetof(TProduct, Stock)));
            else
                ds.Cargo_ItemStore[i - 1] = far_ptr((uint16_t)(pfar >> 16),
                                        (uint16_t)((uint16_t)pfar + offsetof(TGroup, Items) + (i - 1) * 2));
        }
    }
    if (home) {
        ds.Cargo_Stores = ds_far(offsetof(DSeg, Game_HomeStock));
    } else {
        ds.Cargo_Stores = far_ptr((uint16_t)(ds.Game_CurBody >> 16),
                         (uint16_t)((uint16_t)ds.Game_CurBody + offsetof(TBody, Stores)));
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
                             (const uint8_t *)PSTR("Transfer to planet"), 0, 0);
        for (int16_t i = 1; i <= n; i++)
            Mouse_SetHotspot((int16_t)(i + 0x13), 0xEB, (uint8_t)(i * 8 + 0x35), 0x10, 7,
                             (const uint8_t *)PSTR("Transfer to ship"), 0, 0);
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
        const uint8_t *name = (const uint8_t *)&ds.Names_Ores[i - 1];
        if (i == row)
            Gfx_DrawTextFC(name, 0x70, (int16_t)(i * 8 + 0x35), 0x10);
        else
            Gfx_DrawTextF9(name, 0x70, (int16_t)(i * 8 + 0x35), 0x10);
    }
    int16_t n = (int16_t)ds.Cargo_ItemCount;
    for (int16_t i = 1; i <= n; i++) {
        int8_t prod = ds.Cargo_Items[ds.Cargo_ItemList[i - 1] - 1];
        const uint8_t *name = (const uint8_t *)&ds.Research_Products[prod - 1].Name;
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
    TBody *body = far_host(ds.Game_CurBody);
    PString s, a, b;

    if (at_own_colony(g)) {
        for (int16_t i = 1; i <= 6; i++) {
            System_StrLong(*store_at(i), 6, &s, 255);
            Gfx_DrawTextF9((const uint8_t *)&s, 0xC, (int16_t)(i * 8 + 0x35), 6);
        }
    }
    if (g->Status == 1) {
        if (body->Owner == 1 && body->Colony != 0
            && Surface_CountBuildings(0xC, g->System, g->Planet, g->Moon, false) > 0
            && (int16_t)ds.Cargo_ItemCount > 0) {
            int16_t n = (int16_t)ds.Cargo_ItemCount;
            for (int16_t i = 1; i <= n; i++) {
                System_StrLong(*item_store((int16_t)ds.Cargo_ItemList[i - 1]), 6, &s, 255);
                Gfx_DrawTextF9((const uint8_t *)&s, 0xC, (int16_t)(i * 8 + 0x65), 6);
            }
        }
    }
    for (int16_t i = 1; i <= 6; i++) {
        System_StrLong(*(int32_t *)&g->Cargo[i - 1], 6, &s, 255);
        Gfx_DrawTextF9((const uint8_t *)&s, 0x10E, (int16_t)(i * 8 + 0x35), 6);
    }
    int16_t n = (int16_t)ds.Cargo_ItemCount;
    for (int16_t i = 1; i <= n; i++) {
        System_StrLong(g->Items[ds.Cargo_ItemList[i - 1] - 1], 6, &s, 255);
        Gfx_DrawTextF9((const uint8_t *)&s, 0x10E, (int16_t)(i * 8 + 0x65), 6);
    }
    System_StrLong(ds.Cargo_ShipUsed, 6, &a, 255);
    System_StrLong(ds.Cargo_ShipCap, 0, &b, 255);
    PString t;
    System_StrLoad(&t, PSTR("Space: "));
    System_StrConcat(&t, &a);
    System_StrConcat(&t, PSTR("/"));
    System_StrConcat(&t, &b);
    Gfx_DrawTextF9((const uint8_t *)&t, 5, 0xC1, 0x19);
    System_StrLong(ds.Cargo_PlanetCap, 0, &s, 255);
    System_StrLoad(&t, PSTR("Storage space on planet: "));
    System_StrConcat(&t, &s);
    Gfx_DrawTextF9((const uint8_t *)&t, 0x80, 0xC1, 0x1F);
}
