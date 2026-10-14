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
    .cargo_planet_cap = true,
    .station_storage = true,
    .garrison_losses = true,
    .colony_setups = true,
    .demolish_droids = true,
    .building_limit = true,
    .spy_reports = true,
    .league_tip = true,
};

Fixes fixes = ALL;

const SwitchInfo Fixes_List[] = {
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
    {"cargo_planet_cap", "Loading cargo",
     "A ship takes on as much ore as it has room for, whatever the storage.",
     &fixes.cargo_planet_cap},
    {"station_storage", "Miner stations",
     "Bodies with only a Miner station keep mining after a conquest.",
     &fixes.station_storage},
    {"garrison_losses", "Garrison losses",
     "After a space battle each garrison loses the ships it really lost.",
     &fixes.garrison_losses},
    {"colony_setups", "Colony set-up",
     "Two new colonies get their starter buildings on time.", &fixes.colony_setups},
    {"demolish_droids", "Demolishing mines",
     "A demolished mine's droid goes back into the colony's stock.", &fixes.demolish_droids},
    {"building_limit", "Building limit",
     "At 1,000 buildings a new one is refused before it is paid for.", &fixes.building_limit},
    {"spy_reports", "Spy reports",
     "Spy reports on armies, garrisons and weapons name the race.", &fixes.spy_reports},
    {"league_tip", "League tip",
     "The bartender still sells the League tip after the League attacks.", &fixes.league_tip},
};

const int Fixes_Count = (int)SDL_arraysize(Fixes_List);

void Fixes_Init(void)
{
    for (int i = 0; i < Fixes_Count; i++)
        *Fixes_List[i].value = config_fix(Fixes_List[i].key);
}
