#include "platform/timing.h"

#include <SDL3/SDL.h>

#include "platform/audio.h"
#include "platform/autopilot.h"
#include "platform/input.h"
#include "platform/video.h"

// VGA mode 13h: 25.175 MHz / (800 * 449) = 70.086 Hz.
static const double RETRACE_NS = 1e9 * 800.0 * 449.0 / 25175000.0;

static uint64_t frames;
static uint64_t next_ns;
static void (*quit_handler)(void);
static unsigned idle_polls;

enum : unsigned { IDLE_POLLS_PER_RETRACE = 4 };

void timing_set_quit_handler(void (*fn)(void))
{
    quit_handler = fn;
}

void timing_idle_poll(void)
{
    if (++idle_polls > IDLE_POLLS_PER_RETRACE)
        timing_wait_retrace();
}

// Audio samples owed to the frame clock (fractional).
static double mix_pending;

void timing_reset(void)
{
    mix_pending = 0;
    idle_polls = 0;
}

// Unpaced autopilot runs drive the mixer from the frame clock (the device
// stays paused), so sounds last the same number of frames as in real time.
// Nobody hears them, so nothing is synthesised (audio_advance).
static void mix_one_retrace(void)
{
    audio_set_paused(true);
    mix_pending += AUDIO_RATE * RETRACE_NS / 1e9;
    int n = (int)mix_pending;
    mix_pending -= n;
    audio_advance(n);
}

// Unpaced autopilot runs show at most this many frames a second: presenting
// costs more than a frame of the game and changes nothing it sees.
enum : uint64_t { AUTOPILOT_PRESENT_NS = 1000000000 / 60 };
static uint64_t last_present_ns;

static void present(void)
{
    if (autopilot_active()) {
        uint64_t now = SDL_GetTicksNS();
        if (last_present_ns && now - last_present_ns < AUTOPILOT_PRESENT_NS)
            return;
        last_present_ns = now;
    }
    vid_present();
}

bool timing_wait_retrace(void)
{
    idle_polls = 0;
    present();
    input_pump();
    if (autopilot_init())
        autopilot_frame(frames);

    if (input_quit_requested()) {
        if (quit_handler)
            quit_handler();
        return false;
    }
    if (autopilot_active()) {
        mix_one_retrace();
        frames++;  // headless script: no pacing
        return true;
    }

    uint64_t now = SDL_GetTicksNS();
    if (next_ns == 0 || now > next_ns + (uint64_t)(RETRACE_NS * 4)) {
        // First frame, or we fell far behind (e.g. a window drag): resync
        // instead of rushing to catch up.
        next_ns = now;
    }
    next_ns += (uint64_t)RETRACE_NS;
    if (next_ns > now)
        SDL_DelayPrecise(next_ns - now);
    frames++;
    return !input_quit_requested();
}

uint64_t timing_frames(void)
{
    return frames;
}

bool timing_wait_frames(int n)
{
    for (int i = 0; i < n; i++)
        if (!timing_wait_retrace())
            return false;
    return true;
}
