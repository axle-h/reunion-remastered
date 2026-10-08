// audio: music and sound-effect mixing, run on SDL's dummy audio driver.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "platform/audio.h"
#include "platform/fs.h"

static int16_t buf[AUDIO_RATE * 2];

static int peak(const int16_t *s, int frames)
{
    int p = 0;
    for (int i = 0; i < frames * 2; i++)
        p = SDL_max(p, SDL_abs(s[i]));
    return p;
}

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
    // Keep the device callback out of the way; audio_mix renders directly.
    audio_set_paused(true);
    bool ok = true;

    // NO.SPD is the "no music" track: one empty pattern (just a speed
    // command), though it still carries all the samples of MAIN1.
    CHECK(audio_play_music("ANIM\\NO.SPD", false));
    CHECK(audio_music_playing());
    audio_mix(buf, AUDIO_RATE);
    CHECK(peak(buf, AUDIO_RATE) == 0);
    // 64 rows at speed 6, 50 ticks/s = 7.68 s, then it stops (no loop).
    for (int i = 0; i < 6; i++)
        audio_mix(buf, AUDIO_RATE);
    CHECK(audio_music_playing());
    audio_mix(buf, AUDIO_RATE);
    CHECK(!audio_music_playing());

    // One second of real music (MAIN1 and FAILURE fade in, ATV1 doesn't).
    CHECK(audio_play_music("ANIM\\ATV1.SPD", true));
    audio_mix(buf, AUDIO_RATE);
    int music_peak = peak(buf, AUDIO_RATE);
    printf("ATV1.SPD peak over 1 s: %d\n", music_peak);
    CHECK(music_peak > 10000);

    audio_set_music_volume(0.0f);
    audio_mix(buf, AUDIO_RATE / 10);
    CHECK(peak(buf, AUDIO_RATE / 10) == 0);
    audio_set_music_volume(1.0f);

    CHECK(!audio_play_music("ANIM\\NOSUCH.SPD", true));
    CHECK(audio_music_playing()); // a failed load keeps the old music
    audio_stop_music();
    CHECK(!audio_music_playing());
    audio_mix(buf, AUDIO_RATE / 10);
    CHECK(peak(buf, AUDIO_RATE / 10) == 0);

    // 0.1 s of a full-scale 8-bit square wave at 11025 Hz is 4410 output frames.
    static uint8_t square[1102];
    for (size_t i = 0; i < sizeof square; i++)
        square[i] = (i / 25) & 1 ? 0xff : 0x00;
    int v = audio_play_sfx(square, sizeof square, 11025);
    CHECK(v == 0);
    audio_mix(buf, 4000);
    int sfx_peak = peak(buf, 4000);
    printf("square wave peak: %d\n", sfx_peak);
    CHECK(sfx_peak >= 32000);
    CHECK(audio_sfx_playing(v));
    audio_mix(buf, 1000);
    CHECK(!audio_sfx_playing(v));
    audio_mix(buf, 1000);
    CHECK(peak(buf, 1000) == 0);

    audio_set_sfx_volume(0.5f);
    audio_play_sfx(square, sizeof square, 11025);
    audio_mix(buf, 1000);
    CHECK(SDL_abs(peak(buf, 1000) - sfx_peak / 2) < 200);
    audio_set_sfx_volume(1.0f);

    // Nine sounds on eight voices: the oldest one (voice 0) is cut off.
    audio_stop_all();
    for (int i = 0; i < AUDIO_VOICES; i++)
        CHECK(audio_play_sfx(square, sizeof square, 11025) == i);
    CHECK(audio_play_sfx(square, sizeof square, 11025) == 0);
    audio_stop_all();
    for (int i = 0; i < AUDIO_VOICES; i++)
        CHECK(!audio_sfx_playing(i));

    audio_shutdown();
    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
