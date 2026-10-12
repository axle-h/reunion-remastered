#include "play/act2.h"

#include <SDL3/SDL.h>

#include "play/act1.h"
#include "play/play.h"
#include "play/policy.h"
#include "play/ui.h"
#include "play/ui_message.h"
#include "play/ui_office.h"
#include "play/ui_planet.h"
#include "play/ui_ships.h"

enum : int {
    SCR_RESEARCH = 3,
    SCR_PLANET_INFO = 8,
    SCR_SURFACE = 20,
    // Prices (the shop's "Price", the hire screen, the colonisation screen).
    KYLE_PRICE = 45000,
    HUNTER_PRICE = 5700,
    LASER_PRICE = 1550,
    GALLEON_PRICE = 47000,
    TRANSFER_SHIP_PRICE = 3800,
    TRADE_SHIP_PRICE = 4590,
    DROID_PRICE = 3150,
    SATELLITE_PRICE = 780,
    CARRIER_PRICE = 1530,
    RUSSEL_COST = 167000,  // Nuclear Gen., Builder Plant, Mine, Building and the colony
    // J8 (docs/playthrough.md, "Force sizes": 12 Hunters won wave 1 199/200,
    // 14 200/200, 8 only 7/10; a lost wave 1 ends the game).
    ARMY_HUNTERS = 14,
    ARMY_LASERS = 28,
    // Trade 1's ships: 500 t and 1 droid per Transfer ship, 2,000 t and 4
    // droids per Trade ship (walkthrough "Groups and travel").
    HAUL_TRANSFER_SHIPS = 3,
    HAUL_TRADE_SHIPS = 4,
    // Act 3's army takes Energon faster than 9,500 t a trip brings it (New
    // Earth's stock fell to 7,000 t by 2929-02 in one run).
    HAUL_TRADE_SHIPS_ACT3 = 10,
    HAUL_EVERY = 3 * POL_DAY,  // a colony's ore is fetched at most this often
    TRIP_HOURS = 24,           // a flight inside Amnesty: <= 199 legs at pilot experience + 1 an hour
};

// Ores per piece ("Ore needed: (One piece)" in the shop's info panel).
static const int32_t HUNTER_ORES[ORES] = {450, 1875, 406, 454, 78, 0};
static const int32_t LASER_ORES[ORES] = {50, 35, 16, 15, 78, 0};
static const int32_t GALLEON_ORES[ORES] = {8750, 12670, 3226, 11000, 1778, 1936};
static const int32_t TRANSFER_SHIP_ORES[ORES] = {223, 521, 451, 56, 0, 0};
static const int32_t TRADE_SHIP_ORES[ORES] = {300, 1625, 366, 355, 0, 0};
static const int32_t DROID_ORES[ORES] = {10, 10, 5, 3, 0, 0};
static const int32_t SATELLITE_ORES[ORES] = {37, 31, 12, 5, 0, 0};
static const int32_t CARRIER_ORES[ORES] = {55, 105, 27, 18, 0, 0};

// How many of a product New Earth's ore stock allows (at most n).
static int ores_allow(const char *item, const int32_t per[ORES], int n)
{
    int32_t have[ORES];
    if (!pol_ores(item, have))
        return 0;
    for (int k = 0; k < ORES; k++)
        if (per[k] > 0)
            n = SDL_min(n, (int)(have[k] / per[k]));
    return SDL_max(n, 0);
}

// The shop's queue ("Bought items:") of `item`, 0 if none.
static int queued(const char *item)
{
    UiShopQueue q;
    if (!ui_shop_select(item) || !ui_shop_queue(&q) || !q.shown)
        return 0;
    return q.queued;
}

// Buys up to n of `item` as ores (pol_ores) and money (pol_buy) allow.
static int buy_upto(PolPriority pri, const char *item, int n, int32_t price, const int32_t per[ORES])
{
    int k = ores_allow(item, per, n);
    if (k < n)
        play_log("%s: the ores allow %d of %d", item, k, n);
    return k > 0 ? pol_buy(pri, item, k, price) : 0;
}

// A visible event: a box this run has seen, or a line of the message log
// (screen 11; for a step resumed after the box).
static bool seen_or_logged(const char *text)
{
    return pol_seen(text) >= 0 || ui_log_has(text);
}

