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
// AdlibMusic) are skipped. Needs mem_init, fs_init, dseg_init, system_init.
void Game_InitUnits(void);

// Port addition for test fixtures (tests/play/): retraces spent in the main
// loop's wait for an event, i.e. with the current screen set up and taking
// clicks. Not part of the game state.
extern uint64_t Game_EventWaitFrames;
