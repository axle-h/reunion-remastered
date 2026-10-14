// Improvement (game/improvements.h, trade_routes): trade routes. A trade
// group's route lists up to ROUTE_MAX_STOPS pickup stops, bodies the player
// owns, and ends at New Earth. Running, it flies the group to each stop in
// turn, docks there ROUTE_DOCK_HOURS game hours and loads every ore that
// fits; then home, docks as long and unloads it all; and starts again. A
// full group goes home at once; a stop with nothing to load, or home with
// nothing to unload, is passed by. Loading and unloading book as a right
// click on each ore row of the cargo screen does (Cargo_MoveAllOre), so
// what New Earth can't store stays aboard. The flights are the game's own
// (Intel_SetGroupTarget): the pilot, his reach and a Galleon to leave a
// system, as for Move.
//
// The traffic is silent: an arrival adds no log entry, sample or flash
// (Sim_ProcessTravellingGroups), and docking and launching neither. A route
// ends on its own, with one log entry and the reason in its editor's
// footer, when the player gives the group another order (Move, Launch,
// Docking), the group is disbanded or loses a battle, a stop is no longer
// the player's or out of reach.
//
// Routes are stepped each game hour after the travelling groups
// (Routes_Hour), so the docking hours follow the game clock. A route names
// its group by its index in the fleet list: Routes_GroupDisbanded shifts
// them as Intel_DisbandGroup shifts the groups. With the switch off they are
// kept as they are, neither flown nor ended, and the editor and its button
// are gone. The DOS save has no room for them: saves holding one are in the
// port's format (game/saveload.h).
//
// The editor (screen SCREEN_ROUTE, routes.c) is reached by ROUTE on a trade
// group's control panel.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "game/dseg.h"

enum : int {
    ROUTE_MAX_STOPS = 6,
    ROUTE_DOCK_HOURS = 5,
    ROUTE_HOME = 0,  // Route.Next: New Earth
};

typedef enum : uint8_t {
    ROUTE_IDLE,     // not running: never started, or ended by the player
    ROUTE_FLYING,   // to Next
    ROUTE_DOCKED,   // at Next, Hours left
    ROUTE_ENDED,    // ended on its own: Why
} RouteState;

typedef enum : uint8_t {
    ROUTE_END_NONE,
    ROUTE_END_ORDER,      // the player gave the group another order
    ROUTE_END_BATTLE,     // the group lost a battle
    ROUTE_END_LOST,       // stop WhyStop is no longer the player's
    ROUTE_END_REACH,      // stop WhyStop is out of reach (pilot, hyperdrive, ships)
    ROUTE_END_DISBANDED,
} RouteEnd;

typedef struct RouteStop {
    uint8_t System, Planet, Moon;
} RouteStop;

// Bytes only, so the record is its own file layout (saves, the state stream).
typedef struct Route {
    uint8_t StopCount;
    RouteStop Stops[ROUTE_MAX_STOPS];
    uint8_t State;    // RouteState
    uint8_t Next;     // 1..StopCount, or ROUTE_HOME
    uint8_t Hours;    // docking hours left
    uint8_t Why;      // RouteEnd, while ENDED
    uint8_t WhyStop;
} Route;
static_assert(sizeof(Route) == 24);

enum : int { ROUTE_SLOTS = 32 };  // fleet groups 1..32

// The routes of fleet groups 1..32, [group - 1].
extern Route Routes_Table[ROUTE_SLOTS];

// Every route cleared (a new game, a DOS save loaded).
void Routes_Reset(void);
// Some group has a route (stops, or a state other than idle): the game holds
// what a DOS save can't.
bool Routes_Any(void);
// Group 1..32's route.
Route *Routes_Of(int16_t group);
// The group flies or docks on its route (and the switch is on).
bool Routes_Running(int16_t group);

// One game hour, after Sim_ProcessTravellingGroups: docking hours, arrivals
// and departures, and the stop conditions. Nothing with the switch off.
void Routes_Hour(void);

// The player ordered the group elsewhere (Move, Launch, Docking).
void Routes_Order(int16_t group);
// The group lost a battle (Intel_SpaceBattleLost).
void Routes_BattleLost(int16_t group);
// Fleet group `group` is about to be removed and the later ones shifted up
// (Intel_DisbandGroup).
void Routes_GroupDisbanded(int16_t group);

// Editing, while the route is not running. The refusal box's text for a
// stop at system:planet:moon, or nullptr if it can be added (route full
// included).
const char *Routes_StopRefusal(int16_t group, int16_t system, int16_t planet, int16_t moon);
// Inserts the stop after stop `after` (0: first). Returns its number.
int Routes_AddStop(int16_t group, int after, int16_t system, int16_t planet, int16_t moon);
void Routes_RemoveStop(int16_t group, int stop);
// Swaps stop `stop` with the one before (dir < 0) or after.
void Routes_MoveStop(int16_t group, int stop, int dir);
// START ROUTE: the refusal box's text, or nullptr when it runs.
const char *Routes_Start(int16_t group);
// END ROUTE.
void Routes_Stop(int16_t group);

// The editor's footer line for the route's state.
void Routes_StateText(int16_t group, char *out, size_t size);
// The ship info panel's line ("Route: to stop 2/3"), "" without a running
// route.
void Routes_PanelText(int16_t group, char *out, size_t size);

// Screen SCREEN_ROUTE (game/portcmd.h): the route editor of the selected
// fleet group.
void Routes_RouteScreen(void);
// Frees what the editor loaded, when it is left.
void Routes_LeaveScreen(void);
