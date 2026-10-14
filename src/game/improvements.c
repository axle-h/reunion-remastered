#include "game/improvements.h"

#include <SDL3/SDL.h>

#include "platform/config.h"

Improvements improvements;

const SwitchInfo Improvements_List[] = {
    {"more_stations", "More Miner stations",
     "Up to 4 Miner stations on a body, mining every hour, kept by a colony.",
     &improvements.more_stations},
    {"repeat_build", "Repeat build (Shift)",
     "Shift+click places a building and stays in build mode for another.",
     &improvements.repeat_build},
};

const int Improvements_Count = (int)SDL_arraysize(Improvements_List);

void Improvements_Init(bool scripted)
{
    for (int i = 0; i < Improvements_Count; i++)
        *Improvements_List[i].value = !scripted && config_improvement(Improvements_List[i].key);
}