static bool fall_seen(void)
{
    // MSG 11's first line (a log row holds 50 characters).
    return seen_or_logged("attacked and destroyed the");
}

// --- Trade 1 ----------------------------------------------------------------------

// Trade 1 hauls (S6, S7) unless a step flies it (J1, J10).
static struct {
    uint32_t run;
    bool hold;
    int64_t visited[4];   // last fetch from each colony (pol_now), this run
    bool room_known;
    int32_t room[ORES];   // New Earth's storage left per ore (HAUL_ORES order)
    int order[ORES];      // HAUL_ORES by New Earth's stock, lowest first
} haul;

static void haul_check_run(void)
{
    if (haul.run != play_run_id())
        haul = (typeof(haul)){.run = play_run_id()};
}

static void hold_trade_1(bool hold)
{
    haul_check_run();
    haul.hold = hold;
}

// Trade 1 in orbit wherever it is: waits for an arrival, takes off.
static void trade_1_in_orbit(void)
{
    if (ui_group_status("Trade 1") == SHIP_FLYING && !ui_wait_arrival("Trade 1", TRIP_HOURS))
        play_fail("Trade 1 did not arrive in %d h", (int)TRIP_HOURS);
    if (!ui_take_off("Trade 1"))
        play_fail("Trade 1 did not take off");
}

// Flies Trade 1 to `planet` (`moon`) of Amnesty and waits there.
static void trade_1_to(const char *planet, const char *moon)
{
    char at[64];
    SDL_snprintf(at, sizeof at, "%s%s%s", planet, moon ? " " : "", moon ? moon : "");
    if (pol_ends_with(pol_where("Trade 1"), at) && ui_group_status("Trade 1") != SHIP_FLYING)
        return;
    trade_1_in_orbit();
    if (!ui_launch("Trade 1", nullptr, planet, moon))
        play_fail("Trade 1 not launched to %s", at);
    pol_between_point();  // a round trip takes most of a day
    if (!ui_wait_arrival("Trade 1", TRIP_HOURS))
        play_fail("Trade 1 did not reach %s", at);
}

// --- S6, S7: the colonies' droids and ore -----------------------------------------

typedef struct {
    const char *planet, *moon;
    const char *founding;  // the act-1 goal that founds it (done: founded or under way)
} Place;

static const Place COLONIES[] = {
    {"New Earth", "Apollo", "E18_apollo"},
    {"Zeus", "Penelope", "E20_penelope"},
    {"Amnesty 2", "Mir", "E19_mir"},
    {"Jade", "Russel", "J11_russel"},
};
static_assert(SDL_arraysize(COLONIES) == SDL_arraysize(haul.visited));

// The ores, in the order loaded when New Earth's stock is not known:
// Raenium and Texon first (New Earth has none: J8, J13), then what New
// Earth mines least.
static const char *const HAUL_ORES[ORES] = {"Raenium", "Texon", "Energon", "Detoxin", "Lepitium",
                                             "Kremir"};

typedef struct {
    bool mining;  // founded, with a finished Space Port (droids are items, cargo.c:113)
    int droids;   // working
    int need;     // finished mines without a droid, less the droids in its stock
} ColonyMines;

static ColonyMines colony_mines(const Place *p)
{
    ColonyMines m = {};
    if (!pol_goal_done(p->founding))
        return m;
    ui_open_body(p->planet, p->moon, SCR_PLANET_INFO);
    if (!ui_body_type_says("olony") || ui_body_type_says("under"))
        return m;
    ui_open_body(p->planet, p->moon, SCR_SURFACE);
    m.mining = ui_buildings("Space Port").finished > 0;
    m.droids = ui_droids();
    if (m.mining)
        m.need = SDL_max(SDL_min(ui_buildings("Mine").finished, UI_MAX_DROIDS) - m.droids - ui_droid_stock(), 0);
    return m;
}

// The colony shown puts the droids in its stock to work.
static void add_droids(const Place *p)
{
    ui_open_body(p->planet, p->moon, SCR_SURFACE);
    while (ui_droids() < SDL_min(ui_droid_slots(), UI_MAX_DROIDS) && ui_droid_stock() > 0)
        if (!ui_add_droid())
            break;
}

