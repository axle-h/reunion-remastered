// adlib: the AdLib tunes (GRWAR\*.PIC) through the 428e player and the
// emulated OPL2, rendered with audio_mix on SDL's dummy audio driver.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/adlib.h"
#include "game/adlibmusic.h"
#include "game/dseg.h"
#include "game/mem.h"
#include "game/music.h"
#include "game/sound.h"
#include "game/system.h"
#include "platform/audio.h"
#include "platform/config.h"
#include "platform/fs.h"

static int16_t buf[AUDIO_RATE * 2];

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("FAILED line %d: %s\n", __LINE__, #cond);              \
            ok = false;                                                     \
        }                                                                   \
    } while (0)

// Renders `seconds` and returns the peak; *rms gets the RMS over all of it.
static int render(int seconds, double *rms)
{
    int p = 0;
    double sum = 0;
    for (int s = 0; s < seconds; s++) {
        audio_mix(buf, AUDIO_RATE);
        for (int i = 0; i < AUDIO_RATE * 2; i++) {
            p = SDL_max(p, SDL_abs(buf[i]));
            sum += (double)buf[i] * buf[i];
        }
    }
    if (rms)
        *rms = SDL_sqrt(sum / ((double)seconds * AUDIO_RATE * 2));
    return p;
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    mem_init();
    if (!dseg_init()) {
        printf("no REUNION.PRG, skipping\n");
        return 77;
    }
    system_init();
    const int32_t heap_start = system_heap_free();
    SDL_setenv_unsafe("SDL_AUDIO_DRIVER", "dummy", 1);
    Sound_Init();
    if (!Sound_DeviceOpen()) {
        printf("no audio, skipping\n");
        return 77;
    }
    // Keep the device callback out of the way; audio_mix renders directly.
    audio_set_paused(true);
    bool ok = true;

    // A Sound Blaster setup selects the AdLib tunes, as in the original.
    ds.Sound_DeviceType = 2;
    Music_Init();
    AdlibMusic_Init();
    Music_InitMusicDevices();
    CHECK(ds.AdlibMusic_Present && ds.AdlibMusic_Inited);
    // The tune buffer is not on the emulated heap.
    CHECK(ds.AdlibMusic_TuneBuf == 0 && system_heap_free() == heap_start);
    CHECK(ds.Music_Backend == 1 && ds.Game_MusicTrack == 1);
    CHECK(!Adlib_Playing());

    // Un-obfuscated MAIN1 starts with its order list 0, 1, 2, ...
    AdlibMusic_LoadMusicFile((const uint8_t *)"\x05" "main1");
    const uint8_t *tune = AdlibMusic_Tune();
    CHECK(tune[0] == 0 && tune[1] == 1 && tune[2] == 2 && tune[11] == 11);

    // Silence before anything plays (the mixer skips an untouched OPL).
    CHECK(render(1, nullptr) == 0);

    Music_ApplyMusicTrack(); // track 1 = main1
    CHECK(Adlib_Playing());
    double rms;
    int p = render(5, &rms);
    printf("main1: peak %d, rms %.0f\n", p, rms);
    CHECK(p > 2000 && rms > 300);

    // Stopping keys everything off with the fastest release.
    Music_StopMusic();
    CHECK(!Adlib_Playing());
    render(1, nullptr);
    CHECK(render(1, nullptr) < 50);

    static const char *const names[] = { "atv1", "atv2", "atv3", "atv4", "atv5", "choise",
                                         "earth", "failure", "main2", "space", "talk" };
    for (size_t i = 0; i < SDL_arraysize(names); i++) {
        uint8_t name[16];
        name[0] = (uint8_t)SDL_strlen(names[i]);
        SDL_memcpy(name + 1, names[i], name[0]);
        Music_PlayTrack(name);
        p = render(4, &rms);
        printf("%s: peak %d, rms %.0f\n", names[i], p, rms);
        CHECK(Adlib_Playing());
        CHECK(p > 1000 && rms > 100);
    }
    Music_StopMusic();

    // The music volume applies to the OPL too.
    Music_PlayTrack((const uint8_t *)"\x05" "main2");
    render(2, nullptr);
    audio_set_music_volume(0);
    render(1, nullptr); // the 512-frame chunk in progress may still be loud
    CHECK(render(1, nullptr) == 0);
    audio_set_music_volume(1);
    Music_StopMusic();

    // A key off directly followed by a key on (how the player restarts a
    // note) must retrigger a percussive instrument that has died away.
    audio_opl_write(0x20, 0x01); // modulator: multiple 1
    audio_opl_write(0x23, 0x01); // carrier: percussive envelope
    audio_opl_write(0x40, 0x3F);
    audio_opl_write(0x43, 0x00);
    audio_opl_write(0x60, 0xFF);
    audio_opl_write(0x63, 0xF8); // fast attack, fast decay
    audio_opl_write(0x80, 0xFF);
    audio_opl_write(0x83, 0xFF);
    audio_opl_write(0xC0, 0x00);
    audio_opl_write(0xA0, 0x6B);
    audio_opl_write(0xB0, 0x31); // key on, block 4
    CHECK(render(1, nullptr) > 1000);
    CHECK(render(1, nullptr) < 50); // died away
    audio_opl_write(0xB0, 0x00);
    audio_opl_write(0xB0, 0x31);
    CHECK(render(1, nullptr) > 1000);
    audio_opl_write(0xB0, 0x00);

    // The port menu's music choice picks the modules even with an SB setup.
    config.music = CONFIG_MUSIC_GUS;
    ds.Music_Backend = 0;
    Music_InitMusicDevices();
    CHECK(ds.Music_Backend == 2);

    // The port menu switches the music while a tune plays: the tune goes on
    // on the new hardware (talk is not the main theme, so it is the one).
    config.music = CONFIG_MUSIC_SOUND_BLASTER;
    Music_SwitchDevice();
    CHECK(ds.Music_Backend == 1 && ds.Game_MusicTrack == 1);
    Music_PlayTrack((const uint8_t *)"\x04" "talk");
    CHECK(Adlib_Playing());
    config.music = CONFIG_MUSIC_GUS;
    Music_SwitchDevice();
    CHECK(ds.Music_Backend == 2 && !Adlib_Playing() && audio_music_playing());
    config.music = CONFIG_MUSIC_NONE;
    Music_SwitchDevice();
    CHECK(ds.Music_Backend == 0 && ds.Game_MusicTrack == 0 && !audio_music_playing());
    // From no music back to AdLib, as if the game had started without it.
    ds.AdlibMusic_Inited = false;
    config.music = CONFIG_MUSIC_SOUND_BLASTER;
    Music_SwitchDevice();
    CHECK(ds.Music_Backend == 1 && ds.Game_MusicTrack == 1 && Adlib_Playing());
    CHECK(ds.AdlibMusic_TuneBuf == 0 && system_heap_free() == heap_start);

    // The start menu's hero voice stays held back when the music switches to
    // the modules there, and comes in when let go.
    Music_PlayTrack((const uint8_t *)"\x06" "choise");
    Music_HoldHeroVoice(true);
    config.music = CONFIG_MUSIC_GUS;
    Music_SwitchDevice();
    double held, voice;
    render(2, &held);
    Music_HoldHeroVoice(false);
    render(2, &voice);
    printf("choise: rms %.0f held, %.0f with the voice\n", held, voice);
    CHECK(held < voice * 0.95);
    Music_StopMusic();

    // Before Music_InitMusicDevices a switch is left to it.
    Music_Init();
    Music_SwitchDevice();
    CHECK(ds.Music_Backend == 0 && !audio_music_playing());
    config_reset();

    audio_shutdown();
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
