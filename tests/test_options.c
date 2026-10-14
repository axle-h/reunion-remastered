// options: the game side of the port menu. Freezing holds a stock from the
// frame it is switched on, the add cheats raise it, a load re-takes the
// frozen value, saves have no cheats trailer any more, and the fixes' and
// improvements' tables cover every switch and follow the config.
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "game/dseg.h"
#include "game/main_program.h"
#include "game/fixes.h"
#include "game/improvements.h"
#include "game/options.h"
#include "game/saveload.h"
#include "game/system.h"
#include "platform/config.h"
#include "platform/fs.h"
#include "platform/video.h"

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
        const SwitchInfo *f = &Fixes_List[i];
        CHECK((uint8_t *)f->value >= (uint8_t *)&fixes
              && (uint8_t *)f->value < (uint8_t *)&fixes + sizeof fixes);
        CHECK(SDL_strlen(f->label) <= 20 && SDL_strlen(f->help) <= 72);
        CHECK(SDL_strlen(f->key) < CONFIG_KEY_SIZE);
        for (int j = 0; j < i; j++)
            CHECK(f->value != Fixes_List[j].value && SDL_strcmp(f->key, Fixes_List[j].key) != 0);
    }

    // On without a config; the config turns one off.
    Fixes_Init();
    for (int i = 0; i < Fixes_Count; i++)
        CHECK(*Fixes_List[i].value);
    config_set_fix("mine_all_systems", false);
    Fixes_Init();
    CHECK(!fixes.mine_all_systems && fixes.find_jade);
    // As the menu does it: applied at the main loop's next frame.
    config_set_fix("mine_all_systems", true);
    config_set_fix("find_jade", false);
    Options_Request(OPTIONS_SWITCHES);
    CHECK(!fixes.mine_all_systems);
    Options_Frame();
    CHECK(fixes.mine_all_systems && !fixes.find_jade);
    config_reset();
    Fixes_Init();
}

static void check_improvements(void)
{
    // Every field of Improvements is listed once, with a short label.
    CHECK(Improvements_Count * sizeof(bool) == sizeof(Improvements));
    for (int i = 0; i < Improvements_Count; i++) {
        const SwitchInfo *f = &Improvements_List[i];
        CHECK((uint8_t *)f->value >= (uint8_t *)&improvements
              && (uint8_t *)f->value < (uint8_t *)&improvements + sizeof improvements);
        CHECK(SDL_strlen(f->label) <= 20 && SDL_strlen(f->help) <= 72);
        CHECK(SDL_strlen(f->key) < CONFIG_KEY_SIZE);
        for (int j = 0; j < i; j++)
            CHECK(f->value != Improvements_List[j].value
                  && SDL_strcmp(f->key, Improvements_List[j].key) != 0);
        for (int j = 0; j < Fixes_Count; j++)
            CHECK(SDL_strcmp(f->key, Fixes_List[j].key) != 0);
    }

    // On without a config, off for a scripted run; the config turns one
    // off, applied at the main loop's next frame.
    Improvements_Init(false);
    for (int i = 0; i < Improvements_Count; i++)
        CHECK(*Improvements_List[i].value);
    Improvements_Init(true);
    for (int i = 0; i < Improvements_Count; i++)
        CHECK(!*Improvements_List[i].value);
    Improvements_Init(false);
    config_set_improvement("more_stations", false);
    Options_Request(OPTIONS_SWITCHES);
    CHECK(improvements.more_stations);
    Options_Frame();
    CHECK(!improvements.more_stations);
    config_reset();
    Improvements_Init(true);
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
    system_init();
    Game_InitGameData();
    ds.Message_Log = game_alloc(0x31B);
    config_reset();
    Options_Init();
    CHECK(!Options_GameRunning());

    // The disk screen keeps the original's five buttons.
    CHECK(ds.Ui_Screens[0x0C - 1].IconCount == 5);

    // Frozen money: spending is undone, income kept and then held; unfreezing
    // keeps it.
    ds.Game_Credits = 1000;
    freeze(true, false);
    Options_Frame();
    CHECK(Options_GameRunning());
    ds.Game_Credits -= 300;
    Options_Frame();
    CHECK(ds.Game_Credits == 1000);
    ds.Game_Credits += 50;
    Options_Frame();
    CHECK(ds.Game_Credits == 1050);
    ds.Game_Credits -= 50;
    Options_Frame();
    CHECK(ds.Game_Credits == 1050);
    // Two clicks before the next frame: both count, and raise the frozen value.
    Options_Request(OPTIONS_ADD_MONEY_ONCE);
    Options_Request(OPTIONS_ADD_MONEY_ONCE);
    ds.Game_Credits -= 5000;
    Options_Frame();
    CHECK(ds.Game_Credits == 1050 + 2 * OPTIONS_ADD_MONEY);
    freeze(false, false);
    ds.Game_Credits -= 200;
    Options_Frame();
    CHECK(ds.Game_Credits == 850 + 2 * OPTIONS_ADD_MONEY);
    Options_Request(OPTIONS_ADD_MONEY_ONCE);
    Options_Frame();
    CHECK(ds.Game_Credits == 850 + 3 * OPTIONS_ADD_MONEY);

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
    // Ore unloaded or mined at New Earth stays, and is then held.
    ds.Game_HomeStock[1] += 2500;
    Options_Frame();
    CHECK(ds.Game_HomeStock[1] == 200 + OPTIONS_ADD_ORE + 2500);
    ds.Game_HomeStock[1] -= 1000;
    Options_Frame();
    CHECK(ds.Game_HomeStock[1] == 200 + OPTIONS_ADD_ORE + 2500);

    // A save of a game with a cheat on is the original's size; loading it
    // holds the loaded stock, not the one before.
    char dir[128], path[256];
    SDL_snprintf(dir, sizeof dir, "/tmp/reunion_test_options_%d", (int)SDL_GetCurrentTime(nullptr) & 0xFFFF);
    SDL_snprintf(path, sizeof path, "%s/spidysav.1", dir);
    SaveLoad_SetSaveDir(dir);
    SaveLoad_ReadSlotNames();
    uint8_t *slot = ds.SaveLoad_SlotTable;
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
    check_improvements();

    SDL_RemovePath(path);
    SDL_RemovePath(dir);
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
