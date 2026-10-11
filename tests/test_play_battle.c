// Battle drivers (tests/play/ui_battle.c, docs/playthrough-battles.md): the
// game is put into a battle situation with play_test_setup, the way its own
// code starts battles, and the battle is then fought by clicks only.
//
//   test_play_battle DATA                     the checks (ctest)
//   test_play_battle DATA NAME [hold|counter|charge] [FIRST LAST]
//                    [own=H,SF,D,C/Tr,T,Air,ML] [order=mrta|keep]
//                    [kit=noplasma]           one scenario over seeds FIRST..LAST
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/dseg.h"
#include "game/groups.h"
#include "game/intel.h"
#include "platform/fs.h"
#include "play/play.h"
#include "play/ui.h"
#include "play/ui_battle.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

// Units of a side: space Hunter, Starfighter, Destroyer, Cruiser; ground
// Trooper, Battle tank, Aircraft, Missile launcher (the slot order of groups
// and race forces).
typedef struct {
    int space[4];
    int ground[4];
} Force;

// Loadouts per unit: lasers, twins, missiles, plasma (walkthrough.md
// "Weapons and loadouts"). Early Hunters have 2 lasers only (before Twin
// laser gun), early Starfighters 4 twins (before Missile); Destroyers have
// no Plasma gun before Rigel 1 (act 5).
typedef enum { KIT_FULL, KIT_EARLY, KIT_NO_PLASMA } Kit;
static const int space_kit[3][4][4] = {
    {{2, 1, 0, 0}, {0, 4, 1, 0}, {0, 2, 4, 1}, {0, 0, 6, 3}},
    {{2, 0, 0, 0}, {0, 4, 0, 0}, {0, 2, 4, 1}, {0, 0, 6, 3}},
    {{2, 1, 0, 0}, {0, 4, 1, 0}, {0, 2, 4, 0}, {0, 0, 6, 0}},
};
static const int ground_kit[4][4] = {{1, 0, 0, 0}, {3, 1, 0, 0}, {0, 4, 1, 0}, {0, 0, 5, 0}};

static void fill_slots(TGroup *g, const Force *f, Kit kit, bool space, bool ground)
{
    for (int t = 0; t < 4; t++) {
        if (space) {
            g->Units[0][t].ShipType = (int16_t)f->space[t];
            for (int w = 0; w < 4; w++)
                g->Units[0][t].Weapons[w] = (int16_t)(f->space[t] * space_kit[kit][t][w]);
        }
        if (ground) {
            g->Units[1][t].ShipType = (int16_t)f->ground[t];
            for (int w = 0; w < 4; w++)
                g->Units[1][t].Weapons[w] = (int16_t)(f->ground[t] * ground_kit[t][w]);
        }
    }
}

// A new Army group in the fleet list, as the New Group screen leaves it.
static TGroup *add_army(const char *name, int system, int planet, int moon, int status)
{
    ds.Game_GroupCount++;
    ds.Game_ShownGroupCount = ds.Game_GroupCount;
    TGroup *g = Groups_At(0, ds.Game_GroupCount);
    memset(g, 0, sizeof *g);
    g->Kind = 1;
    g->Name.len = (uint8_t)strlen(name);
    memcpy(g->Name.s, name, g->Name.len);
    g->System = (uint8_t)system;
    g->Planet = (uint8_t)planet;
    g->Moon = (uint8_t)moon;
    g->Status = (uint8_t)status;
    return g;
}

static TBody *body(int system, int idx)
{
    FarPtr p = ds.Galaxy_Bodies[system - 1];
    return far_host(far_ptr((uint16_t)(p >> 16), (uint16_t)((uint16_t)p + idx * 0x41 - 0x41)));
}

static TBody *body_at(int system, int planet, int moon)
{
    if (moon == 0)
        return body(system, planet);
    const TMoonList *ml = (const TMoonList *)far_host(ds.Galaxy_MoonLists[system - 1]) + (planet - 1);
    return body(system, ml->Moons[moon - 1]);
}

static int moon_count(int system, int planet)
{
    return ((const TMoonList *)far_host(ds.Galaxy_MoonLists[system - 1]) + (planet - 1))->Count;
}

