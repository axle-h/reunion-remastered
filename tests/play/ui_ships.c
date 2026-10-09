#include "play/ui_ships.h"

#include <SDL3/SDL.h>

#include "game/dseg.h"
#include "game/groups.h"
#include "game/mouse.h"
#include "game/planetinfo.h"
#include "play/play.h"
#include "play/ui.h"
#include "play/ui_message.h"

enum : int {
    SCR_CARGO = 13,
    SCR_SHIP_INFO = 16,
    SCR_GROUP = 17,
    SCR_DESTINATION = 18,
    SCR_NEW_GROUP = 21,
    SCR_EQUIP = 22,
    SCREEN_TIMEOUT = 3000,
    PANEL_X = 0xC,      // the group's location line on screen 22 (Groups_DrawGroupPanel)
    PANEL_Y = 0x43,
};

// Unnamed globals of the cargo screen (cargo.c) and the text editor.
#define CARGO_ITEMS ((const uint16_t *)ds_at(0x7816))  // [0] count, [1..] Cargo item index
#define CARGO_ITEM_STORE ((const uint32_t *)ds_at(0x782E))
#define CARGO_STORES (*(const uint32_t *)ds_at(0x7866))
#define CARGO_SHIP_USED (*(const int32_t *)ds_at(0x786A))
#define CARGO_SHIP_CAP (*(const int32_t *)ds_at(0x786E))
#define CARGO_PLANET_CAP (*(const int32_t *)ds_at(0x7872))
#define TEXT_INPUT (*(const uint8_t *)ds_at(0x7C4A))    // the blinking edit cursor is shown

static bool pstr_is(const void *p, const char *s)
{
    const uint8_t *q = p;
    size_t n = SDL_strlen(s);
    return q[0] == n && SDL_strncasecmp((const char *)q + 1, s, n) == 0;
}

// Clicks hotspot `caption`; fails the run if there is none.
static void click(const char *caption, const char *what)
{
    if (!ui_hotspot(caption))
        play_fail("%s: no '%s' hotspot", what, caption);
}

// --- Observers ------------------------------------------------------------------------

static TGroup *group_named(const char *name, int *index)
{
    for (int i = 1; i <= ds.Game_GroupCount; i++) {
        TGroup *g = Groups_At(0, (int16_t)i);
        if (pstr_is(&g->Name, name)) {
            if (index)
                *index = i;
            return g;
        }
    }
    return nullptr;
}

int ui_group_find(const char *name)
{
    int i = 0;
    return group_named(name, &i) ? i : 0;
}

int ui_group_status(const char *name)
{
    const TGroup *g = group_named(name, nullptr);
    if (!g)
        return SHIP_NONE;
    if (g->Status >= 4 && g->Status <= 6)
        return SHIP_FLYING;
    return g->Status;
}

void ui_group_location(const char *name, char *out, size_t n)
{
    const TGroup *g = group_named(name, nullptr);
    out[0] = 0;
    if (!g)
        return;
    // Groups_DrawGroupPanel's line.
    static const char *const prefix[] = {"", "In the dock of ", "In orbit ", "",
                                         "Fly to ", "Fly to ", "Fly to "};
    PString loc;
    PlanetInfo_GetBodyLocationName(&loc, g->System, g->Planet, g->Moon);
    SDL_snprintf(out, n, "%s%.*s", g->Status <= 6 ? prefix[g->Status] : "", loc.len, loc.s);
}

static int16_t *grid_unit(TGroup *g, int cat, int s)
{
    uint8_t row = *(const uint8_t *)ds_at((uint16_t)(0x0BB9 + g->Kind * 4 + cat));
    return (int16_t *)((uint8_t *)g + row * 0x28 + s * 10 - 21);
}

static const PStr16 *product_name(uint8_t id)
{
    return &ds.Research_Products[id - 1].Name;
}

// Finds `ship` (and `weapon`) in the group's grid pages: category, ship row
// and weapon column (0 for the ship count).
static bool grid_find(const TGroup *g, const char *ship, const char *weapon, int *cat, int *s, int *w)
{
    for (int c = 1; c <= 4; c++) {
        if (!Groups_SlotUsed(g->Kind, (int16_t)c))
            continue;
        const uint8_t *layout = far_host(Groups_SlotLayout(g->Kind, (int16_t)c));
        for (int i = 1; i <= layout[0]; i++) {
            if (!pstr_is(product_name(layout[i * 25 + 0x21]), ship))
                continue;
            *cat = c;
            *s = i;
            *w = 0;
            if (!weapon)
                return true;
            for (int k = 1; k <= layout[1]; k++) {
                if (pstr_is(product_name(layout[k * 14 - 12]), weapon)) {
                    *w = k;
                    return true;
                }
            }
            return false;
        }
    }
    return false;
}

