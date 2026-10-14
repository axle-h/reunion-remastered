// colonystats: the colony stats overlay's numbers (game/colonystats.h): the
// capacities from the powered buildings, the disease timer, and for many
// colonies the attitude and population that the next daily update
// (Sim_ProcessAllBodyEvents) really gives.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/colonystats.h"
#include "game/dseg.h"
#include "game/main_program.h"
#include "game/gfx.h"
#include "game/message.h"
#include "game/sim.h"
#include "game/state.h"
#include "game/system.h"
#include "game/textfile.h"
#include "platform/config.h"
#include "platform/fs.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

enum : int {
    NEW_EARTH = 5,  // Amnesty planet 5
    HOT_PLANET = 2, // near the sun: radiation
};

static bool wait_retrace(void) { return true; }
// Message boxes wait for a key: answer at once.
static bool key_pressed(void) { return true; }
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
    ds.Game_BuildingCount = 0;
    ds.Game_Year = 2928;
    ds.Game_Month = ds.Game_Day = 1;
    ds.Game_Hour = 0;
}

static void add_building(uint8_t kind, int16_t planet, bool powered)
{
    ds.Game_Buildings[ds.Game_BuildingCount++] =
        (TBuilding){.Kind = kind, .System = 1, .Planet = (uint8_t)planet, .Powered = powered};
}

static TBody *colony(int16_t planet)
{
    TBody *b = &ds.Galaxy_AmnestyBodies[planet - 1];
    b->Owner = 1;
    b->Colony = 1;
    return b;
}

static void test_capacities(void)
{
    reset();
    TBody *b = colony(NEW_EARTH);
    b->Population = 30000;
    b->Hazard = -1;
    for (int i = 0; i < 4; i++)
        add_building(7, NEW_EARTH, true);  // Building
    add_building(16, NEW_EARTH, true);     // Farm
    add_building(21, NEW_EARTH, true);     // Food resources
    add_building(15, NEW_EARTH, true);     // Hospital
    add_building(15, NEW_EARTH, false);    // an unpowered Hospital counts for nothing
    add_building(7, 3, true);              // another planet's
    ColonyStats s;
    uint64_t before = state_hash();
    ColonyStats_Compute(1, NEW_EARTH, 0, &s);
    CHECK(state_hash() == before);
    CHECK(s.housing == 38000 && s.food == 48000 && s.health == 40000);
    CHECK(s.powered[15 - 1] == 1 && s.unpowered[15 - 1] == 1);
    CHECK(s.sick == 1 && s.disease_days == -1 && !s.meteor_world && !s.radiation);

    // The disease timer counts down at the start of the day's update.
    b->Hazard = 2;
    ColonyStats_Compute(1, NEW_EARTH, 0, &s);
    CHECK(s.disease_days == 2 && s.sick == 1);
    b->Hazard = 1;
    ColonyStats_Compute(1, NEW_EARTH, 0, &s);
    CHECK(s.disease_days == 1 && s.sick == 2);
    b->Hazard = 0;
    ColonyStats_Compute(1, NEW_EARTH, 0, &s);
    CHECK(s.disease_days == 0 && s.sick == 2);
    b->Hazard = 100;
    ColonyStats_Compute(1, NEW_EARTH, 0, &s);
    CHECK(s.disease_days == -1 && s.sick == 1 && s.meteor_world);

    colony(HOT_PLANET);
    ColonyStats_Compute(1, HOT_PLANET, 0, &s);
    CHECK(s.radiation);
    add_building(19, HOT_PLANET, true);  // Anti-radiation shield
    ColonyStats_Compute(1, HOT_PLANET, 0, &s);
    CHECK(!s.radiation);
}

static void test_available(void)
{
    reset();
    config_reset();
    ds.Game_CurBody = colony(NEW_EARTH);
    CHECK(ColonyStats_Available());
    config.colony_stats = false;
    CHECK(!ColonyStats_Available());
    config.colony_stats = true;
    ds.Game_CurBody = &ds.Galaxy_AmnestyBodies[3];
    ds.Game_CurBody->Owner = 0;
    CHECK(!ColonyStats_Available());
}

static uint32_t rng = 1;

static int roll(int n)
{
    rng = rng * 1103515245u + 12345u;
    return (int)((rng >> 16) % (uint32_t)n);
}

// Population near the thresholds of the daily update, or anywhere.
static int32_t some_population(void)
{
    static const int32_t near[] = {10000, 20000, 30000, 38000, 40000, 48000, 50000,
                                   60000, 70000, 80000, 100000, 150000};
    if (roll(3) == 0)
        return 3000 + roll(180000);
    return near[roll((int)SDL_arraysize(near))] - 300 + roll(600);
}

// Colonies with random buildings, tax, mood and disease: the stats must
// predict the day's attitude and population. Radiation kills a random number
// of people, so its colonies stay small (no threshold in reach) and only the
// attitude is compared.
static void test_cross_check(void)
{
    static const uint8_t kinds[] = {7, 8, 9, 10, 13, 14, 15, 16, 19, 21, 24};
    static const int8_t timers[] = {-1, 0, 1, 2, 50};
    int checked = 0;
    for (int i = 0; i < 800; i++) {
        reset();
        ds.System_RandSeed = (int32_t)i;
        bool hot = roll(6) == 0;
        int16_t planet = hot ? HOT_PLANET : NEW_EARTH;
        TBody *b = colony(planet);
        for (size_t k = 0; k < SDL_arraysize(kinds); k++) {
            int n = roll(4);
            for (int j = 0; j < n; j++)
                add_building(kinds[k], planet, roll(4) != 0);
        }
        b->Population = hot ? 4000 + roll(4000) : some_population();
        b->TechLevel = (uint8_t)roll(8);
        b->Attitude = (int8_t)roll(70);
        b->Hazard = timers[roll((int)SDL_arraysize(timers))];
        b->Resistance = 40;

        ColonyStats s;
        ColonyStats_Compute(1, planet, 0, &s);
        Sim_ProcessAllBodyEvents();
        bool same = b->Attitude == s.attitude_tomorrow && (hot || b->Population == s.population_tomorrow);
        if (!same) {
            printf("case %d: planet %d, pop %d, tax %d, timer %d: attitude %d (stats %d), population %d (stats %d)\n",
                   i, planet, (int)s.population, b->TechLevel, b->Hazard, b->Attitude, s.attitude_tomorrow,
                   (int)b->Population, (int)s.population_tomorrow);
            ok = false;
        }
        checked++;
    }
    printf("%d colonies checked\n", checked);
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    test_capacities();
    test_available();
    test_cross_check();
    config_reset();
    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
