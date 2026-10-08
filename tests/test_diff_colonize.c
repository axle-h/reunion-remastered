// Differential test of unit Colonize (2841): the colony set-up screen (28),
// original (under Unicorn) against the port. Cases enter the screen at a
// planet (builder hired or not), or run a pass of the entered screen (bases
// made by the port entering it at several bodies) clicking the six starter
// buildings, Colonise or Cancel, with credits, the running total, the
// builder's experience and the inventions perturbed. See docs/difftest.md.
// Needs the game data.
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/colonize.h"
#include "game/dseg.h"
#include "game/groups.h"
#include "game/planetinfo.h"

DIFF_PROC(Colonize_ColonizeScreen, Colonize_ColonizeScreen());
DIFF_PROC(Colonize_SetupColonizeHotspots, Colonize_SetupColonizeHotspots());
DIFF_PROC(Colonize_DrawColonizeScreen, Colonize_DrawColonizeScreen());
DIFF_PROC(Colonize_CheckKey1, Colonize_CheckKey1());

#define TOTAL_COST (*(int32_t *)ds_at(0x788A))

static void seed(DiffRng *r)
{
    ds.System_RandSeed = (int32_t)diff_rand(r, 0xFFFFFFFFu);
}

typedef struct Loc {
    int16_t sys, planet, moon;
} Loc;

static const TMoonList *moon_list(int sys, int planet)
{
    return (const TMoonList *)far_host(ds.Galaxy_MoonLists[sys - 1]) + (planet - 1);
}

static TBody *body_at(Loc l)
{
    int idx = l.moon == 0 ? l.planet : moon_list(l.sys, l.planet)->Moons[l.moon - 1];
    return (TBody *)far_host(ds.Galaxy_Bodies[l.sys - 1]) + (idx - 1);
}

static Loc fix_loc(Loc l)
{
    if (l.moon > moon_list(l.sys, l.planet)->Count)
        l.moon = 0;
    return l;
}

static void select_loc(Loc l)
{
    ds.Game_CurSystem = l.sys;
    ds.Game_CurPlanet[l.sys - 1] = l.planet;
    ds.Game_CurMoon[l.sys - 1] = l.moon;
    PlanetInfo_SelectCurBody();
}

// What decides whether a starter building can be picked.
static void perturb_rules(DiffRng *r)
{
    ds.Game_StaffExp[1] = (uint16_t)diff_pick(r, 0, 1, 5, 10, 20, 40, 70);
    for (int k = 0; k < 6; k++) {
        const TBuildingType *bt = &ds.Surface_BuildingTypes[ds.Colony_StarterBuildings[k].Building - 1];
        if (bt->NeedProduct != 0 && (int8_t)bt->NeedProduct > 0)
            ds.Research_Products[bt->NeedProduct - 1].State = (int16_t)diff_pick(r, 0, 3, 5, 5);
    }
    if (diff_chance(r, 25)) { // the builder experience a type needs
        TBuildingType *bt = &ds.Surface_BuildingTypes[ds.Colony_StarterBuildings[diff_rand(r, 6)].Building - 1];
        bt->_10 = (uint8_t)diff_pick(r, 0xFF, 0, 15, 50, 0x90);
    }
    ds.Game_Credits = diff_pick(r, 0, 99999, 100000, 120000, 150000, 250000, 1000000, diff_range(r, 0, 400000));
}

// --- Entering the screen ------------------------------------------------------------------

static void setup_enter(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    Loc l = {(int16_t)diff_pick(r, 1, 1, 2, 3, 4), 0, 0};
    l.planet = (int16_t)diff_range(r, 1, ds.Galaxy_PlanetCount[l.sys - 1]);
    l.moon = (int16_t)diff_pick(r, 0, 0, 1);
    l = fix_loc(l);
    if (diff_chance(r, 30))
        body_at(l)->SurfaceType = (uint8_t)diff_range(r, 1, 10);
    select_loc(l);
    perturb_rules(r);
    ds.Game_StaffHired[1] = (uint16_t)diff_pick(r, 0, 1, 2, 2, 3, 3);
    ds.Game_ColonySetupCount = (int16_t)diff_pick(r, 0, 1, 3, 9);
    ds.Game_CurrentScreen = 0x1C;
    ds.Game_PrevScreen = (int16_t)diff_pick(r, 8, 8, 0x11, 0x14);
    ds.Game_ScreenEntered = 1;
    ds.Game_ScreenSwitch = 0;
    ds.Game_HotspotHit = 0;
    ds.Game_MenuCommand = 0;
    ds.Mouse_HotspotCount = 0;
}

