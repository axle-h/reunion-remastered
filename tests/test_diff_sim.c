// Differential test of unit Sim (1b8a): every hourly routine, original (under
// Unicorn) against the port, from a new game's state with the globals each
// routine reads perturbed at random. See docs/difftest.md. Needs the game data.
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/groups.h"
#include "game/sim.h"

DIFF_PROC(Sim_AdvanceHour, Sim_AdvanceHour());
DIFF_PROC(Sim_ProcessPubTimers, Sim_ProcessPubTimers());
DIFF_PROC(Sim_ProgressResearch, Sim_ProgressResearch());
DIFF_PROC(Sim_ProgressProduction, Sim_ProgressProduction());
DIFF_PROC(Sim_ProcessAllBodyEvents, Sim_ProcessAllBodyEvents());
DIFF_PROC(Sim_UpdateBuildings, Sim_UpdateBuildings());
DIFF_PROC(Sim_UpdateColonySetups, Sim_UpdateColonySetups());
DIFF_PROC(Sim_MineAllBodies, Sim_MineAllBodies());
DIFF_PROC(Sim_ProcessTravellingGroups, Sim_ProcessTravellingGroups());
DIFF_PROC(Sim_CheckEnemyAttacks, Sim_CheckEnemyAttacks());
DIFF_PROC(Sim_GrowStaffExperience, Sim_GrowStaffExperience());
DIFF_PROC(Sim_ProcessStoryTimers, Sim_ProcessStoryTimers());

static int16_t *w16(uint16_t off) { return (int16_t *)ds_at(off); }
static uint8_t *b8(uint16_t off) { return (uint8_t *)ds_at(off); }

// Galaxy::Bodies[s]^ and its length (planets + moons).
static TBody *bodies(int s, int *count)
{
    *count = ds.Galaxy_PlanetCount[s - 1] + ds.Galaxy_MoonCount[s - 1];
    return far_host(ds.Galaxy_Bodies[s - 1]);
}

static void seed(DiffRng *r)
{
    ds.System_RandSeed = (int32_t)diff_rand(r, 0xFFFFFFFFu);
}

// --- AdvanceHour: the date carry -----------------------------------------------

static void setup_date(DiffRng *r, int32_t *a)
{
    (void)a;
    ds.Game_Year = (uint16_t)diff_range(r, 2927, 2935);
    ds.Game_Month = (uint16_t)diff_pick(r, 1, 6, 12, 12, 13);
    ds.Game_Day = (uint16_t)diff_pick(r, 1, 15, 30, 30, 31);
    ds.Game_Hour = (uint16_t)diff_pick(r, 0, 5, 22, 23, 23, 24);
}

// --- ProcessPubTimers: people's timers and deals ---------------------------------

static void setup_pub(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    for (int k = 0; k < 10; k++) {
        TPubPerson *p = &ds.Pub_People[k];
        p->State = (uint8_t)diff_pick(r, 0, 0, 1, 2);
        p->Timer = (int16_t)diff_pick(r, 0, 1, 1, 2, 30);
        if (k + 1 == 2)  // intel: kind * 20 + race
            p->Deal = (uint8_t)(diff_range(r, 0, 3) * 20 + diff_range(r, 2, 12));
        else if (k + 1 == 5)
            p->Deal = (uint8_t)diff_range(r, 0, 2);
        else if (k + 1 == 9)  // deal 1..10, > 10 at half the amounts
            p->Deal = (uint8_t)(diff_range(r, 1, 10) + diff_pick(r, 0, 10));
        else
            p->Deal = (uint8_t)diff_range(r, 0, 3);
    }
    for (int k = 0; k < 10; k++) {
        TPubDeal *d = &ds.Pub_Deals[k];
        for (int j = 0; j < 4; j++) {
            d->Items[j] = (uint8_t)diff_pick(r, 0, diff_range(r, 1, 9), diff_range(r, 11, 45));
            d->Amounts[j] = (uint16_t)diff_range(r, 0, 5000);
        }
        d->Chance = (uint8_t)diff_range(r, 0, 100);
        d->Active = (uint16_t)diff_range(r, 0, 1);
        if (diff_chance(r, 30)) {  // a pirate raid this hour
            d->Year = ds.Game_Year;
            d->Month = (uint8_t)ds.Game_Month;
            d->Day = (uint8_t)ds.Game_Day;
            d->Hour = (uint8_t)ds.Game_Hour;
        }
    }
}

// --- ProgressResearch / ProgressProduction --------------------------------------

