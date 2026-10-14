// intel: race forces (targets, travel and launch timers, landings, mission
// transitions), first contact, body ownership and group helpers, on the
// initial data from the EXE with a fixed RandSeed; plus the AlienTalk menu
// mouse mapping.
#include <stdio.h>

#include "game/alientalk.h"
#include "game/dseg.h"
#include "game/main_program.h"
#include "game/fixes.h"
#include "game/gfx.h"
#include "game/groups.h"
#include "game/intel.h"
#include "game/message.h"
#include "game/system.h"
#include "game/textfile.h"
#include "platform/fs.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static int messages;

static bool wait_retrace(void) { return true; }
static bool key_pressed(void) { messages++; return true; }  // message boxes: answer at once
static char read_key(void) { return '\r'; }

static void reset(void)
{
    dseg_clear();
    vid_clear();
    dseg_init();
    system_init();
    Game_InitGameData();
    Message_Init();
    ds.Gfx_LineStride = 320;
    ds.Gfx_LineStrideCopy = 320;
    gfx_wait_retrace = wait_retrace;
    TextFile_KeyPressed = key_pressed;
    TextFile_ReadKey = read_key;
    messages = 0;
}

static TBody *body(int s, int idx) { return ds.Galaxy_Bodies[s - 1] + (idx - 1); }

// The value Random(range) + base gives after seeding with `seed`.
static int rnd(int32_t seed, uint16_t range, int base)
{
    ds.System_RandSeed = seed;
    return System_Random(range) + base;
}

// Line i of the message log (leading blanks dropped).
static const char *log_line(int i)
{
    static char line[64];
    const uint8_t *e = ds.Message_Log + (i - 1) * 0x35 + 2;
    SDL_snprintf(line, sizeof line, "%.*s", (int)e[0], (const char *)e + 1);
    return line;
}

// The message log's lines joined by '|'.
static const char *log_text(void)
{
    static char text[1024];
    text[0] = '\0';
    for (int i = 1; i <= ds.Message_LogCount; i++) {
        SDL_strlcat(text, log_line(i), sizeof text);
        SDL_strlcat(text, "|", sizeof text);
    }
    return text;
}

// Fix (game/fixes.h): the reports on armies (kind 2), garrisons (3) and
// weapons (4) start with "Information on: <race>", as the colony report (1)
// does; off, the line is lost. The garrison report then has 7 lines, and
// the box shows all of them (its last line lists ground units 3 and 4).
static void test_spy_reports(void)
{
    char info[64], name[32];
    for (int fix = 0; fix <= 1; fix++) {
        reset();
        fixes.spy_reports = fix;
        Intel_ClaimBodyForRace(7, 3, 2, 0);
        Intel_Force(7, 1)->Mission = 1;
        Intel_Force(7, 1)->Active = 1;
        Intel_Race(7)->ForceCount = 1;
        pstr_get(name, sizeof name, &Intel_Race(7)->Name);
        SDL_snprintf(info, sizeof info, "Information on: %s", name);
        for (int kind = 1; kind <= 4; kind++) {
            ds.Message_LogCount = 0;
            Intel_ShowRaceIntel(7, (int16_t)kind);
            const char *log = log_text();
            printf("spy report %d (fix %d): %s\n", kind, fix, log);
            CHECK((SDL_strstr(log, info) == log) == (fix || kind == 1));
            if (kind == 3) {
                // The last report, Lyrae 7's, whole.
                int first = ds.Message_LogCount - (fix ? 6 : 5);
                CHECK(SDL_strcmp(log_line(first), fix ? info : "Their garrison troops are on") == 0);
                CHECK(SDL_strstr(log_line(ds.Message_LogCount), "Missile") != nullptr);
            }
        }
    }
    fixes.spy_reports = true;
}

