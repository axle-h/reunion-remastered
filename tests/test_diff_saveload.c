// Differential test of unit SaveLoad (1a3e): the disk/options screen (set-up,
// buttons, slot selection, a cancelled save, quit), the slot names, the
// buttons and the initial building list. Saving writes a file (the harness
// opens files read-only) and the game data has no saved games, so SaveGame
// is not run and LoadGame only sees empty slots. SaveLoadGame's prototype
// is not in symbols.txt (LoadGame calls it).
// See docs/difftest.md. Needs the game data.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/saveload.h"

#define I(i) ((int16_t)a[i])

DIFF_PROC(SaveLoad_DiskScreen, SaveLoad_DiskScreen());
DIFF_PROC(SaveLoad_LoadGame, SaveLoad_LoadGame());
DIFF_PROC(SaveLoad_LoadInit, SaveLoad_LoadInit());
DIFF_PROC(SaveLoad_ReadSlotNames, SaveLoad_ReadSlotNames());
DIFF_PROC(SaveLoad_PressButton, SaveLoad_PressButton(I(0)));

#define SLOT_SEL (*(int16_t *)ds_at(0x7732))

static void setup_enter(DiffRng *r, int32_t *a)
{
    (void)a;
    ds.Game_ScreenEntered = 1;
    ds.Game_MenuCommand = 0;
    ds.Game_HotspotHit = 0;
    *(uint8_t *)ds_at(0x5DAA) = (uint8_t)diff_pick(r, 0, 0, 2);  // first visit: no fade
    ds.Game_MusicTrack = (int16_t)diff_pick(r, 0, 1, 2);
    ds.Game_SoundFx = (int16_t)diff_pick(r, 0, 1, 2);
}

// On the disk screen: a button, a slot click, a cancelled save or quit.
static void setup_disk(DiffRng *r, int32_t *a)
{
    (void)a;
    ds.Music_Backend = 0;  // the music is not memory-faithful (not compared, like sound)
    ds.Game_ScreenEntered = 0;
    ds.Game_MenuCommand = (int16_t)diff_pick(r, 0, 0, 0, 0x2F, 0x30, 0x49);
    ds.Game_HotspotHit = (int16_t)diff_pick(r, 0, 0x15, 0x16, 0x17, 0x18, 0x19, 0x19, 0x14, 0x1A, 3);
    ds.Mouse_ClickY = (int16_t)diff_pick(r, 0x40, 0x48, 0x49, 0xA0, 0x10, diff_range(r, 0x30, 0xB0));
    ds.Game_MusicTrack = (int16_t)diff_pick(r, 0, 1, 2);
    ds.Game_SoundFx = (int16_t)diff_pick(r, 0, 1, 2);
    SLOT_SEL = (int16_t)diff_range(r, 1, 12);
    if (ds.Game_MenuCommand == 0x30) {  // the name editor: type a little, then Esc
        static const int keys[] = {'a', 8, 'Z', 27};
        int n = diff_range(r, 0, 3);
        diff_set_keys(keys + 3 - n, n + 1);
    }
}

static void setup_load(DiffRng *r, int32_t *a)
{
    (void)a;
    SLOT_SEL = (int16_t)diff_range(r, 1, 12);
}

static void setup_press(DiffRng *r, int32_t *a)
{
    a[0] = diff_pick(r, 0, 1, 2, 3, 4, 5);
}

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    // The port reads saves from the player's directory first: an empty one.
    char dir[1024];
    const char *base = SDL_GetBasePath();
    SDL_snprintf(dir, sizeof dir, "%sdiff_saveload_saves", base ? base : "");
    SaveLoad_SetSaveDir(dir);

    if (!diff_boot_new_game(1500) || !diff_open_output()) {
        printf("FAILED to boot\n");
        return 1;
    }
    DiffState *bases[2];
    bases[0] = diff_save();
    // The disk screen as the main loop enters it (screen 12).
    ds.Game_CurrentScreen = 12;
    ds.Game_ScreenEntered = 1;
    ds.Game_MenuCommand = 0;
    ds.Game_HotspotHit = 0;
    SaveLoad_DiskScreen();
    ds.Game_ScreenEntered = 0;
    diff_quiesce();
    bases[1] = diff_save();

    // The data file variable (DS:91EE, disk.ani): the port keeps it on the host.
    static const DiffRange file_rec[] = {DIFF_DS(0x91EE, 0x80), {0, 0}};
    diff_SaveLoad_DiskScreen.ignore = file_rec;
    diff_run(&diff_SaveLoad_DiskScreen, bases, 1, setup_enter, diff_cases(6));
    diff_run(&diff_SaveLoad_DiskScreen, bases + 1, 1, setup_disk, diff_cases(60));
    diff_run(&diff_SaveLoad_PressButton, bases + 1, 1, setup_press, diff_cases(12));
    diff_run(&diff_SaveLoad_ReadSlotNames, bases, 2, nullptr, diff_cases(4));
    diff_run(&diff_SaveLoad_LoadGame, bases + 1, 1, setup_load, diff_cases(4));
    diff_run(&diff_SaveLoad_LoadInit, bases, 2, nullptr, diff_cases(4));
    diff_free(bases[0]);
    diff_free(bases[1]);
    return diff_finish();
}
