// options: the game side of the port menu. Freezing holds a stock from the
// frame it is switched on, the add cheats raise it, a load re-takes the
// frozen value, saves have no cheats trailer any more, and the fixes table
// covers every fix and follows the config.
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "game/dseg.h"
#include "game/ems.h"
#include "game/fixes.h"
#include "game/options.h"
#include "game/saveload.h"
#include "game/system.h"
#include "platform/config.h"
#include "platform/fs.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static size_t file_size(const char *path)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) ? (size_t)info.size : 0;
}

static void freeze(bool money, bool ore)
{
    config.freeze_money = money;
    config.freeze_ore = ore;
    Options_Request(OPTIONS_CHEATS);
}

static void check_fixes(void)
{
    // Every field of Fixes is listed once, with a short label for the menu.
    CHECK(Fixes_Count * sizeof(bool) == sizeof(Fixes));
    for (int i = 0; i < Fixes_Count; i++) {
        const FixInfo *f = &Fixes_List[i];
        CHECK((uint8_t *)f->value >= (uint8_t *)&fixes
              && (uint8_t *)f->value < (uint8_t *)&fixes + sizeof fixes);
        CHECK(SDL_strlen(f->label) <= 20 && SDL_strlen(f->help) <= 72);
        CHECK(SDL_strlen(f->key) < CONFIG_KEY_SIZE);
        for (int j = 0; j < i; j++)
            CHECK(f->value != Fixes_List[j].value && SDL_strcmp(f->key, Fixes_List[j].key) != 0);
    }

    // On without a config; the config turns one off; REUNION_ORIGINAL all.
    Fixes_Init();
    for (int i = 0; i < Fixes_Count; i++)
        CHECK(*Fixes_List[i].value);
    config_set_fix("mine_all_systems", false);
    Fixes_Init();
    CHECK(!fixes.mine_all_systems && fixes.find_jade);
    // As the menu does it: applied at the main loop's next frame.
    config_set_fix("mine_all_systems", true);
    config_set_fix("find_jade", false);
    Options_Request(OPTIONS_FIXES);
    CHECK(!fixes.mine_all_systems);
    Options_Frame();
    CHECK(fixes.mine_all_systems && !fixes.find_jade);
    SDL_setenv_unsafe("REUNION_ORIGINAL", "1", 1);
    CHECK(Fixes_Locked());
    Fixes_Init();
    for (int i = 0; i < Fixes_Count; i++)
        CHECK(!*Fixes_List[i].value);
    SDL_unsetenv_unsafe("REUNION_ORIGINAL");
    config_reset();
    Fixes_Init();
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
    config_reset();
    Options_Init();
    CHECK(!Options_GameRunning());

    // The disk screen keeps the original's five buttons.
    const uint8_t *rec = (const uint8_t *)&ds.Ui_Screens[0x0C - 1];
    CHECK(rec[0x0C] == 5);

    // Frozen money: spending and income are both undone; unfreezing keeps it.
    ds.Game_Credits = 1000;
    freeze(true, false);
    Options_Frame();
    CHECK(Options_GameRunning());
    ds.Game_Credits -= 300;
    Options_Frame();
    CHECK(ds.Game_Credits == 1000);
    ds.Game_Credits += 50;
    Options_Frame();
    CHECK(ds.Game_Credits == 1000);
    // Two clicks before the next frame: both count, and raise the frozen value.
    Options_Request(OPTIONS_ADD_MONEY_ONCE);
    Options_Request(OPTIONS_ADD_MONEY_ONCE);
    ds.Game_Credits -= 5000;
    Options_Frame();
    CHECK(ds.Game_Credits == 1000 + 2 * OPTIONS_ADD_MONEY);
    freeze(false, false);
    ds.Game_Credits -= 200;
    Options_Frame();
    CHECK(ds.Game_Credits == 800 + 2 * OPTIONS_ADD_MONEY);
    Options_Request(OPTIONS_ADD_MONEY_ONCE);
    Options_Frame();
    CHECK(ds.Game_Credits == 800 + 3 * OPTIONS_ADD_MONEY);

    // Ore, all six stocks.
    for (int i = 0; i < 6; i++)
        ds.Game_HomeStock[i] = 100 * (i + 1);
    Options_Request(OPTIONS_ADD_ORE_ONCE);
    Options_Frame();
    CHECK(ds.Game_HomeStock[0] == 100 + OPTIONS_ADD_ORE && ds.Game_HomeStock[5] == 600 + OPTIONS_ADD_ORE);
    freeze(false, true);
    Options_Frame();
    ds.Game_HomeStock[0] -= 40;
    ds.Game_HomeStock[5] -= 600;
    Options_Frame();
    CHECK(ds.Game_HomeStock[0] == 100 + OPTIONS_ADD_ORE && ds.Game_HomeStock[5] == 600 + OPTIONS_ADD_ORE);

    // A save of a game with a cheat on is the original's size; loading it
    // holds the loaded stock, not the one before.
    char dir[128], path[256];
    SDL_snprintf(dir, sizeof dir, "/tmp/reunion_test_options_%d", (int)SDL_GetCurrentTime(nullptr) & 0xFFFF);
    SDL_snprintf(path, sizeof path, "%s/spidysav.1", dir);
    SaveLoad_SetSaveDir(dir);
    SaveLoad_ReadSlotNames();
    uint8_t *slot = (uint8_t *)far_host(ds.SaveLoad_SlotTable);
    memcpy(slot + 1, "\x05" "Cheat", 6);
    freeze(true, true);
    Options_Frame();
    int32_t saved = ds.Game_Credits;
    CHECK(SaveLoad_Transfer(true));
    CHECK(file_size(path) == SAVELOAD_FILE_SIZE);
    ds.Game_Credits = saved + 12345;
    freeze(false, true);
    Options_Frame();
    freeze(true, true);
    Options_Frame();  // now frozen at saved + 12345
    SaveLoad_ReadSlotNames();
    CHECK(SaveLoad_Transfer(false));
    CHECK(ds.Game_Credits == saved);
    Options_Frame();
    CHECK(ds.Game_Credits == saved);
    ds.Game_Credits -= 99;
    Options_Frame();
    CHECK(ds.Game_Credits == saved);

    // A save from an earlier port version, with the cheats trailer, loads.
    SDL_IOStream *io = SDL_IOFromFile(path, "ab");
    CHECK(io && SDL_WriteIO(io, "RNCH\x01\x03", 6) == 6);
    SDL_CloseIO(io);
    SaveLoad_ReadSlotNames();
    CHECK(SaveLoad_Transfer(false));
    CHECK(ds.Game_Credits == saved);
    freeze(false, false);
    Options_Frame();

    check_fixes();

    SDL_RemovePath(path);
    SDL_RemovePath(dir);
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
