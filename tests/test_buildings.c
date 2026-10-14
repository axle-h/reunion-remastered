// buildings: the no_building_limit improvement (game/improvements.h) without
// the screens. With the switch on the table grows past 1,000 out of the game
// data block, a save in the port's format brings every building back, and
// the cap holds; off, the 1,001st is refused, a game loaded with more keeps
// them, and a save holding more than 1,000 is in the port's format all the
// same.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/dseg.h"
#include "game/improvements.h"
#include "game/main_program.h"
#include "game/message.h"
#include "game/saveload.h"
#include "game/surface.h"
#include "game/system.h"
#include "platform/fs.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static char dir[256];

static void reset(void)
{
    dseg_clear();
    dseg_init();
    system_init();
    Game_InitGameData();
    Message_Init();
    SaveLoad_LoadInit();
    improvements.no_building_limit = true;
}

// One more of New Earth's first building, unplaced: x, y tell them apart.
static void add(int i)
{
    const TBuilding *first = &ds.Game_Buildings[0];
    Surface_AddBuilding(first->Kind, first->System, first->Planet, first->Moon, (uint8_t)(i % 250),
                        (uint8_t)(i / 250));
}

static void add_up_to(int n)
{
    for (int i = ds.Game_BuildingCount; i < n; i++)
        add(i);
}

static bool exists(const char *name)
{
    char path[256];
    SDL_snprintf(path, sizeof path, "%s/%s", dir, name);
    return SDL_GetPathInfo(path, nullptr);
}

static bool save_slot(int16_t slot)
{
    SaveLoad_ReadSlotNames();
    ds.SaveLoad_SelectedSlot = slot;
    SDL_memcpy(ds.SaveLoad_SlotTable + (slot - 1) * 0x16 + 1, "\x04" "Test", 5);
    bool saved = SaveLoad_Transfer(true);
    game_free(ds.SaveLoad_SlotTable);
    return saved;
}

static bool load_slot(int16_t slot)
{
    SaveLoad_ReadSlotNames();
    ds.SaveLoad_SelectedSlot = slot;
    bool loaded = SaveLoad_Transfer(false);
    game_free(ds.SaveLoad_SlotTable);
    return loaded;
}

static void test_growth(void)
{
    reset();
    TBuilding *block = ds.Game_Buildings;
    int16_t start = ds.Game_BuildingCount;
    CHECK(start > 0 && start < BUILDINGS_DOS && Game_BuildingLimit() == BUILDINGS_MAX);
    add_up_to(BUILDINGS_DOS);
    CHECK(ds.Game_BuildingCount == BUILDINGS_DOS && ds.Game_Buildings == block);
    // Past 1,000 the table moves, twice over, with every record.
    TBuilding *before = SDL_malloc(BUILDINGS_DOS * sizeof(TBuilding));
    SDL_memcpy(before, block, BUILDINGS_DOS * sizeof(TBuilding));
    add_up_to(4500);
    CHECK(ds.Game_BuildingCount == 4500 && Game_BuildingsMoved());
    CHECK(SDL_memcmp(ds.Game_Buildings, before, BUILDINGS_DOS * sizeof(TBuilding)) == 0);
    bool all = true;
    for (int i = start; i < 4500; i++) {
        const TBuilding *b = &ds.Game_Buildings[i];
        all &= b->Kind == block[0].Kind && b->X == i % 250 && b->Y == i / 250;
    }
    CHECK(all);
    SDL_free(before);
    // Deleting one shifts the later ones down, past the old table's end too.
    Surface_DeleteBuilding(1500);
    CHECK(ds.Game_BuildingCount == 4499 && ds.Game_Buildings[1499].X == 1500 % 250
          && ds.Game_Buildings[4498].X == 4499 % 250);

    // The cap.
    CHECK(Game_ReserveBuildings(BUILDINGS_MAX) && !Game_ReserveBuildings(BUILDINGS_MAX + 1));
    ds.Game_BuildingCount = BUILDINGS_MAX - 1;
    add(0);
    CHECK(ds.Game_BuildingCount == BUILDINGS_MAX);
    add(0);
    CHECK(ds.Game_BuildingCount == BUILDINGS_MAX);

    // A new start puts the table back in the data block.
    Game_InitGameData();
    CHECK(!Game_BuildingsMoved());
}

static void test_switch_off(void)
{
    reset();
    improvements.no_building_limit = false;
    CHECK(Game_BuildingLimit() == BUILDINGS_DOS);
    add_up_to(BUILDINGS_DOS + 5);
    CHECK(ds.Game_BuildingCount == BUILDINGS_DOS && !Game_BuildingsMoved());
}

static void test_saves(void)
{
    SDL_snprintf(dir, sizeof dir, "/tmp/reunion_test_buildings_%d", (int)SDL_GetCurrentTime(nullptr) & 0xFFFF);
    SaveLoad_SetSaveDir(dir);

    // On: the port's format, loaded into a new game with every building.
    reset();
    add_up_to(2345);
    size_t size = 2345 * sizeof(TBuilding);
    TBuilding *saved = SDL_malloc(size);
    SDL_memcpy(saved, ds.Game_Buildings, size);
    CHECK(save_slot(1) && exists("reunion.1") && !exists("spidysav.1"));
    reset();
    CHECK(ds.Game_BuildingCount < BUILDINGS_DOS && !Game_BuildingsMoved());
    CHECK(load_slot(1));
    CHECK(ds.Game_BuildingCount == 2345 && Game_BuildingsMoved()
          && SDL_memcmp(ds.Game_Buildings, saved, size) == 0);

    // Off: the game keeps them, takes no new one, and still saves in the
    // port's format.
    improvements.no_building_limit = false;
    add(0);
    CHECK(ds.Game_BuildingCount == 2345);
    CHECK(save_slot(2) && exists("reunion.2") && !exists("spidysav.2"));
    reset();
    improvements.no_building_limit = false;
    CHECK(load_slot(2) && ds.Game_BuildingCount == 2345 && SDL_memcmp(ds.Game_Buildings, saved, size) == 0);

    // Down to 1,000 again: the DOS format, its 1,000 records from the moved
    // table.
    ds.Game_BuildingCount = BUILDINGS_DOS;
    CHECK(save_slot(2) && exists("spidysav.2") && !exists("reunion.2"));
    reset();
    improvements.no_building_limit = false;
    CHECK(load_slot(2) && ds.Game_BuildingCount == BUILDINGS_DOS && !Game_BuildingsMoved()
          && SDL_memcmp(ds.Game_Buildings, saved, BUILDINGS_DOS * sizeof(TBuilding)) == 0);
    SDL_free(saved);

    for (int n = 1; n <= 2; n++) {
        char p[256];
        SDL_snprintf(p, sizeof p, "%s/reunion.%d", dir, n);
        SDL_RemovePath(p);
        SDL_snprintf(p, sizeof p, "%s/spidysav.%d", dir, n);
        SDL_RemovePath(p);
    }
    SDL_RemovePath(dir);
    improvements.no_building_limit = false;
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    test_growth();
    test_switch_off();
    test_saves();
    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