int ui_group_units(const char *name, const char *ship, const char *weapon)
{
    TGroup *g = group_named(name, nullptr);
    int cat, s, w;
    if (!g || !grid_find(g, ship, weapon, &cat, &s, &w))
        return 0;
    return grid_unit(g, cat, s)[w];
}

void ui_cargo_space(int32_t *used, int32_t *cap, int32_t *planet_cap)
{
    *used = CARGO_SHIP_USED;
    *cap = CARGO_SHIP_CAP;
    *planet_cap = CARGO_PLANET_CAP;
}

// Row of the cargo screen: 1..6 ores, 7.. the items shown.
static int cargo_row(const char *item, int *item_index)
{
    for (int i = 1; i <= 6; i++)
        if (pstr_is(&ds.Names_Ores[i - 1], item))
            return i;
    for (int r = 1; r <= CARGO_ITEMS[0]; r++) {
        int idx = CARGO_ITEMS[r];
        if (pstr_is(product_name((uint8_t)ds.Cargo_Items[idx - 1]), item)) {
            *item_index = idx;
            return r + 6;
        }
    }
    return 0;
}

int32_t ui_cargo_ship(const char *item)
{
    const TGroup *g = Groups_Selected();
    int idx = 0, row = cargo_row(item, &idx);
    if (row == 0)
        return 0;
    if (row <= 6) {
        int32_t v;
        SDL_memcpy(&v, &g->Cargo[row - 1], 4);
        return v;
    }
    return g->Items[idx - 1];
}

int32_t ui_cargo_planet(const char *item)
{
    int idx = 0, row = cargo_row(item, &idx);
    if (row == 0)
        return 0;
    if (row <= 6)
        return ((const int32_t *)far_host(CARGO_STORES))[row - 1];
    return *(const int16_t *)far_host(CARGO_ITEM_STORE[idx]);
}

// Body hotspots of the system view (PlanetInfo_SetupSystemViewHotspots):
// 0x13 the sun or the zoomed planet, 0x14.. planets or moons.
enum : int { BODY_FIRST = 0x13, BODY_FLAG = 5 };

int ui_map_bodies(char names[][19], int max)
{
    int n = 0;
    for (int j = BODY_FIRST; j <= ds.Mouse_HotspotCount && n < max; j++) {
        const THotspot *h = Mouse_Hotspot((int16_t)j);
        if (h->_1A == BODY_FLAG && h->Caption.len > 0)
            SDL_snprintf(names[n++], 19, "%.*s", h->Caption.len, h->Caption.s);
    }
    return n;
}

bool ui_map_system_listed(const char *system)
{
    for (int j = 1; j <= 8; j++)
        if (ui_hotspot_captioned(j, system))
            return true;
    return false;
}

// --- Drivers ------------------------------------------------------------------------------

void ui_ship_info(void)
{
    ui_open_screen("SHIP INFO", SCR_SHIP_INFO);
}

static bool group_selected(const char *name)
{
    return ds.Game_GroupList == 0 && ds.Game_SelGroup > 0 && pstr_is(&Groups_Selected()->Name, name);
}

void ui_open_group(const char *name, int screen)
{
    if (ui_screen() == screen && group_selected(name))
        return;
    ui_ship_info();
    if (ds.Game_GroupList != 0) {  // the planet-forces list is shown
        click("Change mode", "ui_open_group");
        ui_settle();
    }
    int i = ui_group_find(name);
    if (i == 0)
        play_fail("ui_open_group: no group '%s' in the list", name);
    // Name hotspots follow the 32 icon hotspots (ShipInfo_SetupShipInfoHotspots).
    ui_click_hotspot(i + 0x22, screen == SCR_EQUIP ? UI_CLICK_RIGHT : UI_CLICK);
    if (!ui_wait_screen(screen, SCREEN_TIMEOUT) || !group_selected(name))
        play_fail("ui_open_group: '%s' did not open on screen %d", name, screen);
}

// The New Group screen's type line (Groups_DrawNewGroupNameAndType).
static bool type_shown(const char *type)
{
    return pstr_is(ds_at((uint16_t)(0x591A + Groups_Selected()->Kind * 14)), type);
}

typedef struct {
    uint8_t len;
    uint8_t active;
} NameState;

static bool name_changed(void *arg)
{
    const NameState *s = arg;
    return Groups_Selected()->Name.len != s->len || TEXT_INPUT != s->active;
}

