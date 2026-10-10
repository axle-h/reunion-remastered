#include "play/act3.h"

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
    SCR_RESEARCH = 3,
    SCR_GALACTIC_MAP = 7,
    SCR_SURFACE = 20,
    TRIP_HOURS = 30,  // a flight to another system: <= 249 + 1 + 249 legs at pilot experience + 1 an hour
};

// Mirach 2's first moon, as the destination map names it.
static const char *const MIRACH_2_MOON_1 = "Moon 1";

// --- Visible events ---------------------------------------------------------------

// A box this run has seen, or a line of the message log (for a step
// resumed after the box).
static bool seen_or_logged(const char *text)
{
    return pol_seen(text) >= 0 || ui_log_has(text);
}

static bool event_seen(void *text)
{
    return pol_seen(text) >= 0;
}

// --- The army (M2, M8, M12, M16, M18; S9, S10: army.c) -------------------------

// The shop's queue ("Bought items:") of `item`, 0 if none.
static int queued(const char *item)
{
    UiShopQueue q;
    if (!ui_shop_select(item) || !ui_shop_queue(&q) || !q.shown)
        return 0;
    return q.queued;
}

// Home Guard's count of `ship` (and `weapon` on its row).
static int home_guard(const char *ship, const char *weapon)
{
    ui_ship_info();
    return ui_group_units(army_group(), ship, weapon);
}

void act3_goals_m02(void)
{
    army_goals();
}

// --- Galleon 1 (M3-M6, M11, M15) ------------------------------------------------------

// Galleon 1 in orbit wherever it is: waits for an arrival, takes off.
static void galleon_in_orbit(void)
{
    ui_ship_info();
    if (ui_group_status("Galleon 1") == SHIP_FLYING && !ui_wait_arrival("Galleon 1", TRIP_HOURS))
        play_fail("Galleon 1 did not arrive in %d h", (int)TRIP_HOURS);
    if (!ui_take_off("Galleon 1"))
        play_fail("Galleon 1 did not take off");
}

typedef struct {
    const char *system, *planet, *moon;
} Place;

// Flies Galleon 1 to `planet` (`moon`; planet nullptr: the sun) of `system`
// (nullptr: where it is) without waiting for the arrival.
static void galleon_fly(void *arg)
{
    const Place *p = arg;
    galleon_in_orbit();
    if (!ui_launch("Galleon 1", p->system, p->planet, p->moon))
        play_fail("Galleon 1 not launched to %s %s", p->system ? p->system : "",
                  p->planet ? p->planet : "the sun");
}

static void galleon_to(const char *system, const char *planet, const char *moon)
{
    pol_do(galleon_fly, &(Place){system, planet, moon});
}

// The galactic map shows `planet` in `system`'s view (selecting the system
// in the list; no body is clicked).
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

// --- M1, M2: wave 1, twin lasers -----------------------------------------------------

void act3_m01_wave1(void)
{
    // Won in act 2 (2928-08-03): KIT 13, the Twin laser gun, comes 20-39 h
    // after the first space win (main_program.c:1248), so its icon on the
    // research screen also tells a resumed step.
    if (pol_seen("space battle won") >= 0 || pol_research_state("Twin laser gun") != UI_RS_UNKNOWN)
        return;
    pol_wait_for("M1: wave 1 beaten", event_seen, "space battle won", 750);
}

void act3_m02_twin_lasers(void)
{
    // KIT 13 came in act 2; 200 / 70 = 3 h of research. The army goal (M8)
    // orders a twin per Hunter (13 h each on the Twin line) and fits them
    // as they come; Trooper (KIT 17) follows through S11.
    pol_research("Twin laser gun", 10 * POL_DAY);
}

// --- M3-M7: Phoenix, the Kalls and the Phelonians ------------------------------------

void act3_m03_phoenix(void)
{
    // Phoenix's planets show once a group reaches its sun (sim.c:1068-1084);
    // the first jump plays MSG 34 and scene 2.
    if (planet_on_map("Phoenix", "Phoenix 3"))
        return;
    galleon_to("Phoenix", nullptr, nullptr);
    pol_wait_for("M3: Phoenix explored", event_seen, "explored Phoenix system", 10 * POL_DAY);
}

