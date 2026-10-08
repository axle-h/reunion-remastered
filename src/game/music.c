#include "game/music.h"

#include <SDL3/SDL.h>
#include <ctype.h>
#include <string.h>

#include "game/adlibmusic.h"
#include "game/dseg.h"
#include "game/sound.h"
#include "platform/audio.h"

static bool pstr_eq(const uint8_t *p, const char *s)
{
    return p[0] == strlen(s) && memcmp(p + 1, s, p[0]) == 0;
}

// Pascal string literal for the two track names.
#define PSTR(s) ((const uint8_t *)("\x05" s))

// 424a:0095
void Music_StopMusic(void)
{
    if (ds.Music_Backend == 1)
        AdlibMusic_StopAdlib();
    if (ds.Music_Backend == 2) {
        audio_stop_music();
        ds.SoundDrv_ModLoaded = false;
    }
}

// 424a:000f
void Music_PlayTrack(const uint8_t *name)
{
    ds.Music_MainThemeOn = pstr_eq(name, "main1") || pstr_eq(name, "main2") || pstr_eq(name, "no");
    Music_StopMusic();
    if (ds.Music_Backend == 1)
        AdlibMusic_PlayMusic(name);
    if (ds.Music_Backend == 2) {
        char path[5 + 255 + 5];
        unsigned n = 0;
        memcpy(path, "ANIM\\", 5);
        n = 5;
        for (unsigned i = 0; i < name[0]; i++)
            path[n++] = (char)toupper(name[1 + i]);
        memcpy(path + n, ".SPD", 5);
        ds.SoundDrv_ModLoaded = audio_play_music(path, true);
    }
}

// Port extension: REUNION_MUSIC=mod or =adlib picks the music hardware
// (GUS modules or AdLib tunes) whatever SAVE\SETUP.SPD says; the sound
// effects still follow the setup file.
static uint8_t music_device_type(void)
{
    const char *pick = SDL_getenv("REUNION_MUSIC");
    if (pick && SDL_strcasecmp(pick, "mod") == 0)
        return 1;
    if (pick && SDL_strcasecmp(pick, "adlib") == 0)
        return 4;
    return ds.Sound_DeviceType;
}

// 424a:00b2
void Music_InitMusicDevices(void)
{
    // GusEnable probed the GUS; here it is ready when the audio device
    // opened. AdlibMusic_InitMusic does the same for the emulated OPL2.
    uint8_t type = music_device_type();
    if (type == 1)
        ds.SoundDrv_GusReady = Sound_DeviceOpen();
    if (!ds.SoundDrv_GusReady && (type == 2 || type == 3 || type == 4 || type == 6))
        AdlibMusic_InitMusic();
    if (ds.AdlibMusic_Present)
        ds.Music_Backend = 1;
    if (ds.SoundDrv_GusReady)
        ds.Music_Backend = 2;
    ds.Game_MusicTrack = ds.Music_Backend > 0;
}

// 424a:0137
void Music_ApplyMusicTrack(void)
{
    if (ds.Game_MusicTrack == 0) {
        if (ds.Music_Backend == 1)
            Music_StopMusic();
        if (ds.Music_Backend == 2)
            Music_PlayTrack((const uint8_t *)"\x02no");
    }
    if (ds.Game_MusicTrack == 1)
        Music_PlayTrack(PSTR("main1"));
    if (ds.Game_MusicTrack == 2)
        Music_PlayTrack(PSTR("main2"));
}

// 424a:017e
void Music_RestartMainTheme(void)
{
    if (!ds.Music_MainThemeOn)
        Music_ApplyMusicTrack();
    ds.Music_MainThemeOn = true;
}

// 424a:0193
void Music_Init(void)
{
    ds.Music_Backend = 0;
    ds.Music_MainThemeOn = true;
}
