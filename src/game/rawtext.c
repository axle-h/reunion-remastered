#include "game/rawtext.h"

#include <SDL3/SDL.h>

#include "game/dseg.h"
#include "game/system.h"
#include "platform/fs.h"
#include "platform/video.h"

// GetMem(size) and BlockRead of the whole file; a short or missing file is a
// run-time error (File not found, 2) as the CheckIO after Reset would give.
static FarPtr load_raw(const char *path, uint16_t size)
{
    FarPtr p = System_GetMem(size);
    size_t got = 0;
    uint8_t *data = fs_load(path, &got);
    if (data == nullptr)
        System_RunError(2);
    SDL_memcpy(far_host(p), data, SDL_min(got, (size_t)size));
    SDL_free(data);
    return p;
}

// 4049:0011
void RawText_LoadSzFace(void)
{
    ds.RawText_SzFace = load_raw("text\\sz_face.raw", 0x7B0);
}

// 4049:0083
void RawText_LoadSzTalal(void)
{
    ds.RawText_SzTalal = load_raw("text\\sz_talal.raw", 0x1DAB);
}

// 4049:00f3
void RawText_LoadSzFaj(void)
{
    ds.RawText_SzFaj = load_raw("text\\sz_faj.raw", 0x1440);
}

// 4049:0153
void RawText_Nop(void)
{
}

// 4049:0159
void RawText_Init(void)
{
}

// 4049:0160: `rep outsb` of (last - first + 1) * 3 bytes to port 3C9h after
// 3C8h := first; the DAC index advances every 3 bytes and wraps at 256.
void RawText_SetPaletteRange(int16_t first, int16_t last, FarPtr pal)
{
    uint16_t count = (uint16_t)((last - first + 1) * 3);
    uint16_t entry = (uint8_t)first, comp = 0;
    for (uint16_t i = 0; i < count; i++) {
        uint8_t *src = far_host(far_ptr((uint16_t)(pal >> 16), (uint16_t)((uint16_t)pal + first * 3 + i)));
        vid_palette[entry][comp] = *src & 0x3F;
        if (++comp == 3) {
            comp = 0;
            entry = (entry + 1) & 0xFF;
        }
    }
}
