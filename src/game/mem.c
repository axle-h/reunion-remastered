#include "game/mem.h"

#include <SDL3/SDL.h>

#include "game/dseg.h"
#include "platform/video.h"

uint8_t mem[MEM_SIZE];

FarPtr far_from_host(const void *p)
{
    const uint8_t *b = p, *d = (const uint8_t *)&ds;
    if (b >= d && b < d + sizeof ds)
        return far_ptr(MEM_DS_SEG, (uint16_t)(b - d));
    if (b < mem || b >= mem + MEM_SIZE)
        return 0;
    uint32_t lin = (uint32_t)(b - mem);
    return far_ptr((uint16_t)(lin >> 4), (uint16_t)(lin & 15));
}

void mem_init(void)
{
    SDL_memset(mem, 0, sizeof mem);
    SDL_memset(&ds, 0, sizeof ds);
    SDL_memset(vid_screen, 0, 0x10000);
}
