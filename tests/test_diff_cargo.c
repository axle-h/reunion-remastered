// Differential test of unit Cargo (2583): the cargo transfer screen between a
// planet's stores and a fleet, original (under Unicorn) against the port. A
// new game has no fleet, so each case puts one (fleet 1..3 of list 0) at New
// Earth or another body of system 1, fills its holds and the planet's stores
// at random, and (for all but InitCargoScreen itself) sets up the screen's
// globals with the port's InitCargoScreen first. See docs/difftest.md.
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/cargo.h"
#include "game/dseg.h"
#include "game/groups.h"

DIFF_PROC(Cargo_CargoScreen, Cargo_CargoScreen());
DIFF_PROC(Cargo_InitCargoScreen, Cargo_InitCargoScreen());
DIFF_PROC(Cargo_SetupCargoHotspots, Cargo_SetupCargoHotspots());
DIFF_PROC(Cargo_FlashCargoRow, Cargo_FlashCargoRow((int16_t)a[0]));
DIFF_PROC(Cargo_DrawCargoNames, Cargo_DrawCargoNames((int16_t)a[0]));
DIFF_PROC(Cargo_DrawCargoAmounts, Cargo_DrawCargoAmounts());

#define SHIP_USED (*(uint32_t *)ds_at(0x786A))

static void setup_fleet(DiffRng *r, int32_t *a)
{
    // Products that can be carried: some ready for production.
    for (int i = 0; i < 13; i++) {
        int prod = ds.Cargo_Items[i];
        if (prod > 0 && diff_chance(r, 60))
            ds.Research_Products[prod - 1].State = (int16_t)diff_pick(r, 1, 5, 5, 5);
        if (prod > 0 && diff_chance(r, 50))
            ds.Research_Products[prod - 1].Stock = (int16_t)diff_pick(r, 0, 0, 1, 5, diff_range(r, 0, 300));
    }
    int n = diff_range(r, 1, 3);
    if (ds.Game_GroupCount < n)
        ds.Game_GroupCount = (int16_t)n;
    ds.Game_GroupList = 0;
    ds.Game_SelGroup = (int16_t)diff_range(r, 1, n);
    TGroup *g = Groups_At(0, ds.Game_SelGroup);
    g->Kind = (uint8_t)diff_pick(r, 1, 2, 3, 4);
    g->Name.len = 5;
    memcpy(g->Name.s, "Fleet", 5);
    g->System = 1;
    g->Planet = (uint8_t)diff_pick(r, 5, 5, 5, 3, 4, 6);
    g->Moon = (uint8_t)(g->Planet == 5 ? diff_pick(r, 0, 0, 0, 1) : 0);
    g->Status = (uint8_t)diff_pick(r, 1, 1, 1, 1, 2, 4);
    for (int u = 0; u < 4; u++)
        g->Units[0][u].ShipType = (int16_t)diff_pick(r, 0, 0, 1, 2, 5, diff_range(r, 0, 20));
    for (int c = 0; c < 6; c++) {
        int32_t v = diff_pick(r, 0, 0, 0, 1, 50, 100, 101, 250, diff_range(r, 0, 20000));
        memcpy(&g->Cargo[c], &v, 4);
    }
    for (int i = 0; i < 13; i++)
        g->Items[i] = (int16_t)diff_pick(r, 0, 0, 1, 3, diff_range(r, 0, 30));
    // The planet's ores (New Earth: Game_HomeStock) and the planet force's storage.
    for (int j = 0; j < 6; j++)
        ds.Game_HomeStock[j] = diff_pick(r, 0, 0, 1, 100, 101, 5000, diff_range(r, 0, 300000));
    for (int pf = 1; pf <= 32; pf++) {
        TGroup *f = Groups_At(1, (int16_t)pf);
        if (f->System == 1 && diff_chance(r, 50))
            f->Storage = (int16_t)diff_pick(r, 0, 1, 5, 50, 300, diff_range(r, 0, 1000));
    }
    // Whose colony the bodies of system 1 are.
    TBody *b = far_host(ds.Galaxy_Bodies[0]);
    for (int i = 0; i < ds.Galaxy_PlanetCount[0] + ds.Galaxy_MoonCount[0]; i++) {
        if (!diff_chance(r, 50))
            continue;
        b[i].Owner = (uint8_t)diff_pick(r, 0, 1, 1, 1, 2);
        b[i].Colony = (uint8_t)diff_pick(r, 0, 0, 1, 1, 1);
        b[i]._0A = (uint8_t)diff_pick(r, 0, 0, 1);
        for (int j = 0; j < 6; j++)
            b[i].Stores[j] = diff_pick(r, 0, 1, 100, 101, diff_range(r, 0, 300000));
    }
    a[0] = diff_pick(r, 0, 1, 3, 6, 7, 8, diff_range(r, 0, 19));
}

