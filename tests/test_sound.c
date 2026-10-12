// sound/music: game-facing units on the dummy audio driver.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/dseg.h"
#include "game/music.h"
#include "game/sound.h"
#include "platform/audio.h"
#include "platform/fs.h"

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
    bool ok = true;

    Sound_Init();
    CHECK(Sound_DeviceOpen());
    CHECK(ds.Sound_Enabled);
    CHECK(ds.Game_SoundFx == 2);
    CHECK(!Sound_IsPlaying());

    // PlaySfx only loads; StartSample starts.
    Sound_PlaySfx("SELECT1");
    CHECK(!Sound_IsPlaying());
    Sound_StartSample();
    CHECK(Sound_IsPlaying());
    CHECK(ds.Sound_SbStatus != 0);
    Sound_StopSbVoice();
    CHECK(!Sound_IsPlaying() && ds.Sound_SbStatus == 0);

    Sound_PlayButtonSfx("SELECT2");
    CHECK(Sound_IsPlaying());
    Sound_StopAndFreeSample();

    Sound_PlayWarSnd(1);
    CHECK(Sound_IsPlaying());
    Sound_PlayGrSnd(1);
    CHECK(ds.Sound_LastWarSnd == 1);
    // Alloc waits for the voice; the sample is about a second long.
    Sound_AllocWarSndBuf();
    CHECK(!Sound_IsPlaying());

    Sound_PreloadSelectSfx();
    ds.Sound_DeviceType = 2; // select sfx are not used with a GUS
    Sound_PreloadSelectSfx();
    Sound_PlaySelectSfx(2);
    CHECK(Sound_IsPlaying());
    Sound_FreeSelectSfx();
    CHECK(!Sound_IsPlaying());
    ds.Sound_DeviceType = 1;

    // Music: GUS backend plays ANIM\<name>.SPD.
    Music_Init();
    CHECK(ds.Music_Backend == 0 && ds.Music_MainThemeOn);
    Music_InitMusicDevices();
    CHECK(ds.Music_Backend == 2 && ds.Game_MusicTrack == 1);
    ds.Game_MusicTrack = 1;
    Music_ApplyMusicTrack();
    CHECK(audio_music_playing());
    CHECK(ds.SoundDrv_ModLoaded);
    Music_StopMusic();
    CHECK(!audio_music_playing());
    Music_PlayTrack("atv1x");
    CHECK(!audio_music_playing() && !ds.Music_MainThemeOn);
    Music_PlayTrack("atv1");
    CHECK(audio_music_playing() && !ds.Music_MainThemeOn);
    Music_RestartMainTheme();
    CHECK(audio_music_playing() && ds.Music_MainThemeOn);
    Music_StopMusic();

    // AdLib backend: silent, no crash.
    ds.Music_Backend = 1;
    Music_PlayTrack("main1");
    CHECK(!audio_music_playing());

    audio_shutdown();
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