// --- A pass of the entered screen -------------------------------------------------------------

static void setup_pass(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    perturb_rules(r);
    TColonySetup *cs = &ds.Game_ColonySetups[ds.Game_ColonySetupCount - 1];
    int32_t total = 100000;
    for (int k = 0; k < 6; k++) {
        cs->Timers[k] = (int16_t)diff_pick(r, 0, 0, 1);
        if (cs->Timers[k] != 0)
            total += ds.Surface_BuildingTypes[ds.Colony_StarterBuildings[k].Building - 1].Cost;
    }
    TOTAL_COST = diff_chance(r, 80) ? total : diff_pick(r, 100000, 2000000);
    ds.Game_MenuCommand = (int16_t)(diff_chance(r, 70) ? 0 : diff_pick(r, 0x3B, 0x3B, 0x3B, 0x3A));
    if (ds.Game_MenuCommand != 0)
        ds.Game_HotspotHit = (int16_t)diff_range(r, 1, 12);
    else
        ds.Game_HotspotHit = (int16_t)diff_pick(r, 0, 0x14, 0x17, 0x1E, diff_range(r, 0x18, 0x1D),
                                                diff_range(r, 0x18, 0x1D), diff_range(r, 0x18, 0x1D));
    ds.Game_PlanetForceCount = (int16_t)diff_pick(r, ds.Game_PlanetForceCount, 0, 1, 4);
}

static void setup_draw(DiffRng *r, int32_t *a)
{
    (void)a;
    perturb_rules(r);
    if (diff_chance(r, 40)) {
        TBody *b = far_host(ds.Game_CurBody);
        b->SurfaceType = (uint8_t)diff_range(r, 1, 10);
        ds.Surface_TileSet = *(int16_t *)ds_at((uint16_t)(0x1A38 + (int8_t)b->SurfaceType * 2));
    }
}

static void setup_none(DiffRng *r, int32_t *a)
{
    (void)a;
    ds.Mouse_HotspotCount = (int16_t)diff_range(r, 0, 9);
}

static void setup_key(DiffRng *r, int32_t *a)
{
    (void)a;
    ds.Protect_Key1 = diff_pick(r, 0, 0x90, 0x91, 0x10091, 0x92, -0x28D);
}

static void enter_at(Loc l)
{
    l = fix_loc(l);
    select_loc(l);
    ds.Game_StaffHired[1] = 3;
    ds.Game_PrevScreen = 8;
    ds.Game_CurrentScreen = 0x1C;
    ds.Game_ScreenEntered = 1;
    ds.Game_ScreenSwitch = 0;
    ds.Game_HotspotHit = 0;
    ds.Game_MenuCommand = 0;
    ds.Mouse_HotspotCount = 0;
    Colonize_ColonizeScreen();
    ds.Game_PrevScreen = 0x1C;
    ds.Game_ScreenEntered = 0;
    diff_quiesce();
}

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    static const char *const boots[] = {"1500:quit", "900:click:168,16;1300:quit"};
    enum { BOOTS = sizeof boots / sizeof boots[0] };
    DiffState *enter[BOOTS];
    for (int i = 0; i < BOOTS; i++) {
        if (!diff_boot(boots[i])) {
            printf("FAILED to boot %s\n", boots[i]);
            return 1;
        }
        enter[i] = diff_save();
    }
    static const Loc at[] = {{1, 3, 0}, {1, 6, 0}, {2, 2, 1}, {3, 1, 0}};
    enum { PASSES = sizeof at / sizeof at[0] };
    DiffState *pass[PASSES];
    for (int i = 0; i < PASSES; i++) {
        diff_load(enter[1]);
        enter_at(at[i]);
        pass[i] = diff_save();
    }

    diff_run(&diff_Colonize_ColonizeScreen, enter, BOOTS, setup_enter, diff_cases(40));
    diff_run(&diff_Colonize_ColonizeScreen, pass, PASSES, setup_pass, diff_cases(150));
    diff_run(&diff_Colonize_DrawColonizeScreen, pass, PASSES, setup_draw, diff_cases(16));
    diff_run(&diff_Colonize_SetupColonizeHotspots, pass, PASSES, setup_none, diff_cases(4));
    diff_run(&diff_Colonize_CheckKey1, pass, PASSES, setup_key, diff_cases(12));
    for (int i = 0; i < BOOTS; i++)
        diff_free(enter[i]);
    for (int i = 0; i < PASSES; i++)
        diff_free(pass[i]);
    return diff_finish();
}
