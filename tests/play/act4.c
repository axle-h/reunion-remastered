#include "play/act4.h"

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
#include "play/ui_talk.h"

enum : int {
    SCR_COMMAND = 1,
    SCR_RESEARCH = 3,
    SCR_GALACTIC_MAP = 7,
    SCR_PLANET_INFO = 8,
    SCR_GROUP = 17,
    SCR_SURFACE = 20,
    SCR_PUB = 23,
    TRIP_HOURS = 30,  // a flight to another system: <= 249 + 1 + 249 legs at pilot experience + 1 an hour
    JULIE_PRICE = 120000,
    KLAATOO_PRICE = 250000,
    CARRIER_PRICE = 1530,
    SATELLITE_PRICE = 780,
    SPY_SATELLITE_PRICE = 3546,
    ANTARES_COST = 135000,  // the colony and its Nuclear Gen.
    OBSERVATORY_COST = 42000,
};

static const int32_t CARRIER_ORES[ORES] = {55, 105, 27, 18, 0, 0};
static const int32_t SATELLITE_ORES[ORES] = {37, 31, 12, 5, 0, 0};
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

// The command centre shows a staff member captioned `name` (hired).
static bool hired(const char *name)
{
    if (ui_screen() != SCR_COMMAND)
        ui_command_centre();
    return ui_find_hotspot(name, 1) != 0;
}

// The pub shows a person captioned `name`.
static bool in_pub(const char *name)
{
    if (ui_screen() != SCR_PUB) {
        if (ui_screen() != SCR_COMMAND)
            ui_command_centre();
        ui_open_screen("SPACE LOCAL", SCR_PUB);
    }
    return ui_pub_present(name);
}

// The galactic map shows `planet` in `system`'s view (selecting the system
// in the list; no body is clicked): the system has been explored.
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

static void fly_to(const char *group, const char *system, const char *planet, const char *moon)
{
    pol_do(fly, &(Trip){group, system, planet, moon});
}

// The group is in orbit or docked at `body` (its location line ends so).
static bool group_at(const char *group, const char *body)
{
    ui_ship_info();
    return ui_group_status(group) != SHIP_FLYING && pol_ends_with(pol_where(group), body);
}

static void wait_arrival(void *group)
{
    ui_ship_info();
    if (ui_group_status(group) == SHIP_FLYING && !ui_wait_arrival(group, TRIP_HOURS))
        play_fail("%s did not arrive in %d h", (const char *)group, (int)TRIP_HOURS);
}

// --- L1, L2: staff and their courses ----------------------------------------------------

void act4_l01_pilot(void)
{
    // Jack (pilot grade 2) flies Army and Trade groups between Amnesty,
    // Phoenix and Mirach only (launch.c: the system list); Julie anywhere.
    // Hiring her replaces him.
    if (!hired("Julie-Anne Roxford"))
        pol_hire("PILOTS", "Julie-Anne Roxford", PRI_L1, JULIE_PRICE);
}

void act4_l02_developer(void)
{
    // Klaatoo (5/7/6/5 at hire, experience 40; his caps are 9 at every story
    // stage) for the act-5 products that need 8 or 9 (Plasma gun P8, Mental
    // radar 8/5/8/9); he replaces Dempsey (7/7/7/4).
    if (!hired("Klaatoo"))
        pol_hire("DEVELOPERS", "Klaatoo", PRI_L2, KLAATOO_PRICE);
}

static struct {
    uint32_t run;
    bool julie;  // Julie has been to the university
} courses;

// The developer's skills (Maths, Physics, Electronics, A.I.) under the
// research screen.
static void dev_skills(int skills[4])
{
    ui_goto(SCR_RESEARCH);
    if (!ui_developer_skills(skills))
        play_fail("no developer on the research screen");
}