void act3_m04_kalls(void)
{
    // Phoenix 3 is the Kalls' home: first contact, MSG 15 and talk 5 ("When
    // are we receiving your weapons?": Starfighter to analyse, 10 in stock).
    if (pol_research_state("Starfighter") != UI_RS_UNKNOWN)
        return;
    galleon_to(nullptr, "Phoenix 3", nullptr);
    pol_wait_for("M4: talk 5 with the Kalls", event_seen, "talk 5", 10 * POL_DAY);
    // Starfighter first, whatever S11 started meanwhile (Trooper, KIT 17).
    ui_research_start("Starfighter");
}

static bool at_phoenix_5(void)
{
    return pol_ends_with(pol_where("Galleon 1"), "Phoenix 5");
}

void act3_m06_phelonians(void)
{
    // Phoenix 5 is hidden (status -3): only the Informer's tip shows it
    // (main_program.c:1059-1072). Arriving there meets the Phelonians.
    if (pol_research_state("Battle tank") == UI_RS_DONE || at_phoenix_5())
        return;
    if (!planet_on_map("Phoenix", "Phoenix 5")) {
        pol_wait_money("M6: the Informer", PRI_M8, 500, 10 * POL_DAY);
        ui_command_centre();
        ui_pub_ask("Informer", "Do you have any information?", "All right, talk.");
        play_log("M6: the Informer said '%s'", ui_last_reply());
        if (!planet_on_map("Phoenix", "Phoenix 5"))
            play_fail("M6: Phoenix 5 not on the map after the Informer's tip");
    }
    galleon_to(nullptr, "Phoenix 5", nullptr);
    pol_wait_for("M6: contact with the Phelonians", event_seen, "'Phelonians'", 10 * POL_DAY);
}

static bool tank_granted(void *unused)
{
    (void)unused;
    return pol_seen("talk 7") >= 0;
}

void act3_m07_tank_deal(void)
{
    // Talk 7 20-39 h after the contact, no box (sim.c:1538-1541): "It's a
    // pleasure doing business with you." after the new invention: Battle
    // tank granted, New Earth's Energon halved.
    if (pol_research_state("Battle tank") != UI_RS_DONE)
        pol_wait_for("M7: talk 7 with the Phelonians", tank_granted, nullptr, 60);
    if (pol_research_state("Battle tank") != UI_RS_DONE)
        play_fail("M7: Battle tank not granted");
}

// M5: Phoenix 1 (status -2) is found by a group in flight in Phoenix at
// midnight (sim.c:781-805, run at hour 0 before the groups move): each
// hidden planet rolls 1 in 5 a day, the first that hits is found (Phoenix 4
// is the other one). Galleon 1 shuttles between Phoenix 3 and Phoenix 5,
// sent off again between the goals of a pass (a flight takes ~3 h with
// Jack, a pass of the goals ~3 days of clicks); from 18:00 on the status
// bar's clock it is kept in flight until the clock turns to 0:00 (sent
// off again on each landing, its group screen shown, the clock running).
// Sent off only when found in orbit, it was in flight at one midnight in
// three or four, and found nothing in 30 days in a run resumed at E16.
typedef struct {
    int64_t midnight;
} MidnightWait;

static bool landed_or_midnight(void *arg)
{
    const MidnightWait *w = arg;
    return ui_group_status("Galleon 1") == SHIP_ORBIT || pol_now() >= w->midnight;
}

static void shuttle(void)
{
    ui_ship_info();
    if (pol_s8_active())
        return;
    MidnightWait w = {(pol_now() / POL_DAY + 1) * POL_DAY};
    bool evening = pol_now() % POL_DAY >= 18;
    do {
        if (ui_group_status("Galleon 1") != SHIP_FLYING)
            galleon_fly(&(Place){nullptr, at_phoenix_5() ? "Phoenix 3" : "Phoenix 5", nullptr});
        if (evening) {
            ui_open_group("Galleon 1", 22);
            ui_wait_hours(landed_or_midnight, &w, 8);
        }
    } while (evening && pol_now() < w.midnight);
    pol_new_earth(SCR_SURFACE);  // S1: the goals' maps show Amnesty
}

