// UI drivers for the trade routes improvement (game/routes.h): the route
// editor (screen 39), reached by ROUTE on a trade group's control panel
// (17). It shows the stops as a numbered list over the equipment screen's
// grid, the selected one highlighted (0xFC), the number of the one the
// route heads for in colour 0xF6, and the route's state on the footer's
// second line. ADD STOP opens the destination map (18), which returns to
// the editor; a refusal is a message box over the editor.
#pragma once

#include <stddef.h>

enum : int { UI_ROUTE_STOPS = 6 };

// --- Observers ------------------------------------------------------------------

// The stops listed, as the list writes them ("Amnesty system New Earth
// Apollo"); returns how many.
int ui_route_stops(char names[][48], int max);
// The highlighted stop, 0 if none; the stop whose number is drawn in the
// route's colour (it heads for it), 0 if none.
int ui_route_selected(void);
int ui_route_heading(void);
// The footer's state line ("Route not running", "Flying to stop 1", ...).
void ui_route_state(char *out, size_t size);

// --- Drivers ----------------------------------------------------------------------

// Opens the editor from the group's control panel. False if the panel has
// no ROUTE (not a trade group, or the improvement off).
bool ui_route_open(const char *group);
// ADD STOP, then `planet` (or its moon `moon`) of `system` (nullptr: the
// system the map shows) on the destination map. Returns true when it was
// added; else `refusal` holds the box's text ("" if the map didn't offer
// the body). Planets behind the sun are waited for.
bool ui_route_add(const char *system, const char *planet, const char *moon, char *refusal,
                  size_t size);
// Clicks stop k of the list (selects it).
void ui_route_select(int k);
// The red arrows: moves the selected stop up (dir < 0) or down. False if
// the arrow isn't offered.
bool ui_route_move(int dir);