// L1: Julie's course (experience 40 to 90: travel is experience + 1 legs an
// hour). L2: Klaatoo's courses up to 8/8/8/9 (Mental radar 8/5/8/9, Plasma
// gun P8): Maths & physics. (M+2 P+1), Electr and maths. (M+1 E+2), then
// Artificial int. (A+2) twice; one at a time, none while Aircraft waits for
// him (research stops while he is away).
static bool goal_l1_courses(void)
{
    if (courses.run != play_run_id())
        courses = (typeof(courses)){.run = play_run_id()};
    if (!courses.julie && hired("Julie-Anne Roxford"))
        courses.julie = pol_university(PRI_L1, "Julie-Anne Roxford", nullptr, 80000);
    if (!hired("Klaatoo"))
        return false;
    int st = pol_research_state("Aircraft");
    if (st != UI_RS_UNKNOWN && st != UI_RS_DONE)
        return false;
    int sk[4];
    dev_skills(sk);
    const char *course = sk[0] < 7   ? "Maths & physics."
                         : sk[2] < 8 ? "Electr and maths."
                         : sk[3] < 9 ? "Artificial int."
                         : sk[1] < 8 ? "Phisics.& electr."
                                     : nullptr;
    if (!course)
        return courses.julie;
    pol_university(PRI_L2, "Klaatoo", course, INT32_MAX);
    return false;
}

// Strike (Home Guard renamed for Mirach, M18) comes home and docks at New
// Earth, where the army goal refills it.
static bool goal_strike_home(void)
{
    // Act 5 sends Strike to Earth (a run resumed there registers this goal
    // again).
    if (pol_act() >= 5)
        return true;
    ui_ship_info();
    if (!ui_group_find("Strike"))
        return true;  // none: army.c creates it at the dock
    int st = ui_group_status("Strike");
    if (st == SHIP_FLYING)
        return false;
    bool home = pol_ends_with(pol_where("Strike"), "New Earth");
    if (home && st == SHIP_DOCKED)
        return true;
    if (home)
        ui_dock("Strike");
    else
        ui_launch("Strike", "Amnesty", "New Earth", nullptr);
    pol_new_earth(SCR_SURFACE);  // S1
    return false;
}

void act4_goals_l01(void)
{
    pol_goal("L1_courses", PRI_L1, goal_l1_courses, 0);
    pol_goal("L7_strike_home", PRI_M8, goal_strike_home, 0);
}

// --- L3, L4: the Informer, the carriers, the League tip ----------------------------------

static void ask_informer(void *unused)
{
    (void)unused;
    if (in_pub("Pirate Leader"))
        return;
    ui_pub_ask("Informer", "Do you know anything about the", "All right, talk.");
    play_log("L3: the Informer said '%s'", ui_last_reply());
}

void act4_l03_informer(void)
{
    // The Morgrul in the pub (there since Mirach) masks the bartender's
    // League tip (main_program.c, the bartender's menu) until the Informer
    // explains him (500): the pub then captions him "Pirate Leader". His
    // cargo raids are optional (§5 R11): not taken.
    if (in_pub("Pirate Leader"))
        return;
    pol_wait_money("L3: the Informer", PRI_L1, 500, 10 * POL_DAY);
    pol_do(ask_informer, nullptr);
    if (!in_pub("Pirate Leader"))
        play_fail("L3: no Pirate Leader after the Informer");
}

// A carrier group `name` of `n` carriers, each with one `sat` ("Satellite"
// or "Spy satellite"), bought and fitted at New Earth. True once it exists.
static bool carrier_group(PolPriority pri, const char *name, int n, const char *sat, int32_t sat_price,
                          const int32_t sat_ores[ORES])
{
    ui_ship_info();
    if (ui_group_find(name))
        return true;
    int carriers = stock_up(pri, "Sat.carrier", n, CARRIER_PRICE, CARRIER_ORES);
    int sats = stock_up(pri, sat, n, sat_price, sat_ores);
    if (carriers < n || sats < n)
        return false;
    if (!ui_new_group("Carrier", name))
        play_fail("no Carrier group type");
    if (!ui_equip(name, "Sat.carrier", nullptr, n) || !ui_equip(name, "Sat.carrier", sat, n))
        play_fail("%s not equipped", name);
    return true;
}

// The carrier group is in orbit and may be sent on (a docked one takes off
// unless a wave is announced: S8).
static bool carrier_ready(const char *name)
{
    int st = ui_group_status(name);
    if (st == SHIP_FLYING)
        return false;
    if (st == SHIP_DOCKED && (pol_s8_active() || !ui_take_off(name)))
        return false;
    return true;
}