static bool phoenix_1_found(void *unused)
{
    (void)unused;
    return planet_on_map("Phoenix", "Phoenix 1");
}

void act3_m05_phoenix1(void)
{
    // Before MSG 17 (the Phoenix S.O.S.), so selecting Phoenix 1 is harmless
    // (§5 R5); after it the drifting ship's trigger is armed.
    if (seen_or_logged("S.O.S. signal has been decoded"))
        play_fail("M5: the Phoenix S.O.S. came first");
    pol_between_goals(shuttle);
    pol_poll_for("M5: Phoenix 1 found", phoenix_1_found, nullptr, 30 * POL_DAY, 24);
    pol_between_goals(nullptr);
}

// --- M8-M10: wave 2 --------------------------------------------------------------------

typedef struct {
    int tanks;
} ForceWait;

static bool wave2_force(void *arg)
{
    const ForceWait *w = arg;
    ArmyForce t = army_target(nullptr);
    bool ready = army_holds(nullptr, &t, false) && home_guard("Battle tank", nullptr) >= w->tanks
                 && home_guard("Battle tank", "Laser cannon") >= 3 * w->tanks;
    return ready || pol_s8_active() || seen_or_logged("Morgrul invasion forces are defeated");
}

void act3_m08_wave2_force(void)
{
    // 24 Hunters (2L+1T), 16 Starfighters (4T) and 20 tanks (3L) by wave
    // 2, which leaves Jade 1,800-1,999 h after the fall (the fall + ~1,000
    // h from here); Starfighter 3,490 / 70 = 50 h of research.
    pol_research("Starfighter", 20 * POL_DAY);
    ForceWait w = {20};
    pol_poll_for("M8: the wave-2 force", wave2_force, &w, 1500, 12);
    play_log("M8: Home Guard %d H, %d SF, %d T at %s", home_guard("Hunter", nullptr),
             home_guard("Starfighter", nullptr), home_guard("Battle tank", nullptr), pol_date());
}

static bool wave2_beaten(void *unused)
{
    (void)unused;
    return seen_or_logged("Morgrul invasion forces are defeated");
}

void act3_m09_wave2(void)
{
    // MSG 16 after a space or ground win over wave 2 (main_program.c:1253,
    // 1461): story stage 3, the Tractor Beam idea (its icon tells a resumed
    // step).
    if (pol_research_state("Tractor Beam") != UI_RS_UNKNOWN)
        return;
    pol_wait_for("M9: wave 2 beaten", wave2_beaten, nullptr, 1500);
}

// --- M10: Dempsey's courses --------------------------------------------------------

// The developer's skills (Maths, Physics, Electronics, A.I.) under the
// research screen.
static void dev_skills(int skills[4])
{
    ui_goto(SCR_RESEARCH);
    if (!ui_developer_skills(skills))
        play_fail("no developer on the research screen");
}

// Spy satellite needs A4 (and Missile A3), Destroyer M6 P6 E5, Space station
// M7 E7. Dempsey (5/6/5/2 after J12) takes Artificial int. (A +2) and
// Electr and maths. (M +1, E +2) at story stage 2, whose caps are 6/7/7/5,
// then Maths & physics. (M +2, capped at 7) once stage 3 (wave 2 beaten)
// raises the caps to 7 (advisor.c, DS:57EB). Courses wait for the
// products whose research they would hold up (it stops while he is away).
static bool goal_m10_courses(void)
{
    if (pol_act() >= 4)
        return true;  // Klaatoo has replaced him (L2)
    ui_goto(SCR_RESEARCH);
    if (ui_research_icon("Starfighter") != UI_RS_DONE || ui_research_icon("Twin laser gun") != UI_RS_DONE)
        return false;
    int sk[4];
    dev_skills(sk);
    const char *course = nullptr;
    if (sk[3] < 4)
        course = "Artificial int.";
    else if (sk[0] < 6 || sk[2] < 7)
        course = "Electr and maths.";
    else if (sk[0] < 7 && pol_research_state("Tractor Beam") != UI_RS_UNKNOWN)
        course = "Maths & physics.";
    else
        return sk[0] >= 7;
    pol_university(PRI_M10, "Thomas Dempsey", course, INT32_MAX);
    return false;
}

