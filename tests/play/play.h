// Playthrough fixture (docs/playthrough.md): boots the game in-process and
// runs a policy that plays it through simulated mouse and keyboard input
// only.
//
// The policy runs on its own thread with a strict hand-off: at each retrace
// (autopilot_set_controller) the game thread wakes the policy and waits until
// it yields with play_wait(), so exactly one of them runs at a time and a run
// is deterministic for a seed. Policies read top to bottom: click, wait for
// what the screen shows, click again.
//
// Rules the fixture enforces:
// - Input only: every input goes through play_mouse()/play_key(), which also
//   record it. The emulated memory is hashed around each policy slice and any
//   change fails the run (write barrier).
// - No cheats: the run fails if the Options panel (playtest cheats) opens.
// Policies decide only on what a player can see (docs/playthrough.md); that
// part is a review rule, not checked here.
//
// The recording is a plain autopilot script (seed, mouse and key events by
// frame). play_replay() runs one without any policy code; it must reach the
// same end.
#pragma once

#include <stdint.h>

typedef void (*PlayPolicy)(void *arg);

typedef struct {
    int exit_code;        // Halt code, or -1 if the game quit (window close)
    uint64_t frames;      // retraces played (script frame numbering)
    uint64_t state_hash;  // hash of the emulated memory at the end
    bool failed;          // the policy called play_fail or broke a rule
    char failure[256];
} PlayResult;

// Starts a new boot of the game seeded with `seed` and plays `policy` from
// the first retrace. The game runs until it halts or quits; the policy ends
// it with play_quit() or play_fail(). The input is recorded as an autopilot
// script in `record_path` (may be nullptr).
PlayResult play_run(uint32_t seed, PlayPolicy policy, void *arg, const char *record_path);
// Boots and plays an autopilot script (e.g. "@path") with no policy.
PlayResult play_replay(const char *script);

// --- Policy side: call only from the policy thread. ---

// Retrace number counted from the start of the run.
uint64_t play_frame(void);
// Gives the game `frames` retraces (at least 1).
void play_wait(int frames);
// Waits until pred(arg) holds, checking once per retrace, for at most
// `timeout` retraces. Returns whether it holds.
bool play_wait_until(bool (*pred)(void *arg), void *arg, int timeout);
// Sets the mouse: position in game pixels, buttons bit 0 left, bit 1 right.
void play_mouse(int x, int y, int buttons);
// Left click: press for 4 retraces, release, then 2 retraces.
void play_click(int x, int y);
// Queues a Crt.ReadKey code (0 then a scan code for extended keys).
void play_key(int code);
// Ends the run as a window close would.
[[noreturn]] void play_quit(void);
// Ends the run as failed.
[[noreturn, gnu::format(printf, 1, 2)]] void play_fail(const char *fmt, ...);
[[gnu::format(printf, 1, 2)]] void play_log(const char *fmt, ...);
// Saves the screen as $PLAY_DUMP_DIR/<frame>-<name>.png when PLAY_DUMP_DIR
// is set (debugging policies).
[[gnu::format(printf, 1, 2)]] void play_dump(const char *fmt, ...);
