#include "game/fixes.h"

#include <SDL3/SDL.h>

Fixes fixes = {.find_jade = true};

void Fixes_Init(void)
{
    const char *env = SDL_getenv("REUNION_ORIGINAL");
    if (env && *env && *env != '0')
        Fixes_Original();
    else
        fixes = (Fixes){.find_jade = true};
}

void Fixes_Original(void)
{
    fixes = (Fixes){0};
}