void act3_goals_m08(void)
{
    pol_goal("M10_courses", PRI_M10, goal_m10_courses, 0);
}

static bool spy_skills(void *unused)
{
    (void)unused;
    int sk[4];
    dev_skills(sk);
    return sk[3] >= 4;
}

void act3_m10_course(void)
{
    // The goal M10_courses sends him; the step waits for A4 (Spy satellite,
    // M14).
    pol_poll_for("M10: Dempsey's A.I. course", spy_skills, nullptr, 40 * POL_DAY, 24);
}

// --- M11: the drifting ship ------------------------------------------------------

// S-K (§4): the star map zoomed on Phoenix 5 lists the Kalls' fleet 2
// there (allied forces are listed, planetinfo.c SetupSystemViewHotspots)
// once their alliance timer moved it (sim.c:1516-1521).
static bool kall_fleet_at_phoenix_5(void *unused)
{
    (void)unused;
    if (!planet_on_map("Phoenix", "Phoenix 5"))
        return false;
    ui_hotspot("Phoenix 5");  // zooms in on the planet and its moons
    ui_settle();
    play_wait(20);
    // The planets move on at each game hour, so the click can zoom in on
    // another one: on Phoenix 3, the Kalls' home, fleet 2 is listed before
    // it leaves (seen so in one run: talk 6 came too early, and the Kalls
    // turned on us, MSG 24). The zoom must list Phoenix 5 and not Phoenix 3.
    bool zoomed = ui_toolbar_has("ZOOM OUT") && ui_find_hotspot("Phoenix 5", 1) != 0
                  && ui_find_hotspot("Phoenix 3", 1) == 0;
    bool there = false;
    for (int j = 1; j <= 18 && zoomed; j++) {
        const char *c = ui_hotspot_caption(j);
        if (SDL_strcasestr(c, "Kall") && SDL_strstr(c, "Fleet 2"))
            there = true;
    }
    if (ui_toolbar_has("ZOOM OUT")) {
        ui_toolbar("ZOOM OUT");
        ui_settle();
    }
    return there;
}

static bool sos_seen(void *unused)
{
    (void)unused;
    return seen_or_logged("S.O.S. signal has been decoded");
}

static void wait_arrival(void *unused)
{
    (void)unused;
    if (!ui_wait_arrival("Galleon 1", TRIP_HOURS))
        play_fail("M11: Galleon 1 did not reach Phoenix 1");
}

static bool no_wave_due(void *unused)
{
    (void)unused;
    return !pol_s8_active();
}

