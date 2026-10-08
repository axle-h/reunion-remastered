// Differential test of unit Intel (29b9): the race AI helpers, story events,
// body ownership changes, group disbanding and the intel reports, original
// (under Unicorn) against the port, from new-game states on several screens
// with the races, forces, bodies and groups perturbed at random. See
// docs/difftest.md. Needs the game data. SetGroupTarget is covered by
// test_travel.
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/groups.h"
#include "game/intel.h"

DIFF_PROC(Intel_FirstContact, Intel_FirstContact((int16_t)a[0], far_host((FarPtr)a[1])));
DIFF_PROC(Intel_ClearBody, Intel_ClearBody((int16_t)a[0], (int16_t)a[1], (int16_t)a[2], (uint8_t)a[3] != 0));
DIFF_PROC(Intel_DispatchForce, Intel_DispatchForce((int16_t)a[0], (int16_t)a[1], (int16_t)a[2], (int16_t)a[3],
                                                   (int16_t)a[4], (uint8_t)a[5]));
DIFF_PROC(Intel_DispatchForceNow, Intel_DispatchForceNow((int16_t)a[0], (int16_t)a[1], (int16_t)a[2],
                                                         (int16_t)a[3], (int16_t)a[4], (uint8_t)a[5],
                                                         (int16_t)a[6]));
DIFF_PROC(Intel_ScheduleForceLaunch,
          Intel_ScheduleForceLaunch((int16_t)a[0], (int16_t)a[1], (int16_t)a[2], (int16_t)a[3]));
