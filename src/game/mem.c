#include "game/mem.h"

#include <SDL3/SDL.h>

#include "platform/video.h"

uint8_t mem[MEM_SIZE];

void mem_init(void)
{
    SDL_memset(mem, 0, sizeof mem);
    vid_screen = (uint8_t (*)[VID_WIDTH])(mem + MEM_VIDEO_SEG * 16);
}