void act3_m11_drifting_ship(void)
{
    // MSG 17 (Radio Done, 100-119 h after wave 2 left) arms the trigger: a
    // group arriving anywhere while Phoenix 1 is the selected planet
    // (sim.c:1046) finds the ship, MSG 18 and talk 6. "Tell me, what are
    // your plans?" sends the Kalls' fleet 2 to Mirach 3 as allies; asked
    // before their alliance timer has moved it to Phoenix 5 (1,500-1,999 h
    // after their contact) that timer would later start their betrayal
    // (main_program.c:1552-1556): so the fleet is seen at Phoenix 5 first.
    // "All right." adds 250 h to wave 3 if it has not left yet.
    if (pol_seen("talk 6") >= 0)
        return;
    pol_wait_for("M11: MSG 17, the Phoenix S.O.S.", sos_seen, nullptr, 1000);
    pol_poll_for("M11: the Kalls' fleet 2 at Phoenix 5", kall_fleet_at_phoenix_5, nullptr, 1500, 24);
    // R7: no first arrival while a wave is announced (its attack would
    // replace the talk).
    pol_wait_for("M11: no wave due", no_wave_due, nullptr, 150);
    // Launched to Phoenix 1, the group's screens keep Phoenix 1 selected
    // (its target) until it is there: no standing orders meanwhile (S1's
    // exception).
    for (int tries = 0; pol_seen("talk 6") < 0; tries++) {
        if (tries == 3)
            play_fail("M11: no drifting ship at Phoenix 1");
        galleon_to(nullptr, "Phoenix 1", nullptr);
        if (!pol_try(wait_arrival, nullptr))
            pol_service();
        pol_wait_for("M11: talk 6", event_seen, "talk 6", 10);
    }
    pol_new_earth(SCR_SURFACE);  // S1: Amnesty again
}

// --- M12, M13: wave 3, the Spy -----------------------------------------------------

static bool wave3_beaten(void *arg)
{
    const int64_t *since = arg;
    return seen_or_logged("second Morgrul invasion has failed")
           || pol_seen_since("Morgrul invasion forces are defeated", *since);
}

void act3_m12_wave3(void)
{
    // Wave 3 (20H 10SF, 15Tr 2T) leaves 1,100-1,149 h after wave 2 (+250 h
    // after talk 6). MSG 19 (space) or MSG 16 (ground): the Spy in the pub,
    // KIT 20 (Spy satellite) 30-39 h later (main_program.c:1260-1268).
    if (pol_research_state("Spy satellite") != UI_RS_UNKNOWN)
        return;
    int64_t since = pol_now();
    pol_wait_for("M12: wave 3 beaten", wave3_beaten, &since, 1700);
}

static bool mirach_listed(void)
{
    ui_open_screen("GALACTIC MAP", SCR_GALACTIC_MAP);
    if (ui_toolbar_has("ZOOM OUT")) {
        ui_toolbar("ZOOM OUT");
        ui_settle();
    }
    return ui_map_system_listed("Mirach");
}

static void ask_spy(void *unused)
{
    (void)unused;
    ui_command_centre();
    ui_pub_ask("Bartender", "Who is that suspicious character?", "How dangerous is he?",
               "I think I will speak to him.");
    // The bartender's answer unmasks him: the pub then captions him "Spy".
    ui_pub_ask(ui_pub_present("Spy") ? "Spy" : "Stranger", "Would you work for me?",
               "Will you do the job for 30000?", "Do you know anything important?");
    play_log("M13: the Spy said '%s'", ui_last_reply());
}

void act3_m13_spy(void)
{
    // The Spy takes 30,000 (unchecked, R6) and names Mirach: it joins the
    // star map's system list, no box (main_program.c:1024-1033).
    if (mirach_listed())
        return;
    pol_wait_money("M13: the Spy", PRI_M8, 30000, 10 * POL_DAY);
    pol_do(ask_spy, nullptr);
    if (!mirach_listed())
        play_fail("M13: Mirach not in the system list after the Spy");
}

// --- M14-M17: Spy satellite, Explorer 1, Destroyers -----------------------------------

static const int32_t SPY_SATELLITE_ORES[ORES] = {53, 255, 80, 335, 890, 900};
static const int32_t CARRIER_ORES[ORES] = {55, 105, 27, 18, 0, 0};

static bool ores_cover(const char *item, const int32_t per[ORES])
{
    int32_t have[ORES];
    return pol_ores(item, have) && pol_ores_cover(have, per);
}