// L5: "Scout", a carrier with a satellite, explores Antares (carriers meet
// nobody, sim.c:1117) and puts the satellite on Antares 3, whose intel then
// reaches 30 for the COLONIZATION icon (satellite.c); then home, out of
// Antares before the supernova.
static bool goal_l4_scout(void)
{
    if (!carrier_group(PRI_L5, "Scout", 1, "Satellite", SATELLITE_PRICE, SATELLITE_ORES)
        || !carrier_ready("Scout"))
        return false;
    const char *loc = pol_where("Scout");
    bool in_antares = SDL_strstr(loc, "Antares") != nullptr;
    if (ui_group_units("Scout", "Sat.carrier", "Satellite") > 0) {
        if (!in_antares)
            ui_launch("Scout", "Antares", nullptr, nullptr);
        else if (!pol_ends_with(loc, "Antares 3"))
            ui_launch("Scout", nullptr, "Antares 3", nullptr);
        else
            ui_deploy_carrier_satellite("Scout");
    } else if (in_antares) {
        ui_launch("Scout", "Amnesty", "New Earth", nullptr);
    }
    pol_new_earth(SCR_SURFACE);  // S1
    return !in_antares && ui_group_units("Scout", "Sat.carrier", "Satellite") == 0;
}

// L8: the targets of the invasions (L13) and their systems, in the order
// the spy satellites go down.
typedef struct {
    const char *system, *planet, *race;
    ArmyForce force;  // Strike's for the invasion (army.c tiers)
} Target;

static const Target TARGETS[] = {
    {"Orionis", "Orionis 3", "The Druedians", {{40, 40, 6, 30, 20}}},
    {"Lyrae", "Lyrae 5", "The Hirachi", {{40, 40, 6, 30, 20}}},
    {"Lyrae", "Lyrae 7", "The Lisonians", {{50, 50, 8, 30, 30}}},
};
enum { TARGET_COUNT = (int)SDL_arraysize(TARGETS) };

// L8: "Spy 1" (M17's carrier, its satellite on Mirach 3) comes home, docks
// and takes three carriers with a Spy satellite each; it explores Orionis
// (the League tip, L4) and Lyrae and puts a satellite on each target: their
// intel grows to 31 and, after the contact (L9), to 40 (sim.c:648-649).
// Lyrae 3 (Undorlings, allied after the Eran's job) needs none.
static bool refit_spies(void)
{
    ui_ship_info();
    if (!ui_group_find("Spy 1"))
        return carrier_group(PRI_L8, "Spy 1", TARGET_COUNT, "Spy satellite", SPY_SATELLITE_PRICE,
                             SPY_SATELLITE_ORES);
    int carriers = ui_group_units("Spy 1", "Sat.carrier", nullptr);
    if (carriers >= TARGET_COUNT)
        return true;
    int st = ui_group_status("Spy 1");
    if (st == SHIP_FLYING)
        return false;
    if (!pol_ends_with(pol_where("Spy 1"), "New Earth")) {
        if (!pol_s8_active())
            ui_launch("Spy 1", "Amnesty", "New Earth", nullptr);
        return false;
    }
    if (st != SHIP_DOCKED && !ui_dock("Spy 1"))
        return false;
    int sats = ui_group_units("Spy 1", "Sat.carrier", "Spy satellite");
    int have_c = stock_up(PRI_L8, "Sat.carrier", TARGET_COUNT - carriers, CARRIER_PRICE, CARRIER_ORES);
    int have_s = stock_up(PRI_L8, "Spy satellite", TARGET_COUNT - sats, SPY_SATELLITE_PRICE,
                          SPY_SATELLITE_ORES);
    if (have_c < TARGET_COUNT - carriers || have_s < TARGET_COUNT - sats)
        return false;
    if (!ui_equip("Spy 1", "Sat.carrier", nullptr, TARGET_COUNT)
        || !ui_equip("Spy 1", "Sat.carrier", "Spy satellite", TARGET_COUNT))
        play_fail("L8: Spy 1 not refitted");
    return true;
}

