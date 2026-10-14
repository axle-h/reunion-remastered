// UI drivers and observers for the planet screens (docs/playthrough.md):
// planet surface (20), planet info (8), droids (4), colonisation
// (28), the body actions in the toolbars of 8/20/17, the colony list (37)
// and picking a body on the galactic map (7).
//
// Observers read what the screen shows: text is read back from the screen
// pixels with the game font (ui_read_text, ui_message.h), so they decide on
// exactly what a player sees. The few observers that read game memory say
// which on-screen element they stand for.
#pragma once

#include <stdint.h>

// --- Navigation ---

// Command centre (screen 1) or planet info (8) -> planet surface (20). From
// the command centre this is always New Earth.
void ui_planet_main(void);
// Planet surface (20) -> planet info (8) of the same body.
void ui_planet_info(void);
// Any screen with BACK TO M.SCREEN -> command centre (1).
void ui_command_centre(void);
// Galactic map: shows the body `planet` (moon nullptr) or its moon `moon` on
// `screen` 8 (planet info) or 20 (surface). Starts from any screen with a
// GALACTIC MAP button and uses the system shown there. On the map a right
// click on a body opens its info, a left click on a planet zooms in to it
// and its moons (ZOOM OUT goes back), a left click there opens the surface.
// The info's NAME line must name the body; the surface must show the hour
// of the click (the bodies move on at each hour).
void ui_open_body(const char *planet, const char *moon, int screen);
// The name of the body shown on screen 8 (after "PLANET'S NAME:", or "MOON'S
// NAME:" where it reads "<planet> <moon>", e.g. "New Earth Apollo"), or ""
// elsewhere.
const char *ui_body_name(void);

// --- Planet info (screen 8) ---

// Tax levels as the TAX line spells them.
enum : int {
    TAX_NONE, TAX_VERY_LOW, TAX_LOW, TAX_NORMAL, TAX_HIGH, TAX_DIFFICULT, TAX_VERY_DIFFICULT,
    TAX_OPPRESSING,
};
// The level on the TAX line, -1 if it can't be read.
int ui_tax_level(void);
// Clicks INCREASE / DECREASE TAX until the TAX line reads `level`.
void ui_set_tax(int level);
// The POP line: population (0 for "No ...") and mood band 0 ("revolting
// men") .. 6 ("loyal people"), -1 if no mood word is shown.
int32_t ui_population(void);
int ui_mood(void);
// The objects list (bottom right: "Colony", "2 Miner Droid", "Satellite",
// ...) holds a line starting with `line`.
bool ui_body_lists(const char *line);
// The TYPE line contains `s` (e.g. "colony", "under colonization").
bool ui_body_type_says(const char *s);

// --- Planet surface (screen 20) ---

// The building kind named under the preview, padding trimmed.
const char *ui_surface_kind(void);
// Clicks Invention up / down until the preview names `kind`. False if the
// list doesn't offer it (not researched, wrong surface, builder too weak).
bool ui_select_kind(const char *kind);

typedef enum {
    UI_BUILD_OK,          // placed; construction started
    UI_BUILD_UNAVAILABLE, // the kind isn't in the list
    UI_BUILD_REFUSED,     // a box refused (its text: ui_last_box)
    UI_BUILD_NO_MONEY,    // the Build button didn't start blinking
    UI_BUILD_NO_ROOM,     // no free spot found on the map
} UiBuildResult;
// Builds one `kind` on the body whose surface is shown: picks the kind,
// clicks Build, finds free ground on the map (the cursor footprint must
// show green on every cell) and clicks it, then returns to the map view.
UiBuildResult ui_build(const char *kind);

// Building again with Shift (improvements.repeat_build), on the body whose
// surface is shown. ui_build_start picks `kind` and clicks Build: true if
// Build blinks. ui_build_place clicks free ground for it (moving the window
// with the radar, which keeps the build mode, to find some; Shift held if
// `shift`), true if the map then shows one more of the kind.
// ui_build_on_taken Shift+clicks a building in the window.
// ui_build_blinks: Build still blinks, waiting for a cell.
bool ui_build_start(const char *kind);
bool ui_build_place(const char *kind, bool shift);
void ui_build_on_taken(void);
bool ui_build_blinks(void);

// Buildings of `kind` on the body whose surface or info is shown, as its
// surface map draws them: `total` footprints, `building` still drawn as a
// construction site, `finished` drawn as the building, `active` those of
// the finished that work (the passive ones blink a red outline, and their
// info reads "Status : Passive").
typedef struct {
    int total, building, finished, active;
} UiBuildings;
UiBuildings ui_buildings(const char *kind);
// Fast-forwards (ui_fast_forward) until `n` buildings of `kind` are finished
// and active, for at most `hours`.
bool ui_wait_buildings_active(const char *kind, int n, int hours);

// Clicks a finished building of `kind` on the surface shown (moving the
// window with the radar to find one), reads the number on its info line
// starting with `label` ("Status", "Production", "Workers", "Energy",
// "Working") and goes back to the map. -1 if there is no such building or
// line. Not for Mines (a click on one opens the droid screen).
int32_t ui_building_info(const char *kind, const char *label);

// Demolishes a finished `kind` on the surface shown (moving the window with
// the radar to find one): Destroy, then a click on it. True when it was
// paid for (2,000 credits).
bool ui_demolish(const char *kind);

// Droids working on the body shown (the droid figures on screen 4 and the
// "n Miner Droid" line on screen 8), droids in stock for it (the big digits
// on screen 4) and places for droids (finished Mines and Miner stations: the
// small digits on screen 4). ADD DROIDS works while droids < places,
// < UI_MAX_DROIDS, and stock > 0: further Mines get none.
enum : int { UI_MAX_DROIDS = 9 };
int ui_droids(void);
int ui_droid_stock(void);
int ui_droid_slots(void);
// From the surface: clicks a finished Mine to open the droid screen, ADD
// DROIDS once, back to the surface. True if the count went up.
bool ui_add_droid(void);

// --- Body actions (toolbar of screens 8, 20, 17) ---

// Clicks the body-action toolbar button `label` (ui_toolbar) and waits for
// the screen to settle (ui_settle). False if the toolbar doesn't offer it. Labels:
// "ADD SATELLITE" (stock 0x46, or a carrier's 0x14), "ADD SPY SAT" (0x43),
// "ADD SPY SHIP" (0x44), "ADD SOLAR SAT" (0x45), "ADD MINER STATION" (10, a
// trade group carrying one), "COLONIZATION" (5).
bool ui_body_action(const char *label);
// Puts a stock satellite (bought in the shop) on the body shown with ADD
// SATELLITE. True when the toolbar then no longer offers it (a body takes
// one satellite). Only around New Earth does the object list then read
// "Satellite": elsewhere the satellite is lost within hours and never listed
// (the Klatoo box, E8). (A carrier group's satellites:
// ui_deploy_carrier_satellite.)
bool ui_deploy_stock_satellite(void);
// Opens colonisation (screen 28) for the body shown, picks the starter
// buildings named in `starters` (n of them) and clicks OK BUILD IT. True when
// the game took it and went to the new colony's surface.
bool ui_found_colony(const char *const *starters, int n);

// --- Colony list (screen 37, MAIN COMPUTER) ---

// Opens the list from the command centre in mode `button` ("YOUR PLANETS",
// "USEFUL PLANETS" or "ALIEN PLANETS").
void ui_planet_list(const char *button);
// Reads row `row` (1..15) of the list; "" past the end.
const char *ui_planet_list_row(int row);