// M14: a carrier group "Spy 1" with a Spy satellite, docked at New Earth
// until M17 flies it.
static bool goal_m14_spy_1(void)
{
    ui_ship_info();
    if (ui_group_find("Spy 1"))
        return true;
    if (pol_research_state("Spy satellite") != UI_RS_DONE)
        return false;
    int sats = pol_stores("Spy satellite"), carriers = pol_stores("Sat.carrier");
    if (sats == 0 && queued("Spy satellite") == 0 && ores_cover("Spy satellite", SPY_SATELLITE_ORES))
        pol_buy(PRI_M14, "Spy satellite", 1, 3546);
    if (carriers == 0 && queued("Sat.carrier") == 0 && ores_cover("Sat.carrier", CARRIER_ORES))
        pol_buy(PRI_M14, "Sat.carrier", 1, 1530);
    if (sats == 0 || carriers == 0)
        return false;
    if (!ui_new_group("Carrier", "Spy 1"))
        play_fail("M14: no Carrier group type");
    if (!ui_equip("Spy 1", "Sat.carrier", nullptr, 1) || !ui_equip("Spy 1", "Sat.carrier", "Spy satellite", 1))
        play_fail("M14: Spy 1 not equipped");
    return true;
}

void act3_goals_m14(void)
{
    pol_goal("M14_spy_1", PRI_M14, goal_m14_spy_1, 0);
}

void act3_m14_spy_satellite(void)
{
    // KIT 20 30-39 h after wave 3; 3,900 / 70 = 56 h (needs A4: M10).
    pol_research("Spy satellite", 30 * POL_DAY);
}

void act3_m15_explorer(void)
{
    // Mirach's planets show once a group reaches its sun, Mirach 2's moons
    // once one reaches Mirach 2; docking at its first moon finds Explorer 1
    // (shipmove.c:158-163): MSG 21, scene 4 with MSG 36, Missile and
    // Destroyer to analyse.
    if (pol_research_state("Destroyer") != UI_RS_UNKNOWN)
        return;
    if (!planet_on_map("Mirach", "Mirach 2")) {
        galleon_to("Mirach", nullptr, nullptr);
        pol_wait_for("M15: Mirach explored", event_seen, "explored Mirach system", 10 * POL_DAY);
    }
    galleon_to(nullptr, "Mirach 2", nullptr);
    pol_wait_for("M15: Mirach 2's moons", event_seen, "explored Mirach 2's moons", 5 * POL_DAY);
    galleon_to(nullptr, "Mirach 2", MIRACH_2_MOON_1);
    ui_ship_info();
    if (!ui_wait_arrival("Galleon 1", TRIP_HOURS) || !ui_dock("Galleon 1"))
        play_fail("M15: Galleon 1 not docked at %s", MIRACH_2_MOON_1);
    pol_wait_for("M15: Explorer 1", event_seen, "found Explorer 1", 5 * POL_DAY);
}

// M16: one Space station (a Destroyer's queue is capped at the stations in
// stock, shop.c:280-286, but units build one at a time anyway); the army
// goal then keeps a Destroyer queued.
static const int32_t STATION_ORES[ORES] = {60000, 48000, 6590, 2585, 2000, 3270};

static bool goal_m16_station(void)
{
    if (pol_research_state("Space station") != UI_RS_DONE)
        return false;
    if (pol_stores("Space station") > 0)
        return true;
    if (queued("Space station") == 0 && ores_cover("Space station", STATION_ORES))
        pol_buy(PRI_M16, "Space station", 1, 28500);
    return false;
}

void act3_goals_m16(void)
{
    pol_goal("M16_station", PRI_M16, goal_m16_station, 0);
}

static bool destroyers_ready(void *unused)
{
    (void)unused;
    return home_guard("Destroyer", nullptr) >= 5;
}

void act3_m16_destroyers(void)
{
    // Missile 2,600 / 70 = 37 h; Destroyer 9,500 / 70 = 136 h (M6, after
    // Electr and maths.); KIT 24 50-119 h later; Space station 7,100 / 70 =
    // 101 h (M7: Maths & physics. at stage 3); then 5 Destroyers, 67 h each.
    pol_research("Missile", 30 * POL_DAY);
    pol_research("Destroyer", 30 * POL_DAY);
    pol_research("Space station", 40 * POL_DAY);
    pol_poll_for("M16: 5 Destroyers", destroyers_ready, nullptr, 60 * POL_DAY, 24);
}

