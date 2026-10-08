// Differential test of unit Satellite (28cd): deploying satellites, colonising
// and acquiring bodies, the group toolbars and the carrier / trade-ship
// counters, original (under Unicorn) against the port, from new-game states
// with carrier and trade groups placed at random. See docs/difftest.md.
// Needs the game data. CheckStoryLocations (nested) is covered through the
// Deploy* routines.
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/gfxpath.h"
#include "game/groups.h"
#include "game/planetinfo.h"
#include "game/satellite.h"

DIFF_PROC(Satellite_DeployStockSatellite, Satellite_DeployStockSatellite());
DIFF_PROC(Satellite_DeploySatellite, Satellite_DeploySatellite());
DIFF_PROC(Satellite_DeploySpySat, Satellite_DeploySpySat());
DIFF_PROC(Satellite_DeploySpyShip, Satellite_DeploySpyShip());
DIFF_PROC(Satellite_DeploySolarSat, Satellite_DeploySolarSat());
DIFF_PROC(Satellite_ColonizeBody, Satellite_ColonizeBody());
DIFF_PROC(Satellite_AcquireBody, Satellite_AcquireBody());
DIFF_PROC(Satellite_SetupGroupScreenMenu, Satellite_SetupGroupScreenMenu());
DIFF_PROC(Satellite_UpdateGroupsMenu, Satellite_UpdateGroupsMenu());
DIFF_PROC(Satellite_UpdateShipInfoMenu, Satellite_UpdateShipInfoMenu());
DIFF_PROC(Satellite_AddBodyActionItems,
          Satellite_AddBodyActionItems((int16_t)a[0], (int16_t)a[1], (int16_t)a[2], far_host((FarPtr)a[3])));
DIFF_PROC(Satellite_HandleBodyActions, Satellite_HandleBodyActions());
DIFF_FUNC(Satellite_CountSatellites,
          Satellite_CountSatellites((int16_t)a[0], (int16_t)a[1], (int16_t)a[2], (int16_t)a[3]));
DIFF_PROC(Satellite_DeductSatellites,
          Satellite_DeductSatellites((int16_t)a[0], (int16_t)a[1], (int16_t)a[2], (int16_t)a[3], a[4]));