static bool goal_l8_spies(void)
{
    if (!refit_spies() || !carrier_ready("Spy 1"))
        return false;
    int left = ui_group_units("Spy 1", "Sat.carrier", "Spy satellite");
    if (left == 0)
        return true;
    const Target *t = &TARGETS[TARGET_COUNT - left];
    const char *loc = pol_where("Spy 1");
    if (!SDL_strstr(loc, t->system)) {
        // Its sun first if the system is unexplored (its planets unlisted).
        bool known = planet_on_map(t->system, t->planet);
        ui_launch("Spy 1", t->system, known ? t->planet : nullptr, nullptr);
    } else if (!pol_ends_with(loc, t->planet)) {
        ui_launch("Spy 1", nullptr, t->planet, nullptr);
    } else {
        ui_open_group("Spy 1", SCR_GROUP);
        if (!ui_body_action("ADD SPY SAT"))
            play_fail("L8: no ADD SPY SAT at %s", t->planet);
        play_log("L8: spy satellite on %s", t->planet);
    }
    pol_new_earth(SCR_SURFACE);  // S1
    return false;
}

void act4_goals_l04(void)
{
    pol_goal("L4_scout", PRI_L5, goal_l4_scout, 0);
    pol_goal("L8_spies", PRI_L8, goal_l8_spies, 0);
}

static bool orionis_explored(void *unused)
{
    (void)unused;
    return planet_on_map("Orionis", "Orionis 3");
}

static bool tip_bought(void)
{
    return SDL_strstr(ui_last_reply(), "League doesn't like") != nullptr;
}

static void ask_bartender(void *unused)
{
    (void)unused;
    if (tip_bought())
        return;
    if (ui_screen() != SCR_PUB)
        ui_command_centre();
    ui_pub_ask("Bartender", "Have you heard about the league?", "Tell it!");
    play_log("L4: the bartender said '%s'", ui_last_reply());
}

void act4_l04_league_tip(void)
{
    // The bartender offers the tip (5,000) once Orionis or Lyrae is explored
    // and until the League's first wave leaves, 200-249 h after the first
    // League contact (main_program.c, intel.c:86-92). The Eran's job needs
    // it (L10).
    pol_poll_for("L4: Orionis explored", orionis_explored, nullptr, 30 * POL_DAY, 12);
    pol_wait_money("L4: the League tip", PRI_L1, 5000, 10 * POL_DAY);
    pol_do(ask_bartender, nullptr);
    if (!tip_bought())
        play_fail("L4: no League tip ('%s')", ui_last_reply());
    pol_note("League tip");
}

// --- L5, L6: Antares and the Erans ----------------------------------------------------

static const char *const ANTARES_STARTERS[] = {"Nuclear Gen."};

// Antares 3's planet info (the system selected first: the map shows the
// selected system).
static void open_antares_3(int screen)
{
    planet_on_map("Antares", "Antares 3");
    ui_open_body("Antares 3", nullptr, screen);
}

// L5: the colony on Antares 3 once the satellite's intel offers
// COLONIZATION (William Forbs can found colonies anywhere: walkthrough
// "Builder tier"), with a Nuclear Gen. for the Observatory (750 kWh), then
// the Observatory: 50 h after it works, MSG 25 (sim.c:1550-1556).
static bool goal_l5_antares(void)
{
    if (!planet_on_map("Antares", "Antares 3"))
        return false;
    open_antares_3(SCR_PLANET_INFO);
    bool done = false;
    if (ui_body_type_says("olony") && !ui_body_type_says("under")) {
        open_antares_3(SCR_SURFACE);
        if (ui_buildings("Observatory").total > 0)
            done = true;
        else if (pol_build(PRI_L5, "Observatory", OBSERVATORY_COST) == UI_BUILD_OK)
            play_log("L5: Observatory on Antares 3 at %s", pol_date());
    } else if (!ui_body_type_says("olony") && ui_toolbar_has("COLONIZATION")
               && pol_afford(PRI_L5, ANTARES_COST) && ui_found_colony(ANTARES_STARTERS, 1)) {
        play_log("L5: colony founded on Antares 3 at %s", pol_date());
    }
    pol_new_earth(SCR_SURFACE);  // S1
    return done;
}

