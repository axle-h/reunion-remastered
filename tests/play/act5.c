#include "play/act5.h"

#include <SDL3/SDL.h>

#include "play/army.h"
#include "play/play.h"
#include "play/policy.h"
#include "play/run.h"
#include "play/ui.h"
#include "play/ui_battle.h"
#include "play/ui_message.h"
#include "play/ui_office.h"
#include "play/ui_planet.h"
#include "play/ui_ships.h"

enum : int {
    SCR_GALACTIC_MAP = 7,
    SCR_GROUP = 17,
    SCR_SURFACE = 20,
    TRIP_HOURS = 30,  // a flight to another system: <= 249 + 1 + 249 legs at pilot experience + 1 an hour
    CARRIER_PRICE = 1530,
    SPY_SATELLITE_PRICE = 3546,
};

static const int32_t CARRIER_ORES[ORES] = {55, 105, 27, 18, 0, 0};
static const int32_t SPY_SATELLITE_ORES[ORES] = {53, 255, 80, 335, 890, 900};

// --- Visible events ---------------------------------------------------------------

// A box this run has seen (pol_seen_like: a few characters may be
// misread), or a line of the message log (for a step resumed after the
// box).
static bool seen_or_logged(const char *text)
{
    return pol_seen_like(text, 0) >= 0 || ui_log_has(text);
}

static bool event_seen(void *text)
{
    return pol_seen_like(text, 0) >= 0;
}

static bool event_or_logged(void *text)
{
    return seen_or_logged(text);
}

static bool no_wave_due(void *unused)
{
    (void)unused;
    return !pol_s8_active();
}

// The galactic map shows `planet` in `system`'s view (selecting the system
// in the list; no body is clicked): the system has been explored. False
// also if the system is not listed.
static bool planet_on_map(const char *system, const char *planet)
{
    ui_open_screen("GALACTIC MAP", SCR_GALACTIC_MAP);
    if (ui_toolbar_has("ZOOM OUT")) {
        ui_toolbar("ZOOM OUT");
        ui_settle();
    }
    if (!ui_wait_hotspot(system, 200) || !ui_hotspot(system))
        return false;
    ui_settle();
    play_wait(20);
    return ui_find_hotspot(planet, 1) != 0;
}

// The galactic map's system list shows `system`.
static bool system_listed(const char *system)
{
    ui_open_screen("GALACTIC MAP", SCR_GALACTIC_MAP);
    if (ui_toolbar_has("ZOOM OUT")) {
        ui_toolbar("ZOOM OUT");
        ui_settle();
    }
    return ui_map_system_listed(system);
}

// The shop's queue ("Bought items:") of `item`, 0 if none.
static int queued(const char *item)
{
    UiShopQueue q;
    if (!ui_shop_select(item) || !ui_shop_queue(&q) || !q.shown)
        return 0;
    return q.queued;
}

// Buys up to `n` of `item` (at `pri`, as New Earth's ores allow) unless the
// stores or the queue already hold them. Returns the stores.
static int stock_up(PolPriority pri, const char *item, int n, int32_t price, const int32_t per[ORES])
{
    int stores = pol_stores(item), q = queued(item);
    int32_t have[ORES];
    if (stores + q < n && pol_ores(item, have)) {
        int k = n - stores - q;
        for (int i = 0; i < ORES; i++)
            if (per[i] > 0)
                k = SDL_min(k, (int)(have[i] / per[i]));
        if (k > 0)
            pol_buy(pri, item, k, price);
    }
    return stores;
}

