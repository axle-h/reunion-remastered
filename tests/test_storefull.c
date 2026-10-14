// Improvement (game/improvements.h, storage_full_message): game/storefull.h
// with the hourly mining and Derricks. A body filling up warns once; a
// second ore filling says nothing; one drained and refilled warns again; a
// Derrick's ore and New Earth's own store warn too; a load with full stores
// is quiet, one without arms the body; the random stream and the stores
// are the same with the switch on and off, and off nothing is logged.
// Needs the game data (SAVE\INIT).
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/dseg.h"
#include "game/groups.h"
#include "game/improvements.h"
#include "game/lang.h"
#include "game/main_program.h"
#include "game/message.h"
#include "game/planetinfo.h"
#include "game/saveload.h"
#include "game/sim.h"
#include "game/storefull.h"
#include "game/system.h"
#include "platform/fs.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static int32_t cap;

// New Earth's first moon, the player's with one droid and no colony, mining
// ores 2 and 3 into empty stores.
static TBody *moon(void)
{
    return PlanetInfo_BodyAt(1, 5, 1);
}

static void reset(bool on)
{
    dseg_clear();
    vid_clear();
    dseg_init();
    system_init();
    Game_InitGameData();
    Message_Init();
    SaveLoad_LoadInit();
    Groups_InitPlanetForce();  // New Earth's planet force
    Groups_At(1, 1)->Storage = 10;  // 10,000 t of each ore
    TBody *b = moon();
    b->Owner = 1;
    b->Colony = 0;
    b->Droids = 1;
    SDL_zeroa(b->Deposits);
    SDL_zeroa(b->Stores);
    b->Deposits[1] = 50;
    b->Deposits[2] = 50;
    cap = Groups_PlanetStorage(1, 5, 1) * 1000;
    for (int k = 0; k < 6; k++)
        ds.Game_HomeStock[k] = 0;
    ds.System_RandSeed = 4321;
    improvements.storage_full_message = on;
    StoreFull_Reset();  // as a new game does
    ds.Message_LogCount = 0;
}

static void hour(void)
{
    Sim_UpdateBuildings();
    Sim_MineAllBodies();
}

// Log entry n (1-based) is the warning for (system, planet, moon).
static bool warned(int16_t n, int16_t system, int16_t planet, int16_t m)
{
    char name[64], want[128], got[64];
    PlanetInfo_GetBodyFullName(name, sizeof name, system, planet, m);
    SDL_snprintf(want, sizeof want, "%s%s", Lang_Text("Storage is full on "), name);
    const uint8_t *e = ds.Message_Log + (n - 1) * 0x35;
    SDL_memcpy(got, e + 3, e[2]);
    got[e[2]] = '\0';
    return SDL_strcmp(got, want) == 0;
}

static void test_warnings(void)
{
    reset(true);
    TBody *b = moon();
    hour();
    CHECK(ds.Message_LogCount == 0);

    // Ore 2 full: one entry, once.
    b->Stores[1] = cap;
    hour();
    CHECK(ds.Message_LogCount == 1 && warned(1, 1, 5, 1));
    for (int h = 0; h < 5; h++)
        hour();
    CHECK(ds.Message_LogCount == 1);

    // Ore 3 full too: nothing more.
    b->Stores[2] = cap;
    hour();
    CHECK(ds.Message_LogCount == 1);

    // Ore 2 moved away: no entry; filled again, a second one.
    b->Stores[1] = 0;
    hour();
    CHECK(ds.Message_LogCount == 1);
    b->Stores[1] = cap;
    hour();
    CHECK(ds.Message_LogCount == 2 && warned(2, 1, 5, 1));
    hour();
    CHECK(ds.Message_LogCount == 2);

    // A Derrick at work on the moon, its ore 1 full.
    reset(true);
    b = moon();
    b->Deposits[0] = 50;
    b->Stores[0] = cap;
    CHECK(Game_ReserveBuildings(ds.Game_BuildingCount + 1));
    ds.Game_Buildings[ds.Game_BuildingCount++] =
        (TBuilding){.Kind = 5, .System = 1, .Planet = 5, .Moon = 1, .Powered = 1};
    Sim_UpdateBuildings();
    CHECK(ds.Message_LogCount == 1 && warned(1, 1, 5, 1));
    CHECK(b->Stores[0] == cap);

    // New Earth's own store, Game_HomeStock.
    reset(true);
    TBody *home = PlanetInfo_BodyAt(1, 5, 0);
    home->Owner = 1;
    home->Droids = 1;
    home->Deposits[1] = 50;
    ds.Game_HomeStock[1] = Groups_PlanetStorage(1, 5, 0) * 1000;
    hour();
    CHECK(ds.Message_LogCount == 1 && warned(1, 1, 5, 0));
}