DIFF_FUNC(Satellite_CountTradeShips, Satellite_CountTradeShips((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_PROC(Satellite_TakeTradeShips, Satellite_TakeTradeShips((int16_t)a[0], (int16_t)a[1], (int16_t)a[2], a[3]));
DIFF_PROC(Satellite_Init, Satellite_Init());

static uint8_t *b8(uint16_t off) { return (uint8_t *)ds_at(off); }

static void seed(DiffRng *r)
{
    ds.System_RandSeed = (int32_t)diff_rand(r, 0xFFFFFFFFu);
}

// --- Locations and bodies ------------------------------------------------------------

typedef struct Loc {
    int16_t sys, planet, moon; // moon 0 = the planet itself
} Loc;

static const TMoonList *moon_list(int sys, int planet)
{
    return (const TMoonList *)far_host(ds.Galaxy_MoonLists[sys - 1]) + (planet - 1);
}

// A valid body, with the story locations (3:2:1, 7:1:0) and New Earth (1:5)
// picked often.
static Loc random_loc(DiffRng *r)
{
    Loc l;
    switch (diff_rand(r, 6)) {
    case 0:
        l = (Loc){1, 5, 0};
        break;
    case 1:
        l = (Loc){3, 2, 1};
        break;
    case 2:
        l = (Loc){7, 1, 0};
        break;
    case 3:
        l = (Loc){1, 7, 0};
        break;
    default:
        l.sys = (int16_t)diff_range(r, 1, 8);
        l.planet = (int16_t)diff_range(r, 1, ds.Galaxy_PlanetCount[l.sys - 1]);
        l.moon = 0;
        break;
    }
    int nm = moon_list(l.sys, l.planet)->Count;
    if (l.moon > nm || (nm > 0 && diff_chance(r, 25)))
        l.moon = nm > 0 ? (int16_t)diff_range(r, 1, nm) : 0;
    return l;
}

static TBody *body_at(Loc l)
{
    int idx = l.moon == 0 ? l.planet : moon_list(l.sys, l.planet)->Moons[l.moon - 1];
    return (TBody *)far_host(ds.Galaxy_Bodies[l.sys - 1]) + (idx - 1);
}

static void select_loc(Loc l)
{
    ds.Game_CurSystem = l.sys;
    ds.Game_CurPlanet[l.sys - 1] = l.planet;
    ds.Game_CurMoon[l.sys - 1] = l.moon;
    PlanetInfo_SelectCurBody();
}

// The fields the body actions test, at boundary values.
static void perturb_body(DiffRng *r, TBody *b)
{
    b->Owner = (uint8_t)diff_pick(r, 0, 0, 1, 1, 2, 5, b->Owner);
    b->Colony = (uint8_t)diff_pick(r, 0, 0, 1);
    b->_0A = (uint8_t)diff_pick(r, 0, 0, 0, 1);
    b->Satellite = (uint8_t)diff_pick(r, 0, 0, 0, 1, 2, 0xE2);
    b->_09 = (uint8_t)diff_pick(r, 0, 0, 1);
    b->_0B = (uint8_t)diff_pick(r, 0, 1, 4, 5, 6);
    b->InfoLevel = (int8_t)diff_pick(r, 0, 5, 9, 10, 29, 30, 45, 50, 55, 60, b->InfoLevel);
    b->Miner = (int8_t)diff_pick(r, 0, 0, 0, 1, 2);
    b->Surveyed = (uint8_t)diff_pick(r, 0, 1, 1);
    b->_03 = (uint8_t)diff_pick(r, 0, 1, 1);
    if (diff_chance(r, 30))
        b->SurfaceType = (uint8_t)diff_range(r, 1, 10);
}

static void perturb_story(DiffRng *r)
{
    ds.Research_Products[2].Stock = (int16_t)diff_pick(r, 0, 1, 1, 5, -1);
    ds.Research_Products[6].State = (int16_t)diff_pick(r, 0, 0, 3, 5, 5);
    ds.Research_Products[0xB].State = (int16_t)diff_pick(r, 0, 0, 5);
    ds.Research_Products[0x15].State = (int16_t)diff_pick(r, 0, 0, 5);
    ds.Research_Products[0x1E].State = (int16_t)diff_pick(r, 0, 0, 5);
    ds.Game_IdeaTimer[6] = (int16_t)diff_pick(r, -1, -1, 0, 20);
    *b8(0x5D4C) = (uint8_t)diff_pick(r, 0, 0, 1); // stock satellites only at 1:5
    ds.Game_ColonySetupCount = (int16_t)diff_pick(r, 0, 2, 9, 10);
    // Amnesty's body 7 (1:7) +0Ch, tested by AcquireBody.
    ((uint8_t *)far_host(ds.Galaxy_Bodies[0]))[0x186] = (uint8_t)diff_pick(r, 0, 2, 5);
}

// --- Groups ---------------------------------------------------------------------------

// n groups of list `list`, most at l (or at its planet), mixed kinds and statuses.
static void random_groups(DiffRng *r, int16_t list, int n, Loc l)
{
    for (int i = 1; i <= n; i++) {
        TGroup *g = Groups_At(list, (int16_t)i);
        g->Kind = (uint8_t)diff_pick(r, 1, 2, 2, 3, 4, 4, 4, 5);
        g->Name.len = 5;
        memcpy(g->Name.s, "Group", 5);
        if (diff_chance(r, 75)) {
            g->System = (uint8_t)l.sys;
            g->Planet = (uint8_t)l.planet;
            g->Moon = (uint8_t)diff_pick(r, l.moon, l.moon, 0, 1);
        } else {
            Loc o = random_loc(r);
            g->System = (uint8_t)o.sys;
            g->Planet = (uint8_t)o.planet;
            g->Moon = (uint8_t)o.moon;
        }
        g->Status = (uint8_t)diff_pick(r, 1, 1, 2, 2, 4, 7, 0);
        bool cargo = diff_chance(r, 50); // else no ships and no cargo
        for (int s = 0; s < 4; s++) {
            g->Units[0][s].ShipType = (int16_t)(cargo ? diff_pick(r, 0, 1, 3) : 0);
            g->Units[1][s].ShipType = (int16_t)(cargo ? diff_pick(r, 0, 1, 3) : 0);
            for (int w = 0; w < 4; w++)
                g->Units[0][s].Weapons[w] = (int16_t)diff_pick(r, 0, 0, 1, 1, 2, 3, 7, 300, -2, 32767);
        }
        for (int k = 0; k < 6; k++) {
            g->Cargo[k].A = (int16_t)(cargo ? diff_pick(r, 0, 0, 0, 1) : 0);
            g->Cargo[k].B = (int16_t)(cargo ? diff_pick(r, 0, 0, 0, 5) : 0);
        }
        for (int k = 0; k < 13; k++)
            g->Items[k] = (int16_t)(cargo ? diff_pick(r, 0, 0, 0, 2) : 0);
    }
}

static void random_fleets(DiffRng *r, Loc l)
{
    int n = diff_pick(r, 0, 1, 2, 3, 5, 8);
    ds.Game_GroupCount = (int16_t)n;
    random_groups(r, 0, n, l);
    int m = diff_pick(r, 0, 1, 2);
    ds.Game_PlanetForceCount = (int16_t)m;
    random_groups(r, 1, m, l);
    ds.Game_SelGroup0 = (int16_t)(n > 0 ? diff_range(r, 1, n) : diff_pick(r, 1, 0));
    ds.Game_CurrentScreen = (int16_t)diff_pick(r, 0x11, 0x11, 7, 8, 0x14);
}

// --- Counters ---------------------------------------------------------------------------

// Often the group selected in screen 0x11 is a carrier / trade group there.
static void select_at(DiffRng *r, Loc l, uint8_t kind)
{
    if (ds.Game_GroupCount == 0 || !diff_chance(r, 50))
        return;
    TGroup *g = Groups_At(0, ds.Game_SelGroup0);
    g->Kind = kind;
    g->System = (uint8_t)l.sys;
    g->Planet = (uint8_t)l.planet;
    g->Status = (uint8_t)diff_pick(r, 1, 2);
    ds.Game_CurrentScreen = 0x11;
}

static void setup_count(DiffRng *r, int32_t *a)
{
    Loc l = random_loc(r);
    random_fleets(r, l);
    select_at(r, l, 4);
    a[0] = l.sys;
    a[1] = l.planet;
    a[2] = l.moon;
    a[3] = diff_range(r, 1, 4);
    a[4] = diff_pick(r, 0, 1, 1, 1, 2, 5, 400, 70000);
}

static void setup_trade(DiffRng *r, int32_t *a)
{
    Loc l = random_loc(r);
    random_fleets(r, l);
    select_at(r, l, 2);
    a[0] = l.sys;
    a[1] = l.planet;
    a[2] = l.moon;
    a[3] = diff_pick(r, 0, 1, 1, 1, 2, 5, 400, 70000);
}

// --- Deployments, colonising -------------------------------------------------------------

static void setup_deploy(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    Loc l = random_loc(r);
    random_fleets(r, l);
    perturb_story(r);
    perturb_body(r, body_at(l));
    select_loc(l);
}

// --- Toolbars ------------------------------------------------------------------------------

static void setup_menu(DiffRng *r, int32_t *a)
{
    (void)a;
    Loc l = random_loc(r);
    random_fleets(r, l);
    perturb_story(r);
    perturb_body(r, body_at(l));
    select_loc(l);
    ds.Game_GroupList = (int16_t)diff_pick(r, 0, 0, 0, 1);
    int n = ds.Game_GroupList == 0 ? ds.Game_GroupCount : ds.Game_PlanetForceCount;
    ds.Game_SelGroup = (int16_t)(n > 0 ? diff_range(r, 1, n) : 1);
    ds.Game_GroupsEnabled = (uint8_t)diff_pick(r, 0, 1);
    ds.Game_SatButtons = (int16_t)diff_pick(r, -1, 0, 1, 5, 7, 15);
    ds.Game_ShipInfoButtons = (int16_t)diff_pick(r, -1, 0, 1, 3, 9, 15);
    ds.Game_HotspotHit = (int16_t)diff_pick(r, 0, 0x15, 3);
    if (diff_chance(r, 30)) { // a colony of ours (screen 0x16's colony button)
        body_at(l)->Owner = 1;
        body_at(l)->Colony = 1;
    }
}

static void setup_shipinfo_menu(DiffRng *r, int32_t *a)
{
    setup_menu(r, a);
    if (diff_chance(r, 25))
        ds.Game_SelGroup = 0;
}

static void setup_actions_items(DiffRng *r, int32_t *a)
{
    Loc l = random_loc(r);
    random_fleets(r, l);
    perturb_story(r);
    perturb_body(r, body_at(l));
    if (diff_chance(r, 25)) { // a colony of ours: solar satellites
        body_at(l)->Owner = 1;
        body_at(l)->Colony = 1;
    }
    // The current selection is usually the location, not always.
    select_loc(diff_chance(r, 80) ? l : random_loc(r));
    GfxPath_SetToolbar((int16_t)diff_pick(r, 7, 8, 0x11, 0x14));
    if (diff_chance(r, 20))
        GfxPath_AddToolbarIcon((int16_t)diff_pick(r, 0x46, 0x14, 0x43, 0x44, 0x45, 10, 5));
    a[0] = l.sys;
    a[1] = l.planet;
    a[2] = l.moon;
    a[3] = (int32_t)DIFF_SCRATCH;
    *(uint8_t *)far_host(DIFF_SCRATCH) = (uint8_t)diff_pick(r, 0, 1, 0xAA);
}

static void setup_actions(DiffRng *r, int32_t *a)
{
    setup_deploy(r, a);
    ds.Game_MenuCommand = (int16_t)diff_pick(r, 0, 5, 10, 10, 0x14, 0x43, 0x44, 0x45, 0x46, 0x2E);
    ds.Game_CurrentScreen = (int16_t)diff_pick(r, 8, 8, 0x11, 0x14, 7);
    ds.PlanetInfo_AlienMode = (uint8_t)diff_pick(r, 0, 0, 1);
    ds.Game_GroupList = 0;
    ds.Game_SelGroup = ds.Game_GroupCount > 0 ? (int16_t)diff_range(r, 1, ds.Game_GroupCount) : 1;
}

static void setup_none(DiffRng *r, int32_t *a)
{
    (void)r;
    (void)a;
}

// Base states: a new game on the command centre and the screens the body
// actions run in (their toolbars and redraws draw over what is shown).
static const struct {
    const char *name;
    const char *script;
} states[] = {
    {"command centre", "1500:quit"},
    {"system view (7)", "900:click:168,16;1300:quit"},
    {"ship info (16)", "900:click:120,16;1300:quit"},
    {"surface (20)", "900:click:216,16;1300:quit"},
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
        {&diff_Satellite_CountSatellites, setup_count, 40},
        {&diff_Satellite_DeductSatellites, setup_count, 100},
        {&diff_Satellite_CountTradeShips, setup_trade, 40},
        {&diff_Satellite_TakeTradeShips, setup_trade, 60},
        {&diff_Satellite_DeployStockSatellite, setup_deploy, 24},
        {&diff_Satellite_DeploySatellite, setup_deploy, 40},
        {&diff_Satellite_DeploySpySat, setup_deploy, 40},
        {&diff_Satellite_DeploySpyShip, setup_deploy, 40},
        {&diff_Satellite_DeploySolarSat, setup_deploy, 24},
        {&diff_Satellite_ColonizeBody, setup_deploy, 60},
        {&diff_Satellite_AcquireBody, setup_deploy, 40},
        {&diff_Satellite_SetupGroupScreenMenu, setup_menu, 40},
        {&diff_Satellite_UpdateGroupsMenu, setup_menu, 100},
        {&diff_Satellite_UpdateShipInfoMenu, setup_shipinfo_menu, 60},
        {&diff_Satellite_AddBodyActionItems, setup_actions_items, 140},
        {&diff_Satellite_HandleBodyActions, setup_actions, 80},
        {&diff_Satellite_Init, setup_none, 4},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, bases, STATES, fns[i].setup, diff_cases(fns[i].cases));
    for (int i = 0; i < STATES; i++)
        diff_free(bases[i]);
    return diff_finish();
}