void act4_goals_l05(void)
{
    pol_goal("L5_antares", PRI_L5, goal_l5_antares, 0);
}

void act4_l05_antares(void)
{
    // Colony (1-2 days to the box), the Observatory, then 50 h: MSG 25
    // "...the sun in the Antares System will shortly become a Supernova".
    pol_wait_for("L5: the nova warning (MSG 25)", event_or_logged, "Observatory reports that", 60 * POL_DAY);
}

static bool no_wave_due(void *unused)
{
    (void)unused;
    return !pol_s8_active();
}

// Every group the policy flies, for L6's check that none is in Antares.
static const char *const FLOWN[] = {"Galleon 1", "Scout", "Spy 1", "Strike", "Trade 1"};

static bool antares_empty(void)
{
    for (size_t i = 0; i < SDL_arraysize(FLOWN); i++) {
        ui_ship_info();
        if (ui_group_find(FLOWN[i]) && SDL_strstr(pol_where(FLOWN[i]), "Antares"))
            return false;
    }
    return true;
}

static void leave_antares(void *unused)
{
    (void)unused;
    for (size_t i = 0; i < SDL_arraysize(FLOWN); i++) {
        ui_ship_info();
        if (!ui_group_find(FLOWN[i]) || !SDL_strstr(pol_where(FLOWN[i]), "Antares"))
            continue;
        if (!ui_take_off(FLOWN[i]) || !ui_launch(FLOWN[i], "Amnesty", "New Earth", nullptr))
            play_fail("L6: %s not sent out of Antares", FLOWN[i]);
        play_log("L6: %s leaves Antares", FLOWN[i]);
    }
}

void act4_l06_erans(void)
{
    // After MSG 25 a group arriving at our colony in Antares meets the
    // Erans (sim.c:1138-1144): MSG 45, talk 9 ("Yes ! My people will welcome
    // you."): the Eran in the pub, MSG 46. The supernova comes 560-569 h
    // after the talk and deletes everything in Antares: every group leaves
    // at once and none goes back (R13).
    if (pol_seen("talk 9") < 0) {
        // R7: no arrival while a wave is announced.
        pol_wait_for("L6: no wave due", no_wave_due, nullptr, 150);
        fly_to("Galleon 1", "Antares", "Antares 3", nullptr);
        if (!pol_try(wait_arrival, "Galleon 1"))
            pol_service();
        pol_wait_for("L6: talk 9 with the Erans", event_seen, "talk 9", 2 * POL_DAY);
    }
    pol_do(leave_antares, nullptr);
    if (!antares_empty())
        play_fail("L6: a group is still in Antares");
    pol_new_earth(SCR_SURFACE);  // S1
}

// --- L7: the army at home ---------------------------------------------------------------

static const ArmyForce WAVE_FORCE = {{40, 40, 6, 0, 0}};

static bool strike_home_ready(void *unused)
{
    (void)unused;
    return group_at("Strike", "New Earth") && army_holds("Strike", &WAVE_FORCE, false);
}

void act4_l07_home_guard(void)
{
    // Strike, docked at New Earth, defends it against the Lisonian wave
    // (20H 20SF 3D; 30Tr 15T 5Air), which leaves 200-249 h after the first
    // League contact (L9): 40H 40SF 6D won 10/10 in space losing ~9H 2SF;
    // the 40-tank reserve alone loses the landing to the 5 aircraft, so the
    // space win is the defence until Aircraft (L11).
    pol_poll_for("L7: Strike at home with 40H 40SF 6D", strike_home_ready, nullptr, 150 * POL_DAY, 24);
    play_log("L7: Strike %d H, %d SF, %d D, %d T at %s", army_count("Strike", "Hunter", nullptr),
             army_count("Strike", "Starfighter", nullptr), army_count("Strike", "Destroyer", nullptr),
             army_count("Strike", "Battle tank", nullptr), pol_date());
}

// --- L8, L9: spy satellites and the League contact -----------------------------------

