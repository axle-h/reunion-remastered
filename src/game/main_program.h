// The main program body (1000:01a1 Main), from after the unit initialisers:
// start-up (title, start menu, new-game state) and the event loop that polls
// the mouse once per retrace, runs the hourly simulation, frees the buffers of
// the screen being left and dispatches to the current screen's handler.
//
// Screens (Game_CurrentScreen; the menu-bar command that opens one is in
// parentheses). Clock: T the game clock runs, F it is stopped, - as left.
//    1 command centre (24), T        19 unit assignment, F: after a won
//    2 hire (14), T                     space battle or before a ground one
//    3 research (2), T               20 planet surface (41), T
//    4 droids (8), T                 21 new group, F: from the group lists
//    5 shop / production (23), T     22 groups (49), T
//    7 star map (34), T              23 pub (45), T
//    8 base efficiency (9), T        24 advisor, F: from the command centre
//   10 credits (6), F                25 pub dialogue, F: a person in 23
//   11 message log (4), T            28 colonisation, F
//   12 disk (1), F                   29 space battle, F: ends on 7 or 19
//   13 cargo (3), T                  32 ground battle, F: from 19
//   16 ship info (13), T             33 story interstitial, F, then 1
//   17 group control panel (16), T   34 alien talk, F
//   18 destination map, F            35 death ending, 36 victory: Halt
//                                    37 colony list (74), -
//                                    39 route editor, T: the port's
//                                       (game/routes.h), from 17
// Ids 6, 9, 14, 15, 26, 27, 30, 31 and 38 are unused (command 7 opens the
// empty screen 9, a dead end).
//
// Timing: Game_Ticks counts retraces while the clock runs and the hour
// passes at Ticks > 100, so a game hour is 101 retraces (1.44 s at 70.09 Hz,
// 34.6 s a day). The clock button sets Ticks to 100 (95 with the right
// button), one hour per press or repeat.
#pragma once

#include <stdint.h>

// 1000:0205 onwards. Never returns in the original (it ends with Halt). In the
// port it returns only if timing_wait_retrace() reports a quit request and no
// quit handler unwound the stack first (tests).
void Game_Main(void);

// The unit initialisers that head Main (1000:01a1..0200), in the original
// order. Units the port replaces by platform code (Crt, Gus, SoundDrv,
// AdlibMusic) are skipped. Needs fs_init, dseg_init, system_init.
void Game_InitUnits(void);

// 4719:0325 (unit Ems's initialiser): allocates the game data block
// (Ems_Buffer, 8770h bytes) and points Game_Groups, Game_Buildings and
// Gfx_Charset into it (Buildings = Groups + 2840h, Charset = Groups + 5EF0h).
// The original moved the block into EMS when an EMM was installed (TEmsBlock
// state 2, mapped before every use); the port keeps it on the heap (state 1),
// as on a DOS machine without an EMM.
void Game_InitGameData(void);

enum : int16_t {
    BUILDINGS_DOS = 1000,  // the data block's table, all the DOS game has
    // The improvement's cap (no_building_limit): building indices are
    // int16_t, and the loops `for (i = 1; i <= count; i++)` must end.
    BUILDINGS_MAX = INT16_MAX - 1,
};
// The buildings the game may hold now: BUILDINGS_DOS, or BUILDINGS_MAX with
// no_building_limit on. A game loaded with more keeps them, but takes no new
// one.
int16_t Game_BuildingLimit(void);
// Port addition (no_building_limit): room in Game_Buildings for n records.
// Past BUILDINGS_DOS the table moves out of the game data block to a heap
// array that grows; records 1-1,000 of the block are then stale. False past
// BUILDINGS_MAX.
bool Game_ReserveBuildings(int32_t n);
// The table is on that heap array.
bool Game_BuildingsMoved(void);

// The pub dialogue's opening menu (Pub_MenuIdx) for Pub_Person, from the
// story flags (part of screen 25's set-up).
void Game_PickPubMenu(void);

// Port addition for test fixtures (tests/play/): retraces spent in the main
// loop's wait for an event, i.e. with the current screen set up and taking
// clicks. Not part of the game state.
extern uint64_t Game_EventWaitFrames;