// --- Scenarios ------------------------------------------------------------------
//
// Defence: the game's waves (sim.c:1484-1510, 1565-1584) are race forces
// dispatched to New Earth (1/5/0) with mission 2; after arriving they wait
// Timer hours, then CheckEnemyAttacks (sim.c:1263-1328) shows "Your colony is
// under attack" and opens screen 29; a lost space battle leads to the unit
// assignment (19) and the ground battle (32). The setup puts the force at
// New Earth with one hour to go, as after the ATTENTION box. The defenders
// are an Army group "Home Guard" docked at New Earth (space units) and New
// Earth's planet force (ground units).
//
// Invasion: the player's way in (launch.c:207-238) is an Army group at a
// colony with an intel level of 40 or more, selected on the star map; GROUND
// WAR starts the space battle against everything in orbit, and a win leads
// to the unit assignment and the ground battle. The setup puts an Army group
// "Strike" in orbit of the target and gives what the policy's earlier steps
// would have: the intel (a spy satellite's work), the map knowledge
// (Hyperspace drive, the system explored), the race at war, and the waves
// that race has already sent (spent) taken out of its forces.

typedef struct {
    const char *name;
    bool invasion;
    int race, force;           // defence: the attacking force
    int system, planet, moon;  // invasion: the target
    const char *system_name, *planet_name;
    int spent;                 // invasion: forces 1..spent of the race are gone
    int back;                  // invasion: force `back` came home from a lost wave: a
                               // fifth of its ships (SpWar_ApplySpaceLosses), all its troops
    Force own;                 // Home Guard + planet force, or Strike
    Kit kit;
    bool ground_only;          // no own ships (defence) / no enemy ships (invasion)
} Scenario;

typedef struct {
    bool space_won, ground_fought, ground_won;
    UiLosses space, ground;
    int exit_code;
    bool failed;
} Outcome;