// Unloads all ore at the dock Trade 1 is in (as much as the planet's
// storage takes, cargo.c:101-145).
static void unload(void)
{
    ui_cargo("Trade 1", HAUL_ORES[0], 0);  // opens the cargo screen
    for (int k = 0; k < ORES; k++)
        if (ui_cargo_ship(HAUL_ORES[k]) > 0)
            ui_cargo("Trade 1", HAUL_ORES[k], -SHIP_CARGO_ALL);
}

// Docked at New Earth, its cargo screen shown: what the next load should
// be, from New Earth's column ("Storage space on planet" less the stock):
// the ores New Earth has least of first, none it has no room for.
static void plan_load(void)
{
    int32_t used, cap, planet_cap, stock[ORES];
    ui_cargo_space(&used, &cap, &planet_cap);
    for (int k = 0; k < ORES; k++) {
        stock[k] = ui_cargo_planet(HAUL_ORES[k]);
        haul.room[k] = planet_cap - stock[k];
        haul.order[k] = k;
    }
    for (int i = 1; i < ORES; i++)
        for (int j = i; j > 0 && stock[haul.order[j]] < stock[haul.order[j - 1]]; j--) {
            int t = haul.order[j];
            haul.order[j] = haul.order[j - 1];
            haul.order[j - 1] = t;
        }
    haul.room_known = true;
    play_log("S6: New Earth stock D%d E%d K%d L%d R%d T%d of %d", stock[3], stock[2], stock[5],
             stock[4], stock[0], stock[1], planet_cap);
}

// Docked at a colony: ore New Earth could not take goes back, then the
// scarcest ores aboard, each up to New Earth's room for it (a right press
// takes all there is, a left one 100 t, cargo.c:135-160).
static void load_ores(void)
{
    unload();
    for (int i = 0; i < ORES; i++) {
        int k = haul.room_known ? haul.order[i] : i;
        int32_t used, cap, planet_cap, room = haul.room_known ? haul.room[k] : SHIP_CARGO_ALL;
        ui_cargo_space(&used, &cap, &planet_cap);
        int32_t there = ui_cargo_planet(HAUL_ORES[k]);
        if (used >= cap || there <= 0 || room < 1000)
            continue;
        if (there <= room)
            ui_cargo("Trade 1", HAUL_ORES[k], SHIP_CARGO_ALL);
        else
            ui_cargo("Trade 1", HAUL_ORES[k], SDL_min(room, 3000) / 100 * 100);
    }
}

// Trade 1's ships (screen 22 grid) and what they carry.
static void trade_1_fleet(int *transfer, int *trade, int32_t *tonnes, int *droids)
{
    *transfer = ui_group_units("Trade 1", "Transfer ship", nullptr);
    *trade = ui_group_units("Trade 1", "Trade ship", nullptr);
    *tonnes = 500 * *transfer + 2000 * *trade;
    *droids = *transfer + 4 * *trade;
}

// Docked at New Earth: more ships for Trade 1 (Transfer ships, then Trade
// ships once the Jaanosians gave them), bought and put in.
static void grow_fleet(void)
{
    int transfer, trade, droids;
    int32_t tonnes;
    trade_1_fleet(&transfer, &trade, &tonnes, &droids);
    bool trade_ships = ui_shop_select("Trade ship");
    const char *item = trade_ships ? "Trade ship" : "Transfer ship";
    int have = trade_ships ? trade : transfer;
    int want = !trade_ships ? HAUL_TRANSFER_SHIPS : pol_act() >= 3 ? HAUL_TRADE_SHIPS_ACT3 : HAUL_TRADE_SHIPS;
    if (have >= want)
        return;
    int stores = pol_stores(item);
    if (stores > 0) {
        ui_equip("Trade 1", item, nullptr, have + SDL_min(stores, want - have));
        return;
    }
    if (queued(item) == 0)
        buy_upto(PRI_HAUL, item, want - have, trade_ships ? TRADE_SHIP_PRICE : TRANSFER_SHIP_PRICE,
                 trade_ships ? TRADE_SHIP_ORES : TRANSFER_SHIP_ORES);
}

