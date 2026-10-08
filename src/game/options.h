// Port addition: the Options panel on the disk screen (an OPTIONS button in
// the disk operations toolbar) and the playtest cheats it switches.
//
// Cheats belong to a game: every boot starts with them off, and a save of a
// game with one on carries their state in a trailer after the original
// blocks (docs/formats/save.md), so loading it brings them back. They live
// outside the emulated memory.
//
// Freezing money or ore holds the stock where it was when it was switched on
// (put back at every frame of the main loop), so purchases cost nothing.
// Unfreezing leaves it there. The add buttons raise the stock (and a frozen
// value) by a fixed amount per click.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum : int {
    // Toolbar commands the original never uses (0x3D has a blank label).
    OPTIONS_COMMAND = 0x3D,  // OPTIONS on the disk screen
    OPTIONS_BACK = 0x3F,     // BACK TO DISK OPS on the options panel
    // Toolbar icon number past ICON.ALL's 68: Options_Icon (GfxPath_DrawIcon).
    OPTIONS_ICON = 0x44,
    // Save trailer: "RNCH", version, flags.
    OPTIONS_SAVE_SIZE = 4 + 1 + 1,
    OPTIONS_ADD_MONEY = 100000,  // credits per click
    OPTIONS_ADD_ORE = 10000,     // of each ore per click
};

typedef struct Options {
    bool freeze_money;  // Game_Credits held
    bool freeze_ore;    // Game_HomeStock (New Earth's ore) held
} Options;

extern Options options;

// The OPTIONS icon, palette indices like a decoded ICON.ALL icon
// (options_icon.c, made by tools/icon_png.py).
extern const uint8_t Options_Icon[24][40];

// At boot, after dseg_init: cheats off, OPTIONS added to the disk screen's
// toolbar (screen 0Ch) and the BACK TO DISK OPS command set up.
void Options_Init(void);
void Options_SetFreezeMoney(bool on);
void Options_SetFreezeOre(bool on);
// OPTIONS_ADD_MONEY credits; OPTIONS_ADD_ORE of each of New Earth's ores.
void Options_AddMoney(void);
void Options_AddOre(void);
// Once per frame of the main loop: puts frozen stocks back.
void Options_ApplyCheats(void);
// The save trailer: Options_SaveState returns 0 (no trailer) when no cheat
// is on. Options_LoadState with fewer than OPTIONS_SAVE_SIZE bytes or no
// "RNCH" turns the cheats off.
size_t Options_SaveState(uint8_t out[OPTIONS_SAVE_SIZE]);
void Options_LoadState(const uint8_t *in, size_t size);