static const Scenario scenarios[] = {
    // J8: wave 1 (Morgrul force 1: 6H, 3Tr) against 12 Hunters with 2 lasers.
    {"wave1", .race = 3, .force = 1, .own = {{12}}, .kit = KIT_EARLY},
    {"wave1-8h", .race = 3, .force = 1, .own = {{8}}, .kit = KIT_EARLY},
    // M8: wave 2 (25H, 10Tr) against 24H (2L+1T), 16SF (4 twins), 20 tanks.
    {"wave2", .race = 3, .force = 2, .own = {{24, 16}, {0, 20}}, .kit = KIT_EARLY},
    // M12: wave 3 (20H 10SF; 15Tr 2T) against 30H, 24SF, 30 tanks.
    {"wave3", .race = 3, .force = 3, .own = {{30, 24}, {0, 30}}},
    // L7: the Lisonian wave (force 1: 20H 20SF 3D; 30Tr 15T 5Air) against 30H 30SF 5D, 40 tanks.
    {"lisonian", .race = 7, .force = 1, .own = {{30, 30, 5}, {0, 40}}},
    // X6: Earth wave 1 (force 3: 80SF 12D 3C; 30Tr 30T 30Air) against 20SF 4D 2C, 40 tanks, 20 ML.
    {"earth1", .race = 12, .force = 3, .own = {{0, 20, 4, 2}, {0, 40, 0, 20}}},
    // Ground defence alone (no ships at New Earth): the wave's troops land.
    {"wave1-ground", .race = 3, .force = 1, .own = {.ground = {0, 10}}, .ground_only = true},
    {"wave2-ground", .race = 3, .force = 2, .own = {.ground = {0, 20}}, .ground_only = true},
    {"wave3-ground", .race = 3, .force = 3, .own = {.ground = {0, 30}}, .ground_only = true},
    {"lisonian-ground", .race = 7, .force = 1, .own = {.ground = {0, 40}}, .ground_only = true},
    {"earth1-ground", .race = 12, .force = 3, .own = {.ground = {0, 40, 0, 20}}, .ground_only = true},
    // M18: Mirach 3 (Morgruls; waves 1-3 spent) with 20H 30SF 5D, 60 tanks.
    {"mirach", .invasion = true, .race = 3, .system = 3, .planet = 3, .system_name = "Mirach",
     .planet_name = "Mirach 3", .spent = 3, .own = {{20, 30, 5}, {0, 60}}},
    // ... with the 90 tanks docs/playthrough-battles.md recommends.
    {"mirach-90", .invasion = true, .race = 3, .system = 3, .planet = 3, .system_name = "Mirach",
     .planet_name = "Mirach 3", .spent = 3, .own = {{20, 30, 5}, {0, 90}}},
    {"mirach-ground", .invasion = true, .race = 3, .system = 3, .planet = 3, .system_name = "Mirach",
     .planet_name = "Mirach 3", .spent = 3, .own = {{20, 30, 5}, {0, 40}}, .ground_only = true},
    // L13: Orionis 3 (Druedians) and Lyrae 5 (Hirachi) with 30H 30SF 5D, 60T 20Air;
    // Lyrae 7 (Lisonians; their wave spent) with 50H 50SF 8D, 60T 20Air.
    {"orionis", .invasion = true, .race = 10, .system = 5, .planet = 3, .system_name = "Orionis",
     .planet_name = "Orionis 3", .own = {{30, 30, 5}, {0, 60, 20}}},
    {"lyrae5", .invasion = true, .race = 9, .system = 6, .planet = 5, .system_name = "Lyrae",
     .planet_name = "Lyrae 5", .own = {{30, 30, 5}, {0, 60, 20}}},
    {"lyrae7", .invasion = true, .race = 7, .system = 6, .planet = 7, .system_name = "Lyrae",
     .planet_name = "Lyrae 7", .spent = 1, .own = {{50, 50, 8}, {0, 60, 20}}},
    // ... after the Lisonian wave was beaten in space at New Earth: its troops
    // went home with the fifth of its ships that survived.
    {"lyrae7-back", .invasion = true, .race = 7, .system = 6, .planet = 7, .system_name = "Lyrae",
     .planet_name = "Lyrae 7", .back = 1, .own = {{50, 50, 8}, {0, 40, 30}}},
    // X9: Earth with 50H 50SF 10D 6C, 60T 20Air 50ML.
    {"earth", .invasion = true, .race = 12, .system = 8, .planet = 3, .system_name = "Sun",
     .planet_name = "Earth", .own = {{50, 50, 10, 6}, {0, 60, 20, 50}}},
    {"lyrae7-ground", .invasion = true, .race = 7, .system = 6, .planet = 7, .system_name = "Lyrae",
     .planet_name = "Lyrae 7", .spent = 1, .own = {{50, 50, 8}, {0, 60, 20}}, .ground_only = true},
    {"orionis-ground", .invasion = true, .race = 10, .system = 5, .planet = 3, .system_name = "Orionis",
     .planet_name = "Orionis 3", .own = {{30, 30, 5}, {0, 60, 20}}, .ground_only = true},
    {"earth-ground", .invasion = true, .race = 12, .system = 8, .planet = 3, .system_name = "Sun",
     .planet_name = "Earth", .own = {{50, 50, 10, 6}, {0, 60, 20, 50}}, .ground_only = true},
};

static void log_force(const char *who, const Force *f)
{
    play_log("%s: %dH %dSF %dD %dC / %dTr %dT %dAir %dML", who, f->space[0], f->space[1], f->space[2],
             f->space[3], f->ground[0], f->ground[1], f->ground[2], f->ground[3]);
}

static void setup_defence(const Scenario *s)
{
    Intel_Race((int16_t)s->race)->Status = 2;  // at war, as the wave's arrival leaves it
    TRaceForce *fc = Intel_Force((int16_t)s->race, (int16_t)s->force);
    fc->System = 1;
    fc->Body = 5;
    fc->Moon = 0;
    fc->Active = 1;
    fc->_03 = 0;
    fc->Mission = 2;
    fc->NextMission = 2;
    fc->Timer = 1;
    if (!s->ground_only)
        fill_slots(add_army("Home Guard", 1, 5, 0, 1), &s->own, s->kit, true, false);
    fill_slots(Groups_At(1, 1), &s->own, s->kit, false, true);  // New Earth forces
}