// Types one key into the name being edited and waits until the field shows it.
static void type_key(int code)
{
    NameState s = {Groups_Selected()->Name.len, TEXT_INPUT};
    play_key(code);
    if (!ui_wait_until(name_changed, &s, 30))
        play_fail("ui_new_group: key %d did not reach the name field", code);
}

bool ui_new_group(const char *type, const char *name)
{
    ui_ship_info();
    if (!ui_toolbar("NEW UNIT"))
        play_fail("ui_new_group: no NEW UNIT button (groups not enabled, or 32 groups)");
    if (!ui_wait_screen(SCR_NEW_GROUP, SCREEN_TIMEOUT))
        play_fail("ui_new_group: no New Group screen");
    // The screen opens with the name field in edit mode ("New Group").
    if (name && *name) {
        while (Groups_Selected()->Name.len > 0)
            type_key(8);
        for (const char *p = name; *p; p++)
            type_key((unsigned char)*p);
    }
    if (TEXT_INPUT)
        type_key(13);
    for (int i = 0; i < 4 && !type_shown(type); i++)
        click("Change type", "ui_new_group");
    ui_settle();
    if (!type_shown(type)) {
        play_log("ui_new_group: type '%s' not offered", type);
        ui_toolbar("ABORT");
        ui_wait_screen(SCR_SHIP_INFO, SCREEN_TIMEOUT);
        return false;
    }
    play_dump("new-group");
    ui_toolbar("OK,CREATE IT");
    if (!ui_wait_screen(SCR_EQUIP, SCREEN_TIMEOUT))
        play_fail("ui_new_group: no group screen after OK,CREATE IT");
    play_log("created %s group '%.*s'", type, Groups_Selected()->Name.len, Groups_Selected()->Name.s);
    return true;
}

bool ui_equip(const char *group, const char *ship, const char *weapon, int n)
{
    ui_open_group(group, SCR_EQUIP);
    TGroup *g = Groups_Selected();
    int cat, s, w;
    if (!grid_find(g, ship, weapon, &cat, &s, &w))
        play_fail("ui_equip: no %s%s%s in the grid of '%s'", ship, weapon ? "/" : "",
                  weapon ? weapon : "", group);
    for (int i = 0; i < 4 && ds.Groups_SlotCategory != cat; i++) {
        click("Page down", "ui_equip");
        ui_settle();
    }
    // Ship slot buttons are hotspots 8.., weapon buttons 13 + 5 per row.
    int j = w == 0 ? s + 7 : w + 0xC + (s - 1) * 5;
    for (int tries = 0; tries < 4 * n + 20; tries++) {
        int now = grid_unit(g, cat, s)[w];
        if (now == n)
            return true;
        if (j > ds.Mouse_HotspotCount || !ui_hotspot_captioned(j, weapon ? weapon : ship)) {
            play_log("ui_equip: %s not editable here", weapon ? weapon : ship);
            return false;
        }
        ui_click_hotspot(j, now > n ? UI_PRESS_RIGHT : UI_PRESS);
        ui_settle();
        if (grid_unit(g, cat, s)[w] == now) {
            play_log("ui_equip: %s stays at %d (wanted %d)", weapon ? weapon : ship, now, n);
            return false;
        }
    }
    return grid_unit(g, cat, s)[w] == n;
}

typedef struct {
    const char *group;
    int status;
} StatusWait;

static bool status_is(void *arg)
{
    const StatusWait *w = arg;
    return ui_group_status(w->group) == w->status;
}

typedef struct {
    StatusWait status;
    uint64_t since;  // the click
} ActionWait;

// The group's status changed, or a box (closed by the wait) refused it.
static bool status_or_box(void *arg)
{
    ActionWait *a = arg;
    return status_is(&a->status) || ui_box_seen("", a->since);
}

// Clicks the control-panel hotspot `caption` and waits for the group's
// status, or for a refusal box (closed; returns false).
static bool group_action(const char *group, const char *caption, int status)
{
    ui_open_group(group, SCR_GROUP);
    if (!ui_hotspot(caption))
        play_fail("no '%s' on the control panel of '%s'", caption, group);
    ActionWait a = {{group, status}, play_frame()};
    if (!ui_wait_until(status_or_box, &a, 600))
        play_fail("'%s' of '%s' had no effect", caption, group);
    ui_settle();
    return status_is(&a.status);
}

