// Frame pacing. The original paces everything off the VGA's vertical retrace
// (mode 13h refreshes at 70.086 Hz); every place it waited for retrace calls
// timing_wait_retrace() instead.
#pragma once

#include <stdint.h>

// Shows the current frame, handles window/input events, and waits until the
// next 1/70.086 s boundary. Returns false once the user asked to quit (after
// calling the quit handler, if one is set and returns).
bool timing_wait_retrace(void);

// Called by timing_wait_retrace() when the user asked to quit (window
// closed). The game sets one that unwinds to main() with longjmp, so every
// waiting loop ends without checking the return value. nullptr: none.
void timing_set_quit_handler(void (*fn)(void));

// Called by timing_wait_retrace() when the port menu asked for a restart;
// like the quit handler, it does not return. nullptr: none (the request
// waits).
void timing_set_restart_handler(void (*fn)(void));

// Called by timing_wait_retrace() once the frame's events (and autopilot
// input) are in: the game's stand-in for the INT 33h callback, which on DOS
// moved the cursor whatever the game was doing. nullptr: none.
void timing_set_mouse_hook(void (*fn)(void));

// For code that polls the mouse in a tight loop without waiting for retrace
// (fine on DOS, where the INT 33h callback updated the state): every few
// calls without a retrace in between, waits for one, so events are pumped,
// the screen is shown and the loop takes real time. Mouse_Update calls it.
void timing_idle_poll(void);

// Forgets the previous boot's sub-frame state (audio samples owed to the
// frame clock, idle polls), so every boot of a scripted run plays the same
// as the first boot of a fresh process. The retrace count goes on.
void timing_reset(void);

// Retrace count since start-up (the original's frame counter equivalent).
uint64_t timing_frames(void);

// Waits for n retraces (Crt.Delay-style waits are converted to retraces).
bool timing_wait_frames(int n);
