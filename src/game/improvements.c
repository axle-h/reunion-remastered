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
    {"trade_routes", "Trade routes",
     "Trade groups haul ore home on routes. Saves then won't load in DOS.",
     &improvements.trade_routes},
    {"keyboard_play", "Keyboard play",
     "Tab, arrows and Enter play every screen; shortcuts in Keys.",
     &improvements.keyboard_play},
    {"no_building_limit", "No building limit",
     "More than 1,000 buildings in the game. Saves then won't load in DOS.",
     &improvements.no_building_limit},
    {"autosave", "Autosave",
     "Saved before battles and talks, weekly and by F11 (Keys).",
     &improvements.autosave},
};

const int Improvements_Count = (int)SDL_arraysize(Improvements_List);

bool Improvements_NeedsPortSaves(int i)
{
    return Improvements_List[i].value == &improvements.trade_routes
           || Improvements_List[i].value == &improvements.no_building_limit;
}

bool Improvements_PortSaves(void)
{
    return improvements.trade_routes || improvements.no_building_limit;
}

void Improvements_Init(bool scripted)
{
    for (int i = 0; i < Improvements_Count; i++)
        *Improvements_List[i].value = !scripted && config_improvement(Improvements_List[i].key);
}
