// Playthrough runner example and speed check (tests/play/run.h): a new game,
// then the command centre with the clock running, watched every retrace as
// a policy waiting on a condition does. Prints the fixture's frames per
// second and the game hours per retrace; PLAY_BENCH_FRAMES (default 20000)
// sets how long it watches. Needs the game data.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/dseg.h"
#include "game/main_program.h"
#include "play/run.h"
#include "play/ui.h"

static int game_hours(void)
{
    return ((ds.Game_Year * 12 + ds.Game_Month) * 31 + ds.Game_Day) * 24 + ds.Game_Hour;
}

// Messages (e.g. the observatory finding a planet, then asking for its name)
// stop the main loop, and with it the clock, until answered.
static struct { uint64_t last; int still; } loop;

static bool message_shown(void *unused)
{
    (void)unused;
    loop.still = Game_EventWaitFrames == loop.last ? loop.still + 1 : 0;
    loop.last = Game_EventWaitFrames;
    return loop.still >= 10;
}

static void start(void)
{
    ui_new_game();
}

static void watch(void)
{
    const char *env = SDL_getenv("PLAY_BENCH_FRAMES");
    int frames = env && *env ? SDL_atoi(env) : 20000;
    uint64_t f0 = play_frame(), t0 = SDL_GetTicksNS();
    int h0 = game_hours(), stops = 0;
    while (play_frame() - f0 < (uint64_t)frames) {
        if (!play_wait_until(message_shown, nullptr, frames - (int)(play_frame() - f0)))
            break;
        stops++;
        play_key(13);  // dismisses a message, accepts a suggested name
        play_wait(2);
        loop.still = 0;
    }
    double s = (double)(SDL_GetTicksNS() - t0) / 1e9;
    uint64_t df = play_frame() - f0;
    int dh = game_hours() - h0;
    play_log("bench: %llu frames in %.2f s (%.0f frames/s), %d game hours (%.0f frames/hour), "
             "%d clock stops", (unsigned long long)df, s, (double)df / s, dh,
             dh ? (double)df / dh : 0.0, stops);
}

static const PlayStep steps[] = {
    {"start", start},
    {"watch", watch},
};

int main(int argc, char *argv[])
{
    return play_main(argc, argv,
                     &(PlayPlan){.name = "play_bench", .steps = steps,
                                 .count = SDL_arraysize(steps), .expect_exit = -1});
}
