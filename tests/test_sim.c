// sim: the hourly simulation from the game's initial data: date carry,
// research and production arithmetic, and a regression checksum of the
// simulated state after a fixed number of hours with a fixed RandSeed.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/dseg.h"
#include "game/ems.h"
#include "game/gfx.h"
#include "game/message.h"
#include "game/sim.h"
#include "game/system.h"
#include "game/textfile.h"
#include "platform/fs.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static int messages;

static bool wait_retrace(void) { return true; }
// Message boxes wait for a key: answer at once.
static bool key_pressed(void) { messages++; return true; }
static char read_key(void) { return '\r'; }

static void reset(void)
{
    mem_init();
    CHECK(dseg_init());
    system_init();
    Ems_Init();
    Message_Init();
    // dseg_init copies the typed constants without the EXE's relocations:
    // the far pointers to the per-system tables still hold the file's DS
    // segment (3A81h). The DOS loader adds the load segment.
    for (int s = 0; s < 8; s++) {
        uint32_t *tables[3] = {&ds.Galaxy_Names[s], &ds.Galaxy_MoonLists[s], &ds.Galaxy_Bodies[s]};
        for (int t = 0; t < 3; t++) {
            if (*tables[t] >> 16 == 0x3A81)
                *tables[t] = far_ptr(MEM_DS_SEG, (uint16_t)*tables[t]);
        }
    }
    ds.System_SegA000 = 0xA000;
    ds.Gfx_ScreenSeg = 0xA000;
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

    // The developer away at the university (DS:5D9E = 4) stops research.
    p->State = 2;
    p->Progress = 5000;
    *(int16_t *)ds_at(0x5D9E) = 4;
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

// FNV-1a over a DS range.
static uint32_t fnv(uint32_t h, uint16_t from, uint16_t to)
{
    const uint8_t *p = ds_at(from);
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
    h = fnv(h, 0x0000, 0x759E);
    h = fnv(h, 0x91A0, 0x91EC);
    h = fnv(h, 0x95A0, 0x95E6);
    h = fnv(h, 0xA21A, 0xA2BC);
    h = fnv(h, (uint16_t)offsetof(DSeg, System_RandSeed), (uint16_t)(offsetof(DSeg, System_RandSeed) + 4));
    return h;
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
    mem_init();
    if (!dseg_init()) {
        printf("no game data, skipping\n");
        return 77;
    }
    test_date();
    test_research();
    test_production();
    test_golden();
    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