static bool spies_down(void *unused)
{
    (void)unused;
    return pol_goal_done("L8_spies");
}

void act4_l08_spy_satellites(void)
{
    pol_poll_for("L8: the spy satellites down", spies_down, nullptr, 40 * POL_DAY, 12);
}

// The League's home worlds in the order Galleon 1 visits them (L9): the
// Lisonians first (MSG 28 and the wave's timer), the Undorlings (both for
// the Eran's job), then the two others (their intel stalls at 31 until
// contact).
static const struct {
    const char *system, *planet, *race;
} LEAGUE[] = {
    {"Lyrae", "Lyrae 7", "'The Lisonians'"},
    {"Lyrae", "Lyrae 3", "'Undorlings'"},
    {"Lyrae", "Lyrae 5", "'The Hirachi'"},
    {"Orionis", "Orionis 3", "'The Druedians'"},
};

void act4_l09_league_contact(void)
{
    // A trade group arriving at a League home world meets its race (status
    // 2 at once, intel.c:47-101); the first contact shows MSG 28 "The
    // League declares war against you" and starts the wave's timer.
    for (size_t i = 0; i < SDL_arraysize(LEAGUE); i++) {
        if (pol_seen_like(LEAGUE[i].race, 0) >= 0)
            continue;
        pol_wait_for("L9: no wave due", no_wave_due, nullptr, 150);
        fly_to("Galleon 1", LEAGUE[i].system, LEAGUE[i].planet, nullptr);
        if (!pol_try(wait_arrival, "Galleon 1"))
            pol_service();
        pol_wait_for("L9: the contact", event_seen, (void *)LEAGUE[i].race, 2 * POL_DAY);
    }
    play_log("L9: the League met, MSG 28 at %d", (int)pol_seen_like("League declares war", 0));
    // Home (S8 sends it on to Penelope when the wave is announced).
    fly_to("Galleon 1", "Amnesty", "New Earth", nullptr);
    pol_new_earth(SCR_SURFACE);  // S1
}

// --- L10, L11: the Eran's job, Aircraft ------------------------------------------------

static void ask_eran(void *unused)
{
    (void)unused;
    // His job running, the pub does not draw him (pub.c).
    if (!in_pub("Eran"))
        return;
    ui_pub_ask("Eran", "I have saved your race", "As I understand", "If any race joins", "Undorlings.");
    play_log("L10: the Eran said '%s'", ui_last_reply());
}

void act4_l10_eran_job(void)
{
    // He offers it with the League tip bought and the Lisonians and the
    // Undorlings met (main_program.c, his menu); 100-199 h later MSG 40 and
    // MSG 44: the Undorlings allied, Ion cannon granted, Aircraft to analyse
    // (sim.c:217-231).
    if (pol_seen_like("separating the Undorlings", 0) >= 0)
        return;
    pol_do(ask_eran, nullptr);
    if (SDL_strstr(ui_last_reply(), "give it a try") == nullptr)
        play_fail("L10: the Eran said '%s'", ui_last_reply());
    pol_wait_for("L10: the Undorlings allied (MSG 44)", event_or_logged, "separating the Undorlings", 250);
}

void act4_l11_aircraft(void)
{
    // 2,500 / experience h of research (6/7/5/1); the army goal then builds
    // the aircraft with their loadouts (4 Twin, 1 Missile).
    pol_research("Aircraft", 20 * POL_DAY);
}

// --- L12, L13: the Lisonian wave, the invasions ----------------------------------------

static bool wave_beaten(void *unused)
{
    (void)unused;
    int64_t league = pol_seen_like("League declares war", 0);
    return (league >= 0 && pol_seen_since("space battle won", league))
           || ui_log_has("under attack by The Lisonians");
}

void act4_l12_lisonian_wave(void)
{
    // Lisonian force 1 leaves Lyrae 7 200-249 h after the first contact,
    // reaches New Earth 50-69 h later and attacks within 5-34 h (speed 70,
    // intel.c): S8 sends the trade and carrier groups away; Strike defends.
    pol_wait_for("L12: the Lisonian wave beaten", wave_beaten, nullptr, 450);
    if (pol_seen("space battle lost") >= 0)
        play_log("L12: the space battle at New Earth was lost");
}