int main(int argc, char *argv[])
{
    dseg_clear();
    vid_clear();
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data\n");
        return 77;
    }
    dseg_init();
    reset();

    // Record addressing: Races[2] at 6BCE, forces 27 bytes apart starting 0Ch into the record.
    CHECK((uint8_t *)Intel_Race(2) == (uint8_t *)&ds.Races_Races[0]);
    CHECK((uint8_t *)Intel_Race(12) == (uint8_t *)&ds.Races_Races[10]);
    CHECK((uint8_t *)&Intel_Force(5, 1)->Mission == (uint8_t *)Intel_Race(5) + 0x27);
    CHECK((uint8_t *)&Intel_Force(5, 3)->Mission == (uint8_t *)Intel_Race(5) + 0x0C + 3 * 27);

    // IsRaceActive: Status 2 and 4 only.
    int8_t saved = Intel_Race(5)->Status;
    for (int s = -1; s <= 7; s++) {
        Intel_Race(5)->Status = (int8_t)s;
        CHECK(Intel_IsRaceActive(5) == (s == 2 || s == 4));
    }
    Intel_Race(5)->Status = saved;

    // ScheduleForceLaunch only stores the timer.
    Intel_ScheduleForceLaunch(7, 2, 123, 2);
    CHECK(ds.Intel_LaunchRace == 7 && ds.Intel_LaunchForce == 2);
    CHECK(ds.Intel_LaunchTimer == 123 && ds.Intel_LaunchMission == 2);

    // SetForceTarget: travel time by distance, Active := 2, timers cleared; no change for the same target.
    const int race = 3, force = 1;
    TRaceForce *f = Intel_Force(race, force);
    f->System = 2; f->Body = 4; f->Moon = 0;
    f->Active = 1; f->Timer = 99; f->NextMission = 5; f->TravelTime = 0;
    ds.System_RandSeed = 1000;
    Intel_SetForceTarget(race, force, 2, 4, 0);
    CHECK(f->Active == 1 && f->Timer == 99 && f->NextMission == 5 && f->TravelTime == 0);
    int32_t seed = 1000;
    int expect = rnd(seed, 5, 6);
    ds.System_RandSeed = seed;
    Intel_SetForceTarget(race, force, 2, 4, 1);     // same body, other moon
    CHECK(f->TravelTime == expect && f->TravelTime >= 6 && f->TravelTime <= 10);
    CHECK(f->Moon == 1 && f->Active == 2 && f->Timer == 0 && f->NextMission == 0);
    expect = rnd(seed, 10, 0x14);
    ds.System_RandSeed = seed;
    Intel_SetForceTarget(race, force, 2, 6, 0);     // same system
    CHECK(f->TravelTime == expect && f->TravelTime >= 20 && f->TravelTime <= 29 && f->Body == 6);
    expect = rnd(seed, 0x14, 0x32);
    ds.System_RandSeed = seed;
    Intel_SetForceTarget(race, force, 5, 1, 0);     // other system
    CHECK(f->TravelTime == expect && f->TravelTime >= 50 && f->TravelTime <= 69 && f->System == 5);

    // DispatchForce: arrival timer Random(100 - Speed) + 5, NextMission stored.
    Intel_Race(race)->Speed = 40;
    // SetForceTarget draws first (different system), then the timer.
    ds.System_RandSeed = 2000;
    System_Random(0x14);
    int timer = System_Random(60) + 5;
    ds.System_RandSeed = 2000;
    Intel_DispatchForce(race, force, 6, 2, 0, 3);
    CHECK(f->Timer == timer && f->Timer >= 5 && f->Timer <= 64);
    CHECK(f->NextMission == 3 && f->Active == 2 && f->Mission != 3);
    // DispatchForceNow also starts the mission at once.
    Intel_DispatchForceNow(race, force, 7, 1, 0, 4, 0);
    CHECK(f->Mission == 4 && f->NextMission == 4 && f->System == 7);

    // ForceLands: units join the garrison of the target body; Active := 0.
    TBody *b = body(2, 3);
    b->Owner = 5;
    for (int k = 0; k < 8; k++)
        b->Stores[k] = 100;
    TRaceForce *g = Intel_Force(5, 1);
    g->System = 2; g->Body = 3; g->Moon = 0; g->Active = 1; g->Mission = 2;
    for (int k = 0; k < 8; k++)
        g->Units[k] = (int16_t)(k + 1);
    Intel_ForceLands(5, 1);
    for (int k = 0; k < 8; k++)
        CHECK(b->Stores[k] == 100 + k + 1);
    CHECK(g->Active == 0);
    // Not the owner: the units are lost, Active still cleared.
    b->Owner = 6;
    g->Active = 1;
    Intel_ForceLands(5, 1);
    CHECK(b->Stores[0] == 101 && g->Active == 0);

    // SpaceBattleWon: a Status-2 race force at the battle lands at home or retreats there.
    TRace *r = Intel_Race(5);
    r->Status = 2;
    r->ForceCount = 2;
    r->HomeSystem = 2;
    r->HomeBody = 3;
    b->Owner = 5;
    for (int k = 0; k < 8; k++)
        b->Stores[k] = 0;
    TRaceForce *f1 = Intel_Force(5, 1), *f2 = Intel_Force(5, 2);
    for (TRaceForce *x = f1; x <= f2; x++) {
        x->Active = 1; x->Mission = 2; x->System = 2; x->Body = 3; x->Moon = 0;
        x->Units[0] = 5;
    }
    f2->Body = 4;            // force 2 is at another body of the system
    Intel_SpaceBattleWon(2, 4);
    CHECK(f2->Active == 2 && f2->System == 2 && f2->Body == 3);   // sent home
    CHECK(f1->Active == 1);                                        // not there
    Intel_SpaceBattleWon(2, 3);
    CHECK(f1->Active == 0 && b->Stores[0] == 5);                   // lands on its home body
    r->Status = 4;           // other statuses are ignored
    f1->Active = 1;
    Intel_SpaceBattleWon(2, 3);
    CHECK(f1->Active == 1);

    // ClearForce zeroes the record, EliminateRace the race.
    Intel_ClearForce(5, 2);
    for (size_t i = 0; i < sizeof(TRaceForce); i++)
        CHECK(((uint8_t *)f2)[i] == 0);
    b->Stores[0] = 7;
    b->Colony = 1;
    Intel_EliminateRace(5);
    CHECK(r->Status == -1 && r->ForceCount == 0 && r->Speed == 0 && r->HomeSystem == 0);
    CHECK(r->Credits == 0 && r->_20[0] == -1 && r->_20[5] == -1);
    CHECK(b->Owner == 0 && b->Colony == 0 && b->Stores[0] == 0);
    CHECK(f1->Mission == 0 && f1->Units[0] == 0);
    CHECK(!Intel_IsRaceActive(5));

    // ClaimBodyForRace: population, attitude, race 7 garrison from the tables.
    seed = 77;
    ds.System_RandSeed = seed;
    Intel_ClaimBodyForRace(7, 3, 2, 0);
    TBody *c = body(3, 2);
    ds.System_RandSeed = seed;
    uint32_t pop = (uint32_t)System_Random(3000) + 2000;
    CHECK(c->Owner == 7 && c->Colony == 1 && c->Population == (int32_t)pop);
    CHECK(c->Resistance == 20 && c->TechLevel == 3 && c->Attitude == 30);
    for (int k = 1; k <= 8; k++) {
        uint8_t base = ds.Intel_Race7Stores[k - 1], range = ds.Intel_Race7StoresRange[k - 1];
        int32_t v = c->Stores[k - 1];
        CHECK(v >= base && v < base + (range ? range : 1));
    }
    Intel_ClearBody(3, 2, 0, false);
    CHECK(c->Owner == 0 && c->Colony == 0 && c->Population == (int32_t)pop);
    Intel_ClearBodyFull(3, 2, 0);
    CHECK(c->Population == 0 && c->Attitude == 0);

    // Groups: SetGroupTarget statuses, SendGroupHome, DisbandGroup with the planet force merge.
    ds.Game_Groups = game_alloc(2 * 32 * sizeof(TGroup));
    SDL_memset(ds.Game_Groups, 0, 2 * 32 * sizeof(TGroup));
    ds.Game_GroupCount = 3;
    ds.Game_PlanetForceCount = 1;
    ds.Game_GroupList = 0;
    ds.Game_ShownGroupCount = 3;
    for (int i = 1; i <= 3; i++) {
        TGroup *gr = Groups_At(0, i);
        gr->Kind = 1; gr->System = 2; gr->Planet = 3; gr->Moon = 0; gr->Status = 1;
        gr->Units[0][0].Count = (int16_t)(10 * i);
        gr->Units[0][0].Weapons[0] = (int16_t)i;
        gr->Units[1][3].Weapons[3] = 5;
    }
    TGroup *pf = Groups_At(1, 1);
    pf->Kind = 5; pf->System = 2; pf->Planet = 3; pf->Moon = 0;
    pf->Units[0][0].Count = 1;
    pf->Units[1][3].Weapons[3] = 1;
    TGroup *g1 = Groups_At(0, 1);
    seed = 4000;
    expect = rnd(seed, 0x14, 0x50);
    ds.System_RandSeed = seed;
    Intel_SetGroupTarget(1, 2, 5, 0);                 // same system, other planet
    CHECK(g1->Status == 4 && g1->Leg1 == 0 && g1->Jump == 0 && g1->Planet == 5);
    CHECK(g1->Leg2 >= 0x96 && g1->Leg2 <= 0x96 + 49);
    Intel_SetGroupTarget(1, 3, 1, 0);                 // other system
    CHECK(g1->Status == 6 && g1->Jump == 1 && g1->System == 3);
    CHECK(g1->Leg1 >= 200 && g1->Leg1 <= 249 && g1->Leg2 >= 200 && g1->Leg2 <= 249);
    Intel_SendGroupHome(1);
    CHECK(g1->System == 1 && g1->Planet == 5 && g1->Moon == 0 && g1->Status == 6);
    Intel_SetGroupTarget(1, 1, 5, 0);                 // unchanged target: nothing happens
    CHECK(g1->Status == 6);
    (void)expect;

    // DisbandGroup(0, 2): group 2 merges into the planet force at 2:3:0, groups close up.
    Intel_DisbandGroup(0, 2);
    CHECK(ds.Game_GroupCount == 2 && ds.Game_ShownGroupCount == 2);
    CHECK(ds.Game_SelGroup == 0 && ds.Game_SelGroup0 == 0 && ds.Game_SelGroup1 == 0);
    CHECK(pf->Units[0][0].Count == 1 + 20 && pf->Units[0][0].Weapons[0] == 2);
    CHECK(pf->Units[1][3].Weapons[3] == 1 + 5);
    CHECK(Groups_At(0, 2)->Units[0][0].Count == 30);   // old group 3 moved up
    // A planet-force disband only shifts and counts.
    Intel_DisbandGroup(1, 1);
    CHECK(ds.Game_PlanetForceCount == 0 && ds.Game_ShownGroupCount == 2);
    // SpaceBattleLost: stationary groups at Earth are disbanded, others sent home.
    Groups_At(0, 1)->System = 1; Groups_At(0, 1)->Planet = 5; Groups_At(0, 1)->Status = 1;
    Groups_At(0, 2)->System = 2; Groups_At(0, 2)->Planet = 3; Groups_At(0, 2)->Status = 1;
    ds.Game_ShownGroupCount = 2;
    Intel_SpaceBattleLost(2, 3);
    CHECK(ds.Game_GroupCount == 2 && Groups_At(0, 2)->System == 1 && Groups_At(0, 2)->Status == 6);
    Intel_SpaceBattleLost(1, 5);   // group 1 (stationary at Earth) goes; group 2 is travelling
    CHECK(ds.Game_GroupCount == 1);

    // Fix (game/fixes.h): with the planet forces shown (2 of them), disbanding
    // fleet 1 of 3 shifts all three fleets. The original shifts up to the
    // shown count: fleet 2 is duplicated and fleet 3 lost.
    for (int fix = 0; fix <= 1; fix++) {
        ds.Game_GroupCount = 3;
        ds.Game_PlanetForceCount = 2;
        ds.Game_GroupList = 1;
        ds.Game_ShownGroupCount = 2;
        for (int i = 1; i <= 3; i++) {
            TGroup *gr = Groups_At(0, i);
            gr->Kind = 2; gr->System = 3; gr->Planet = (uint8_t)i; gr->Status = 1;
        }
        fixes.disband_list_count = fix;
        Intel_DisbandGroup(0, 1);
        CHECK(ds.Game_GroupCount == 2 && ds.Game_ShownGroupCount == 2);
        CHECK(Groups_At(0, 1)->Planet == 2);
        CHECK(Groups_At(0, 2)->Planet == (fix ? 3 : 2));
    }

    // Ideas: scheduling needs state 0 and no timer; grant / offer change the state.
    TProduct *p = &ds.Research_Products[9];
    p->State = 0;
    ds.Game_IdeaTimer[9] = -1;
    seed = 321;
    expect = rnd(seed, 20, 100);
    ds.System_RandSeed = seed;
    Intel_ScheduleIdea(10, 100, 20);
    CHECK(ds.Game_IdeaTimer[9] == expect);
    ds.System_RandSeed = seed;
    Intel_ScheduleIdea(10, 5, 5);
    CHECK(ds.Game_IdeaTimer[9] == expect);          // already scheduled
    Intel_OfferInventionForAnalysis(10);
    CHECK(p->State == 3);
    Intel_GrantInvention(10);
    CHECK(p->State == 3);                           // only from state 0
    p->State = 0;
    p->Progress = 55;
    Intel_GrantInvention(10);
    CHECK(p->State == 5 && p->Progress == 0);

    // FirstContact: Status by race, message once, story timers from fixed seeds.
    reset();
    for (int rc = 2; rc <= 12; rc++)
        Intel_Race(rc)->Status = 0;
    ds.Research_Products[7].State = 0;
    ds.Game_IdeaTimer[7] = -1;
    seed = 555;
    ds.System_RandSeed = seed;
    messages = 0;
    Intel_FirstContact(2, "Hello");
    CHECK(Intel_Race(2)->Status == 4 && messages > 0);
    CHECK(ds.Game_IdeaTimer[7] >= 0x14 && ds.Game_IdeaTimer[7] <= 0x14 + 0x13);
    uint16_t t1 = (uint16_t)ds.Intel_Race2Timer1, t2 = (uint16_t)ds.Intel_Race2Timer2;
    uint16_t t3 = (uint16_t)ds.Intel_Race2Timer3, t4 = (uint16_t)ds.Intel_Race2Timer4;
    CHECK(t1 >= 0x28A && t1 < 0x28A + 0x46);
    CHECK(t2 >= t1 + 0x2D0 && t2 < t1 + 0x2D0 + 0x32);
    CHECK(t3 >= t2 + 0x4C4 && t3 < t2 + 0x4C4 + 100);
    CHECK(t4 >= t3 + 100 && t4 < t3 + 100 + 0x14);
    messages = 0;
    ds.System_RandSeed = seed;
    Intel_FirstContact(2, "Hello");               // second contact: nothing
    CHECK(messages == 0 && ds.System_RandSeed == seed);
    Intel_FirstContact(0, "Hello");               // race 0: nothing
    CHECK(messages == 0);
    Intel_FirstContact(3, "Hello");
    CHECK(Intel_Race(3)->Status == 2);
    Intel_FirstContact(4, "Hello");
    CHECK(Intel_Race(4)->Status == 6);
    CHECK(ds.Game_CurrentScreen == 0x22 && ds.AlienTalk_Race == 5 && ds.Game_ScreenSwitch);
    CHECK(ds.Intel_Race4Timer >= 0x5DC && ds.Intel_Race4Timer < 0x5DC + 500);
    for (int rc = 7; rc <= 10; rc++) {
        Intel_FirstContact((int16_t)rc, "Hello");
        CHECK(Intel_Race(rc)->Status == 2);
    }
    CHECK(ds.Intel_Race7to10Timer >= 200 && ds.Intel_Race7to10Timer < 250);
    Intel_FirstContact(5, "Hello");
    CHECK(Intel_Race(5)->Status == 4);
    CHECK(ds.Intel_Race5Timer >= 0x14 && ds.Intel_Race5Timer < 0x28);
    Intel_FirstContact(0xB, "Hello");
    CHECK(Intel_Race(0xB)->Status == 6 && ds.AlienTalk_Race == 10);
    seed = 888;
    ds.System_RandSeed = seed;
    Intel_FirstContact(0xC, "Hello");
    CHECK(Intel_Race(0xC)->Status == 2);
    CHECK(ds.Intel_LaunchRace == 12 && ds.Intel_LaunchForce == 3 && ds.Intel_LaunchMission == 2);
    CHECK(ds.Intel_LaunchTimer >= 1000 && ds.Intel_LaunchTimer < 1200);
    CHECK(ds.Races_Races[10].Status == 2);

    // Intel reports: one message per owned colony (kinds 1, 3), per active force (2), one for weapons (4).
    reset();
    Intel_ClaimBodyForRace(7, 3, 2, 0);
    Intel_ClaimBodyForRace(7, 3, 3, 0);
    body(3, 3)->MainPlanet = 1;
    messages = 0;
    Intel_ShowRaceIntel(7, 1);
    CHECK(messages > 0);
    Intel_ShowRaceIntel(7, 3);
    Intel_ShowRaceIntel(7, 4);
    Intel_Force(7, 1)->Mission = 1;
    Intel_Force(7, 1)->Active = 1;
    Intel_Race(7)->ForceCount = 1;
    Intel_ShowRaceIntel(7, 2);

    test_spy_reports();

    // Intel_Init installs the group hook.
    Intel_Init();
    CHECK(ds.Groups_IntelHook == Intel_DisbandGroup);

    // AlienTalk: scene / talk switches and the question menu mouse mapping.
    AlienTalk_StartAlienScene(7);
    CHECK(ds.Game_CurrentScreen == 0x21 && ds.AlienTalk_Scene == 7 && ds.Game_ScreenSwitch);
    AlienTalk_StartAlienTalk(11);
    CHECK(ds.Game_CurrentScreen == 0x22 && ds.AlienTalk_Race == 11);
    CHECK(ds.AlienTalk_RaceStr.len == 2 && ds.AlienTalk_RaceStr.s[0] == '1');
    AlienTalk_SetupTalkHotspots();
    CHECK(ds.Mouse_HotspotCount == 1 && ds.Mouse_Hotspots[0].X == 6 && ds.Mouse_Hotspots[0].Y == 0x86);
    CHECK(ds.Mouse_Hotspots[0].W == 0xD2 && ds.Mouse_Hotspots[0].H == 0x3F);
    CHECK(ds.Mouse_Hotspots[0].Caption.len == 7);
    CHECK(ds.AlienTalk_HoverCol == 0xFFFF && ds.AlienTalk_HoverRow == -1);

    printf(ok ? "intel OK\n" : "intel FAILED\n");
    return ok ? 0 : 1;
}
