// sim: the hourly simulation from the game's initial data: date carry,
// research and production arithmetic, and a regression checksum of the
// simulated state after a fixed number of hours with a fixed RandSeed.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/cargo.h"
#include "game/dseg.h"
#include "game/main_program.h"
#include "game/fixes.h"
#include "game/gfx.h"
#include "game/groups.h"
#include "game/intel.h"
#include "game/message.h"
#include "game/sim.h"
#include "game/system.h"
#include "game/textfile.h"
#include "platform/fs.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static int messages;

static bool wait_retrace(void) { return true; }
// Message boxes wait for a key: answer at once.
static bool key_pressed(void) { messages++; return true; }
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
    ds.Game_ProductionRate = 10;
    gfx_wait_retrace = wait_retrace;
    TextFile_KeyPressed = key_pressed;
    TextFile_ReadKey = read_key;
    messages = 0;
}

static void set_date(uint16_t y, uint16_t m, uint16_t d, uint16_t h)
{
    ds.Game_Year = y;
    ds.Game_Month = m;
    ds.Game_Day = d;
    ds.Game_Hour = h;
}

static bool date_is(uint16_t y, uint16_t m, uint16_t d, uint16_t h)
{
    return ds.Game_Year == y && ds.Game_Month == m && ds.Game_Day == d && ds.Game_Hour == h;
}

// One game hour, as Main runs it (minus Main's own calls in between).
static void hour(void)
{
    Sim_AdvanceHour();
    Sim_ProcessStoryTimers();
    Sim_ProgressResearch();
    Sim_ProgressProduction();
    if (ds.Game_Hour == 0)
        Sim_ProcessAllBodyEvents();
    Sim_GrowStaffExperience();
    Sim_UpdateBuildings();
    Sim_UpdateColonySetups();
    Sim_MineAllBodies();
    Sim_ProcessTravellingGroups();
    Sim_CheckEnemyAttacks();
    Sim_ProcessPubTimers();
}

static void test_date(void)
{
    reset();
    set_date(2100, 12, 30, 22);
    Sim_AdvanceHour();
    CHECK(date_is(2100, 12, 30, 23));
    Sim_AdvanceHour();  // 23 -> 0 carries into the day, the month and the year
    CHECK(date_is(2101, 1, 1, 0));

    set_date(2100, 6, 15, 7);
    for (int i = 0; i < 24; i++)
        Sim_AdvanceHour();
    CHECK(date_is(2100, 6, 16, 7));
    // 30-day months, 12-month years: 8640 hours a year.
    for (int i = 0; i < 24 * 30 * 12; i++)
        Sim_AdvanceHour();
    CHECK(date_is(2101, 6, 16, 7));
    set_date(2100, 2, 29, 23);
    Sim_AdvanceHour();
    CHECK(date_is(2100, 2, 30, 0));  // no short months
}

static void clear_products(void)
{
    for (int k = 0; k < 35; k++) {
        ds.Research_Products[k].State = 0;
        ds.Research_Products[k]._1D = 0;
    }
}

static void test_research(void)
{
    reset();
    clear_products();
    TProduct *p = &ds.Research_Products[0];  // invention 1
    ds.Game_StaffExp[3] = 10;
    for (int j = 0; j < 4; j++) {
        p->Skills[j] = 3;
        ds.Game_DevSkill[j] = 3;
    }
    p->State = 2;
    p->DevTime = 1000;
    p->Progress = 5000;
    ds.Game_CurResearch = 1;
    // No skill shortfall: Progress -= StaffExp[4] * 10000 / DevTime each hour.
    Sim_ProgressResearch();
    CHECK(p->Progress == 4900);
    p->State = 4;  // analysis progresses the same way
    Sim_ProgressResearch();
    CHECK(p->Progress == 4800);

    // One point short in a field: progress stops at 10000 - 10000 / 2.
    ds.Game_DevSkill[0] = 2;
    p->Progress = 5050;
    Sim_ProgressResearch();
    CHECK(p->Progress == 4950);
    Sim_ProgressResearch();
    CHECK(p->Progress == 4950);

    // Done: state 5, CurResearch cleared, "The ... is ready for production".
    ds.Game_DevSkill[0] = 3;
    p->Progress = 50;
    messages = 0;
    Sim_ProgressResearch();
    CHECK(p->Progress == 0 && p->State == 5 && ds.Game_CurResearch == 0);
    CHECK(messages > 0);
    Sim_ProgressResearch();
    CHECK(p->Progress == 0 && p->State == 5);

    // The developer away at the university (Advisor::TrainingKind = 4) stops research.
    p->State = 2;
    p->Progress = 5000;
    ds.Advisor_TrainingKind = 4;
    Sim_ProgressResearch();
    CHECK(p->Progress == 5000);
}