static char eliminated[64];

static const char *eliminated_text(const Target *t)
{
    SDL_snprintf(eliminated, sizeof eliminated, "eliminated '%s'", t->race);
    return eliminated;
}

static const ArmyForce HOME_FLOOR = {{0, 0, 0, 40, 10}};

// Logs a group's grid: units and weapons per row.
static void log_group(const char *group)
{
    static const char *const rows[][3] = {
        {"Hunter", "Laser cannon", "Twin laser gun"}, {"Starfighter", "Twin laser gun", "Missile"},
        {"Destroyer", "Twin laser gun", "Missile"},   {"Battle tank", "Laser cannon", "Twin laser gun"},
        {"Aircraft", "Twin laser gun", "Missile"},
    };
    char line[256] = "";
    for (size_t r = 0; r < SDL_arraysize(rows); r++)
        SDL_snprintf(line + SDL_strlen(line), sizeof line - SDL_strlen(line), " %s %d (%d/%d)",
                     rows[r][0], army_count(group, rows[r][0], nullptr),
                     army_count(group, rows[r][0], rows[r][1]), army_count(group, rows[r][0], rows[r][2]));
    play_log("%s at %s:%s", group, pol_date(), line);
}

static bool strike_ready_for(void *arg)
{
    const Target *t = arg;
    bool ready = group_at("Strike", "New Earth") && ui_group_status("Strike") == SHIP_DOCKED
                 && army_holds("Strike", &t->force, true) && army_holds("Home Guard", &HOME_FLOOR, true);
    if (!ready) {
        log_group("Strike");
        log_group("Home Guard");
    }
    return ready;
}

static void strike_out(void *arg)
{
    const Target *t = arg;
    if (pol_seen_like(eliminated_text(t), 0) >= 0 || group_at("Strike", t->planet))
        return;
    fly(&(Trip){"Strike", t->system, t->planet, nullptr});
    wait_arrival("Strike");
}

// GROUND WAR at the target: offered once its intel is 40 (the spy
// satellite, after the contact; launch.c:133). True if the battles began
// (pol_try leaves the sequence at screen 29) or the race is gone.
static bool invade(void *arg)
{
    const Target *t = arg;
    if (pol_seen_like(eliminated_text(t), 0) >= 0)
        return true;
    return ui_invade(t->system, t->planet, "Strike");
}

static void strike_home(void *unused)
{
    (void)unused;
    if (group_at("Strike", "New Earth")) {
        ui_dock("Strike");
        return;
    }
    fly(&(Trip){"Strike", "Amnesty", "New Earth", nullptr});
    wait_arrival("Strike");
    ui_dock("Strike");
}

void act4_l13_league(void)
{
    // Each target in turn: Strike with its force (army.c), Home Guard with
    // the reserve at home (S9), out to the target, GROUND WAR once the
    // intel allows it (" Your satellite found <race> colony at", sim.c:
    // 682-689), the battles (pol_service), home to refit. The last win
    // brings MSG 32 "The races of the League surrender!" (main_program.c).
    for (int i = 0; i < TARGET_COUNT; i++) {
        const Target *t = &TARGETS[i];
        if (seen_or_logged(eliminated_text(t)))
            continue;
        char what[96];
        SDL_snprintf(what, sizeof what, "L13: Strike ready for %s", t->planet);
        pol_poll_for(what, strike_ready_for, (void *)t, 150 * POL_DAY, 24);
        pol_do(strike_out, (void *)t);
        // The battles come up from the check (pol_check handles them); the
        // next check finds the race eliminated.
        SDL_snprintf(what, sizeof what, "L13: %s taken", t->planet);
        pol_poll_for(what, invade, (void *)t, 30 * POL_DAY, 6);
        play_log("L13: %s taken at %s", t->planet, pol_date());
        log_group("Strike");
        pol_do(strike_home, nullptr);
        pol_new_earth(SCR_SURFACE);  // S1
    }
    pol_wait_for("L13: the League surrenders (MSG 32)", event_or_logged, "League surrender", 48);
}
