// Scripted input for headless runs and tests. The script comes from the
// environment variable REUNION_AUTOPILOT (or autopilot_set_script) and lists
// events by retrace number (counted from the first retrace after the script
// was set), separated by ';':
//
//   FRAME:click:X,Y    left button down at (X,Y), released 4 frames later
//   FRAME:rclick:X,Y   the same with the right button
//   FRAME:move:X,Y     moves the mouse
//   FRAME:mouse:X,Y,B  sets the mouse exactly: position and buttons (bit 0
//                      left, bit 1 right, bit 2 Shift held), held until the
//                      next mouse event
//   FRAME:key:N        queues ReadKey code N (decimal, e.g. 13 = Enter)
//   FRAME:dump:PATH    saves the frame shown at that retrace (.png or .bmp)
//   FRAME:quit         requests quit (as closing the window)
//   seed:N             seeds the game's Randomize with N instead of the clock
//   random:SEED:FRAMES[:START]
//                      random clicks, drags and key presses from frame START
//                      (default 0), seeded and deterministic; quits at frame
//                      FRAMES. Mixes with the events above (tools/soak.sh).
//
// Events may come in any order; same-frame events play in script order. A
// script starting with '@' names a file holding the script, where newlines
// also separate events and lines starting with '#' are comments.
//
// While a script is active timing_wait_retrace() does not sleep, so runs go
// as fast as the CPU allows; frames still count one per retrace.
#pragma once

#include <stddef.h>
#include <stdint.h>

// Reads REUNION_AUTOPILOT once. Returns true if a script is active.
bool autopilot_init(void);
// Replaces the script (nullptr or "" turns the autopilot off).
void autopilot_set_script(const char *script);
bool autopilot_active(void);
// In random mode: true, with *seed = SEED, so the game's Randomize can be
// made deterministic too.
bool autopilot_random_seed(uint32_t *seed);
// SEED from "seed:N", else from random mode: the value Randomize uses.
bool autopilot_seed(uint32_t *seed);
// Calls `fn` at every retrace while a script is active, after the script's
// events for that frame, with the frame number counted as the script's are.
// Test fixtures drive the game from here (tests/play/).
void autopilot_set_controller(void (*fn)(uint64_t frame));
// Called by timing_wait_retrace() after input_pump() for retrace `frame`
// (timing_frames()).
void autopilot_frame(uint64_t frame);
// Frame of the script's last event (0 if none).
uint64_t autopilot_last_event(void);
// Requests quit at script frame `frame` if the run is still going (0: no
// limit), so a replay that never halts ends. Cleared by the next script.
void autopilot_set_frame_limit(uint64_t frame);
// The run was ended by the frame limit.
bool autopilot_limit_reached(void);
// Hash of a memory image: the end state reported for scripted runs (the
// playthrough fixture, reunion --replay), so the two can be compared.
uint64_t autopilot_hash(const void *data, size_t size);
