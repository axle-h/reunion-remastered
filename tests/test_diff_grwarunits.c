// Differential test of unit GrWarUnits (1430): the unit assignment screen
// before a ground battle, original (under Unicorn) against the port. The base
// states (forces at a target body, the screen set up) are built with the
// port's routines, see diff_grwar.h and docs/difftest.md.
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "diff_grwar.h"
#include "game/dseg.h"
#include "game/grwarunits.h"

DIFF_PROC(GrWarUnits_UnitAssignScreen, GrWarUnits_UnitAssignScreen());
DIFF_PROC(GrWarUnits_UnitAssignDrawScreen, GrWarUnits_UnitAssignDrawScreen());
DIFF_PROC(GrWarUnits_CollectAvailableForces, GrWarUnits_CollectAvailableForces());
DIFF_PROC(GrWarUnits_SetupUnitHotspots, GrWarUnits_SetupUnitHotspots());
DIFF_PROC(GrWarUnits_DrawUnitList, GrWarUnits_DrawUnitList());

// Longint tables of the attackers (side 1 of the GrWar tables): totals at
// 7656, pool at 7676, attack sums at 7686.
static void set_total(int t, int32_t v) { gw_set_tbl(0, 1, t, v); }
static void set_pool(int t, int32_t v) { gw_set_tbl(0x30, 0, t, v); }
static void set_attack(int t, int32_t v) { gw_set_tbl(0x30, 1, t, v); }

// --- CollectAvailableForces ----------------------------------------------------

static void setup_collect(DiffRng *r, int32_t *a)
{
    (void)a;
    gw_perturb_owners(r);
    // A planet force (list 1) at the site now and then.
    if (diff_chance(r, 30) && ds.Game_PlanetForceCount > 0) {
        TGroup *g = Groups_At(1, (int16_t)diff_range(r, 1, ds.Game_PlanetForceCount));
        g->System = GW_SYSTEM;
        g->Planet = GW_PLANET;
        g->Moon = 0;
        g->Status = (uint8_t)diff_pick(r, 1, 2, 7);
        g->Kind = (uint8_t)diff_pick(r, 1, 5);
        for (int t = 0; t < 4; t++)
            g->Units[1][t].ShipType = (int16_t)diff_pick(r, 0, 3, 40, 900);
    }
    // More races with forces at the site.
    for (int k = 0; k < 11; k++) {
        TRace *rc = &ds.Races_Races[k];
        if (!diff_chance(r, 10))
            continue;
        rc->Status = (int8_t)diff_pick(r, 2, 6);
        rc->ForceCount = (int8_t)diff_range(r, 1, 3);
        for (int f = 0; f < rc->ForceCount; f++) {
            TRaceForce *fc = &rc->Forces[f];
            fc->System = GW_SYSTEM;
            fc->Body = (uint8_t)diff_pick(r, GW_PLANET, GW_PLANET, 1);
            fc->Moon = 0;
            fc->Active = (uint8_t)diff_pick(r, 1, 1, 0);
            for (int t = 4; t < 8; t++)
                fc->Units[t] = (int16_t)diff_pick(r, 0, 1, 25, 400);
        }
    }
    if (diff_chance(r, 15))  // the slot lists overflow
        for (int t = 0; t < 4; t++)
            Groups_At(0, 1)->Units[1][t].ShipType = (int16_t)diff_pick(r, 3000, 20000, 32767);
}

// --- The screen ------------------------------------------------------------------

static void perturb_tables(DiffRng *r)
{
    for (int t = 1; t <= 4; t++) {
        if (diff_chance(r, 30))
            set_total(t, diff_pick(r, 0, 0, 1, 40, 900, -1));
        if (diff_chance(r, 30))
            set_pool(t, diff_pick(r, 0, 0, 1, 7, 300, 70000));
        if (diff_chance(r, 30))
            set_attack(t, diff_pick(r, 0, 1, 500, 100000, 3000000));
    }
}

static void perturb_slots(DiffRng *r)
{
    if (diff_chance(r, 50))
        ds.GrWarUnits_AttackerSlotCount = (int16_t)diff_pick(r, 0, 1, diff_range(r, 1, 20), 19, 20);
    for (int i = 1; i <= ds.GrWarUnits_AttackerSlotCount; i++) {
        GrBattleUnit *u = gw_unit(1, i);
        if (u->type < 1 || u->type > 4 || diff_chance(r, 30))
            u->type = (uint8_t)diff_range(r, 1, 4);
        if (diff_chance(r, 40))
            u->count = (int16_t)diff_pick(r, 0, 1, diff_range(r, 1, gw_cap(u->type)), gw_cap(u->type));
    }
}

static void setup_draw_screen(DiffRng *r, int32_t *a)
{
    (void)a;
    perturb_tables(r);
    *gw_b(0x778B) = (uint8_t)diff_range(r, 0, 1);
}

static void setup_draw_list(DiffRng *r, int32_t *a)
{
    (void)a;
    perturb_slots(r);
    perturb_tables(r);
}

static void setup_screen(DiffRng *r, int32_t *a)
{
    (void)a;
    perturb_slots(r);
    perturb_tables(r);
    // Not on entry: that starts the music (the timer handler, AdLib ports),
    // which the harness does not emulate; the rest of the entry (Collect-
    // AvailableForces, UnitAssignDrawScreen, SetupUnitHotspots) is tested alone.
    ds.Game_ScreenEntered = 0;
    ds.Game_MenuCommand = (int16_t)diff_pick(r, 0, 0, 0xB, 0x2A, 0xC, 0x13, 0x2C, 0x24, 0xC, 0x24, 5);
    int n = ds.GrWarUnits_AttackerSlotCount;
    ds.Game_HotspotHit = (int16_t)diff_pick(r, 0, 0, 0x14 + diff_range(r, 1, 20), 0x28 + diff_range(r, 1, 20),
                                            0x14 + gw_clamp(n, 1, 20), 0x28 + gw_clamp(n, 1, 20), 0x14, 0x29,
                                            0x3D, 0x15, 0x3C);
    ds.Mouse_HeldLeft = (uint8_t)diff_pick(r, 0, 1, 1);
    // "Add/sub" on a slot with units left in the pool and room in the slot.
    int hit = ds.Game_HotspotHit - 0x28;
    if (hit >= 1 && hit <= n && diff_chance(r, 60)) {
        GrBattleUnit *u = gw_unit(1, hit);
        set_pool(u->type, diff_pick(r, 1, 2, 50));
        u->count = (int16_t)diff_pick(r, 0, 1, gw_cap(u->type) - 1, gw_cap(u->type));
    }
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
    GwStates st;
    if (!gw_build_states(&st))
        return 1;

    const struct {
        DiffFn *fn;
        DiffState **bases;
        DiffSetup setup;
        int cases;
    } fns[] = {
        {&diff_GrWarUnits_CollectAvailableForces, st.pre, setup_collect, 60},
        {&diff_GrWarUnits_UnitAssignDrawScreen, st.assign, setup_draw_screen, 12},
        {&diff_GrWarUnits_SetupUnitHotspots, st.assign, setup_none, 3},
        {&diff_GrWarUnits_DrawUnitList, st.assign, setup_draw_list, 40},
        {&diff_GrWarUnits_UnitAssignScreen, st.assign, setup_screen, 80},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, fns[i].bases, GW_VARIANTS, fns[i].setup, diff_cases(fns[i].cases));
    gw_free_states(&st);
    return diff_finish();
}