// Logs a group's grid: units and weapons per row.
static void log_group(const char *group)
{
    static const char *const rows[][4] = {
        {"Hunter", "Laser cannon", "Twin laser gun"},
        {"Starfighter", "Twin laser gun", "Missile"},
        {"Destroyer", "Twin laser gun", "Missile", "Plasma gun"},
        {"Cruiser", "Missile", "Plasma gun"},
        {"Battle tank", "Laser cannon", "Twin laser gun"},
        {"Aircraft", "Twin laser gun", "Missile"},
        {"Missile launcher", "Missile"},
    };
    char line[384] = "";
    for (size_t r = 0; r < SDL_arraysize(rows); r++) {
        SDL_snprintf(line + SDL_strlen(line), sizeof line - SDL_strlen(line), " %s %d (", rows[r][0],
                     army_count(group, rows[r][0], nullptr));
        for (int w = 1; w < 4 && rows[r][w]; w++)
            SDL_snprintf(line + SDL_strlen(line), sizeof line - SDL_strlen(line), "%s%d", w > 1 ? "/" : "",
                         army_count(group, rows[r][0], rows[r][w]));
        SDL_strlcat(line, ")", sizeof line);
    }
    play_log("%s at %s:%s", group, pol_date(), line);
}

// --- Groups in flight ----------------------------------------------------------------

typedef struct {
    const char *group, *system, *planet, *moon;
} Trip;

// Flies `group` to `planet` (`moon`; planet nullptr: the sun) of `system`
// (nullptr: where it is), from wherever it is (an arrival waited for, a
// docked group taken off), without waiting for the arrival.
static void fly(void *arg)
{
    const Trip *t = arg;
    ui_ship_info();
    if (ui_group_status(t->group) == SHIP_FLYING && !ui_wait_arrival(t->group, TRIP_HOURS))
        play_fail("%s did not arrive in %d h", t->group, (int)TRIP_HOURS);
    if (!ui_take_off(t->group))
        play_fail("%s did not take off", t->group);
    if (!ui_launch(t->group, t->system, t->planet, t->moon))
        play_fail("%s not launched to %s %s", t->group, t->system ? t->system : "",
                  t->planet ? t->planet : "the sun");
}

// The group is in orbit or docked at planet `planet` of `system` (its
// location line ends "<system> system <planet>": "Earth" alone would also
// match "New Earth").
static bool group_at(const char *group, const char *system, const char *planet)
{
    char body[64];
    SDL_snprintf(body, sizeof body, "%s system %s", system, planet);
    ui_ship_info();
    return ui_group_status(group) != SHIP_FLYING && pol_ends_with(pol_where(group), body);
}

// The group's location line names `system`.
static bool group_in(const char *group, const char *system)
{
    ui_ship_info();
    return SDL_strstr(pol_where(group), system) != nullptr;
}

static void wait_arrival(void *group)
{
    ui_ship_info();
    if (ui_group_status(group) == SHIP_FLYING && !ui_wait_arrival(group, TRIP_HOURS))
        play_fail("%s did not arrive in %d h", (const char *)group, (int)TRIP_HOURS);
}

// Flies `group` to `planet` of `system`, through the system's sun if the
// system is unexplored (its planets unlisted), and waits for the arrival.
static void fly_there(void *arg)
{
    const Trip *t = arg;
    wait_arrival((void *)t->group);
    if (group_at(t->group, t->system, t->planet))
        return;
    if (group_in(t->group, t->system)) {
        fly(&(Trip){t->group, nullptr, t->planet, nullptr});
    } else {
        bool known = planet_on_map(t->system, t->planet);
        fly(&(Trip){t->group, t->system, known ? t->planet : nullptr, nullptr});
    }
    wait_arrival((void *)t->group);
}

static bool arrived(void *arg)
{
    const Trip *t = arg;
    return group_at(t->group, t->system, t->planet);
}

// fly_there until the group is there (a sun on the way takes two trips).
static void go_to(const char *group, const char *system, const char *planet)
{
    Trip t = {group, system, planet, nullptr};
    for (int leg = 0; leg < 4 && !pol_check(arrived, &t); leg++)
        pol_do(fly_there, &t);
    if (!pol_check(arrived, &t))
        play_fail("%s did not reach %s", group, planet);
}

static void log_forces(void *unused)
{
    (void)unused;
    log_group("Strike");
    log_group("Home Guard");
}

// --- X1, X2: Rigel 1 and its research ------------------------------------------------

