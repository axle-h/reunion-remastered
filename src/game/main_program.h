// The main program body (1000:01a1 Main), from after the unit initialisers:
// start-up (title, start menu, new-game state) and the event loop that polls
// the mouse once per retrace, runs the hourly simulation, frees the buffers of
// the screen being left and dispatches to the current screen's handler. See
// docs/screens.md.
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

// Port addition for test fixtures (tests/play/): retraces spent in the main
// loop's wait for an event, i.e. with the current screen set up and taking
// clicks. Not part of the game state.
extern uint64_t Game_EventWaitFrames;