// At New Earth (droids < mines there go to New Earth first, E21): buys the
// droids the colonies wait for and loads up to `max` of those in the shop's
// stores. Returns how many it loaded.
static int load_droids(int need, int max)
{
    pol_new_earth(SCR_SURFACE);
    bool home_short = ui_droids() < SDL_min(ui_buildings("Mine").finished, UI_MAX_DROIDS);
    int stores = pol_stores("Miner droid");
    if (stores + queued("Miner droid") < need)
        buy_upto(PRI_HAUL, "Miner droid", need - stores - queued("Miner droid"), DROID_PRICE,
                 DROID_ORES);
    if (home_short || stores == 0)
        return 0;
    return (int)ui_cargo("Trade 1", "Miner droid", SDL_min(SDL_min(stores, need), max));
}

// S6, S7: one round trip of Trade 1 a pass: from New Earth, with the
// droids a colony's mines lack, or empty to the colony whose ore was
// fetched longest ago; there droids to work and ore aboard; home and
// unload. Every colony's ore at most every HAUL_EVERY hours. During a wave
// (S8) nothing goes to New Earth or Apollo.
static bool goal_haul(void)
{
    haul_check_run();
    if (haul.hold)
        return false;
    ui_ship_info();
    if (!ui_group_find("Trade 1"))
        return false;
    if (ui_group_status("Trade 1") == SHIP_FLYING && !ui_wait_arrival("Trade 1", TRIP_HOURS))
        return false;
    const char *loc = pol_where("Trade 1");
    // A colony (unloaded, or where S8 sent it): its business there first.
    int at = -1;
    for (int i = 0; i < (int)SDL_arraysize(COLONIES); i++) {
        char name[64];
        SDL_snprintf(name, sizeof name, "%s %s", COLONIES[i].planet, COLONIES[i].moon);
        if (pol_ends_with(loc, name))
            at = i;
    }
    bool home = at < 0 && pol_ends_with(loc, "New Earth");
    if (at < 0 && !home) {  // e.g. Jade after the contact
        if (pol_s8_active())
            return false;
        trade_1_to("New Earth", nullptr);
        home = true;
    }
    int target = at;
    if (home) {
        if (!ui_dock("Trade 1"))
            return false;
        unload();
        plan_load();
        grow_fleet();
        int transfer, trade, droid_room;
        int32_t tonnes;
        trade_1_fleet(&transfer, &trade, &tonnes, &droid_room);
        if (pol_s8_active())
            return false;
        // Droids first, else the colony visited longest ago.
        ColonyMines m[SDL_arraysize(COLONIES)];
        int need = 0;
        for (int i = 0; i < (int)SDL_arraysize(COLONIES); i++) {
            m[i] = colony_mines(&COLONIES[i]);
            need += m[i].need;
            if (m[i].need > 0 && (target < 0 || m[target].need == 0))
                target = i;
        }
        int loaded = need > 0 ? load_droids(need, droid_room) : 0;
        if (loaded == 0) {
            target = -1;
            for (int i = 0; i < (int)SDL_arraysize(COLONIES); i++)
                if (m[i].droids > 0 && pol_now() >= haul.visited[i] + HAUL_EVERY
                    && (target < 0 || haul.visited[i] < haul.visited[target]))
                    target = i;
        }
        if (target < 0)
            return false;
        play_log("S6/S7: Trade 1 to %s with %d droids", COLONIES[target].moon, loaded);
        trade_1_to(COLONIES[target].planet, COLONIES[target].moon);
    }
    // At the colony: droids off and to work, ore aboard.
    const Place *p = &COLONIES[target];
    if (!ui_dock("Trade 1"))
        return false;
    // Droids off; to work with any left in the colony's stock from before.
    ui_cargo("Trade 1", HAUL_ORES[0], 0);  // opens the cargo screen
    int droids = ui_cargo_ship("Miner droid");
    if (droids > 0)
        ui_cargo("Trade 1", "Miner droid", -droids);
    add_droids(p);
    load_ores();
    haul.visited[target] = pol_now();
    if (pol_s8_active())
        return false;
    trade_1_to("New Earth", nullptr);
    if (ui_dock("Trade 1"))
        unload();
    pol_new_earth(SCR_SURFACE);  // S1
    return false;  // a standing order
}