// The screen as InitCargoScreen leaves it, with the holds perturbed once more.
static void setup_screen(DiffRng *r, int32_t *a)
{
    setup_fleet(r, a);
    Cargo_InitCargoScreen();
    if (diff_chance(r, 40))
        SHIP_USED = (uint32_t)diff_pick(r, 0, 0, 1, 99999, diff_range(r, 0, 50000));
    ds.Game_ScreenEntered = (uint8_t)diff_chance(r, 10);
    ds.Game_MenuCommand = (int16_t)diff_pick(r, 0, 0, 0, 0x10);
    // 15h..26h ship -> planet, 28h..3Ah planet -> ship.
    ds.Game_HotspotHit = (int16_t)diff_pick(r, 0, 0x14, 0x27, 0x3B, diff_range(r, 0x15, 0x1A),
                                            diff_range(r, 0x1B, 0x26), diff_range(r, 0x28, 0x2D),
                                            diff_range(r, 0x2E, 0x3A), diff_range(r, 0x15, 0x1A),
                                            diff_range(r, 0x28, 0x2D));
    ds.Mouse_HeldLeft = (uint8_t)diff_range(r, 0, 1);
}

// Moving items (rows 7..) between New Earth's stock and a docked fleet.
static void setup_items(DiffRng *r, int32_t *a)
{
    setup_fleet(r, a);
    TGroup *g = Groups_Selected();
    g->Planet = 5;
    g->Moon = 0;
    g->Status = 1;
    for (int i = 0; i < 13; i++) {
        int prod = ds.Cargo_Items[i];
        if (prod > 0 && diff_chance(r, 80)) {
            ds.Research_Products[prod - 1].State = 5;
            ds.Research_Products[prod - 1].Stock = (int16_t)diff_pick(r, 0, 1, 2, 40);
        }
    }
    TBody *b = far_host(ds.Galaxy_Bodies[0]);
    b[4].Owner = 1;
    b[4].Colony = 1;
    Cargo_InitCargoScreen();
    if (diff_chance(r, 50))
        SHIP_USED = (uint32_t)diff_pick(r, 0, 0, 1000, 99999);
    int rows = *(uint16_t *)ds_at(0x7816);
    int row = rows > 0 ? diff_range(r, 1, rows) : 1;
    ds.Game_ScreenEntered = 0;
    ds.Game_MenuCommand = 0;
    ds.Game_HotspotHit = (int16_t)(diff_pick(r, 0x14, 0x27, 0x27) + 6 + row);
    ds.Mouse_HeldLeft = (uint8_t)diff_range(r, 0, 1);
}

static const struct {
    const char *name;
    const char *script;
} states[] = {
    {"command centre", "1500:quit"},
    {"ship info (16)", "900:click:120,16;1300:quit"},
};
enum { STATES = sizeof states / sizeof states[0] };

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    DiffState *bases[STATES];
    for (int i = 0; i < STATES; i++) {
        if (!diff_boot(states[i].script)) {
            printf("FAILED to reach %s\n", states[i].name);
            return 1;
        }
        bases[i] = diff_save();
    }

    const struct {
        DiffFn *fn;
        DiffSetup setup;
        int cases;
    } fns[] = {
        {&diff_Cargo_InitCargoScreen, setup_fleet, 40},
        {&diff_Cargo_SetupCargoHotspots, setup_screen, 20},
        {&diff_Cargo_DrawCargoNames, setup_screen, 20},
        {&diff_Cargo_FlashCargoRow, setup_screen, 12},
        {&diff_Cargo_DrawCargoAmounts, setup_screen, 40},
        {&diff_Cargo_CargoScreen, setup_screen, 200},
        {&diff_Cargo_CargoScreen, setup_items, 40},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, bases, STATES, fns[i].setup, diff_cases(fns[i].cases));
    for (int i = 0; i < STATES; i++)
        diff_free(bases[i]);
    return diff_finish();
}