static void setup_invasion(const Scenario *s)
{
    ds.Research_Products[0xD].State = 5;  // Hyperspace drive: the system list
    ds.Galaxy_SystemStatus[s->system - 1] = 1;
    ds.Galaxy_PlanetStatus[s->system - 1][s->planet - 1] = 1;
    TRace *r = Intel_Race((int16_t)s->race);
    r->Status = 2;
    for (int f = 1; f <= s->spent; f++)
        memset(Intel_Force((int16_t)s->race, (int16_t)f)->Units, 0, sizeof r->Forces[0].Units);
    if (s->back)
        for (int t = 0; t < 4; t++)
            Intel_Force((int16_t)s->race, (int16_t)s->back)->Units[t] /= 5;
    TBody *b = body_at(s->system, s->planet, s->moon);
    b->InfoLevel = 40;
    if (s->ground_only) {
        for (int m = 0; m <= moon_count(s->system, s->planet); m++)
            for (int t = 0; t < 4; t++)
                body_at(s->system, s->planet, m)->Stores[t] = 0;
        for (int race = 2; race <= 12; race++)
            for (int f = 1; f <= Intel_Race((int16_t)race)->ForceCount; f++) {
                TRaceForce *fc = Intel_Force((int16_t)race, (int16_t)f);
                for (int t = 0; t < 4 && fc->System == s->system && fc->Body == s->planet; t++)
                    fc->Units[t] = 0;
            }
    }
    fill_slots(add_army("Strike", s->system, s->planet, s->moon, 2), &s->own, s->kit, true, true);
}

static void setup(void *arg)
{
    const Scenario *s = arg;
    if (s->invasion)
        setup_invasion(s);
    else
        setup_defence(s);
}

// The tactic and the formation (rear to front) of the runs.
static UiTactic tactic = UI_TACTIC_COUNTER;
static bool keep_order;  // keep the game's pre-filled slots
static int rear_to_front[4] = {UI_LAUNCHER, UI_TROOPER, UI_TANK, UI_AIRCRAFT};
static char order_name[8] = "mrta";  // the same as letters: r trooper, t tank, a aircraft, m launcher
static const Scenario *current;
static Outcome outcome;

static void fight(void *unused)
{
    (void)unused;
    const Scenario *s = current;
    Outcome *o = &outcome;
    ui_new_game();
    ui_hire("FIGHTERS", "Kyle Douglas");
    play_test_setup(setup, (void *)s);
    log_force(s->invasion ? "strike" : "home guard and planet force", &s->own);
    if (s->invasion) {
        if (!ui_invade(s->system_name, s->planet_name, "Strike"))
            play_fail("no GROUND WAR");
    } else {
        // Idle on the message screen with the clock running until the attack.
        ui_toolbar("MESSAGES");
        if (!ui_wait_screen(29, 3000))
            play_fail("no attack");
    }
    o->space_won = ui_space_battle(&o->space);
    // Defending, a lost space battle lands the enemy; attacking, a won one
    // lands the player (main_program.c:1239-1244).
    if (o->space_won == s->invasion) {
        if (!ui_wait_screen(19, 3000))
            play_fail("no unit assignment");
        o->ground_fought = true;
        ui_assign_units(keep_order ? nullptr : rear_to_front);
        o->ground_won = ui_ground_battle(tactic, &o->ground);
    }
    play_wait(100);
    ui_wait_idle(5000);  // closing the boxes that follow
    play_dump("after");
    play_quit();
}

static Outcome run(const Scenario *s, uint32_t seed)
{
    current = s;
    outcome = (Outcome){};
    PlayResult r = play_run(seed, fight, nullptr, nullptr);
    outcome.exit_code = r.exit_code;
    outcome.failed = r.failed;
    if (r.failed)
        printf("%s seed %u: FAILED: %s\n", s->name, seed, r.failure);
    return outcome;
}

// The battle that decides the scenario: the ground battle if there was one.
static bool won(const Outcome *o)
{
    if (o->ground_fought)
        return o->ground_won;
    return o->space_won;
}