static void setup_research(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    for (int k = 0; k < 35; k++) {
        TProduct *p = &ds.Research_Products[k];
        p->State = (int16_t)diff_pick(r, 0, 1, 2, 2, 3, 4, 5);
        p->Progress = (int16_t)diff_pick(r, 0, 1, 50, diff_range(r, 0, 10000));
        p->DevTime = (uint16_t)diff_range(r, 1, 3000);
        for (int j = 0; j < 4; j++)
            p->Skills[j] = (uint8_t)diff_range(r, 0, 6);
    }
    for (int j = 0; j < 4; j++)
        ds.Game_DevSkill[j] = (uint16_t)diff_range(r, 0, 6);
    ds.Game_StaffExp[3] = (uint16_t)diff_range(r, 0, 70);
    *w16(0x5D52) = (int16_t)diff_pick(r, 0, 0, 0, 5);  // story timer blocking research
    *w16(0x5D9E) = (int16_t)diff_pick(r, 0, 0, 0, 4);  // staff member at the university
}

static void setup_production(DiffRng *r, int32_t *a)
{
    (void)a;
    for (int k = 0; k < 35; k++) {
        TProduct *p = &ds.Research_Products[k];
        p->_1D = (uint16_t)diff_pick(r, 0, 0, 1, 2, 5);
        p->_1F = (uint16_t)diff_range(r, 1, 200);
        p->_21 = (uint16_t)diff_range(r, 0, 60);
        p->Stock = (int16_t)diff_range(r, 0, 20);
    }
    ds.Game_ProductionRate = (uint16_t)diff_range(r, 1, 40);
}

// --- Bodies: events, mining -------------------------------------------------------

static void perturb_body(DiffRng *r, TBody *b)
{
    if (diff_chance(r, 30)) {
        b->Owner = (uint8_t)diff_pick(r, 0, 1, 1, 2, 3, 6);
        b->Colony = (uint8_t)diff_pick(r, 0, 0, 1);
    }
    if (diff_chance(r, 50)) {
        b->Population = diff_pick(r, 500, 5000, 40000, 80000, 160000, diff_range(r, 0, 250000));
        b->Attitude = (int8_t)diff_range(r, 0, 69);
        b->TechLevel = (uint8_t)diff_range(r, 0, 7);
        b->Resistance = (int8_t)diff_range(r, 0, 60);
        b->_14 = (int8_t)diff_pick(r, 0, 1, 50, 100, 100);
    }
    b->InfoLevel = (int8_t)diff_pick(r, b->InfoLevel, 0, 5, 9, 28, 29, 38, 39, 60);
    b->Satellite = (uint8_t)diff_pick(r, b->Satellite, 0, 1, 0xF6);
    b->_0A = (uint8_t)diff_pick(r, b->_0A, 0, 1, 3);
    b->Miner = (int8_t)diff_pick(r, 0, 0, 0, 1, 2);
    b->Surveyed = (uint8_t)diff_range(r, 0, 1);
    for (int j = 0; j < 8; j++)
        if (diff_chance(r, 30))
            b->Stores[j] = diff_range(r, 0, 200000);
    for (int j = 0; j < 6; j++)
        if (diff_chance(r, 30))
            b->Deposits[j] = (uint8_t)diff_range(r, 0, 100);
}

static void setup_bodies(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    for (int s = 2; s <= 8; s++)
        if (diff_chance(r, 15))
            ds.Galaxy_SystemStatus[s - 1] = 1;
    for (int s = 1; s <= 8; s++) {
        if (ds.Galaxy_SystemStatus[s - 1] != 1)
            continue;
        int n;
        TBody *b = bodies(s, &n);
        for (int i = 0; i < n; i++)
            if (diff_chance(r, 40))
                perturb_body(r, &b[i]);
    }
    for (int j = 0; j < 6; j++)
        ds.Game_HomeStock[j] = diff_range(r, 0, 300000);
}

// --- Buildings and colony set-ups ------------------------------------------------

static void setup_buildings(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    TBuilding *bl = far_host(ds.Game_Buildings);
    for (int i = 0; i < ds.Game_BuildingCount; i++) {
        if (diff_chance(r, 40))
            bl[i].Health = (uint8_t)diff_pick(r, 0, 1, 39, 40, 41, 79, 80, 81, 100, 200);
        if (diff_chance(r, 20))
            bl[i].Flag = (uint8_t)diff_range(r, 0, 1);
    }
    ds.Game_StaffExp[1] = (uint16_t)diff_range(r, 0, 70);
    ds.Game_StaffHired[1] = (uint16_t)diff_range(r, 0, 3);
    for (int j = 0; j < 6; j++)
        ds.Game_HomeStock[j] = diff_range(r, 0, 300000);
}

static void setup_colony_setups(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    int n = diff_range(r, 0, 4);
    ds.Game_ColonySetupCount = (int16_t)n;
    for (int k = 0; k < n; k++) {
        TColonySetup *s = &ds.Game_ColonySetups[k];
        s->Active = (uint8_t)diff_pick(r, 0, 1, 1);
        s->System = 1;
        s->Planet = (uint8_t)diff_pick(r, 3, 4, 5);
        s->Moon = 0;
        for (int j = 0; j < 6; j++)
            s->Timers[j] = (int16_t)diff_pick(r, 0, 0, 1, 1, 2, 10);
    }
}