// A load seeds the bits from the stores: full ones say nothing, and a body
// with none full warns when one fills.
static void test_load(void)
{
    char path[256];
    SDL_snprintf(path, sizeof path, "/tmp/reunion_test_storefull_%d",
                 (int)SDL_GetCurrentTime(nullptr) & 0xFFFF);
    uint8_t name[0x14] = {0};

    reset(true);
    TBody *b = moon();
    b->Stores[1] = cap;
    CHECK(SaveLoad_TransferFile(path, name, true));
    reset(true);
    CHECK(SaveLoad_TransferFile(path, name, false));
    b = moon();
    CHECK(b->Stores[1] == cap);
    hour();
    CHECK(ds.Message_LogCount == 0);
    // Ore 2 is still full: ore 3 filling says nothing either.
    b->Stores[2] = cap;
    hour();
    CHECK(ds.Message_LogCount == 0);
    // Draining one re-arms the body as in play.
    b->Stores[1] = 0;
    hour();
    b->Stores[1] = cap;
    hour();
    CHECK(ds.Message_LogCount == 1);

    reset(true);
    b = moon();
    b->Stores[1] = cap - 1;
    CHECK(SaveLoad_TransferFile(path, name, true));
    reset(true);
    b = moon();
    b->Stores[2] = cap;
    StoreFull_Check(1, 5, 1, 3, cap, cap);  // full before the load
    CHECK(ds.Message_LogCount == 1);
    CHECK(SaveLoad_TransferFile(path, name, false));
    ds.Message_LogCount = 0;
    b = moon();
    b->Stores[1] = cap;
    hour();
    CHECK(ds.Message_LogCount == 1 && warned(1, 1, 5, 1));
    SDL_RemovePath(path);
}

// The checks draw no random numbers: a hundred hours of filling stores,
// with a Derrick, give the same seed and stores with the switch on and
// off; off, nothing is logged.
static void test_random(void)
{
    int32_t seeds[2], stores[2][6], home[2][6];
    int16_t logged[2];
    for (int on = 0; on <= 1; on++) {
        reset(on);
        TBody *b = moon();
        b->Deposits[0] = 50;
        b->Stores[1] = cap - 100;
        b->Stores[2] = cap - 20;
        CHECK(Game_ReserveBuildings(ds.Game_BuildingCount + 1));
        ds.Game_Buildings[ds.Game_BuildingCount++] =
            (TBuilding){.Kind = 5, .System = 1, .Planet = 5, .Moon = 1, .Powered = 1};
        b->Stores[0] = cap - 10;
        for (int h = 0; h < 100; h++)
            hour();
        seeds[on] = ds.System_RandSeed;
        logged[on] = ds.Message_LogCount;
        for (int k = 0; k < 6; k++) {
            stores[on][k] = b->Stores[k];
            home[on][k] = ds.Game_HomeStock[k];
        }
    }
    CHECK(seeds[0] == seeds[1]);
    CHECK(SDL_memcmp(stores[0], stores[1], sizeof stores[0]) == 0);
    CHECK(SDL_memcmp(home[0], home[1], sizeof home[0]) == 0);
    CHECK(stores[1][0] >= cap && stores[1][1] >= cap && stores[1][2] >= cap);
    CHECK(logged[0] == 0 && logged[1] >= 1);
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    test_warnings();
    test_load();
    test_random();
    improvements.storage_full_message = false;
    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