static const Scenario *find(const char *name)
{
    for (size_t i = 0; i < SDL_arraysize(scenarios); i++)
        if (!strcmp(scenarios[i].name, name))
            return &scenarios[i];
    return nullptr;
}

static const char *tactic_names[] = {"hold", "counter", "charge"};

static int sweep(int argc, char *argv[])
{
    const Scenario *found = find(argv[2]);
    if (!found) {
        printf("no scenario %s\n", argv[2]);
        return 2;
    }
    Scenario s = *found;
    uint32_t first = 1, last = 10;
    for (int i = 3; i < argc; i++) {
        if (!strncmp(argv[i], "own=", 4)) {
            Force *f = &s.own;
            sscanf(argv[i] + 4, "%d,%d,%d,%d/%d,%d,%d,%d", &f->space[0], &f->space[1], &f->space[2],
                   &f->space[3], &f->ground[0], &f->ground[1], &f->ground[2], &f->ground[3]);
        } else if (argv[i][0] >= '0' && argv[i][0] <= '9') {
            first = (uint32_t)strtoul(argv[i], nullptr, 10);
            last = i + 1 < argc ? (uint32_t)strtoul(argv[++i], nullptr, 10) : first;
        } else {
            if (!strcmp(argv[i], "kit=noplasma"))
                s.kit = KIT_NO_PLASMA;
            if (!strcmp(argv[i], "order=keep")) {
                keep_order = true;
                strcpy(order_name, "keep");
            } else if (!strncmp(argv[i], "order=", 6) && strlen(argv[i]) == 10) {
                for (int k = 0; k < 4; k++)
                    rear_to_front[k] = (int)(strchr("rtam", argv[i][6 + k]) - "rtam") + 1;
                memcpy(order_name, argv[i] + 6, 4);
            }
            for (int t = 0; t < 3; t++)
                if (!strcmp(argv[i], tactic_names[t]))
                    tactic = (UiTactic)t;
        }
    }
    int wins = 0, n = 0, space_wins = 0, grounds = 0;
    for (uint32_t seed = first; seed <= last; seed++) {
        Outcome o = run(&s, seed);
        n++;
        wins += won(&o);
        space_wins += o.space_won;
        grounds += o.ground_fought;
        const UiLosses *a = &o.space, *g = &o.ground;
        printf("RESULT %s %s/%s seed %u: space %s (lost %d/%d/%d/%d, killed %d/%d/%d/%d)", s.name,
               tactic_names[tactic], order_name, seed, o.space_won ? "won" : "lost", a->own[0], a->own[1],
               a->own[2], a->own[3], a->enemy[0], a->enemy[1], a->enemy[2], a->enemy[3]);
        if (o.ground_fought)
            printf(", ground %s (lost %d/%d/%d/%d, killed %d/%d/%d/%d)", o.ground_won ? "won" : "lost",
                   g->own[0], g->own[1], g->own[2], g->own[3], g->enemy[0], g->enemy[1], g->enemy[2],
                   g->enemy[3]);
        printf("%s\n", o.failed ? " FAILED" : "");
        fflush(stdout);
    }
    printf("SUMMARY %s %s/%s: %d/%d won (space won %d, ground battles %d)\n", s.name, tactic_names[tactic],
           order_name, wins, n, space_wins, grounds);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_EVENTS)) {
        printf("SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (argc > 2) {
        int rc = sweep(argc, argv);
        SDL_Quit();
        return rc;
    }

    // Wave 1 beaten in space by the Home Guard.
    Outcome o = run(find("wave1"), 12345);
    CHECK(!o.failed && o.space_won && !o.ground_fought);
    // With no ships at home, wave 1's troopers land and the planet force's
    // tanks beat them.
    o = run(find("wave1-ground"), 12345);
    CHECK(!o.failed && !o.space_won && o.ground_fought && o.ground_won);
    // The Mirach 3 invasion: GROUND WAR on the star map, space battle,
    // assignment, ground battle.
    o = run(find("mirach-90"), 12345);
    CHECK(!o.failed && o.space_won && o.ground_fought && o.ground_won);

    SDL_Quit();
    return ok ? 0 : 1;
}
