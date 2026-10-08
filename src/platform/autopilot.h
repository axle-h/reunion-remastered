// Scripted input for headless runs and tests. The script comes from the
// environment variable REUNION_AUTOPILOT (or autopilot_set_script) and lists
// events by retrace number (counted from the first retrace after the script
// was set), separated by ';':
//
//   FRAME:click:X,Y    left button down at (X,Y), released 4 frames later
//   FRAME:rclick:X,Y   the same with the right button
//   FRAME:move:X,Y     moves the mouse
//   FRAME:key:N        queues ReadKey code N (decimal, e.g. 13 = Enter)
//   FRAME:dump:PATH    saves the frame shown at that retrace (.png or .bmp)
//   FRAME:quit         requests quit (as closing the window)
//   random:SEED:FRAMES[:START]
//                      random clicks, drags and key presses from frame START
//                      (default 0), seeded and deterministic; quits at frame
//                      FRAMES. Mixes with the events above (tools/soak.sh).
//
// While a script is active timing_wait_retrace() does not sleep, so runs go
// as fast as the CPU allows; frames still count one per retrace.
#pragma once

#include <stdint.h>

// Reads REUNION_AUTOPILOT once. Returns true if a script is active.
bool autopilot_init(void);
// Replaces the script (nullptr or "" turns the autopilot off).
void autopilot_set_script(const char *script);
bool autopilot_active(void);
// In random mode: true, with *seed = SEED, so the game's Randomize can be
// made deterministic too.
bool autopilot_random_seed(uint32_t *seed);
// Called by timing_wait_retrace() after input_pump() for retrace `frame`
// (timing_frames()).
void autopilot_frame(uint64_t frame);
