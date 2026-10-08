#include "game/sound.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "assets/smp.h"
#include "game/dseg.h"
#include "platform/audio.h"
#include "platform/fs.h"

static bool device_open;
static int voice = -1; // the single "SB voice"
static struct smp sample; // 89c6: current sample
static struct smp warsnd; // 89d6: WARSND/GRSND buffer
static struct smp select_sfx[4]; // [1..3] preloaded SELECTn

bool Sound_DeviceOpen(void)
{
    return device_open;
}

// Mirrors the SB driver clearing its status word when a VOC ends.
static void refresh_status(void)
{
    if (voice >= 0 && !audio_sfx_playing(voice))
        voice = -1;
    ds.Sound_SbStatus = voice >= 0;
}

// 431a:0192
bool Sound_IsSbPlaying(void)
{
    refresh_status();
    return ds.Sound_SbStatus != 0;
}

bool Sound_IsPlaying(void)
{
    return Sound_IsSbPlaying();
}

// Stops the previous voice (the SB had only one) and starts s.
static void start_voice(const struct smp *s)
{
    if (voice >= 0)
        audio_stop_sfx(voice);
    voice = s->samples ? audio_play_sfx(s->samples, s->length, (int)s->rate) : -1;
    ds.Sound_SbStatus = voice >= 0;
}

static void stop_voice(void)
{
    if (voice >= 0)
        audio_stop_sfx(voice);
    voice = -1;
    ds.Sound_SbStatus = 0;
}

// Pascal string to a NUL-terminated C string (at most 255 chars).
static void pstr(const uint8_t *p, char out[256])
{
    memcpy(out, p + 1, p[0]);
    out[p[0]] = '\0';
}

// 431a:01a8
void Sound_StopAndFreeSample(void)
{
    if (ds.Sound_Enabled && ds.Sound_DeviceType != 1 && sample.samples) {
        stop_voice();
        smp_free(&sample);
    }
}

// 431a:01e6
void Sound_FreeSampleIfIdle(void)
{
    if (ds.Sound_Enabled && ds.Sound_DeviceType != 1 && !Sound_IsSbPlaying())
        Sound_StopAndFreeSample();
}

// 431a:0209
void Sound_LoadSample(const uint8_t *name, uint16_t offset, uint16_t size)
{
    if (!ds.Sound_Enabled)
        return;
    if (sample.samples)
        Sound_StopAndFreeSample();
    // The GUS branch only differed in where the data went; both end up here.
    char path[256];
    pstr(name, path);
    size_t total;
    uint8_t *data = fs_load(path, &total);
    if (!data)
        return;
    size_t len = size ? size : total - SDL_min(total, offset);
    if ((size_t)offset + len > total)
        len = total - SDL_min(total, offset);
    smp_free(&sample);
    smp_decode(data + offset, len, &sample);
    ds.Sound_SampleSize = (uint16_t)len;
    SDL_free(data);
}

// 431a:031a
void Sound_PlayButtonSfx(const uint8_t *name)
{
    Sound_PlaySfx(name);
    Sound_StartSample();
}

// 431a:0353
void Sound_PlaySfx(const uint8_t *name)
{
    if (!ds.Sound_Enabled)
        return;
    // "SOUND\" + name + ".SMP" as a Pascal string
    uint8_t full[256];
    unsigned n = 6;
    memcpy(full + 1, "SOUND\\", 6);
    memcpy(full + 1 + n, name + 1, name[0]);
    n += name[0];
    if (n + 4 > 255)
        return;
    memcpy(full + 1 + n, ".SMP", 4);
    full[0] = (uint8_t)(n + 4);
    Sound_LoadSample(full, 0, 0);
}

// 431a:03a8
void Sound_StartSample(void)
{
    if (ds.Sound_Enabled && sample.samples)
        start_voice(&sample);
}

