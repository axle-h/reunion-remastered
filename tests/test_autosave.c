// Improvement (game/improvements.h, autosave): game/autosave.h without the
// main loop. The hour-start copy is what a battle or talk writes, once per
// hour; the seventh midnight since the last write writes too, and any write
// starts the week again; the scenes don't count; the quick-save key writes
// the game as it is, but not with the clock stopped or outside the main
// loop's wait; the disk screen lists the file as a 13th row and loads it.
// With the switch off nothing is written and the screen has its 12 rows.
// Needs the game data (SAVE\INIT).
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "game/autosave.h"
#include "game/dseg.h"
#include "game/improvements.h"
#include "game/main_program.h"
#include "game/saveload.h"
#include "game/sim.h"
#include "game/system.h"
#include "platform/fs.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static char dir[128], path[256];

static bool written(void)
{
    return SDL_GetPathInfo(path, nullptr);
}

static void forget(void)
{
    SDL_RemovePath(path);
}

// One game hour as the main loop runs it, as far as the autosave goes.
static void hour(void)
{
    Autosave_HourStart();
    Sim_AdvanceHour();
    ds.Game_Credits += 1000;  // the hour's work
}

// The autosave's row of the disk screen, loaded as LOAD does. False if it
// holds no save.
static bool load_autosave(void)
{
    SaveLoad_ReadSlotNames();
    ds.SaveLoad_SelectedSlot = AUTOSAVE_SLOT;
    if (ds.SaveLoad_SlotTable[(AUTOSAVE_SLOT - 1) * 0x16] == 0)
        return false;
    SaveLoad_LoadGame();
    return true;
}

static void new_game(void)
{
    SaveLoad_LoadInit();
    ds.Game_Credits = 120000;
    ds.Game_Year = 2927; ds.Game_Month = 8; ds.Game_Day = 13; ds.Game_Hour = 23;
    ds.Game_ClockRunning = 1;
    Autosave_Reset();
}

static void test_switch_off(void)
{
    improvements.autosave = false;
    new_game();
    for (int h = 0; h < 24 * 8; h++)
        hour();
    Autosave_ScreenEntered(34);
    Autosave_QuickSaveKey(true);
    Autosave_Frame();
    CHECK(!written());
    int16_t y, rows;
    SaveLoad_SlotArea(&y, &rows);
    CHECK(y == 0x40 && rows == 12);
}

static void test_triggers(void)
{
    improvements.autosave = true;
    new_game();
    int16_t y, rows;
    SaveLoad_SlotArea(&y, &rows);
    CHECK(y == AUTOSAVE_ROWS_Y && rows == 13);

    // Not yet written: the row is listed by its name, and doesn't load.
    CHECK(!load_autosave());
    const uint8_t *row = ds.SaveLoad_SlotTable + (AUTOSAVE_SLOT - 1) * 0x16;
    CHECK(row[1] == 20 && memcmp(row + 2, "Autosave------------", 20) == 0);

    // A talk writes the copy of its hour's start, not the state at the screen.
    hour();
    hour();
    CHECK(!written());
    int32_t start_credits = ds.Game_Credits - 1000;
    int16_t start_hour = ds.Game_Hour - 1;
    Autosave_ScreenEntered(34);
    CHECK(written());
    // Once per hour: the ground battle after it writes nothing.
    forget();
    Autosave_ScreenEntered(19);
    Autosave_ScreenEntered(32);
    CHECK(!written());
    Autosave_ScreenEntered(34);
    hour();
    Autosave_ScreenEntered(33);  // a scene
    Autosave_ScreenEntered(1);
    CHECK(!written());
    Autosave_ScreenEntered(29);
    CHECK(written());
    CHECK(load_autosave());
    CHECK(ds.Game_Hour == start_hour + 1 && ds.Game_Credits == start_credits + 1000);
    row = ds.SaveLoad_SlotTable + (AUTOSAVE_SLOT - 1) * 0x16;
    CHECK(row[1] == 18 && memcmp(row + 2, "Autosave 2927-8-14", 18) == 0);

    // The load took a copy: the next battle in the hour writes it.
    forget();
    Autosave_ScreenEntered(29);
    CHECK(written());
    CHECK(load_autosave());
    CHECK(ds.Game_Hour == start_hour + 1 && ds.Game_Credits == start_credits + 1000);

    // The week: the seventh midnight since the last write writes its
    // hour-start copy (23:00), the sixth doesn't.
    forget();
    int midnights = 0;
    while (midnights < 6) {
        hour();
        midnights += ds.Game_Hour == 0;
    }
    for (int h = 0; h < 23; h++)
        hour();
    CHECK(!written());
    int16_t day = ds.Game_Day;
    hour();
    CHECK(written() && ds.Game_Hour == 0);
    CHECK(load_autosave());
    CHECK(ds.Game_Day == day && ds.Game_Hour == 23);

    // The battle after it, a day later, starts the week again.
    for (int h = 0; h < 24; h++)
        hour();
    Autosave_ScreenEntered(29);
    forget();
    for (int h = 0; h < 24 * 6; h++)
        hour();
    CHECK(!written());
    for (int h = 0; h < 24; h++)
        hour();
    CHECK(written());

    // The quick-save key: the game as it is, while the main loop waits with
    // the clock running.
    forget();
    hour();
    ds.Game_Credits = 777;
    Autosave_QuickSaveKey(false);
    Autosave_Frame();
    CHECK(!written());
    ds.Game_ClockRunning = 0;
    Autosave_QuickSaveKey(true);
    Autosave_Frame();
    CHECK(!written());
    ds.Game_ClockRunning = 1;
    Autosave_Frame();  // the refused press is gone
    CHECK(!written());
    Autosave_QuickSaveKey(true);
    Autosave_Frame();
    CHECK(written());
    ds.Game_Credits = 1;
    CHECK(load_autosave() && ds.Game_Credits == 777);

    // The row only loads; with the switch off it isn't listed.
    improvements.autosave = false;
    SaveLoad_ReadSlotNames();
    int16_t slot_y;
    SaveLoad_SlotArea(&slot_y, &rows);
    CHECK(rows == 12);
    improvements.autosave = true;
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

    // Never touch the real save directory.
    SDL_snprintf(dir, sizeof dir, "/tmp/reunion_test_autosave_%d", (int)SDL_GetCurrentTime(nullptr) & 0xFFFF);
    SaveLoad_SetSaveDir(dir);
    SDL_snprintf(path, sizeof path, "%s/reunion.auto", dir);

    test_switch_off();
    test_triggers();

    forget();
    SDL_RemovePath(dir);
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
