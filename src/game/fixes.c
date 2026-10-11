#include "game/fixes.h"

#include <SDL3/SDL.h>

static constexpr Fixes ALL = {
    .find_jade = true,
    .mine_all_systems = true,
    .meteorite_droids = true,
    .build_on_moons = true,
    .derricks_on_moons = true,
    .drifting_ship = true,
    .disband_list_count = true,
    .annihilate_leaves_screen = true,
    .autoplace_when_full = true,
};

Fixes fixes = ALL;

void Fixes_Init(void)
{
    const char *env = SDL_getenv("REUNION_ORIGINAL");
    if (env && *env && *env != '0')
        Fixes_Original();
    else
        fixes = ALL;
}

void Fixes_Original(void)
{
    fixes = (Fixes){0};
}