// 431a:03e8
void Sound_AllocWarSndBuf(void)
{
    if (!ds.Sound_Enabled)
        return;
    // The original spun on the SB status word; the poll ends as soon as the
    // mixer reports the voice finished (or immediately without a device).
    while (Sound_IsSbPlaying())
        SDL_Delay(1);
    Sound_StopAndFreeSample();
    smp_free(&warsnd);
    ds.Sound_LastWarSnd = 0;
}

// 431a:0424
void Sound_FreeWarSndBuf(void)
{
    if (!ds.Sound_Enabled)
        return;
    stop_voice();
    smp_free(&warsnd);
}

// Shared body of PlayWarSnd and PlayGrSnd; they differ only in the directory.
static void play_numbered(int16_t n, const char *prefix)
{
    if (!ds.Sound_Enabled)
        return;
    // BUG? The cache key is shared by WARSND and GRSND, so GrSnd n right
    // after WarSnd n replays the war sample. Kept as in the original.
    // The extra !warsnd.samples test covers n == 0 right after AllocWarSndBuf,
    // where the original played an uninitialised buffer.
    if (n < 0 || ds.Sound_LastWarSnd != n || !warsnd.samples) {
        char path[64];
        snprintf(path, sizeof path, "%s%d.SMP", prefix, n);
        struct smp loaded;
        if (!smp_load(path, &loaded))
            return;
        smp_free(&warsnd);
        warsnd = loaded;
    }
    ds.Sound_LastWarSnd = n;
    stop_voice();
    start_voice(&warsnd);
}

// 431a:0468
void Sound_PlayWarSnd(int16_t n)
{
    play_numbered(n, "SOUND2\\WARSND");
}

// 431a:05ca
void Sound_PlayGrSnd(int16_t n)
{
    play_numbered(n, "SOUND3\\GRSND");
}

// 431a:059e
void Sound_StopSbVoice(void)
{
    if (ds.Sound_Enabled && ds.Sound_DeviceType != 1)
        stop_voice();
}

// 431a:0712
void Sound_PreloadSelectSfx(void)
{
    if (!ds.Sound_Enabled || ds.Sound_DeviceType == 1)
        return;
    for (int i = 1; i <= 3; i++) {
        char path[32];
        snprintf(path, sizeof path, "SOUND\\SELECT%d.SMP", i);
        smp_free(&select_sfx[i]);
        if (!smp_load(path, &select_sfx[i]))
            return; // the original stopped at the first missing file
    }
}

// 431a:08aa
void Sound_FreeSelectSfx(void)
{
    if (!ds.Sound_Enabled || ds.Sound_DeviceType == 1)
        return;
    for (int i = 1; i <= 3; i++)
        smp_free(&select_sfx[i]);
    stop_voice();
}

// 431a:0903
void Sound_PlaySelectSfx(int32_t index)
{
    if (!ds.Sound_Enabled || ds.Sound_DeviceType == 1)
        return;
    if (index >= 1 && index <= 3 && select_sfx[index].samples)
        start_voice(&select_sfx[index]);
}

// 431a:09c1
void Sound_Init(void)
{
    uint8_t *setup = nullptr;
    size_t size = 0;
    setup = fs_load("SAVE\\SETUP.SPD", &size);
    if (setup && size >= 1) {
        ds.Sound_DeviceType = setup[0];
        // Bytes 1..6 are the SB port/IRQ/DMA words; no use without the driver.
    } else {
        ds.Sound_DeviceType = 1; // no setup file: behave like a GUS
    }
    SDL_free(setup);

    ds.Sound_Enabled = ds.Sound_DeviceType == 1 || ds.Sound_DeviceType == 2 ||
                       ds.Sound_DeviceType == 3 || ds.Sound_DeviceType == 6;
    // Stands in for the "Sound Blaster detected" / GUS detection.
    device_open = audio_init();
    if (!device_open)
        ds.Sound_Enabled = false;
    ds.Game_SoundFx = ds.Sound_Enabled ? 2 : 0;
    ds.Sound_SbStatus = 0;
    ds.Sound_LastWarSnd = 0;
    voice = -1;
}