void act2_goals_j01(void)
{
    pol_goal("S6_S7_haul", PRI_HAUL, goal_haul, 0);
}

// --- J1-J6: the Jaanosians --------------------------------------------------------

static bool date_reached(void *arg)
{
    return pol_now() >= *(const int64_t *)arg;
}

static bool event_seen(void *text)
{
    return pol_seen(text) >= 0;
}

static void fly_to_jade(void *unused)
{
    (void)unused;
    trade_1_in_orbit();
    if (!ui_launch("Trade 1", nullptr, "Jade", nullptr))
        play_fail("J1: Trade 1 not launched to Jade");
}

void act2_j01_contact(void)
{
    // The meetings, the S.O.S. and the Morgrul waves are fixed offsets from
    // the contact (intel.c Intel_FirstContact; the S.O.S. 2,590-2,807 h
    // after it), and the Jaanosians' deadline (MSG 6) is 6,000-6,399 h after
    // the start, 2928-04-23 at the earliest (main_program.c:240): the
    // contact waits for the economy until 2928-03-01, the player's choice by
    // the date on the status bar (not before ~02-15).
    // Until then nothing is paid unchecked (talk 3 is the first): act
    // 1's reserve.
    UiDate when = {2928, 3, 1, 0};
    int64_t at = ui_date_hours(&when);
    if (pol_now() < at) {
        pol_set_reserve(20000);
        pol_wait_for("J1: the contact date", date_reached, &at, (int)(at - pol_now()) + 2);
        pol_set_reserve(50000);
    }
    hold_trade_1(true);
    pol_do(fly_to_jade, nullptr);
    pol_wait_for("J1: contact with the Jaanosians", event_seen, "'Jaanosians'", 7 * POL_DAY);
    hold_trade_1(false);
}

void act2_j02_communicator(void)
{
    // KIT 8 20-39 h after the contact; 1,100 / 70 = 16 h.
    pol_research("Communicator", 300);
}

void act2_j03_developer_home(void)
{
    // Meeting 1 counts down only while the developer is home (sim.c:1425):
    // no course for Dempsey until then (the only one left, J12's, waits for
    // the base's fall), and the reserve covers meeting 2's price.
    ui_goto(SCR_RESEARCH);
    int skills[4];
    if (!ui_developer_skills(skills))
        play_fail("J3: no developer on the research screen");
    play_log("J3: Dempsey %d/%d/%d/%d, reserve %d", skills[0], skills[1], skills[2], skills[3],
             pol_reserve());
}

void act2_j04_meeting1(void)
{
    // Talk 2 650-719 developer hours after the contact (pol_service plays
    // it: "O.K."), MSG 8 60-89 h later: the Trade ship.
    pol_wait_for("J4: meeting 1", event_seen, "talk 2", 900);
    pol_wait_for("J4: MSG 8, the scientist back", event_seen, "Your scientist has returned", 120);
}

void act2_j05_meeting2(void)
{
    // MSG 9 and talk 3 720-769 h after meeting 1: "Yes." pays 16,000.
    pol_wait_for("J5: meeting 2", event_seen, "talk 3", 1000);
}

// J6 waits for the S.O.S. and, from 700 h on (the contact + ~2,100 h, ~500 h
// before the earliest S.O.S.), saves for J7 and J8.
typedef struct {
    int64_t save_from;
} SosWait;

static bool sos_seen(void *arg)
{
    const SosWait *w = arg;
    if (pol_now() >= w->save_from)
        pol_step_claim(PRI_J8, KYLE_PRICE + ARMY_HUNTERS * HUNTER_PRICE + ARMY_LASERS * LASER_PRICE);
    return pol_seen("talk 4") >= 0;
}

void act2_j06_sos(void)
{
    // MSG 10 and talk 4 1,220-1,319 h after meeting 2: Hunter, Laser cannon
    // and Army groups.
    SosWait w = {pol_now() + 700};
    pol_wait_for("J6: the S.O.S.", sos_seen, &w, 1500);
    pol_step_claim(PRI_J8, 0);
    if (pol_research_state("Hunter") != UI_RS_DONE || pol_research_state("Laser cannon") != UI_RS_DONE)
        play_fail("J6: Hunter and Laser cannon not granted");
}

