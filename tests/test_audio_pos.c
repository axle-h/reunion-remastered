// audio: module position reporting, on SDL's dummy audio driver.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "platform/audio.h"
#include "platform/fs.h"

static int16_t buf[AUDIO_RATE * 2];

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("FAILED line %d: %s\n", __LINE__, #cond);              \
            ok = false;                                                     \
        }                                                                   \
    } while (0)

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    SDL_setenv_unsafe("SDL_AUDIO_DRIVER", "dummy", 1);
    if (!audio_init()) {
        printf("no audio, skipping\n");
        return 77;
    }
    audio_set_paused(true);
    bool ok = true;
    int order = -1, row = -1;

    CHECK(!audio_music_position(&order, &row));
    CHECK(!audio_music_ended());

    // NO.SPD: one 64-row pattern at speed 6 (0.12 s per row), no loop.
    CHECK(audio_play_music("ANIM\\NO.SPD", false));
    CHECK(audio_music_position(&order, &row));
    CHECK(order == 0 && row == 0);
    audio_mix(buf, AUDIO_RATE);
    CHECK(audio_music_position(&order, &row));
    printf("after 1 s: order %d row %d\n", order, row);
    CHECK(order == 0 && row >= 7 && row <= 9);
    CHECK(audio_music_position(nullptr, nullptr));
    CHECK(!audio_music_ended());
    for (int i = 0; i < 7; i++)
        audio_mix(buf, AUDIO_RATE);
    CHECK(audio_music_ended());
    CHECK(audio_music_position(&order, &row));

    // A looping song with several orders: the order advances, and wrapping
    // is reported.
    CHECK(audio_play_music("ANIM\\ATV1.SPD", true));
    CHECK(!audio_music_ended());
    int last = 0;
    for (int s = 0; s < 120 && !audio_music_ended(); s++) {
        audio_mix(buf, AUDIO_RATE);
        audio_music_position(&order, &row);
        last = SDL_max(last, order);
    }
    printf("highest order seen: %d\n", last);
    CHECK(last > 0);

    audio_stop_music();
    CHECK(!audio_music_position(&order, &row));
    audio_shutdown();
    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
