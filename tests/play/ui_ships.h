// UI drivers for ships, groups, cargo and travel: ship info
// (screen 16), new group (21), group equipment (22), the group's control
// panel (17), cargo transfer (13), the destination map (18) and the star map
// (7). Groups are named by what the ship-info list shows; bodies and systems
// by the map captions.
//
// How the game works here (from src/game/):
// - A group is created docked at New Earth (Groups_ClearGroup). Ships and
//   weapons are put in or taken out only while it is docked at an own colony
//   (screen 22 grid; a left press +1, a right press -1: UI_PRESS, ui.h).
// - Pilots are not attached to groups: one hired pilot flies them all, and
//   every group, carriers included, moves (pilot experience + 1) legs per
//   hour (sim.c ProcessTravellingGroups). A carrier needs no pilot to start
//   but crawls at 1 leg/h without one: 80-99 h to a moon of the same planet,
//   150-199 h to another planet (intel.c SetGroupTarget).
// - Miner stations are not cargo: they are the "Mine" weapon column of a
//   trade group's grid (screen 22). Cargo (screen 13) is ore and the
//   ground/weapon items, only while docked at an own colony.
//
// Every observer reads what a screen shows; the comment names the element.
// Drivers fail the run (play_fail) when the game refuses something they
// assumed possible; the ones returning bool report expected refusals.
#pragma once

#include <stddef.h>
#include <stdint.h>

// --- Observers ------------------------------------------------------------------

enum : int {
    SHIP_NONE = 0,     // no such group
    SHIP_DOCKED = 1,   // "In the dock of <system> system <body>"
    SHIP_ORBIT = 2,    // "In orbit <system> system <body>"
    SHIP_FLYING = 4,   // "Fly to <system> system <body>" (statuses 4-6)
};

// The ship-info list (screen 16) shows a group named `name`: its 1-based
// position in the list, 0 if none.
int ui_group_find(const char *name);
// The group's location line on screen 22 (screen 16's panel has it as
// "Currently on:" / "Destination:"): one of SHIP_DOCKED, SHIP_ORBIT,
// SHIP_FLYING; SHIP_NONE if no such group.
int ui_group_status(const char *name);
// The same line as text, e.g. "In orbit Amnesty system New Earth".
void ui_group_location(const char *name, char *out, size_t n);
// The number shown in the group's grid (screen 22) for `ship`, or for
// `weapon` on the row of `ship` when weapon is not nullptr. Names as the grid
// hotspots caption them (product names, e.g. "Sat.carrier", "Satellite").
int ui_group_units(const char *name, const char *ship, const char *weapon);
// Screen 13: "Space: used/cap" and "Storage space on planet: cap".
void ui_cargo_space(int32_t *used, int32_t *cap, int32_t *planet_cap);
// Screen 13: the ship's (right) and the planet's (left) column for `item`
// (an ore name such as "Energon" or an item such as "Laser cannon").
int32_t ui_cargo_ship(const char *item);
int32_t ui_cargo_planet(const char *item);
// The bodies (planets, or the zoomed planet and its moons) listed on the
// star map or destination screen, as their hotspot captions; returns the
// count.
int ui_map_bodies(char names[][19], int max);
// The system list of the star map / destination screen shows `system`.
bool ui_map_system_listed(const char *system);

// --- Drivers ----------------------------------------------------------------------

// Ship info (screen 16), from anywhere.
void ui_ship_info(void);
// Screen 16 -> NEW UNIT -> screen 21: picks `type` ("Army", "Trade",
// "Carrier") with Change type, names the group `name` (nullptr keeps "New
// Group"), OK,CREATE IT. Ends on screen 22 with the new group. Returns false
// if the type is not offered.
bool ui_new_group(const char *type, const char *name);
// Opens the group from the ship-info list: screen 17 (control panel) or 22
// (equipment).
void ui_open_group(const char *name, int screen);
// Screen 22 "Change name": renames the group (backspaces, types `name`,
// Enter). True when the list then shows the new name.
bool ui_rename_group(const char *group, const char *name);
// Screen 22: sets the grid count of `ship` (weapon nullptr) or of `weapon`
// on the row of `ship` to n. Returns false if the count stops short (stock,
// capacity, or not docked at a colony).
bool ui_equip(const char *group, const char *ship, const char *weapon, int n);
// Screen 17 Launch / Docking. Return false (box closed) on a refusal.
bool ui_take_off(const char *group);
bool ui_dock(const char *group);
// Screen 17 Move -> 18: flies the group (in orbit) to `planet`, or its moon
// `moon`, of `system` (nullptr = the group's system); planet nullptr flies to
// the sun to explore the system. Returns false (box closed) on a refusal,
// true when it flies or (a fast pilot, a short trip) is there already.
bool ui_launch(const char *group, const char *system, const char *planet, const char *moon);
// Screen 13 via screen 17: moves `item` between the group (docked at a
// colony) and the planet: amount > 0 to the ship, < 0 to the planet. Ores
// move up to 100 t per left press until |amount| has moved; SHIP_CARGO_ALL
// moves everything (one right press, capped by the ship's room and the
// planet's storage). Items move one per press. Returns the amount moved.
enum : int32_t { SHIP_CARGO_ALL = 0x7FFFFFFF };
int32_t ui_cargo(const char *group, const char *item, int32_t amount);
// Screen 17 body action: deploys one of the carrier group's satellites
// (ADD SATELLITE) on the body it orbits. Returns false if the button is not
// offered (no satellite aboard, body already has one, or a colony). (A
// satellite from stock: ui_deploy_stock_satellite.)
bool ui_deploy_carrier_satellite(const char *group);
// Screen 22 shows the group's location line as ui_group_location has it
// (checks the observer against the pixels).
bool ui_group_panel_shows(const char *group);
// On screen 22, waits (ui_wait_hours) until the group is in orbit at its
// destination and its line says so.
bool ui_wait_arrival(const char *group, int hours);