bool ui_take_off(const char *group)
{
    if (ui_group_status(group) == SHIP_ORBIT)
        return true;
    bool ok = group_action(group, "Launch", SHIP_ORBIT);
    play_log("take off '%s': %s", group, ok ? "in orbit" : "refused");
    return ok;
}

bool ui_dock(const char *group)
{
    if (ui_group_status(group) == SHIP_DOCKED)
        return true;
    bool ok = group_action(group, "Docking", SHIP_DOCKED);
    play_log("dock '%s': %s", group, ok ? "docked" : "refused");
    return ok;
}

// Destination screen state: zoomed on a planet (ZOOM OUT on the toolbar).
static bool zoomed(void)
{
    return ds.Game_CurPlanet[ds.Game_CurSystem - 1] != 0;
}

static int body_hotspot(const char *name)
{
    for (int j = BODY_FIRST; j <= ds.Mouse_HotspotCount; j++) {
        const THotspot *h = Mouse_Hotspot((int16_t)j);
        if (h->_1A == BODY_FLAG && ui_hotspot_captioned(j, name))
            return j;
    }
    return 0;
}

static void zoom_out(void)
{
    if (!zoomed())
        return;
    if (!ui_toolbar("ZOOM OUT"))
        play_fail("ui_launch: no ZOOM OUT");
    ui_settle();
}

// A destination click refused with a box (closed by the wait after the
// click at frame `since`): leave the screen.
static bool destination_refused(uint64_t since)
{
    ui_settle();
    if (ui_screen() != SCR_DESTINATION || !ui_box_seen("", since))
        return false;
    ui_toolbar("ABORT MOVE");
    ui_wait_screen(SCR_GROUP, SCREEN_TIMEOUT);
    return true;
}

static bool pick_destination(const char *system, const char *planet, const char *moon)
{
    if (system && !pstr_is(&ds.Names_Systems[ds.Game_CurSystem - 1], system)) {
        zoom_out();
        int j = 0;
        for (int k = 1; k <= 8 && j == 0; k++)
            if (ui_hotspot_captioned(k, system))
                j = k;
        if (j == 0)
            play_fail("ui_launch: system %s not listed", system);
        ui_click_hotspot(j, UI_CLICK);
        if (destination_refused(play_frame()))
            return false;
    }
    if (!planet) {  // the sun: explore the system
        zoom_out();
        const THotspot *sun = Mouse_Hotspot(BODY_FIRST);
        if (ds.Mouse_HotspotCount < BODY_FIRST || sun->_1A != BODY_FLAG || sun->Caption.len == 0) {
            play_log("ui_launch: the sun is not offered (system explored)");
            ui_toolbar("ABORT MOVE");
            ui_wait_screen(SCR_GROUP, SCREEN_TIMEOUT);
            return false;
        }
        ui_click_hotspot(BODY_FIRST, UI_CLICK);
        return true;
    }
    bool on_planet = zoomed() && body_hotspot(planet) == BODY_FIRST;
    if (!moon) {
        if (on_planet) {
            ui_click_hotspot(BODY_FIRST, UI_CLICK);
            return true;
        }
        zoom_out();
        int j = body_hotspot(planet);
        if (j == 0)
            play_fail("ui_launch: planet %s not on the map", planet);
        ui_click_hotspot(j, UI_CLICK_RIGHT);
        return true;
    }
    if (!on_planet) {
        zoom_out();
        int j = body_hotspot(planet);
        if (j == 0)
            play_fail("ui_launch: planet %s not on the map", planet);
        ui_click_hotspot(j, UI_CLICK);
        ui_settle();
    }
    int j = body_hotspot(moon);
    if (j == 0 || j == BODY_FIRST) {
        play_log("ui_launch: moon %s of %s not on the map", moon, planet);
        ui_toolbar("ABORT MOVE");
        ui_wait_screen(SCR_GROUP, SCREEN_TIMEOUT);
        return false;
    }
    ui_click_hotspot(j, UI_CLICK);
    return true;
}

// The destination screen opened, or a box (closed by the wait) refused it.
static bool destination_or_box(void *arg)
{
    return ui_screen() == SCR_DESTINATION || ui_box_seen("", *(const uint64_t *)arg);
}