DIFF_PROC(Intel_SpaceBattleWon, Intel_SpaceBattleWon((int16_t)a[0], (int16_t)a[1]));
DIFF_PROC(Intel_SpaceBattleLost, Intel_SpaceBattleLost((int16_t)a[0], (int16_t)a[1]));
DIFF_PROC(Intel_ClearBodyFull, Intel_ClearBodyFull((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_PROC(Intel_PurgeBody, Intel_PurgeBody((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_PROC(Intel_DestroyBody, Intel_DestroyBody((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_PROC(Intel_ClaimBodyForRace,
          Intel_ClaimBodyForRace((int16_t)a[0], (int16_t)a[1], (int16_t)a[2], (int16_t)a[3]));
DIFF_PROC(Intel_ClaimBodyForPlayer, Intel_ClaimBodyForPlayer((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_PROC(Intel_ForceLands, Intel_ForceLands((int16_t)a[0], (int16_t)a[1]));
DIFF_PROC(Intel_DisbandGroup, Intel_DisbandGroup((int16_t)a[0], (int16_t)a[1]));
DIFF_PROC(Intel_SendForceHome, Intel_SendForceHome((int16_t)a[0], (int16_t)a[1]));
DIFF_PROC(Intel_SendGroupHome, Intel_SendGroupHome((int16_t)a[0]));
DIFF_PROC(Intel_SetForceTarget,
          Intel_SetForceTarget((int16_t)a[0], (int16_t)a[1], (int16_t)a[2], (int16_t)a[3], (int16_t)a[4]));
DIFF_PROC(Intel_ClearForce, Intel_ClearForce((int16_t)a[0], (int16_t)a[1]));
DIFF_PROC(Intel_EliminateRace, Intel_EliminateRace((int16_t)a[0]));
DIFF_PROC(Intel_AnnihilateSystem4, Intel_AnnihilateSystem4());
DIFF_PROC(Intel_ScheduleIdea, Intel_ScheduleIdea((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_PROC(Intel_GrantInvention, Intel_GrantInvention((int16_t)a[0]));
DIFF_PROC(Intel_OfferInventionForAnalysis, Intel_OfferInventionForAnalysis((int16_t)a[0]));
DIFF_FUNC(Intel_IsRaceActive, Intel_IsRaceActive((int16_t)a[0]));
DIFF_PROC(Intel_ShowRaceIntel, Intel_ShowRaceIntel((int16_t)a[0], (int16_t)a[1]));
DIFF_PROC(Intel_Init, Intel_Init());

static int16_t *w16(uint16_t off) { return (int16_t *)ds_at(off); }
static uint8_t *b8(uint16_t off) { return (uint8_t *)ds_at(off); }

static void seed(DiffRng *r)
{
    ds.System_RandSeed = (int32_t)diff_rand(r, 0xFFFFFFFFu);
}

static uint8_t moons_of(int s, int p)
{
    return *((uint8_t *)far_host(ds.Galaxy_MoonLists[s - 1]) + p * 9 - 9);
}

static TBody *body(int s, int p, int m)
{
    TBody *b = far_host(ds.Galaxy_Bodies[s - 1]);
    if (m == 0)
        return &b[p - 1];
    return &b[*((uint8_t *)far_host(ds.Galaxy_MoonLists[s - 1]) + p * 9 - 9 + m) - 1];
}

// A valid location: system, planet, moon (often New Earth or system 4).
static void location(DiffRng *r, int32_t *s, int32_t *p, int32_t *m)
{
    *s = diff_pick(r, 1, 1, 1, 4, diff_range(r, 1, 8));
    *p = diff_range(r, 1, ds.Galaxy_PlanetCount[*s - 1]);
    if (*s == 1 && diff_chance(r, 40))
        *p = 5;
    int n = moons_of(*s, *p);
    *m = n > 0 && diff_chance(r, 40) ? diff_range(r, 1, n) : 0;
}

static void perturb_body(DiffRng *r, TBody *b)
{
    b->Owner = (uint8_t)diff_pick(r, 0, 1, 2, 3, 7, 8, 12, diff_range(r, 0, 12));
    b->_01 = (uint8_t)diff_range(r, 0, 1);
    b->Colony = (uint8_t)diff_pick(r, 0, 1, 1);
    b->Miner = (int8_t)diff_range(r, 0, 2);
    b->Population = diff_range(r, 0, 200000);
    b->Resistance = (int8_t)diff_range(r, 0, 60);
    for (int k = 0; k < 8; k++)
        b->Stores[k] = diff_pick(r, 0, 1, 50, diff_range(r, 0, 70000));
}

// A race: random status, a few forces anywhere (often at the location given).
static void perturb_race(DiffRng *r, int race, int s, int p, int m)
{
    TRace *rc = Intel_Race((int16_t)race);
    rc->Status = (int8_t)diff_pick(r, 0, 1, 2, 2, 4, 6, 6, -1);
    rc->Speed = (uint8_t)diff_range(r, 0, 60);
    rc->ForceCount = (int8_t)diff_pick(r, 0, 1, 3, 7);
    for (int k = 0; k < 8; k++)
        rc->KnownSystems[k] = (uint8_t)diff_range(r, 0, 1);
    for (int f = 1; f <= rc->ForceCount; f++) {
        TRaceForce *fc = Intel_Force((int16_t)race, (int16_t)f);
        fc->Mission = (uint8_t)diff_pick(r, 0, 1, 2, 3);
        fc->Active = (uint8_t)diff_pick(r, 0, 1, 1, 2);
        fc->_03 = (uint16_t)diff_range(r, 0, 50);
        fc->Timer = (int16_t)diff_range(r, 0, 30);
        fc->NextMission = (uint8_t)diff_range(r, 0, 3);
        if (diff_chance(r, 50)) {
            fc->System = (uint8_t)s;
            fc->Body = (uint8_t)p;
            fc->Moon = (uint8_t)m;
        } else {
            int32_t ls, lp, lm;
            location(r, &ls, &lp, &lm);
            fc->System = (uint8_t)ls;
            fc->Body = (uint8_t)lp;
            fc->Moon = (uint8_t)lm;
        }
        for (int k = 0; k < 8; k++)
            fc->Units[k] = (int16_t)diff_pick(r, 0, 1, 20, diff_range(r, 0, 2000));
    }
    if (diff_chance(r, 50)) {
        rc->HomeSystem = (uint8_t)s;
        rc->HomeBody = (uint8_t)p;
    }
}

static void random_group(DiffRng *r, TGroup *g, int list, int s, int p, int m)
{
    g->Kind = (uint8_t)(list == 1 ? 5 : diff_range(r, 1, 4));
    g->Name.len = 5;
    memcpy(g->Name.s, "Group", 5);
    if (diff_chance(r, 60)) {
        g->System = (uint8_t)s;
        g->Planet = (uint8_t)p;
        g->Moon = (uint8_t)m;
    } else {
        int32_t ls, lp, lm;
        location(r, &ls, &lp, &lm);
        g->System = (uint8_t)ls;
        g->Planet = (uint8_t)lp;
        g->Moon = (uint8_t)lm;
    }
    g->Status = (uint8_t)(list == 1 ? 7 : diff_pick(r, 1, 2, 2, 4, 6));
    for (int t = 0; t < 2; t++)
        for (int u = 0; u < 4; u++) {
            g->Units[t][u].ShipType = (int16_t)diff_pick(r, 0, 1, diff_range(r, 0, 300));
            for (int w = 0; w < 4; w++)
                g->Units[t][u].Weapons[w] = (int16_t)diff_pick(r, 0, diff_range(r, 0, 300));
        }
}

// Groups of both lists, many at (s, p, m).
static void random_groups(DiffRng *r, int s, int p, int m)
{
    int fleets = diff_range(r, 0, 6);
    int forces = diff_range(r, 1, 6);
    for (int i = 1; i <= fleets; i++)
        random_group(r, Groups_At(0, (int16_t)i), 0, s, p, m);
    for (int i = 1; i <= forces; i++)
        if (diff_chance(r, 70))
            random_group(r, Groups_At(1, (int16_t)i), 1, s, p, m);
    ds.Game_GroupCount = (int16_t)fleets;
    ds.Game_PlanetForceCount = (int16_t)forces;
    ds.Game_GroupList = (int16_t)diff_range(r, 0, 1);
    ds.Game_ShownGroupCount = ds.Game_GroupList == 0 ? ds.Game_GroupCount : ds.Game_PlanetForceCount;
    ds.Game_SelGroup = (int16_t)diff_range(r, 0, ds.Game_ShownGroupCount);
}

// Buildings: some moved to (s, p, m).
static void random_buildings(DiffRng *r, int s, int p, int m)
{
    TBuilding *bl = far_host(ds.Game_Buildings);
    for (int i = 0; i < ds.Game_BuildingCount; i++)
        if (diff_chance(r, 25)) {
            bl[i].System = (uint8_t)s;
            bl[i].Planet = (uint8_t)p;
            bl[i].Moon = (uint8_t)m;
        }
}

static void perturb_world(DiffRng *r, int s, int p, int m)
{
    seed(r);
    perturb_body(r, body(s, p, m));
    for (int race = 2; race <= 12; race++)
        if (diff_chance(r, 40))
            perturb_race(r, race, s, p, m);
}

// --- First contact -----------------------------------------------------------------

static void setup_first_contact(DiffRng *r, int32_t *a)
{
    seed(r);
    int race = diff_pick(r, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 2, 4, 12);
    if (race > 0)
        Intel_Race((int16_t)race)->Status = (int8_t)diff_pick(r, 0, 0, 0, 0, 1, 2, -1);
    static const uint16_t flags[] = {0x5D50, 0x5D56, 0x5D5A, 0x5D5E, 0x5D78, 0x5D84, 0x5D98};
    for (size_t i = 0; i < sizeof flags / sizeof flags[0]; i++)
        *b8(flags[i]) = (uint8_t)diff_pick(r, 0, 0, 1);
    *w16(0x5D96) = (int16_t)diff_pick(r, 0, 0, -3, 5);
    *w16(0x5D4E) = (int16_t)diff_range(r, 0, 1000);
    ds.Research_Products[7].State = (int16_t)diff_pick(r, 0, 0, 1, 5);
    ds.Game_IdeaTimer[7] = (int16_t)diff_pick(r, -1, -1, 0, 30);
    uint8_t *msg = far_host(DIFF_SCRATCH);
    const char *text = diff_chance(r, 50) ? " First contact! | They greet you." : "";
    msg[0] = (uint8_t)strlen(text);
    memcpy(msg + 1, text, msg[0]);
    a[0] = race;
    a[1] = (int32_t)DIFF_SCRATCH;
}

// --- Bodies --------------------------------------------------------------------------

static void setup_body(DiffRng *r, int32_t *a)
{
    location(r, &a[0], &a[1], &a[2]);
    perturb_world(r, a[0], a[1], a[2]);
    a[3] = diff_range(r, 0, 1);
}

static void setup_purge(DiffRng *r, int32_t *a)
{
    location(r, &a[0], &a[1], &a[2]);
    perturb_world(r, a[0], a[1], a[2]);
    random_buildings(r, a[0], a[1], a[2]);
    random_groups(r, a[0], a[1], a[2]);
}

static void setup_destroy(DiffRng *r, int32_t *a)
{
    setup_purge(r, a);
    if (diff_chance(r, 30)) {
        a[0] = 1;
        a[1] = 5;
        a[2] = 0;
    }
    ds.Game_CurrentScreen = (int16_t)diff_pick(r, 1, 3, 4, 7, 8, 0xD, 0x10, 0x11, 0x14, 0x16, 0x20);
}

static void setup_claim_race(DiffRng *r, int32_t *a)
{
    a[0] = diff_pick(r, 2, 3, 3, 7, 7, 8, 8, 12, 12, diff_range(r, 2, 12));
    location(r, &a[1], &a[2], &a[3]);
    perturb_world(r, a[1], a[2], a[3]);
    *b8(0x5D6C) = (uint8_t)diff_range(r, 0, 1);
}

static void setup_claim_player(DiffRng *r, int32_t *a)
{
    setup_purge(r, a);
    ds.Game_PlanetForceCount = (int16_t)diff_range(r, 0, 8);
}

// --- Forces ------------------------------------------------------------------------------

static void setup_force(DiffRng *r, int32_t *a)
{
    int32_t s, p, m;
    location(r, &s, &p, &m);
    perturb_world(r, s, p, m);
    int race = diff_range(r, 2, 12);
    perturb_race(r, race, s, p, m);
    TRace *rc = Intel_Race((int16_t)race);
    if (rc->ForceCount == 0)
        rc->ForceCount = 1;
    a[0] = race;
    a[1] = diff_range(r, 1, rc->ForceCount);
    // The destination: often the force's own system / body.
    TRaceForce *fc = Intel_Force((int16_t)race, (int16_t)a[1]);
    switch (diff_rand(r, 4)) {
    case 0:
        a[2] = fc->System;
        a[3] = fc->Body;
        a[4] = fc->Moon;
        break;
    case 1:
        a[2] = fc->System;
        a[3] = fc->Body;
        a[4] = fc->Moon == 0 && moons_of(fc->System, fc->Body) > 0 ? 1 : 0;
        break;
    case 2:
        a[2] = fc->System;
        a[3] = diff_range(r, 1, ds.Galaxy_PlanetCount[fc->System - 1]);
        a[4] = 0;
        break;
    default:
        a[2] = s;
        a[3] = p;
        a[4] = m;
        break;
    }
    a[5] = diff_range(r, 0, 4);
    a[6] = diff_range(r, 0, 100);
}

static void setup_launch(DiffRng *r, int32_t *a)
{
    a[0] = diff_range(r, 2, 12);
    a[1] = diff_range(r, 1, 7);
    a[2] = diff_range(r, -5, 2000);
    a[3] = diff_pick(r, 0, 2, 7);
}

static void setup_force_lands(DiffRng *r, int32_t *a)
{
    setup_force(r, a);
    TRaceForce *fc = Intel_Force((int16_t)a[0], (int16_t)a[1]);
    TBody *b = body(fc->System, fc->Body, fc->Moon);
    if (diff_chance(r, 60))
        b->Owner = (uint8_t)a[0];
    for (int k = 0; k < 8; k++)
        if (diff_chance(r, 30))
            fc->Units[k] = (int16_t)diff_pick(r, -1, 30000, -30000);
}

static void setup_race(DiffRng *r, int32_t *a)
{
    int32_t s, p, m;
    location(r, &s, &p, &m);
    perturb_world(r, s, p, m);
    a[0] = diff_range(r, 2, 12);
    perturb_race(r, a[0], s, p, m);
    // Bodies and moons it owns all over the galaxy.
    for (int sys = 1; sys <= 8; sys++)
        for (int pl = 1; pl <= ds.Galaxy_PlanetCount[sys - 1]; pl++)
            for (int mo = 0; mo <= moons_of(sys, pl); mo++)
                if (diff_chance(r, 15)) {
                    TBody *b = body(sys, pl, mo);
                    b->Owner = (uint8_t)a[0];
                    b->Colony = (uint8_t)diff_range(r, 0, 1);
                    b->_01 = (uint8_t)diff_range(r, 0, 1);
                    for (int k = 0; k < 8; k++)
                        b->Stores[k] = diff_range(r, 0, 3000);
                }
    a[1] = diff_pick(r, 1, 2, 3, 4, 0, 5);
}

static void setup_is_active(DiffRng *r, int32_t *a)
{
    a[0] = diff_range(r, 1, 12);
    Intel_Race((int16_t)a[0])->Status = (int8_t)diff_pick(r, -1, 0, 1, 2, 3, 4, 5, 6);
}

// --- Battles -----------------------------------------------------------------------------

static void setup_battle(DiffRng *r, int32_t *a)
{
    int32_t m;
    location(r, &a[0], &a[1], &m);
    m = 0;
    perturb_world(r, a[0], a[1], m);
    random_groups(r, a[0], a[1], m);
    for (int race = 2; race <= 12; race++)
        if (diff_chance(r, 50))
            perturb_race(r, race, a[0], a[1], m);
}

static void setup_disband(DiffRng *r, int32_t *a)
{
    int32_t s, p, m;
    location(r, &s, &p, &m);
    seed(r);
    random_groups(r, s, p, m);
    a[0] = diff_range(r, 0, 1);
    int n = a[0] == 0 ? ds.Game_GroupCount : ds.Game_PlanetForceCount;
    if (n == 0) {
        a[0] = 1;
        n = ds.Game_PlanetForceCount;
    }
    a[1] = diff_range(r, 1, n);
    ds.Game_SelGroup0 = (int16_t)diff_range(r, 0, 3);
    ds.Game_SelGroup1 = (int16_t)diff_range(r, 0, 3);
}

static void setup_send_home(DiffRng *r, int32_t *a)
{
    setup_disband(r, a);
    a[0] = diff_range(r, 1, ds.Game_GroupCount > 0 ? ds.Game_GroupCount : 1);
}

static void setup_system4(DiffRng *r, int32_t *a)
{
    (void)a;
    int32_t s = 4, p = diff_range(r, 1, ds.Galaxy_PlanetCount[3]), m = 0;
    perturb_world(r, s, p, m);
    random_buildings(r, s, p, m);
    random_groups(r, s, p, m);
    for (int race = 2; race <= 12; race++)
        if (diff_chance(r, 50))
            perturb_race(r, race, s, p, m);
}

// --- Ideas -----------------------------------------------------------------------------------

static void setup_idea(DiffRng *r, int32_t *a)
{
    seed(r);
    a[0] = diff_range(r, 1, 35);
    ds.Research_Products[a[0] - 1].State = (int16_t)diff_pick(r, 0, 0, 0, 1, 3, 5);
    ds.Research_Products[a[0] - 1].Progress = (int16_t)diff_range(r, 0, 500);
    ds.Game_IdeaTimer[a[0] - 1] = (int16_t)diff_pick(r, -1, -1, 0, 20);
    a[1] = diff_range(r, 0, 200);
    a[2] = diff_pick(r, 1, 20, 100);
    ds.Game_SelInvention = (int16_t)diff_range(r, 1, 35);
}

static void setup_none(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
}

// Base states: a new game on the command centre and the screens of the menu
// bar (the reports and the invention updates draw on the current screen).
static const struct {
    const char *name;
    const char *script;
} states[] = {
    {"command centre", "1500:quit"},
    {"research (3)", "900:click:72,16;1300:quit"},
    {"system view (7)", "900:click:168,16;1300:quit"},
    {"surface (20)", "900:click:216,16;1300:quit"},
    {"message log (11)", "900:click:264,16;1300:quit"},
    {"group screen (22)", "900:click:120,16;1100:click:100,194;1300:click:32,68;1600:quit"},
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
        {&diff_Intel_FirstContact, setup_first_contact, 100},
        {&diff_Intel_ClearBody, setup_body, 20},
        {&diff_Intel_ClearBodyFull, setup_body, 10},
        {&diff_Intel_PurgeBody, setup_purge, 30},
        {&diff_Intel_DestroyBody, setup_destroy, 30},
        {&diff_Intel_ClaimBodyForRace, setup_claim_race, 40},
        {&diff_Intel_ClaimBodyForPlayer, setup_claim_player, 30},
        {&diff_Intel_DispatchForce, setup_force, 25},
        {&diff_Intel_DispatchForceNow, setup_force, 25},
        {&diff_Intel_ScheduleForceLaunch, setup_launch, 5},
        {&diff_Intel_SetForceTarget, setup_force, 30},
        {&diff_Intel_SendForceHome, setup_force, 20},
        {&diff_Intel_ForceLands, setup_force_lands, 30},
        {&diff_Intel_ClearForce, setup_force, 10},
        {&diff_Intel_SpaceBattleWon, setup_battle, 30},
        {&diff_Intel_SpaceBattleLost, setup_battle, 40},
        {&diff_Intel_DisbandGroup, setup_disband, 40},
        {&diff_Intel_SendGroupHome, setup_send_home, 15},
        {&diff_Intel_EliminateRace, setup_race, 25},
        {&diff_Intel_AnnihilateSystem4, setup_system4, 25},
        {&diff_Intel_ScheduleIdea, setup_idea, 20},
        {&diff_Intel_GrantInvention, setup_idea, 30},
        {&diff_Intel_OfferInventionForAnalysis, setup_idea, 30},
        {&diff_Intel_IsRaceActive, setup_is_active, 20},
        {&diff_Intel_ShowRaceIntel, setup_race, 50},
        {&diff_Intel_Init, setup_none, 2},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, bases, STATES, fns[i].setup, diff_cases(fns[i].cases));
    for (int i = 0; i < STATES; i++)
        diff_free(bases[i]);
    return diff_finish();
}