// --- Fleets in flight -----------------------------------------------------------------

// n fleets (list 0) somewhere in systems 1..3, with the given statuses.
static void random_fleets(DiffRng *r, int n, bool flying)
{
    ds.Game_GroupCount = (int16_t)n;
    for (int i = 1; i <= n; i++) {
        TGroup *g = Groups_At(0, (int16_t)i);
        g->Kind = (uint8_t)diff_pick(r, 1, 2, 3, 4);
        g->Name.len = 5;
        memcpy(g->Name.s, "Fleet", 5);
        g->System = (uint8_t)diff_pick(r, 1, 1, 1, 2, 3, diff_range(r, 4, 8));
        g->Planet = (uint8_t)diff_pick(r, 0, 1, 3, 5, 5);
        g->Moon = (uint8_t)diff_pick(r, 0, 0, 0, 1);
        if (g->Planet == 0 || g->Planet > ds.Galaxy_PlanetCount[g->System - 1])
            g->Moon = 0;
        if (g->Planet > ds.Galaxy_PlanetCount[g->System - 1])
            g->Planet = 1;
        if (flying) {
            g->Status = (uint8_t)diff_pick(r, 2, 4, 5, 6);
            g->Leg1 = (int16_t)diff_pick(r, 0, 0, 1, 5, 200);
            g->Jump = (int16_t)diff_pick(r, 0, 0, 1, 2);
            g->Leg2 = (int16_t)diff_pick(r, 0, 1, 1, 5, 150);
        } else {
            g->Status = (uint8_t)diff_pick(r, 1, 2, 2, 4);
        }
    }
    ds.Game_SelGroup = (int16_t)diff_range(r, 0, n);
}

static void setup_groups(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    random_fleets(r, diff_range(r, 1, 4), true);
    ds.Game_StaffExp[0] = (uint16_t)diff_range(r, 0, 40);
    *b8(0x5D48) = (uint8_t)diff_pick(r, 1, 1, 1, 0);  // first hyperspace jump seen
    // Unexplored systems and planets, alien colonies, races not yet met.
    for (int s = 2; s <= 8; s++) {
        if (diff_chance(r, 30))
            ds.Galaxy_SystemStatus[s - 1] = (int8_t)diff_pick(r, -1, 0, 1);
        for (int pl = 0; pl < 8; pl++)
            if (diff_chance(r, 20))
                ds.Galaxy_PlanetStatus[s - 1][pl] = (int8_t)diff_pick(r, -1, 0, 1);
    }
    for (int s = 1; s <= 3; s++) {
        int n;
        TBody *b = bodies(s, &n);
        for (int i = 0; i < n; i++)
            if (diff_chance(r, 10))
                b[i].Owner = (uint8_t)diff_range(r, 2, 12);
    }
    for (int k = 0; k < 11; k++)
        if (diff_chance(r, 30))
            ds.Races_Races[k].Status = (int8_t)diff_pick(r, 0, 0, 2, 6);
    TGroup *g = Groups_At(0, 1);
    switch (diff_rand(r, 4)) {
    case 0:  // into hyperspace now; the first time shows the scene
        g->Status = 5;
        g->Leg1 = 0;
        g->Jump = 1;
        g->Leg2 = 10;
        *b8(0x5D48) = (uint8_t)diff_range(r, 0, 1);
        break;
    case 1:  // arriving in a system nobody has explored
        g->System = (uint8_t)diff_range(r, 2, 8);
        g->Planet = 0;
        g->Moon = 0;
        g->Status = 4;
        g->Leg1 = g->Jump = 0;
        g->Leg2 = 1;
        ds.Galaxy_SystemStatus[g->System - 1] = 0;
        break;
    default:
        break;
    }
    *b8(0x5D71) = (uint8_t)diff_pick(r, 0, 0, 1);  // story flag: alien talk on arrival
    *b8(0x5D95) = (uint8_t)diff_pick(r, 0, 0, 1);
}

// --- Alien forces ----------------------------------------------------------------------

static void setup_enemies(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    for (int k = 0; k < 11; k++) {
        TRace *rc = &ds.Races_Races[k];
        if (diff_chance(r, 50))
            rc->Status = (int8_t)diff_pick(r, 0, 1, 2, 2, 3, 6);
        for (int f = 0; f < rc->ForceCount && f < 7; f++) {
            TRaceForce *fc = &rc->Forces[f];
            if (!diff_chance(r, 30))
                continue;
            fc->Active = (uint8_t)diff_pick(r, 0, 1, 2, 2);
            fc->_03 = (uint16_t)diff_pick(r, 0, 1, 1, 2, 10);
            fc->Timer = (int16_t)diff_pick(r, 0, 1, 1, 2, 30);
            fc->NextMission = (uint8_t)diff_range(r, 0, 4);
            fc->System = 1;
            fc->Body = (uint8_t)diff_pick(r, 3, 4, 5, 5);
            fc->Moon = 0;
        }
    }
    random_fleets(r, diff_range(r, 0, 4), false);
}