bool ui_launch(const char *group, const char *system, const char *planet, const char *moon)
{
    ui_open_group(group, SCR_GROUP);
    const char *move = ui_group_status(group) == SHIP_FLYING ? "Change destination" : "Move";
    if (ui_group_status(group) == SHIP_DOCKED)
        play_fail("ui_launch: '%s' is docked", group);
    click(move, "ui_launch");
    uint64_t since = play_frame();
    ui_wait_until(destination_or_box, &since, 300);
    if (ui_screen() != SCR_DESTINATION && ui_box_seen("", since)) {
        play_log("ui_launch: '%s' refused", group);
        return false;
    }
    if (!ui_wait_screen(SCR_DESTINATION, SCREEN_TIMEOUT))
        play_fail("ui_launch: no destination screen");
    play_dump("destination");
    char bodies[20][19], line[200] = "";
    for (int i = 0, n = ui_map_bodies(bodies, 20); i < n; i++)
        SDL_snprintf(line + SDL_strlen(line), sizeof line - SDL_strlen(line), "%s'%s'", i ? " " : "",
                     bodies[i]);
    play_log("destination map: %s", line);
    if (!pick_destination(system, planet, moon))
        return false;
    if (!ui_wait_screen(SCR_GROUP, SCREEN_TIMEOUT))
        play_fail("ui_launch: no way back to the control panel");
    char loc[128];
    ui_group_location(group, loc, sizeof loc);
    play_log("launch '%s': %s", group, loc);
    return ui_group_status(group) == SHIP_FLYING;
}

int32_t ui_cargo(const char *group, const char *item, int32_t amount)
{
    if (ui_screen() != SCR_CARGO || !group_selected(group)) {
        ui_open_group(group, SCR_GROUP);
        if (!ui_hotspot("Transfer"))
            play_fail("ui_cargo: '%s' has no Transfer (not a trade group)", group);
        if (!ui_wait_screen(SCR_CARGO, SCREEN_TIMEOUT))
            play_fail("ui_cargo: no cargo screen");
    }
    int idx = 0, row = cargo_row(item, &idx);
    if (row == 0)
        play_fail("ui_cargo: no row '%s'", item);
    // Hotspots 1.. move to the planet, 20.. to the ship (Cargo_SetupCargoHotspots),
    // only while docked at an own colony.
    int j = amount > 0 ? row + 0x13 : row;
    const THotspot *h = Mouse_Hotspot((int16_t)j);
    if (j > ds.Mouse_HotspotCount || h->Caption.len == 0) {
        play_log("ui_cargo: no transfer here (not docked at a colony)");
        return 0;
    }
    bool all = amount == SHIP_CARGO_ALL || amount == -SHIP_CARGO_ALL;
    int32_t want = amount < 0 ? -amount : amount, moved = 0;
    while (moved < want) {
        int32_t before = ui_cargo_ship(item);
        ui_click_hotspot(j, all && row <= 6 ? UI_PRESS_RIGHT : UI_PRESS);
        ui_settle();
        int32_t delta = ui_cargo_ship(item) - before;
        if (delta < 0)
            delta = -delta;
        if (delta == 0)
            break;
        moved += delta;
    }
    int32_t used, cap, pcap;
    ui_cargo_space(&used, &cap, &pcap);
    play_log("cargo '%s' %s %d %s: space %d/%d", group, amount > 0 ? "loaded" : "unloaded", moved, item,
             used, cap);
    return moved;
}

bool ui_deploy_carrier_satellite(const char *group)
{
    ui_open_group(group, SCR_GROUP);
    int before = ui_group_units(group, "Sat.carrier", "Satellite");
    // ADD SATELLITE is command 20 here (a carrier's satellite); the stock
    // satellite button (70) has the same label but only on planet screens.
    if (!ui_toolbar("ADD SATELLITE")) {
        play_log("ui_deploy_carrier_satellite: no ADD SATELLITE for '%s'", group);
        return false;
    }
    ui_settle();
    play_dump("carrier-satellite");
    int after = ui_group_units(group, "Sat.carrier", "Satellite");
    play_log("'%s' deployed a satellite: %d left", group, after);
    return after < before;
}

bool ui_group_panel_shows(const char *group)
{
    char loc[128];
    ui_group_location(group, loc, sizeof loc);
    return ui_screen() == SCR_EQUIP && group_selected(group) && ui_text_at(PANEL_X, PANEL_Y, loc);
}

bool ui_wait_arrival(const char *group, int hours)
{
    // Screen 22 redraws the selected group's line when it arrives (sim.c
    // ProcessTravellingGroups), and its clock runs.
    ui_open_group(group, SCR_EQUIP);
    StatusWait sw = {group, SHIP_ORBIT};
    bool ok = ui_wait_hours(status_is, &sw, hours);
    ui_settle();
    char loc[128];
    ui_group_location(group, loc, sizeof loc);
    bool shown = ui_group_panel_shows(group);
    play_log("'%s': %s%s", group, loc, shown ? "" : " (NOT on the panel)");
    return ok && shown;
}