static void test_production(void)
{
    reset();
    clear_products();
    TProduct *p = &ds.Research_Products[2];  // product 3
    int16_t stock = p->Stock;
    p->_1D = 2;   // two queued
    p->_1F = 25;  // work per item
    p->_21 = 25;  // work left on the current one
    ds.Game_ProductionRate = 10;
    Sim_ProgressProduction();
    CHECK(p->_21 == 15 && p->Stock == stock);
    Sim_ProgressProduction();
    CHECK(p->_21 == 5);
    Sim_ProgressProduction();  // 5 - 10 < 1: done, the next one starts
    CHECK(p->Stock == stock + 1 && p->_1D == 1 && p->_21 == 25);
    for (int i = 0; i < 3; i++)
        Sim_ProgressProduction();
    CHECK(p->Stock == stock + 2 && p->_1D == 0 && p->_21 == 0);
    Sim_ProgressProduction();  // empty queue: untouched
    CHECK(p->Stock == stock + 2 && p->_1D == 0 && p->_21 == 0);
}

// An hour that strikes between a ship's Transfer click and the cargo
// screen's set-up (game/sim.c) draws nothing: Cargo::Stores is not set yet.
// Once the screen is set up, the hour draws its amounts.
static void test_cargo_switch_pending(void)
{
    reset();
    ds.Game_CurBody = &ds.Galaxy_Bodies[0][4];  // New Earth
    ds.Game_GroupList = 0;
    ds.Game_SelGroup = 1;
    TGroup *g = Groups_Selected();
    *g = (TGroup){.Kind = 2, .System = 1, .Planet = 5, .Status = 1, .Units = {{{.ShipType = 1}}}};
    ds.Game_CurrentScreen = 0xD;
    ds.Game_ScreenSwitch = 1;
    CHECK(ds.Cargo_Stores == nullptr);
    static uint8_t before[VID_HEIGHT][VID_WIDTH];
    SDL_memcpy(before, vid_screen, sizeof before);
    Sim_ProgressProduction();
    CHECK(SDL_memcmp(before, vid_screen, sizeof before) == 0);
    ds.Game_ScreenSwitch = 0;
    Cargo_InitCargoScreen();
    Sim_ProgressProduction();
    CHECK(SDL_memcmp(before, vid_screen, sizeof before) != 0);
}

// FNV-1a over the DS fields from offset `from` up to `to`.
static uint32_t fnv(uint32_t h, size_t from, size_t to)
{
    const uint8_t *p = (const uint8_t *)&ds + from;
    for (uint32_t i = 0; i < (uint32_t)(to - from); i++)
        h = (h ^ p[i]) * 16777619u;
    return h;
}