// --- J7, J8: the commander and the army for wave 1 ------------------------------

static bool goal_s12_kyle(void)
{
    return pol_university(PRI_S12, "Kyle Douglas", nullptr, 60000);
}

void act2_goals_j08(void)
{
    pol_goal("S12_kyle_university", PRI_S12, goal_s12_kyle, 0);
}

void act2_j07_commander(void)
{
    pol_hire("FIGHTERS", "Kyle Douglas", PRI_J7, KYLE_PRICE);
}

// J8: 14 Hunters with 2 Laser cannons each in "Home Guard", docked at New
// Earth: bought as money and ores allow (Raenium comes from the colonies),
// put in as they are built.
static bool army_ready(void *unused)
{
    (void)unused;
    ui_ship_info();
    bool group = ui_group_find("Home Guard") != 0;
    int hunters = group ? ui_group_units("Home Guard", "Hunter", nullptr) : 0;
    int lasers = group ? ui_group_units("Home Guard", "Hunter", "Laser cannon") : 0;
    if (hunters >= ARMY_HUNTERS && lasers >= ARMY_LASERS)
        return true;
    int sh = pol_stores("Hunter"), qh = queued("Hunter");
    int sl = pol_stores("Laser cannon"), ql = queued("Laser cannon");
    play_log("J8: Home Guard %d H %d L, stores %d H %d L, queued %d H %d L", hunters, lasers, sh, sl,
             qh, ql);
    int need_h = ARMY_HUNTERS - hunters - sh - qh, need_l = ARMY_LASERS - lasers - sl - ql;
    // Hunters and their lasers together: three Raenium-costing pieces each.
    if (need_h > 0)
        buy_upto(PRI_J8, "Hunter", need_h, HUNTER_PRICE, HUNTER_ORES);
    if (need_l > 0)
        buy_upto(PRI_J8, "Laser cannon", need_l, LASER_PRICE, LASER_ORES);
    if (sh == 0 && sl == 0)
        return false;
    if (!group && !ui_new_group("Army", "Home Guard"))
        play_fail("J8: no Army group type");
    if (sh > 0)
        ui_equip("Home Guard", "Hunter", nullptr, hunters + sh);
    hunters = ui_group_units("Home Guard", "Hunter", nullptr);
    if (sl > 0)
        ui_equip("Home Guard", "Hunter", "Laser cannon", SDL_min(lasers + sl, 2 * hunters));
    return false;
}

void act2_j08_army(void)
{
    // Wave 1 attacks 800 h or more after the S.O.S.; 280 h of Hunter line.
    pol_poll_for("J8: 14 Hunters with 28 lasers", army_ready, nullptr, 700, 6);
    play_log("J8: Home Guard ready at %s", pol_date());
}

// --- J9-J13: the fall, Radio, Russel, Hyperspace drive, Galleon ----------------

static bool fall_pred(void *unused)
{
    (void)unused;
    return fall_seen();
}

void act2_j09_base_falls(void)
{
    // MSG 11 and scene 1 200-249 h after the S.O.S.
    pol_wait_for("J9: the base falls", fall_pred, nullptr, 300);
}

// J12's course: Dempsey takes Phisics.& electr. once the fall raised the
// caps (story stage 2): Hyperspace drive needs P5, Galleon P5 E4. The
// skills under the research screen tell when he has been.
static bool goal_j12_course(void)
{
    if (!fall_seen())
        return false;
    ui_goto(SCR_RESEARCH);
    int skills[4];
    if (ui_developer_skills(skills) && skills[1] >= 5 && skills[2] >= 4)
        return true;
    return pol_university(PRI_J12, "Thomas Dempsey", "Phisics.& electr.", INT32_MAX);
}

void act2_goals_j09(void)
{
    pol_goal("J12_course", PRI_J12, goal_j12_course, 0);
}

static void radio_at_jade(void *unused)
{
    (void)unused;
    trade_1_to("Jade", nullptr);
    if (!ui_dock("Trade 1"))
        play_fail("J10: Trade 1 can't dock at Jade");
}