// --- Staff and story -----------------------------------------------------------------------

static void setup_staff(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    for (int k = 0; k < 4; k++) {
        ds.Game_StaffHired[k] = (uint16_t)diff_range(r, 0, 3);
        ds.Game_StaffExp[k] = (uint16_t)diff_range(r, 0, 70);
        ds.Game_DevSkill[k] = (uint16_t)diff_range(r, 0, 6);
    }
    *w16(0x5D9E) = (int16_t)diff_pick(r, 0, 1, 2, 3, 4, 4);  // staff type at the university
    *w16(0x5D9C) = (int16_t)diff_pick(r, 0, 1, 1, 5);      // hours left
    *w16(0x5DA0) = (int16_t)diff_range(r, 1, 3);           // the course
    ds.Game_StoryStage = (int16_t)diff_range(r, 1, 6);
    // Many hours of rolls, so the 1-in-100 experience gain happens too.
}

static void setup_story(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    ds.Game_StaffExp[3] = (uint16_t)diff_pick(r, 0, 10, 10);
    ds.Game_StaffHired[3] = (uint16_t)diff_range(r, 0, 3);
    for (int k = 0; k < 35; k++)
        ds.Game_IdeaTimer[k] = (int16_t)diff_pick(r, -1, -1, 0, 1, 1, 2, 51);
    // Story timers (word) and their done flags (byte), DS:5D4A..5D98.
    static const uint16_t timers[] = {0x5D4A, 0x5D4E, 0x5D52, 0x5D54, 0x5D58, 0x5D5C, 0x5D60, 0x5D64,
                                      0x5D6A, 0x5D6E, 0x5D72, 0x5D76, 0x5D7A, 0x5D7E, 0x5D82, 0x5D86,
                                      0x5D8A, 0x5D8E, 0x5D92, 0x5D96, 0x5D40};
    for (size_t i = 0; i < sizeof timers / sizeof timers[0]; i++) {
        if (!diff_chance(r, 15))
            continue;
        *w16(timers[i]) = (int16_t)diff_pick(r, 0, 1, 1, 2);
        if (timers[i] != 0x5D8A && timers[i] != 0x5D40)
            *b8((uint16_t)(timers[i] + 2)) = (uint8_t)diff_pick(r, 0, 0, 1);
    }
    ds.Races_Races[0].Status = (int8_t)diff_pick(r, 0, 1, 3, 3);
    *w16(0x5D9C) = (int16_t)diff_pick(r, 0, 0, 3);
    *w16(0x5D9E) = (int16_t)diff_pick(r, 0, 0, 4);
}

// Base states: a new game on the command centre, and the screens of the menu
// bar (routines redraw what the current screen shows).
static const struct {
    const char *name;
    const char *script;
} states[] = {
    {"command centre", "1500:quit"},
    {"research (3)", "900:click:72,16;1300:quit"},
    {"shop (5)", "900:click:24,16;1300:quit"},
    {"system view (7)", "900:click:168,16;1300:quit"},
    {"ship info (16)", "900:click:120,16;1300:quit"},
    {"surface (20)", "900:click:216,16;1300:quit"},
    {"message log (11)", "900:click:264,16;1300:quit"},
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
        {&diff_Sim_AdvanceHour, setup_date, 21},
        {&diff_Sim_ProcessPubTimers, setup_pub, 42},
        {&diff_Sim_ProgressResearch, setup_research, 42},
        {&diff_Sim_ProgressProduction, setup_production, 42},
        {&diff_Sim_ProcessAllBodyEvents, setup_bodies, 42},
        {&diff_Sim_UpdateBuildings, setup_buildings, 42},
        {&diff_Sim_UpdateColonySetups, setup_colony_setups, 28},
        {&diff_Sim_MineAllBodies, setup_bodies, 28},
        {&diff_Sim_ProcessTravellingGroups, setup_groups, 42},
        {&diff_Sim_CheckEnemyAttacks, setup_enemies, 42},
        {&diff_Sim_GrowStaffExperience, setup_staff, 28},
        {&diff_Sim_ProcessStoryTimers, setup_story, 42},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, bases, STATES, fns[i].setup, diff_cases(fns[i].cases));
    for (int i = 0; i < STATES; i++)
        diff_free(bases[i]);
    return diff_finish();
}