// The state the simulation owns: the typed-constant block (galaxy, products,
// races, pubs, story timers), idea timers and story stage, staff, research,
// credits and stocks, the date, the colony set-ups and RandSeed.
static uint32_t sim_checksum(void)
{
    uint32_t h = 2166136261u;
    h = fnv(h, 0, DSEG_INIT_SIZE);
    h = fnv(h, offsetof(DSeg, Game_IdeaTimer), offsetof(DSeg, Gfx_ScreenSeg));
    h = fnv(h, offsetof(DSeg, Game_SelInvention), offsetof(DSeg, Game_CurrentScreen));
    h = fnv(h, offsetof(DSeg, Game_ColonySetupCount), offsetof(DSeg, Game_Buildings));
    h = fnv(h, offsetof(DSeg, System_RandSeed), offsetof(DSeg, System_RandSeed) + 4);
    return h;
}

// Galaxy::PlanetStatus of Jade (Amnesty planet 7): -3 hidden (observatories
// only), 0 known.
static int8_t *jade_status(void)
{
    return &ds.Galaxy_PlanetStatus[1 - 1][7 - 1];
}

// Fix (game/fixes.h): Jade, still hidden on 2928-04-01, is found that day
// without an observatory roll; before that date, and with the fix off, it
// stays hidden (no observatory in Amnesty here, so no roll either).
static void test_jade_found(void)
{
    reset();
    ds.Sim_Observatories[1 - 1] = 0;  // no observatory in Amnesty
    CHECK(*jade_status() == -3);

    fixes.find_jade = true;
    set_date(2928, 3, 30, 0);
    Sim_ProcessAllBodyEvents();
    CHECK(*jade_status() == -3);

    fixes.find_jade = false;
    set_date(2928, 4, 1, 0);
    Sim_ProcessAllBodyEvents();
    CHECK(*jade_status() == -3);

    fixes.find_jade = true;
    int before = messages;
    Sim_ProcessAllBodyEvents();
    CHECK(*jade_status() == 0);
    CHECK(messages > before);  // the discovery message and the name prompt

    // Later dates too (a game loaded after the date).
    reset();
    ds.Sim_Observatories[1 - 1] = 0;
    set_date(2931, 1, 1, 0);
    Sim_ProcessAllBodyEvents();
    CHECK(*jade_status() == 0);
}

// Fix (game/fixes.h): a colony mines whichever system the map shows; with
// the fix off it mines only while its own system is selected.
static void test_mine_all_systems(void)
{
    reset();
    const int16_t sys = 3;  // Mirach, body 1
    ds.Galaxy_SystemStatus[sys - 1] = 1;
    TBody *b = ds.Galaxy_Bodies[sys - 1];
    b->Owner = 1;
    b->Colony = 1;
    b->_0A = 2;
    b->Deposits[1] = 50;  // ore 2
    ds.Game_PlanetForceCount = 1;
    TGroup *g = Groups_At(1, 1);
    g->System = sys;
    g->Planet = 1;
    g->Moon = 0;
    g->Storage = 10;

    ds.Game_CurSystem = 1;
    fixes.mine_all_systems = false;
    // Off, the bodies without a planet force read Groups^[1][0] (0 here).
    fixes.station_storage = false;
    Sim_MineAllBodies();
    CHECK(b->Stores[1] == 0);

    fixes.mine_all_systems = true;
    Sim_MineAllBodies();
    CHECK(b->Stores[1] == 50 * 2 / 10);

    fixes.mine_all_systems = false;
    ds.Game_CurSystem = sys;
    Sim_MineAllBodies();
    CHECK(b->Stores[1] == 2 * (50 * 2 / 10));
    fixes.mine_all_systems = true;
    fixes.station_storage = true;
}

// The first Amnesty planet with a moon.
static int16_t planet_with_moon(void)
{
    for (int16_t p = 1; p <= 7; p++) {
        if (ds.Galaxy_AmnestyMoons[p - 1].Count > 0)
            return p;
    }
    return 0;
}

static TBuilding *add_building(uint8_t kind, int16_t planet, int16_t moon, uint8_t health)
{
    TBuilding *bl = &ds.Game_Buildings[ds.Game_BuildingCount++];
    *bl = (TBuilding){.Kind = kind, .System = 1, .Planet = (uint8_t)planet,
                      .Moon = (uint8_t)moon, .Health = health, .Flag = 1};
    return bl;
}

