// audio: muting a module sample and restarting (the "choose your hero" voice
// of ANIM\CHOISE.SPD, sample 6, pattern 0 row 3), on SDL's dummy driver.
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#include "platform/audio.h"
#include "platform/fs.h"

static int16_t buf[AUDIO_RATE * 2];


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

    // Reference: with only sample `n` left audible nothing else is needed;
    // instead compare the whole mix with and without sample 6 over the
    // first bars (the voice sits at row 3).
    long ref = 0, muted = 0, restored = 0;
    audio_play_music("ANIM\\CHOISE.SPD", true);
    for (int i = 0; i < 4; i++) {
        audio_mix(buf, AUDIO_RATE / 2);
        for (int j = 0; j < AUDIO_RATE; j++)
            ref += SDL_abs(buf[j]);
    }
    audio_play_music("ANIM\\CHOISE.SPD", true);
    ok &= audio_music_mute_sample(6, true);
    for (int i = 0; i < 4; i++) {
        audio_mix(buf, AUDIO_RATE / 2);
        for (int j = 0; j < AUDIO_RATE; j++)
            muted += SDL_abs(buf[j]);
    }
    ok &= audio_music_mute_sample(6, false);
    ok &= audio_restart_music();
    for (int i = 0; i < 4; i++) {
        audio_mix(buf, AUDIO_RATE / 2);
        for (int j = 0; j < AUDIO_RATE; j++)
            restored += SDL_abs(buf[j]);
    }
    printf("sum |x| over 2 s: reference %ld, muted %ld, restored %ld\n", ref, muted, restored);
    if (!(muted < ref))
        ok = false, printf("FAILED: mute changed nothing\n");
    if (labs(restored - ref) > ref / 100)
        ok = false, printf("FAILED: restart is not like the original\n");
    audio_shutdown();
    return ok ? 0 : 1;
}