static void spy_to_mirach_3(void *unused)
{
    (void)unused;
    if (ui_group_status("Spy 1") == SHIP_DOCKED && !ui_take_off("Spy 1"))
        play_fail("M17: Spy 1 did not take off");
    if (!pol_ends_with(pol_where("Spy 1"), "Mirach 3") && ui_group_status("Spy 1") != SHIP_FLYING
        && !ui_launch("Spy 1", "Mirach", "Mirach 3", nullptr))
        play_fail("M17: Spy 1 not launched");
    if (!ui_wait_arrival("Spy 1", TRIP_HOURS))
        play_fail("M17: Spy 1 did not reach Mirach 3");
    ui_open_group("Spy 1", 17);
    if (!ui_body_action("ADD SPY SAT"))
        play_fail("M17: no ADD SPY SAT at Mirach 3");
}

static bool spy_1_ready(void *unused)
{
    (void)unused;
    return goal_m14_spy_1();
}

void act3_m17_spy_on_mirach(void)
{
    // The spy satellite's intel on Mirach 3 reaches 40 in days: " Your
    // satellite found The Morgruls colony at | Mirach 3" (sim.c:682-689)
    // opens GROUND WAR there.
    if (pol_seen("colony at") < 0) {
        pol_poll_for("M17: Spy 1", spy_1_ready, nullptr, 20 * POL_DAY, 24);
        pol_do(spy_to_mirach_3, nullptr);
    }
    pol_wait_for("M17: Mirach 3's intel", event_seen, "Morgruls colony at", 20 * POL_DAY);
}

// --- M18: Mirach 3 -----------------------------------------------------------------

static bool strike_ready(void *unused)
{
    (void)unused;
    ArmyForce t = army_target(nullptr);
    return army_holds(nullptr, &t, false) && home_guard("Battle tank", nullptr) >= t.n[ARMY_TANK]
           && home_guard("Battle tank", "Laser cannon") >= 3 * t.n[ARMY_TANK];
}

static void invade(void *unused)
{
    (void)unused;
    if (pol_seen("eliminated 'The Morgruls'") >= 0)
        return;  // pol_do runs it again after the battles
    ui_ship_info();
    if (ui_group_status("Strike") == SHIP_DOCKED && !ui_take_off("Strike"))
        play_fail("M18: Strike did not take off");
    if (!pol_ends_with(pol_where("Strike"), "Mirach 3")) {
        if (ui_group_status("Strike") != SHIP_FLYING && !ui_launch("Strike", "Mirach", "Mirach 3", nullptr))
            play_fail("M18: Strike not launched");
        if (!ui_wait_arrival("Strike", TRIP_HOURS))
            play_fail("M18: Strike did not reach Mirach 3");
    }
    if (!ui_invade("Mirach", "Mirach 3", "Strike"))
        play_fail("M18: no GROUND WAR at Mirach 3");
}

void act3_m18_mirach(void)
{
    // Strike: Home Guard renamed, with the army_target force; the Kalls'
    // fleet 2 (15H 10SF, 10Tr 35T) fights on our side there after talk 6.
    // Waves 1-3 are spent and wave 4 leaves 8,000-8,499 h after wave 3
    // (sim.c:1504-1510), so New Earth keeps no army meanwhile.
    pol_poll_for("M18: the strike force", strike_ready, nullptr, 200 * POL_DAY, 24);
    ui_ship_info();
    if (!ui_group_find("Strike") && !ui_rename_group("Home Guard", "Strike"))
        play_fail("M18: Home Guard not renamed");
    for (int tries = 0; pol_seen("eliminated 'The Morgruls'") < 0; tries++) {
        if (tries == 3)
            play_fail("M18: Mirach 3 not taken");
        pol_do(invade, nullptr);
        pol_service();
        pol_wait_for("M18: the Morgruls eliminated", event_seen, "eliminated", 24);
    }
    play_log("M18: Mirach 3 taken at %s", pol_date());
}