void act2_j10_radio(void)
{
    // Docking at Jade after the fall finds the radio (MSG 13,
    // shipmove.c:153-156); Trade 1 goes back to hauling from there.
    if (pol_research_state("Radio") == UI_RS_UNKNOWN) {
        hold_trade_1(true);
        pol_do(radio_at_jade, nullptr);
        pol_wait_for("J10: MSG 13, the radio", event_seen, "Sub-Space radio", 2);
        hold_trade_1(false);
    }
    pol_research("Radio", 10 * POL_DAY);
}

// J11: a satellite on Russel (Jade's moon, owner 0 after the fall) until
// the COLONIZATION icon appears, then the colony. The satellite flies on a
// carrier of its own, "Survey 2" (Survey 1's grid is E16's progress).
static const char *const RUSSEL_STARTERS[] = {"Nuclear Gen.", "Builder Plant", "Mine", "Building"};

static bool goal_j11(void)
{
    if (!fall_seen())
        return false;
    ui_open_body("Jade", "Russel", SCR_PLANET_INFO);
    if (ui_body_type_says("olony"))
        return true;
    if (ui_toolbar_has("COLONIZATION")) {
        if (!pol_afford(PRI_J11, RUSSEL_COST) || !ui_found_colony(RUSSEL_STARTERS, 4))
            return false;
        play_log("J11: colony founded on Russel at %s", pol_date());
        return true;
    }
    if (ui_body_lists("Satellite"))
        return false;  // its intel grows to 30
    ui_ship_info();
    if (!ui_group_find("Survey 2")) {
        int carriers = pol_stores("Sat.carrier"), sats = pol_stores("Satellite");
        if (carriers == 0 && queued("Sat.carrier") == 0)
            buy_upto(PRI_J11, "Sat.carrier", 1, CARRIER_PRICE, CARRIER_ORES);
        if (sats == 0 && queued("Satellite") == 0)
            buy_upto(PRI_J11, "Satellite", 1, SATELLITE_PRICE, SATELLITE_ORES);
        if (carriers == 0 || sats == 0)
            return false;
        if (!ui_new_group("Carrier", "Survey 2"))
            play_fail("J11: no Carrier group type");
        if (!ui_equip("Survey 2", "Sat.carrier", nullptr, 1)
            || !ui_equip("Survey 2", "Sat.carrier", "Satellite", 1))
            play_fail("J11: Survey 2 not equipped");
    }
    int status = ui_group_status("Survey 2");
    if (status == SHIP_FLYING || ui_group_units("Survey 2", "Sat.carrier", "Satellite") == 0)
        return false;
    if (pol_ends_with(pol_where("Survey 2"), "Jade Russel"))
        ui_deploy_carrier_satellite("Survey 2");
    else if (!pol_s8_active() && ui_take_off("Survey 2"))
        ui_launch("Survey 2", nullptr, "Jade", "Russel");
    return false;
}

void act2_goals_j11(void)
{
    pol_goal("J11_russel", PRI_J11, goal_j11, 0);
    act1_goals_russel();
}

void act2_j11_russel(void)
{
    pol_orders();
}

void act2_j12_hyperspace(void)
{
    // MSG 30 300-349 developer hours after the fall; 6,500 / 70 = 93 h.
    pol_research("Hyperspace drv", 900);
}

// J13: the Galleon bought (ores: Raenium and Texon from the colonies) and
// "Galleon 1" made of it at New Earth.
static bool galleon_ready(void *unused)
{
    (void)unused;
    ui_ship_info();
    if (ui_group_find("Galleon 1"))
        return ui_group_units("Galleon 1", "Galleon", nullptr) > 0;
    if (pol_stores("Galleon") == 0) {
        if (queued("Galleon") == 0)
            buy_upto(PRI_J13, "Galleon", 1, GALLEON_PRICE, GALLEON_ORES);
        return false;
    }
    if (!ui_new_group("Trade", "Galleon 1"))
        play_fail("J13: no Trade group type");
    return ui_equip("Galleon 1", "Galleon", nullptr, 1);
}

void act2_j13_galleon(void)
{
    // KIT 15 50-119 developer hours after J12; 4,600 / 70 = 66 h; 60 h to
    // build.
    pol_research("Galleon", 30 * POL_DAY);
    pol_poll_for("J13: Galleon 1", galleon_ready, nullptr, 30 * POL_DAY, 6);
}
