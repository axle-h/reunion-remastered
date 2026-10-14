// Trade routes (game/routes.h) through the route editor, by clicks only:
// a trade group's route is built (ADD STOP on the destination map, the red
// arrows, REMOVE STOP), each refusal box is met (New Earth, a stop already
// on the route, a body not the player's, a system out of reach, a full
// route), the route is started, docks and loads, shows a green square on
// the groups list, is ended with END ROUTE, and ends on its own when the
// group is given another order, with one log entry. Driver test: test setup
// switches the improvement on and gives the ships and the stops (by play
// they take weeks), so it is no proof. Needs the game data.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/dseg.h"
#include "game/improvements.h"
#include "game/planetinfo.h"
#include "game/system.h"
#include "platform/fs.h"
#include "play/play.h"
#include "play/ui.h"
#include "play/ui_message.h"
#include "play/ui_office.h"
#include "play/ui_routes.h"
#include "play/ui_ships.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

enum : int {
    SCR_SHIP_INFO = 16,
    SCR_ROUTE = 39,
    STOPS = 6,
    GREEN = 0x1E,  // the group list's square of a group on a route
};

// The bodies the setup gives: the stops (New Earth's moons, then Amnesty's
// planets) and one left to another owner, by name.
static struct {
    char planet[STOPS][32], moon[STOPS][32], label[STOPS][48];
    char other_planet[32], far_system[32];
} body;

static TProduct *product(const char *name)
{
    for (int k = 0; k < 35; k++) {
        TProduct *p = &ds.Research_Products[k];
        if (p->Name.len == SDL_strlen(name) && SDL_strncmp(p->Name.s, name, p->Name.len) == 0)
            return p;
    }
    play_fail("no product %s", name);
}

static void own(int16_t planet, int16_t moon, int k)
{
    TBody *b = PlanetInfo_BodyAt(1, planet, moon);
    b->Owner = 1;
    b->Droids = 1;
    b->SurfaceType = 1;
    b->InfoLevel = 30;
    b->Stores[k % 6] = 300;
    PlanetInfo_GetBodyName(body.planet[k], sizeof body.planet[k], 1, planet, 0);
    if (moon)
        PlanetInfo_GetBodyName(body.moon[k], sizeof body.moon[k], 1, planet, moon);
    PlanetInfo_GetBodyLocationName(body.label[k], sizeof body.label[k], 1, planet, moon);
}

static void setup(void *unused)
{
    (void)unused;
    improvements.trade_routes = true;
    product("Transfer ship")->State = 5;
    product("Transfer ship")->Stock = 3;
    ds.Game_GroupsEnabled = 1;
    ds.Game_GroupKindAllowed[1] = 1;  // Trade
    int k = 0;
    for (int16_t m = 1; m <= ds.Galaxy_MoonLists[0][4].Count && k < 2; m++)
        own(5, m, k++);
    for (int16_t p = 1; p <= ds.Galaxy_PlanetCount[0] && k < STOPS; p++) {
        if (p == 5 || p == 7)
            continue;
        ds.Galaxy_PlanetStatus[0][p - 1] = 1;  // found: on the map
        if (!body.other_planet[0]) {  // the first one stays a race's
            PlanetInfo_BodyAt(1, p, 0)->Owner = 3;
            PlanetInfo_BodyAt(1, p, 0)->SurfaceType = 1;
            PlanetInfo_GetBodyName(body.other_planet, sizeof body.other_planet, 1, p, 0);
            continue;
        }
        own(p, 0, k++);
    }
    for (int16_t p = 1; p <= ds.Galaxy_PlanetCount[0] && k < STOPS; p++)
        for (int16_t m = 1; p != 5 && p != 7 && m <= ds.Galaxy_MoonLists[0][p - 1].Count && k < STOPS; m++)
            own(p, m, k++);
    if (k < STOPS)
        play_fail("only %d bodies for stops", k);
    pstr_get(body.far_system, sizeof body.far_system, &ds.Names_Systems[1]);
    ds.Research_Products[0xD].State = 5;  // a hyperdrive: the map lists the systems
}

