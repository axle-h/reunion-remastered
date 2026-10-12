#include "game/adlibmusic.h"

#include <SDL3/SDL.h>

#include "game/adlib.h"
#include "game/dseg.h"
#include "game/system.h"
#include "platform/fs.h"

enum : uint16_t { TUNE_BUF_SIZE = 0x7BD4 };

// The original allocated the tune buffer on its heap when it found an AdLib
// (ds.AdlibMusic_TuneBuf). Here it is the port's own, so the heap and the
// memory the game has left are the same whatever music it started with, and
// the port menu can switch to AdLib at any time. ds.AdlibMusic_TuneBuf stays
// nil.
static uint8_t tune_buf[TUNE_BUF_SIZE];

// 4265:000c
void AdlibMusic_LoadMusicFile(const char *name)
{
    if (!ds.AdlibMusic_Present)
        return;
    char path[6 + 255 + 5];
    SDL_snprintf(path, sizeof path, "grwar\\%s.pic", name);
    size_t size = 0;
    uint8_t *data = fs_load(path, &size);
    if (data == nullptr)
        System_RunError(2, path); // CheckIO after Reset: File not found
    if (size < TUNE_BUF_SIZE + 1) {
        uint8_t *buf = tune_buf;
        SDL_memset(buf, 0, TUNE_BUF_SIZE);
        SDL_memcpy(buf, data, size);
        uint16_t n = (uint16_t)size;
        for (uint16_t i = 1; i <= n / 2; i++) {
            uint8_t t = buf[i - 1];
            buf[i - 1] = buf[n - i];
            buf[n - i] = t;
        }
        for (uint16_t i = 0; i < n; i++)
            buf[i] = (uint8_t)(buf[i] + 0x2F);
    }
    SDL_free(data);
}

// 4265:0290
void AdlibMusic_StartAdlibTune(void)
{
    Adlib_StartPlayer(tune_buf, sizeof tune_buf);
}

// 4265:0296
void AdlibMusic_StopAdlibTune(void)
{
    Adlib_StopPlayer();
}

// 4265:022d
void AdlibMusic_StopAdlib(void)
{
    if (ds.AdlibMusic_Present)
        AdlibMusic_StopAdlibTune();
}

// 4265:01d6. The original then counts to 50 in an empty loop.
void AdlibMusic_PlayMusic(const char *name)
{
    if (!ds.AdlibMusic_Present)
        return;
    AdlibMusic_StopAdlib();
    AdlibMusic_LoadMusicFile(name);
    AdlibMusic_StartAdlibTune();
}

// 4265:0242, without the heap allocation (tune_buf).
void AdlibMusic_InitMusic(void)
{
    ds.AdlibMusic_Present = Adlib_DetectAdlib() != -1;
    ds.AdlibMusic_Inited = true;
}

const uint8_t *AdlibMusic_Tune(void)
{
    return tune_buf;
}

// 4265:027c
void AdlibMusic_Init(void)
{
    ds.AdlibMusic_Present = false;
    ds.AdlibMusic_Inited = false;
}
