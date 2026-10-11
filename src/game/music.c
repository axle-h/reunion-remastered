#include "game/music.h"

#include <ctype.h>
#include <string.h>

#include "game/adlibmusic.h"
#include "game/dseg.h"
#include "game/sound.h"
#include "platform/audio.h"
#include "platform/config.h"

static bool pstr_eq(const uint8_t *p, const char *s)
{
    return p[0] == strlen(s) && memcmp(p + 1, s, p[0]) == 0;
}

// Pascal string literal for the two track names.
#define PSTR(s) ((const uint8_t *)("\x05" s))

// The last track started, for Music_SwitchDevice.
static uint8_t last_track[256];

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
    if (name != last_track)
        memcpy(last_track, name, name[0] + 1u);
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

// Port extension: the music hardware is the port menu's (config.music), or
// SAVE\SETUP.SPD's card when the config has none (tests and scripted runs);
// the sound effects always follow the setup file. Amiga is the GUS modules
// through the Paula mixer.
static uint8_t music_device_type(void)
{
    switch (config.music) {
    case CONFIG_MUSIC_NONE: return 0;
    case CONFIG_MUSIC_ADLIB: return 4;
    case CONFIG_MUSIC_GUS:
    case CONFIG_MUSIC_AMIGA: return 1;
    default: return ds.Sound_DeviceType;
    }
}

static bool adlib_type(uint8_t type)
{
    return type == 2 || type == 3 || type == 4 || type == 6;
}

// 424a:00b2
void Music_InitMusicDevices(void)
{
    // GusEnable probed the GUS; here it is ready when the audio device
    // opened. AdlibMusic_InitMusic does the same for the emulated OPL2.
    uint8_t type = music_device_type();
    audio_set_amiga_mixer(config.music == CONFIG_MUSIC_AMIGA);
    if (type == 1)
        ds.SoundDrv_GusReady = Sound_DeviceOpen();
    if (!ds.SoundDrv_GusReady && adlib_type(type))
        AdlibMusic_InitMusic();
    if (ds.AdlibMusic_Present)
        ds.Music_Backend = 1;
    if (ds.SoundDrv_GusReady)
        ds.Music_Backend = 2;
    ds.Game_MusicTrack = ds.Music_Backend > 0;
}

void Music_SwitchDevice(void)
{
    Music_StopMusic();
    bool had_music = ds.Music_Backend > 0;
    uint8_t type = music_device_type();
    audio_set_amiga_mixer(config.music == CONFIG_MUSIC_AMIGA);
    ds.Music_Backend = 0;
    ds.SoundDrv_GusReady = type == 1 && Sound_DeviceOpen();
    if (!ds.SoundDrv_GusReady && adlib_type(type)) {
        AdlibMusic_InitMusic();
        if (ds.AdlibMusic_Present)
            ds.Music_Backend = 1;
    }
    if (ds.SoundDrv_GusReady)
        ds.Music_Backend = 2;
    if (ds.Music_Backend == 0) {
        ds.Game_MusicTrack = 0;
        return;
    }
    if (!had_music)
        ds.Game_MusicTrack = 1;
    if (ds.Music_MainThemeOn || !last_track[0])
        Music_ApplyMusicTrack();
    else
        Music_PlayTrack(last_track);
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
    last_track[0] = 0;
    ds.Music_Backend = 0;
    ds.Music_MainThemeOn = true;
}