static struct {
    int stops_empty, stops_one, stops_two, sel_two, moved_sel, moved_back_sel, stops_full;
    bool no_remove_empty, no_start_empty, up_refused;
    char order_after_move[2][48], after_remove[48];
    char r_home[128], r_again[128], r_other[128], r_far[128], r_full[128];
    char state_idle[64], state_started[64], state_loading[64], state_ended[64], state_order[64];
    int heading;
    bool only_end, green, red_selected, panel_route, edit_again, logged, no_arrivals;
    char panel[32];
} seen;

static bool state_starts(void *prefix)
{
    char t[64];
    ui_route_state(t, sizeof t);
    return SDL_strncmp(t, prefix, SDL_strlen(prefix)) == 0;
}

static void add(int k)
{
    char refusal[128];
    if (!ui_route_add(nullptr, body.planet[k], body.moon[k][0] ? body.moon[k] : nullptr, refusal,
                      sizeof refusal))
        play_fail("stop %s not added: '%s'", body.label[k], refusal);
}

static void refused(const char *system, const char *planet, const char *moon, char *out)
{
    if (ui_route_add(system, planet, moon, out, 128))
        play_fail("%s added", planet);
    play_log("refused: '%s'", out);
}

static void policy(void *unused)
{
    (void)unused;
    ui_new_game();
    play_test_setup(setup, nullptr);
    if (!ui_new_group("Trade", "Trade 1"))
        play_fail("no Trade type");
    ui_equip("Trade 1", "Transfer ship", nullptr, 2);
    ui_open_screen("BACK TO M.SCREEN", 1);
    ui_hire("PILOTS", "Jack Benson");

    if (!ui_route_open("Trade 1"))
        play_fail("no ROUTE");
    char names[STOPS][48];
    seen.stops_empty = ui_route_stops(names, STOPS);
    seen.no_remove_empty = !ui_toolbar_has("REMOVE STOP");
    seen.no_start_empty = !ui_toolbar_has("START ROUTE");
    ui_route_state(seen.state_idle, sizeof seen.state_idle);
    play_dump("empty");

    add(0);
    seen.stops_one = ui_route_stops(names, STOPS);
    refused(nullptr, "New Earth", nullptr, seen.r_home);
    refused(nullptr, body.planet[0], body.moon[0], seen.r_again);
    refused(nullptr, body.other_planet, nullptr, seen.r_other);
    refused(body.far_system, body.planet[2], nullptr, seen.r_far);
    add(1);
    seen.stops_two = ui_route_stops(names, STOPS);
    seen.sel_two = ui_route_selected();
    play_dump("two");

    // The arrows: stop 1 down, then back up; no up arrow on stop 1.
    ui_route_select(1);
    ui_route_move(1);
    ui_route_stops(names, STOPS);
    SDL_strlcpy(seen.order_after_move[0], names[0], 48);
    SDL_strlcpy(seen.order_after_move[1], names[1], 48);
    seen.moved_sel = ui_route_selected();
    ui_route_move(-1);
    seen.moved_back_sel = ui_route_selected();
    seen.up_refused = !ui_route_move(-1);

    // REMOVE STOP takes the selected one.
    ui_route_select(2);
    ui_toolbar("REMOVE STOP");
    ui_settle();
    ui_route_stops(names, STOPS);
    SDL_strlcpy(seen.after_remove, names[0], 48);
    add(1);

    // Full at six.
    for (int k = 2; k < STOPS; k++)
        add(k);
    seen.stops_full = ui_route_stops(names, STOPS);
    refused(nullptr, "New Earth", nullptr, seen.r_full);
    play_dump("full");
    for (int k = 2; k < STOPS; k++) {
        ui_route_select(3);
        ui_toolbar("REMOVE STOP");
        ui_settle();
    }

    // Started: only END ROUTE, heading for stop 1, until it loads there.
    ui_toolbar("START ROUTE");
    ui_settle();
    seen.only_end = ui_toolbar_has("END ROUTE") && !ui_toolbar_has("ADD STOP");
    ui_route_state(seen.state_started, sizeof seen.state_started);
    seen.heading = ui_route_heading();
    if (!ui_wait_hours(state_starts, "Loading at stop 1", 48))
        play_fail("never loading at stop 1");
    ui_route_state(seen.state_loading, sizeof seen.state_loading);
    play_dump("loading");

    // The groups list: a green square while another group is selected.
    if (!ui_new_group("Trade", "Trade 2"))
        play_fail("no second group");
    ui_ship_info();
    seen.green = ui_pixel_is(5, 0x41, GREEN);
    ui_click_hotspot(3, UI_CLICK);  // Trade 1's icon selects it
    ui_settle();
    seen.red_selected = !ui_pixel_is(5, 0x41, GREEN);
    ui_read_text(0xCA, 0x6C, 19, seen.panel, sizeof seen.panel);
    seen.panel_route = SDL_strncmp(seen.panel, "Route: ", 7) == 0;
    play_dump("groups");

    // END ROUTE, then started again and given another order.
    ui_route_open("Trade 1");
    ui_toolbar("END ROUTE");
    ui_settle();
    ui_route_state(seen.state_ended, sizeof seen.state_ended);
    seen.edit_again = ui_toolbar_has("ADD STOP") && ui_toolbar_has("START ROUTE");
    ui_toolbar("START ROUTE");
    ui_settle();
    if (!ui_wait_hours(state_starts, "Loading at stop", 48))
        play_fail("never loading again");
    ui_take_off("Trade 1");
    seen.logged = ui_log_has("Trade 1: route ended, another order");
    seen.no_arrivals = !ui_log_has("Trade 1 arrived");
    ui_route_open("Trade 1");
    ui_route_state(seen.state_order, sizeof seen.state_order);
    play_dump("ended");
    play_quit();
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, SDL_getenv("PLAY_AUDIO") ? SDL_getenv("PLAY_AUDIO") : "dummy");
    if (!SDL_Init(SDL_INIT_EVENTS)) {
        printf("SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    PlayResult r = play_run(12345, policy, nullptr, nullptr);
    printf("route run: exit %d after %llu frames%s%s\n", r.exit_code, (unsigned long long)r.frames,
           r.failed ? ", FAILED: " : "", r.failure);
    CHECK(!r.failed && r.tainted);
    printf("refusals: '%s' / '%s' / '%s' / '%s' / '%s'\n", seen.r_home, seen.r_again, seen.r_other,
           seen.r_far, seen.r_full);
    printf("states: '%s' / '%s' / '%s' / '%s' / '%s'; panel '%s'\n", seen.state_idle,
           seen.state_started, seen.state_loading, seen.state_ended, seen.state_order, seen.panel);
    CHECK(seen.stops_empty == 0 && seen.no_remove_empty && seen.no_start_empty);
    CHECK(SDL_strcmp(seen.state_idle, "Route not running") == 0);
    CHECK(seen.stops_one == 1 && seen.stops_two == 2 && seen.sel_two == 2);
    CHECK(SDL_strstr(seen.r_home, "New Earth is where the route unloads"));
    CHECK(SDL_strstr(seen.r_again, "on the route already"));
    CHECK(SDL_strstr(seen.r_other, "Only your colonies"));
    CHECK(SDL_strstr(seen.r_far, "No hyperdrive"));
    CHECK(SDL_strstr(seen.r_full, "all its 6 stops"));
    CHECK(SDL_strcmp(seen.order_after_move[0], body.label[1]) == 0
          && SDL_strcmp(seen.order_after_move[1], body.label[0]) == 0);
    CHECK(seen.moved_sel == 2 && seen.moved_back_sel == 1 && seen.up_refused);
    CHECK(SDL_strcmp(seen.after_remove, body.label[0]) == 0);
    CHECK(seen.stops_full == STOPS);
    CHECK(seen.only_end && seen.heading == 1);
    CHECK(SDL_strcmp(seen.state_started, "Flying to stop 1") == 0);
    CHECK(SDL_strncmp(seen.state_loading, "Loading at stop 1", 17) == 0);
    CHECK(seen.green && seen.red_selected && seen.panel_route);
    CHECK(SDL_strcmp(seen.state_ended, "Route not running") == 0 && seen.edit_again);
    CHECK(seen.logged && seen.no_arrivals);
    CHECK(SDL_strcmp(seen.state_order, "Route ended: another order") == 0);
    SDL_Quit();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