// Fix (game/fixes.h): a Derrick on a moon fills the moon's store; with the
// fix off it fills the planet's while a planet is selected.
static void test_derricks_on_moons(void)
{
    for (int fix = 0; fix <= 1; fix++) {
        reset();
        int16_t p = planet_with_moon();
        CHECK(p != 0);
        TBody *planet = &ds.Galaxy_AmnestyBodies[p - 1];
        TBody *moon = &ds.Galaxy_AmnestyBodies[ds.Galaxy_AmnestyMoons[p - 1].Moons[0] - 1];
        planet->Deposits[0] = moon->Deposits[0] = 70;
        planet->Stores[0] = moon->Stores[0] = 0;
        add_building(5, p, 1, 0);
        ds.Game_PlanetForceCount = 1;
        TGroup *g = Groups_At(1, 1);
        g->System = 1;
        g->Planet = p;
        g->Moon = 1;
        g->Storage = 10;
        ds.Game_CurSystem = 1;
        ds.Game_CurMoon[0] = 0;
        fixes.derricks_on_moons = fix;
        for (int i = 0; i < 50; i++)
            Sim_UpdateBuildings();
        CHECK((fix ? moon : planet)->Stores[0] > 0);
        CHECK((fix ? planet : moon)->Stores[0] == 0);
    }
    fixes.derricks_on_moons = true;
}

// Fix (game/fixes.h): building on a moon goes at the moon's speed (body
// bytes 4..5); with the fix off, at the planet's while a planet is selected.
static void test_build_on_moons(void)
{
    uint8_t left[2];
    for (int fix = 0; fix <= 1; fix++) {
        reset();
        int16_t p = planet_with_moon();
        TBody *planet = &ds.Galaxy_AmnestyBodies[p - 1];
        TBody *moon = &ds.Galaxy_AmnestyBodies[ds.Galaxy_AmnestyMoons[p - 1].Moons[0] - 1];
        planet->_04[0] = planet->_04[1] = 0;
        moon->_04[0] = 0x30;  // 30,000
        moon->_04[1] = 0x75;
        TBuilding *bl = add_building(5, p, 1, 250);
        ds.Game_CurSystem = 1;
        ds.Game_CurMoon[0] = 0;
        fixes.build_on_moons = fix;
        for (int i = 0; i < 1000 && bl->Health == 250; i++)
            Sim_UpdateBuildings();
        left[fix] = bl->Health;
    }
    CHECK(left[0] < 250);
    CHECK(left[1] < left[0]);
    fixes.build_on_moons = true;
}

// Fix (game/fixes.h): a meteorite that destroys a building takes a droid
// from the hit colony; with the fix off, from the selected body.
static void test_meteorite_droids(void)
{
    for (int fix = 0; fix <= 1; fix++) {
        reset();
        set_date(2928, 1, 1, 0);
        TBody *hit = &ds.Galaxy_AmnestyBodies[4];  // New Earth
        TBody *shown = &ds.Galaxy_AmnestyBodies[0];
        CHECK(hit->Owner == 1 && hit->Colony != 0);
        hit->_0A = shown->_0A = 5;
        ds.Game_CurBody = shown;
        for (int k = 0; k < 20; k++)
            add_building(2, 5, 0, 0);
        fixes.meteorite_droids = fix;
        int16_t before = ds.Game_BuildingCount;
        for (int i = 0; i < 5000 && ds.Game_BuildingCount == before; i++) {
            hit->_14 = 100;  // meteorites hit only at 100
            Sim_ProcessAllBodyEvents();
        }
        CHECK(ds.Game_BuildingCount == before - 1);
        CHECK((fix ? hit : shown)->_0A == 4);
        CHECK((fix ? shown : hit)->_0A == 5);
    }
    fixes.meteorite_droids = true;
}