static void dock_galleon(void *unused)
{
    (void)unused;
    ui_ship_info();
    if (ui_group_status("Galleon 1") != SHIP_DOCKED && !ui_dock("Galleon 1"))
        play_fail("X1: Galleon 1 cannot dock at Rigel 1");
}

void act5_x01_rigel1(void)
{
    // A group docking at Rigel 1 finds the alien shipwreck (shipmove.c):
    // MSG 31 and scene 6, Plasma gun and Missile launcher to analyse, and
    // Mental radar 100-119 h later. Rigel is listed since the League's
    // surrender; its planets only once a group has explored it (its sun).
    if (seen_or_logged("alien shipwreck"))
        return;
    pol_wait_for("X1: no wave due", no_wave_due, nullptr, 150);
    go_to("Galleon 1", "Rigel", "Rigel 1");
    pol_do(dock_galleon, nullptr);
    pol_wait_for("X1: the shipwreck (MSG 31)", event_or_logged, "alien shipwreck", 2 * POL_DAY);
    pol_new_earth(SCR_SURFACE);  // S1
}

void act5_x02_research(void)
{
    // Klaatoo (8/8/8/9): Missile launcher first (its line and the Missile
    // line are the act's gates), Plasma gun for the Destroyers and Cruisers,
    // then Mental radar (8/5/8/9, 10,000 / 90 h), offered 100-119 h after
    // the shipwreck: it finds Rigel 2 (X4).
    pol_research("Missile launcher", 20 * POL_DAY);
    pol_research("Plasma gun", 20 * POL_DAY);
    pol_research("Mental radar", 30 * POL_DAY);
}

// --- X3: Cruisers and launchers -----------------------------------------------------

static bool cruiser_line_runs(void *unused)
{
    (void)unused;
    return pol_stores("Cruiser") > 0 || queued("Cruiser") > 0;
}

void act5_x03_cruisers(void)
{
    // The army goal (army.c) builds Strike's act-5 force and Home Guard's
    // launchers; the Cruiser line (200 h each, one queued per Space station)
    // and the Missile line (~340 missiles of 10 h) set the date. This step
    // only checks that the Cruiser line runs.
    pol_poll_for("X3: the Cruiser line runs", cruiser_line_runs, nullptr, 10 * POL_DAY, 24);
    pol_try(log_forces, nullptr);
}

// --- X4, X5: Rigel 2, the Sun --------------------------------------------------------

static bool sun_listed(void *unused)
{
    (void)unused;
    return system_listed("Sun");
}

static bool rigel2_on_map(void *unused)
{
    (void)unused;
    return planet_on_map("Rigel", "Rigel 2");
}

void act5_x04_rigel2(void)
{
    // The Psy radar finds Rigel 2 1/10 per day once Mental radar is done
    // (sim.c); the rename prompt is accepted as it comes. A non-carrier
    // group arriving there meets the Syonians: MSG 29 and talk 10 ("Thank
    // you for your confidence in us..."), which lists the Sun system and
    // puts the Bounty-Hunter in the pub (main_program.c).
    if (pol_seen("talk 10") >= 0 || pol_check(sun_listed, nullptr))
        return;
    if (!pol_check(rigel2_on_map, nullptr))
        pol_wait_for("X4: the Psy radar finds Rigel 2", event_or_logged, "Psy-radar radar found", 60 * POL_DAY);
    pol_wait_for("X4: no wave due", no_wave_due, nullptr, 150);
    go_to("Galleon 1", "Rigel", "Rigel 2");
    pol_wait_for("X4: talk 10 with the Syonians", event_seen, "talk 10", 2 * POL_DAY);
    if (!pol_check(sun_listed, nullptr) && !pol_check(sun_listed, nullptr))
        play_fail("X4: no Sun in the system list after talk 10");
    pol_new_earth(SCR_SURFACE);  // S1
}

