// Port addition: the game side of the port menu (platform/menu.h). The
// menu only changes the config and asks for changes; Options_Frame carries
// them out at the main loop, where the game is between two frames. (The
// music is switched at once instead, see main.c.)
//
// Cheats live in the config file and so outlast games and boots. Freezing
// money or ore holds the stock at a value that is taken when the freeze is
// switched on and again whenever the game under it changes (boot, new game,
// load); it is put back at every frame of the main loop, so purchases cost
// nothing. A freeze is only a floor: a rise (income, mining, a ship
// unloading) raises the value held. Unfreezing leaves the stock where it
// is. The add cheats raise the stock (and a frozen value) by a fixed amount
// per click.
#pragma once

#include <stdbool.h>

enum : int {
    OPTIONS_ADD_MONEY = 100000,  // credits per click
    OPTIONS_ADD_ORE = 10000,     // of each ore per click
};

typedef enum : int {
    OPTIONS_CHEATS,     // config.freeze_money / freeze_ore changed
    OPTIONS_ADD_MONEY_ONCE,
    OPTIONS_ADD_ORE_ONCE,
    OPTIONS_FIXES,      // the fixes' switches changed
} OptionsRequest;

// At boot, after dseg_init: no requests, the frozen values to be taken.
void Options_Init(void);
// From the port menu: carried out by the next Options_Frame.
void Options_Request(OptionsRequest request);
// A game has been started or loaded: the frozen values are taken again.
void Options_Rebase(void);
// Once per frame of the main loop: carries out the requests and puts frozen
// stocks back.
void Options_Frame(void);
// The main loop has run since boot (the add cheats need a game).
bool Options_GameRunning(void);