// Fix (game/fixes.h): a body with only a Miner station mines into 10,000 t
// of storage. With the fix off it reads the fleet's group 32, which a
// conquest (the new colony's command centre is placed before its planet
// force exists) sets to 0, and mining there stops.
static void test_station_storage(void)
{
    for (int fix = 0; fix <= 1; fix++) {
        reset();
        TBody *b = &ds.Galaxy_AmnestyBodies[2];  // Amnesty 3, as Satellite_ColonizeBody leaves it
        b->Owner = 1;
        b->Colony = 0;
        b->_0A = 1;
        b->Deposits[1] = 50;  // ore 2
        b->Stores[1] = 0;
        Groups_At(1, 0)->Storage = 10;
        ds.Game_CurSystem = 1;
        fixes.station_storage = fix;
        for (int i = 0; i < 100; i++)
            Sim_MineAllBodies();
        int32_t before = b->Stores[1];
        CHECK(before > 0);
        Intel_ClaimBodyForPlayer(3, 3, 0);  // Mirach 3 conquered
        CHECK(Groups_At(1, 0)->Storage == 0);
        for (int i = 0; i < 100; i++)
            Sim_MineAllBodies();
        CHECK(fix ? b->Stores[1] > before : b->Stores[1] == before);
    }
    fixes.station_storage = true;
}

// Fix (game/fixes.h): New Earth's set-up places its last starter building
// this hour, and Apollo's, next in the list, still counts down in the same
// hour. Off, it moves into the freed slot and is skipped.
static void test_colony_setups(void)
{
    for (int fix = 0; fix <= 1; fix++) {
        reset();
        fixes.colony_setups = fix;
        ds.Game_ColonySetupCount = 2;
        ds.Game_ColonySetups[0] = (TColonySetup){.Active = 1, .System = 1, .Planet = 5, .Timers = {1}};
        ds.Game_ColonySetups[1] =
            (TColonySetup){.Active = 1, .System = 1, .Planet = 5, .Moon = 1, .Timers = {3}};
        Sim_UpdateColonySetups();
        CHECK(ds.Game_ColonySetupCount == 1);
        CHECK(ds.Game_ColonySetups[0].Moon == 1);
        CHECK(ds.Game_ColonySetups[0].Timers[0] == (fix ? 2 : 3));
    }
    fixes.colony_setups = true;
}

static void test_golden(void)
{
    reset();
    set_date(2200, 1, 1, 0);
    ds.System_RandSeed = 12345;
    ds.Game_StaffHired[0] = 1;
    ds.Game_StaffHired[3] = 1;
    ds.Game_StaffExp[3] = 5;
    int32_t pop = ds.Galaxy_AmnestyBodies[4].Population;  // New Earth
    int32_t credits = ds.Game_Credits;
    for (int i = 0; i < 500; i++)
        hour();
    // 500 hours = 20 days and 20 hours.
    CHECK(date_is(2200, 1, 21, 20));
    // New Earth was simulated (taxes paid, population changed).
    CHECK(ds.Galaxy_AmnestyBodies[4].Population != pop);
    CHECK(ds.Game_Credits != credits);
    uint32_t sum = sim_checksum();
    printf("checksum %08X, RandSeed %08X, credits %d, New Earth population %d\n", (unsigned)sum,
           (unsigned)ds.System_RandSeed, (int)ds.Game_Credits,
           (int)ds.Galaxy_AmnestyBodies[4].Population);
    // The checksum is printed for comparison between runs but not checked
    // yet: it changes whenever a stubbed callee is ported or the initial data
    // is corrected. Pin it once every unit Sim calls is ported, ideally
    // against a memory dump of the original in DOSBox-X.
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    dseg_clear();
    vid_clear();
    dseg_init();
    test_date();
    test_research();
    test_production();
    test_cargo_switch_pending();
    test_jade_found();
    test_mine_all_systems();
    test_derricks_on_moons();
    test_build_on_moons();
    test_meteorite_droids();
    test_station_storage();
    test_colony_setups();
    test_golden();
    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
