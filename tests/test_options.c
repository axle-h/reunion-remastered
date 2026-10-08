// options: the cheats are off at boot, freezing holds a stock, the add buttons
// raise it, and the cheats travel with a save (as a trailer only when on).
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "game/dseg.h"
#include "game/ems.h"
#include "game/options.h"
#include "game/saveload.h"
#include "game/system.h"
#include "platform/fs.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static size_t file_size(const char *path)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) ? (size_t)info.size : 0;
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
    system_init();
    Ems_Init();
    ds.Message_Log = System_GetMem(0x31B);
    Options_Init();
    CHECK(!options.freeze_money && !options.freeze_ore);
    // OPTIONS is the disk screen's sixth button.
    const uint8_t *rec = (const uint8_t *)ds_at(0x50FB + 0x0C * 25);
    CHECK(rec[0x0C] == 6 && rec[0x12] == OPTIONS_COMMAND);

    // Frozen money: spending and income are both undone; unfreezing keeps it.
    ds.Game_Credits = 1000;
    Options_SetFreezeMoney(true);
    ds.Game_Credits -= 300;
    Options_ApplyCheats();
    CHECK(ds.Game_Credits == 1000);
    ds.Game_Credits += 50;
    Options_ApplyCheats();
    CHECK(ds.Game_Credits == 1000);
    Options_AddMoney();                // raises the frozen value too
    ds.Game_Credits -= 5000;
    Options_ApplyCheats();
    CHECK(ds.Game_Credits == 1000 + OPTIONS_ADD_MONEY);
    Options_SetFreezeMoney(false);
    ds.Game_Credits -= 200;
    Options_ApplyCheats();
    CHECK(ds.Game_Credits == 800 + OPTIONS_ADD_MONEY);
    Options_AddMoney();
    CHECK(ds.Game_Credits == 800 + 2 * OPTIONS_ADD_MONEY);

    // Ore, all six stocks.
    for (int i = 0; i < 6; i++)
        ds.Game_HomeStock[i] = 100 * (i + 1);
    Options_AddOre();
    CHECK(ds.Game_HomeStock[0] == 100 + OPTIONS_ADD_ORE && ds.Game_HomeStock[5] == 600 + OPTIONS_ADD_ORE);
    Options_SetFreezeOre(true);
    ds.Game_HomeStock[0] -= 40;
    ds.Game_HomeStock[5] -= 600;
    Options_ApplyCheats();
    CHECK(ds.Game_HomeStock[0] == 100 + OPTIONS_ADD_ORE && ds.Game_HomeStock[5] == 600 + OPTIONS_ADD_ORE);

    // A save carries the cheats; loading a save without them turns them off.
    char dir[128], path[256];
    SDL_snprintf(dir, sizeof dir, "/tmp/reunion_test_options_%d", (int)SDL_GetCurrentTime(nullptr) & 0xFFFF);
    SDL_snprintf(path, sizeof path, "%s/spidysav.1", dir);
    SaveLoad_SetSaveDir(dir);
    SaveLoad_ReadSlotNames();
    uint8_t *slot = (uint8_t *)far_host(*(FarPtr *)ds_at(0x7734));
    memcpy(slot + 1, "\x05" "Cheat", 6);
    CHECK(SaveLoad_Transfer(true));
    CHECK(file_size(path) == SAVELOAD_FILE_SIZE + OPTIONS_SAVE_SIZE);

    Options_Init();
    SaveLoad_ReadSlotNames();
    CHECK(SaveLoad_Transfer(false));
    CHECK(options.freeze_ore && !options.freeze_money);
    ds.Game_HomeStock[0] = 7;
    Options_ApplyCheats();
    CHECK(ds.Game_HomeStock[0] == 100 + OPTIONS_ADD_ORE);   // the loaded value stays frozen
    Options_SetFreezeOre(false);

    // No cheat on: the original's size, and loading it turns the cheats off.
    CHECK(SaveLoad_Transfer(true));
    CHECK(file_size(path) == SAVELOAD_FILE_SIZE);
    Options_SetFreezeMoney(true);
    CHECK(SaveLoad_Transfer(false));
    CHECK(!options.freeze_money);

    SDL_RemovePath(path);
    SDL_RemovePath(dir);
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
