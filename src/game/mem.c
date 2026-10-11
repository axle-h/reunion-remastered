#include "game/mem.h"

#include <SDL3/SDL.h>

#include "platform/video.h"

uint8_t mem[MEM_SIZE];

void mem_init(void)
{
    SDL_memset(mem, 0, sizeof mem);
    SDL_memset(vid_screen, 0, 0x10000);
}
