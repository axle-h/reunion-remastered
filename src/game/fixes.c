#include "game/fixes.h"

#include <SDL3/SDL.h>

#include "platform/config.h"

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
    .away_staff_hidden = true,
};

Fixes fixes = ALL;

const FixInfo Fixes_List[] = {
    {"find_jade", "Jade found in time",
     "Jade is found by 1 April 2928 at the latest, so the game can be won.", &fixes.find_jade},
    {"mine_all_systems", "All systems mine",
     "Every system's colonies and droids mine, not only the selected one's.",
     &fixes.mine_all_systems},
    {"meteorite_droids", "Meteorite droids",
     "A meteorite takes a droid from the colony it hit.", &fixes.meteorite_droids},
    {"build_on_moons", "Building on moons",
     "Buildings on a moon are built at the moon's speed.", &fixes.build_on_moons},
    {"derricks_on_moons", "Derricks on moons",
     "A Derrick on a moon fills the moon's store, not the planet's.", &fixes.derricks_on_moons},
    {"drifting_ship", "Drifting ship",
     "The drifting Morgrul ship is found by a group arriving at Phoenix 1.",
     &fixes.drifting_ship},
    {"disband_list_count", "Disbanding groups",
     "Disbanding a group no longer wipes or doubles other fleets.", &fixes.disband_list_count},
    {"annihilate_leaves_screen", "Annihilation",
     "The annihilation first leaves screens showing what it destroys.",
     &fixes.annihilate_leaves_screen},
    {"autoplace_when_full", "New colony buildings",
     "A new colony's buildings are kept when its surface is shown and full.",
     &fixes.autoplace_when_full},
    {"away_staff_hidden", "Staff at university",
     "A staff member at the university stays out of the command centre.",
     &fixes.away_staff_hidden},
};

const int Fixes_Count = (int)SDL_arraysize(Fixes_List);

void Fixes_Init(void)
{
    for (int i = 0; i < Fixes_Count; i++)
        *Fixes_List[i].value = config_fix(Fixes_List[i].key);
}