// X5: "Scout" (L5's carrier) docks at New Earth for a Spy satellite,
// explores the Sun system (its sun; carriers meet nobody, sim.c), flies to
// Earth and puts the satellite down: Earth's intel grows to 31 and, after
// the contact (X7), to 40 for GROUND WAR (sim.c:648-649). Done once Scout
// is in the Sun system with no satellite left.
static bool goal_x5_spy(void)
{
    if (!sun_listed(nullptr))
        return false;
    ui_ship_info();
    if (!ui_group_find("Scout")) {
        int carriers = stock_up(PRI_L8, "Sat.carrier", 1, CARRIER_PRICE, CARRIER_ORES);
        int sats = stock_up(PRI_L8, "Spy satellite", 1, SPY_SATELLITE_PRICE, SPY_SATELLITE_ORES);
        if (carriers < 1 || sats < 1)
            return false;
        if (!ui_new_group("Carrier", "Scout") || !ui_equip("Scout", "Sat.carrier", nullptr, 1)
            || !ui_equip("Scout", "Sat.carrier", "Spy satellite", 1))
            play_fail("X5: Scout not fitted");
        ui_ship_info();
    }
    int st = ui_group_status("Scout");
    if (st == SHIP_FLYING)
        return false;
    const char *loc = pol_where("Scout");
    bool in_sun = SDL_strstr(loc, "Sun") != nullptr;
    int sats = ui_group_units("Scout", "Sat.carrier", "Spy satellite");
    if (sats == 0 && in_sun)
        return true;
    if (sats == 0) {
        // The refit, docked at New Earth.
        if (!pol_ends_with(loc, "New Earth")) {
            if (!pol_s8_active() && ui_take_off("Scout"))
                ui_launch("Scout", "Amnesty", "New Earth", nullptr);
        } else if ((st == SHIP_DOCKED || ui_dock("Scout"))
                   && stock_up(PRI_L8, "Spy satellite", 1, SPY_SATELLITE_PRICE, SPY_SATELLITE_ORES) > 0
                   && !ui_equip("Scout", "Sat.carrier", "Spy satellite", 1)) {
            play_fail("X5: no Spy satellite on Scout");
        }
    } else if (st == SHIP_DOCKED && (pol_s8_active() || !ui_take_off("Scout"))) {
        // A wave announced (S8): stay.
    } else if (!in_sun) {
        bool known = planet_on_map("Sun", "Earth");
        ui_launch("Scout", "Sun", known ? "Earth" : nullptr, nullptr);
    } else if (!pol_ends_with(loc, "Earth")) {
        ui_launch("Scout", nullptr, "Earth", nullptr);
    } else {
        ui_open_group("Scout", SCR_GROUP);
        if (!ui_body_action("ADD SPY SAT"))
            play_fail("X5: no ADD SPY SAT at Earth");
        play_log("X5: spy satellite on Earth at %s", pol_date());
    }
    pol_new_earth(SCR_SURFACE);  // S1
    return false;
}

void act5_goals_x05(void)
{
    pol_goal("X5_spy_on_earth", PRI_L8, goal_x5_spy, 0);
}

static bool earth_spied(void *unused)
{
    (void)unused;
    return pol_goal_done("X5_spy_on_earth");
}

void act5_x05_sun(void)
{
    pol_poll_for("X5: the spy satellite on Earth", earth_spied, nullptr, 30 * POL_DAY, 12);
}

// --- X6, X7: the forces, the contact -------------------------------------------------

static bool home_guard_ready(void *unused)
{
    (void)unused;
    ArmyForce floor = army_target("Home Guard");
    bool ready = group_at("Home Guard", "Amnesty", "New Earth") && army_holds("Home Guard", &floor, true);
    if (!ready)
        log_group("Home Guard");
    return ready;
}

void act5_x06_home_guard(void)
{
    // New Earth keeps no warships: Earth wave 1 (80SF 12D 3C; 30Tr 30T
    // 30Air, 1,000-1,199 h after the contact) beats any home fleet the
    // policy could afford, and its landing loses to Home Guard's 40T 10Air
    // with 10 launchers (army.c). The wave only comes if Earth is not
    // taken within that time (X9's failure branch).
    pol_poll_for("X6: Home Guard with 40T 10Air 10ML", home_guard_ready, nullptr, 150 * POL_DAY, 24);
    pol_try(log_forces, nullptr);
}

static bool strike_ready(void *unused)
{
    (void)unused;
    ArmyForce t = army_target("Strike"), floor = army_target("Home Guard");
    bool ready = group_at("Strike", "Amnesty", "New Earth") && ui_group_status("Strike") == SHIP_DOCKED
                 && army_holds("Strike", &t, true) && army_holds("Home Guard", &floor, true);
    if (!ready)
        log_group("Strike");
    return ready;
}


void act5_x07_contact(void)
{
    // Strike with its whole force (army.c: 60H 60SF 12D 10C; 40T 20Air
    // 40ML) docked at New Earth, then out to Earth: an army group arriving
    // there meets the Earthlings (status 2 at once, intel.c): MSG 38, Earth
    // wave 1 launched 1,000-1,199 h later, the Treasonable in the pub.
    if (seen_or_logged("Earthlings declare war"))
        return;
    pol_poll_for("X7: Strike ready for Earth", strike_ready, nullptr, 150 * POL_DAY, 24);
    pol_try(log_forces, nullptr);
    pol_wait_for("X7: no wave due", no_wave_due, nullptr, 150);
    go_to("Strike", "Sun", "Earth");
    pol_wait_for("X7: the Earthlings declare war (MSG 38)", event_or_logged, "Earthlings declare war",
                 2 * POL_DAY);
    pol_new_earth(SCR_SURFACE);  // S1
}

void act5_x08_bounty_hunter(void)
{
    // Optional: his job only thins Earth's garrison
    // 100-199 h later, and the force of X7 won Earth 10/10 without it.
    play_log("X8: the Bounty-Hunter is not hired");
}

// --- X9: Earth --------------------------------------------------------------------------

// GROUND WAR at Earth: offered once its intel is 40 (the spy satellite,
// after the contact; launch.c). True once a space battle has been fought
// since `*since` (pol_try leaves the check at screen 29; the victory ends
// the game in pol_service).
static bool invade_earth(void *arg)
{
    const int64_t *since = arg;
    if (pol_seen_since("space battle", *since))
        return true;
    if (!group_at("Strike", "Sun", "Earth"))
        return false;
    ui_invade("Sun", "Earth", "Strike");
    return false;
}

static void dock_strike(void *unused)
{
    (void)unused;
    ui_ship_info();
    if (ui_group_status("Strike") != SHIP_DOCKED)
        ui_dock("Strike");
}

void act5_x09_earth(void)
{
    // " Your satellite found Earthlings colony at | Earth" (2-6 intel a day
    // from 31 after the contact), then GROUND WAR: the space battle, the
    // unit assignment and the ground battle (pol_service). The ground win on
    // Earth is the victory: screen 36, Halt(10). A lost battle sends Strike's
    // survivors home (intel.c): rebuild to the full force and go again
    // (X9), Home Guard holding New Earth against wave 1.
    for (int attempt = 0; attempt < 3; attempt++) {
        if (attempt > 0) {
            play_log("X9: attempt %d", attempt + 1);
            go_to("Strike", "Amnesty", "New Earth");
            pol_do(dock_strike, nullptr);
            pol_new_earth(SCR_SURFACE);  // S1
            pol_poll_for("X9: Strike rebuilt", strike_ready, nullptr, 150 * POL_DAY, 24);
            go_to("Strike", "Sun", "Earth");
        }
        int64_t since = pol_now();
        pol_poll_for("X9: the invasion of Earth", invade_earth, &since, 10 * POL_DAY, 6);
        play_log("X9: the invasion of Earth failed at %s", pol_date());
        pol_try(log_forces, nullptr);
    }
    play_fail("X9: Earth not taken in 3 attempts");
}
