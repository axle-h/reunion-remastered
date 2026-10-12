#include "play/act1.h"

#include <SDL3/SDL.h>

#include "play/play.h"
#include "play/policy.h"
#include "play/ui.h"
#include "play/ui_message.h"
#include "play/ui_office.h"
#include "play/ui_planet.h"
#include "play/ui_ships.h"
#include "play/ui_talk.h"

enum : int {
    SCR_COMMAND = 1,
    SCR_PLANET_INFO = 8,
    SCR_SURFACE = 20,
    // Prices ("Cost" in the building info, the shop's "Price", the hire
    // screen).
    MINE_COST = 6000,
    DERRICK_COST = 7000,
    SATELLITE_PRICE = 780,
};

// Ores per piece ("Ore needed: (One piece)" in the shop's info panel).
static const int32_t SATELLITE_ORES[ORES] = {37, 31, 12, 5, 0, 0};

static void scaled(int32_t out[ORES], const int32_t per[ORES], int n)
{
    for (int k = 0; k < ORES; k++)
        out[k] = per[k] * n;
}

// --- Background goals --------------------------------------------------------------

// E5: a third Mine on New Earth, and the droid in stock in it.
static bool goal_e05(void)
{
    pol_new_earth(SCR_SURFACE);
    UiBuildings m = ui_buildings("Mine");
    if (m.total < 3) {
        pol_build(PRI_E5, "Mine", MINE_COST);
        return false;
    }
    if (ui_droids() < m.finished && ui_droid_stock() > 0)
        ui_add_droid();
    return ui_droids() >= 3;
}

// E6: four more Derricks (New Earth starts with one), as money allows.
static bool goal_e06(void)
{
    pol_new_earth(SCR_SURFACE);
    while (ui_buildings("Derrick").total < 5)
        if (pol_build(PRI_E6, "Derrick", DERRICK_COST) != UI_BUILD_OK)
            break;
    return ui_buildings("Derrick").active >= 5;
}

void act1_goals_e05(void)
{
    // Not hard: S2 comes first (the policy's 72 h is logged, not enforced).
    pol_goal("E05_mine", PRI_E5, goal_e05, 0);
}

void act1_goals_e06(void)
{
    // Not hard: the Detoxin for E15 comes before E15's money anyway.
    pol_goal("E06_derricks", PRI_E6, goal_e06, 0);
}

// --- Steps -----------------------------------------------------------------------

void act1_start(void)
{
    ui_new_game();
}

static bool credits_shown(void *unused)
{
    (void)unused;
    int32_t c;
    return ui_credits(&c);
}

void act1_e01_hire_builder(void)
{
    // Credits are drawn from the first game hour on.
    if (!ui_wait_hours(credits_shown, nullptr, 2))
        play_fail("no credits on the status bar");
    pol_hire("BUILDERS", "William Forbs", PRI_E1, 70000);
}

void act1_e02_hire_developer(void)
{
    pol_hire("DEVELOPERS", "Thomas Dempsey", PRI_E2, 15000);
}

void act1_e03_tax(void)
{
    pol_new_earth(SCR_PLANET_INFO);
    play_log("New Earth: pop %d, mood %d, tax %d", ui_population(), ui_mood(), ui_tax_level());
    ui_set_tax(TAX_VERY_DIFFICULT);
}

void act1_e04_satellite(void)
{
    pol_research("Satellite", 20);
    // ‖ Miner droid ("Can be analysed"): S11 starts it once nothing runs.
    pol_orders();
}

void act1_e05_mine(void)
{
    pol_orders();
}

void act1_e06_derricks(void)
{
    pol_orders();
}

static bool satellite_ores(void *arg)
{
    int32_t have[ORES], need[ORES];
    scaled(need, SATELLITE_ORES, *(int *)arg);
    if (!pol_ores("Satellite", have))
        return false;
    play_log("E7: ores D%d E%d K%d L%d", have[ORE_D], have[ORE_E], have[ORE_K], have[ORE_L]);
    return pol_ores_cover(have, need);
}

static bool stores_at_least(void *arg)
{
    return pol_stores("Satellite") >= *(int *)arg;
}

void act1_e07_satellites(void)
{
    int have = pol_stores("Satellite"), n = 2 - have;
    if (n > 0) {
        pol_poll_for("E7: ores for the satellites", satellite_ores, &n, 200, 3);
        while (n > 0) {
            pol_wait_money("E7: satellites", PRI_E7, SATELLITE_PRICE, 10 * POL_DAY);
            n -= pol_buy(PRI_E7, "Satellite", n, SATELLITE_PRICE);
        }
        pol_step_claim(PRI_E7, 0);
    }
    int want = 2;
    pol_poll_for("E7: two satellites in the stores", stores_at_least, &want, 20, 2);
}

static bool msg1_seen(void *unused)
{
    (void)unused;
    return pol_seen("Your satellites were destroyed") >= 0;
}

void act1_e08_deploy(void)
{
    // Apollo (New Earth's moon) keeps its satellite; Klatoo's is lost by
    // design, which starts the carrier research (MSG 1).
    ui_open_body("New Earth", "Apollo", SCR_PLANET_INFO);
    if (!ui_body_lists("Satellite") && !ui_deploy_stock_satellite())
        play_fail("E8: no satellite on Apollo");
    if (pol_stores("Satellite") > 0) {
        ui_open_body("Klatoo", nullptr, SCR_PLANET_INFO);
        if (!ui_deploy_stock_satellite())
            play_fail("E8: no satellite on Klatoo");
    }
    pol_wait_for("E8: MSG 1, the satellites destroyed", msg1_seen, nullptr, 30);
}

// --- Standing orders on the colonies, goals of E9-E21 ------------------------------

enum : int {
    VEHICLE_PLANT_COST = 35000,
    STADIUM_COST = 36000,
    BUILDING_COST = 5000,
    FOOD_COST = 15000,
    HOSPITAL_COST = 7000,
    MEDICINE_COST = 9500,
    MINER_STATION_PRICE = 9700,
    TRANSFER_SHIP_PRICE = 3800,
    CARRIER_PRICE = 1530,
    DROID_PRICE = 3150,
};

static const int32_t MINER_STATION_ORES[ORES] = {1500, 1024, 333, 212, 0, 0};
static const int32_t TRANSFER_SHIP_ORES[ORES] = {223, 521, 451, 56, 0, 0};
static const int32_t CARRIER_ORES[ORES] = {55, 105, 27, 18, 0, 0};
static const int32_t DROID_ORES[ORES] = {10, 10, 5, 3, 0, 0};

// A colony of act 1: where it is, how its boxes name it (the end of
// "... on Amnesty system,planet New Earth,moon Apollo") and its needs.
typedef struct {
    const char *planet, *moon;  // moon nullptr: New Earth itself
    const char *box_name;
    bool medicine;  // MP(n): disease comes after n days (walkthrough "Useful worlds")
    PolPriority pri;
} Colony;

static const Colony NEW_EARTH = {"New Earth", nullptr, "planet New Earth", false, PRI_S2};
static const Colony APOLLO = {"New Earth", "Apollo", "planet New Earth,moon Apollo", false,
                              PRI_APOLLO};
static const Colony MIR = {"Amnesty 2", "Mir", "planet Amnesty 2,moon Mir", true, PRI_MIR};
static const Colony PENELOPE = {"Zeus", "Penelope", "planet Zeus,moon Penelope", false,
                                PRI_PENELOPE};
// Act 2's (J11).
static const Colony RUSSEL = {"Jade", "Russel", "planet Jade,moon Russel", false, PRI_J11};

enum : int {
    // S2 keeps housing and hospitals ahead of the population (the ceiling
    // grows with them, and so does the growth), up to 12 Buildings: New
    // Earth's ceiling then stays under 100,000 people (-20 attitude above).
    LEAD_PERCENT = 160,
    MAX_BUILDINGS = 12,
};

static void open_colony(const Colony *c, int screen)
{
    if (!c->moon)
        pol_new_earth(screen);
    else
        ui_open_body(c->planet, c->moon, screen);
}

// S2 and the colony warning boxes for colony `c`: the building it needs
// next for housing, food and health (nullptr if none), from its planet info
// (population) and surface (building counts).
static const char *s2_need(const Colony *c, int32_t *cost, bool *urgent)
{
    open_colony(c, SCR_PLANET_INFO);
    int32_t pop = ui_population();
    open_colony(c, SCR_SURFACE);
    int buildings = ui_buildings("Building").total, food = ui_buildings("Food Resources").total,
        farms = ui_buildings("Farm").total, hospitals = ui_buildings("Hospital").total;
    // From act 2 on: the colony's boxes of the last two days (a pass can
    // come more than a day after the midnight box), and a diseased world's
    // sick count double (S2).
    bool act2 = pol_act() >= 2;
    int64_t day = pol_now() - (act2 ? 2 : 1) * POL_DAY;
    int32_t living = 10000 + 7000 * buildings, eat = 20000 + 8000 * farms + 20000 * food,
            care = (20000 + 20000 * hospitals) / (act2 && c->medicine ? 2 : 1);
    // No more housing past 45,000 people without a working Stadium (above
    // 50,000 its lack costs 20 attitude).
    bool hold = pop >= 45000 && ui_buildings("Stadium").active == 0;
    int32_t lead = pop / 100 * LEAD_PERCENT;
    // Shortages first (within 3,000 people, or a box naming one: each costs
    // 10-15 attitude, sim.c:457-564), food before housing before hospitals;
    // then the lead.
    const struct {
        bool need, urgent;
        const char *kind;
        int32_t cost;
    } wants[] = {
        {pop > eat - 3000 || pol_seen_about("starving", c->box_name, day), true, "Food Resources",
         FOOD_COST},
        {pol_seen_about("enough living space", c->box_name, day)
             || (pop > living - 3000 && !hold && buildings < MAX_BUILDINGS),
         true, "Building", BUILDING_COST},
        {pop > care - 3000 || pol_seen_about("need a hospital", c->box_name, day), true,
         "Hospital", HOSPITAL_COST},
        {(c->medicine || pol_seen_about("medicine plant", c->box_name, day))
             && ui_buildings("Medicine Plant").total == 0,
         true, "Medicine Plant", MEDICINE_COST},
        {living < lead && buildings < MAX_BUILDINGS && !hold, false, "Building", BUILDING_COST},
        {care < lead && hospitals < 5, false, "Hospital", HOSPITAL_COST},
    };
    for (size_t i = 0; i < SDL_arraysize(wants); i++) {
        // A site still being built counts already: one at a time per kind.
        if (!wants[i].need || ui_buildings(wants[i].kind).building > 0)
            continue;
        *cost = wants[i].cost;
        *urgent = wants[i].urgent;
        play_log("S2 %s: pop %d, %d buildings, %d food, %d farms, %d hospitals: %s%s",
                 c->moon ? c->moon : c->planet, pop, buildings, food, farms, hospitals,
                 wants[i].kind, wants[i].urgent ? " (short)" : "");
        return wants[i].kind;
    }
    return nullptr;
}

// S4 on New Earth, daily, ahead of the shortage: unpowered leisure
// buildings cost attitude, and S3's tax with it.
static bool goal_s4_new_earth(void)
{
    pol_new_earth(SCR_SURFACE);
    pol_power(PRI_S4, true);
    return false;  // a standing order
}

static bool goal_s2_new_earth(void)
{
    int32_t cost;
    bool urgent;
    const char *kind = s2_need(&NEW_EARTH, &cost, &urgent);
    if (kind)
        pol_build(urgent ? PRI_S2_SHORT : PRI_S2, kind, cost);
    return false;  // a standing order
}

// The core of a colony after its founding (E18): S2's needs first, then a
// Vehicle plant and a Spaceport, one building a pass (S4 adds power). From
// act 2 on S3 sets its tax, and its Mines (their droids come by trade
// group, S7) and leisure buildings follow the core.
static const struct {
    const char *kind;
    int32_t cost;
    int n;  // up to n of them (counting those being built)
    int act;
    int32_t pop;  // once the colony has more people
} BUILD_OUT[] = {
    {"Vehicle Plant", 35000, 1, 1, 0},
    {"Space Port", 50000, 1, 1, 0},
    // Above 50,000 people without one: -20 attitude (S2 "Leisure").
    {"Stadium", STADIUM_COST, 1, 2, 40000},
    {"Mine", MINE_COST, 9, 2, 0},
    {"Park", 8000, 1, 2, 0},
    {"Church", 17000, 1, 2, 0},
    {"Leisure Centre", 27000, 1, 2, 0},
    // Above 70,000 people without one: -20 attitude and a ceiling of
    // 80,000 (sim.c:503-508, 577); Apollo passed 70,000 early in act 3.
    {"University", 53000, 1, 3, 60000},
};

// S3 on colony `c` (act 2 on), its surface shown: the tax from its working
// leisure buildings, its population `pop` and its mood word.
static void colony_tax(const Colony *c, int32_t pop, int mood, int tax)
{
    PolLeisure l;
    pol_read_leisure(&l);
    int best = pol_best_tax(pop, &l);
    int want = pol_tax_want(tax, mood, best,
                            pol_seen_about("rebellious forces", c->box_name, pol_now() - POL_DAY));
    if (mood < 0 || tax < 0 || want == tax)
        return;
    play_log("S3 %s: pop %d, mood %d, tax %d -> %d (leisure %d, stadium %d, church %d, park %d)",
             c->moon, pop, mood, tax, want, l.leisure, l.stadium, l.church, l.park);
    open_colony(c, SCR_PLANET_INFO);
    ui_set_tax(want);
    open_colony(c, SCR_SURFACE);
}

static void build_out(const Colony *c)
{
    open_colony(c, SCR_PLANET_INFO);
    // Founded: the TYPE line reads "colony" once the box "The building of a
    // colony started" came (before, "under colonization").
    if (!ui_body_type_says("olony") || ui_body_type_says("under"))
        return;
    int32_t pop = ui_population();
    int mood = ui_mood(), tax = ui_tax_level();
    int32_t cost;
    bool urgent;
    const char *kind = s2_need(c, &cost, &urgent);
    // From act 2 S4 ahead of the shortage, as on New Earth: its Mines draw
    // 1,100 kWh each, and unpowered leisure buildings cost the tax.
    if (pol_act() >= 2) {
        colony_tax(c, pop, mood, tax);
        if (pol_power(c->pri, true))
            return;
    }
    if (kind) {
        pol_build(c->pri, kind, cost);
        return;
    }
    for (size_t i = 0; i < SDL_arraysize(BUILD_OUT); i++) {
        UiBuildings b = ui_buildings(BUILD_OUT[i].kind);
        if (BUILD_OUT[i].act > pol_act() || b.total >= BUILD_OUT[i].n || b.building > 0
            || pop <= BUILD_OUT[i].pop)
            continue;
        // The next Mine once every finished one has its droid (the "n Miner
        // Droid" line of the planet info).
        if (SDL_strcmp(BUILD_OUT[i].kind, "Mine") == 0 && ui_droids() < b.finished)
            continue;
        int r = pol_build(c->pri, BUILD_OUT[i].kind, BUILD_OUT[i].cost);
        if (r != UI_BUILD_UNAVAILABLE)
            return;
    }
}

// Each after its founding goal (which rereads the planet info) is done.
static bool goal_colony_apollo(void)
{
    if (pol_goal_done("E18_apollo"))
        build_out(&APOLLO);
    return false;
}

static bool goal_colony_mir(void)
{
    if (pol_goal_done("E19_mir"))
        build_out(&MIR);
    return false;
}

static bool goal_colony_penelope(void)
{
    if (pol_goal_done("E20_penelope"))
        build_out(&PENELOPE);
    return false;
}

static bool goal_colony_russel(void)
{
    if (pol_goal_done("J11_russel"))
        build_out(&RUSSEL);
    return false;
}

// E9: Vehicle plant, then a Stadium (S2's leisure; +10 to the mood, and
// without one above 50,000 people -20): urgent from 40,000 on, as the two
// cost 71,000 and New Earth grows ~250 a day.
static bool goal_e09(void)
{
    const PolColony *ne = pol_new_earth_seen();
    PolPriority pri = ne->pop >= 40000 ? PRI_E9_URGENT : PRI_E9;
    pol_new_earth(SCR_SURFACE);
    UiBuildings v = ui_buildings("Vehicle Plant");
    if (v.total == 0) {
        pol_build(pri, "Vehicle Plant", VEHICLE_PLANT_COST);
        return false;
    }
    if (v.active == 0)
        return false;
    UiBuildings st = ui_buildings("Stadium");
    if (st.total == 0) {
        int r = pol_build(pri, "Stadium", STADIUM_COST);
        if (r == UI_BUILD_UNAVAILABLE) {
            play_log("E9: the Stadium is not offered");
            return true;
        }
        return false;
    }
    return st.active > 0;
}

// E12: Dempsey takes Maths & physics (Control Centre needs Physics 4: the
// skills under the research screen tell whether he has been).
static bool goal_e12(void)
{
    ui_goto(3);
    int skills[4];
    if (ui_developer_skills(skills) && skills[1] >= 4)
        return true;
    return pol_university(PRI_E12, "Thomas Dempsey", "Maths & physics.", INT32_MAX);
}

// S12 for the pilot: experience + 1 legs an hour for every group.
static bool goal_s12_jack(void)
{
    return pol_university(PRI_S12, "Jack Benson", nullptr, 60000);
}

// E16: two carriers with a satellite each survey Amnesty. The grid's
// satellite count says how far it got: 2 left = Penelope next, 1 = Mir.
static bool goal_e16(void)
{
    ui_ship_info();
    // Bought only once Trade 1 has its ship and station: E15's ores first.
    if (!ui_group_find("Trade 1"))
        return false;
    if (!ui_group_find("Survey 1")) {
        int carriers = pol_stores("Sat.carrier"), sats = pol_stores("Satellite");
        UiShopQueue q;
        ui_shop_select("Sat.carrier");
        ui_shop_queue(&q);
        int queued_c = q.shown ? q.queued : 0;
        ui_shop_select("Satellite");
        ui_shop_queue(&q);
        int queued_s = q.shown ? q.queued : 0;
        int32_t have[ORES], need[ORES];
        if (carriers + queued_c < 2 && pol_ores("Sat.carrier", have)) {
            scaled(need, CARRIER_ORES, 2 - carriers - queued_c);
            if (pol_ores_cover(have, need))
                pol_buy(PRI_E16, "Sat.carrier", 2 - carriers - queued_c, CARRIER_PRICE);
        }
        if (sats + queued_s < 2 && pol_ores("Satellite", have)) {
            scaled(need, SATELLITE_ORES, 2 - sats - queued_s);
            if (pol_ores_cover(have, need))
                pol_buy(PRI_E16, "Satellite", 2 - sats - queued_s, SATELLITE_PRICE);
        }
        if (pol_stores("Sat.carrier") < 2 || pol_stores("Satellite") < 2)
            return false;
        if (!ui_new_group("Carrier", "Survey 1"))
            play_fail("E16: no Carrier group type");
        if (!ui_equip("Survey 1", "Sat.carrier", nullptr, 2)
            || !ui_equip("Survey 1", "Sat.carrier", "Satellite", 2))
            play_fail("E16: Survey 1 not equipped");
    }
    int status = ui_group_status("Survey 1");
    if (status == SHIP_FLYING)
        return false;
    if (status == SHIP_DOCKED && !ui_take_off("Survey 1"))
        play_fail("E16: Survey 1 did not take off");
    int sats = ui_group_units("Survey 1", "Sat.carrier", "Satellite");
    const char *loc = pol_where("Survey 1");
    play_log("E16: Survey 1 %s, %d satellites", loc, sats);
    if (sats == 0)
        return true;
    // Penelope first, then Mir (a moon of Amnesty 2, once that is found).
    const char *planet = sats == 2 ? "Zeus" : "Amnesty 2", *moon = sats == 2 ? "Penelope" : "Mir";
    char at[64];
    SDL_snprintf(at, sizeof at, "%s %s", planet, moon);
    if (pol_ends_with(loc, at)) {
        if (!ui_deploy_carrier_satellite("Survey 1"))
            play_fail("E16: no satellite deployed on %s", moon);
        return false;
    }
    // A moon is offered once its planet has been visited; Amnesty 2 once
    // found (a box and its name prompt; then it is on the map). Meanwhile
    // shuttle Klatoo - Zeus: days in transit find Amnesty's hidden planets.
    if (sats == 2 || pol_on_map(planet, nullptr)) {
        if (ui_launch("Survey 1", nullptr, planet, moon))
            return false;
        if (!pol_ends_with(loc, planet) && ui_launch("Survey 1", nullptr, planet, nullptr))
            return false;
    }
    if (!ui_launch("Survey 1", nullptr, pol_ends_with(loc, "Zeus") ? "Klatoo" : "Zeus", nullptr))
        play_fail("E16: Survey 1 can't be launched");
    return false;
}

// Founds a colony on `planet` (moon `moon`) once its COLONIZATION icon is
// offered and the money is there. True once the planet info's TYPE line
// says colony (or under colonization).
static bool found_colony(PolPriority pri, const char *planet, const char *moon,
                         const char *const *starters, int n, int32_t cost)
{
    ui_open_body(planet, moon, SCR_PLANET_INFO);
    if (ui_body_type_says("colony") || ui_body_type_says("Colony"))
        return true;
    if (!ui_toolbar_has("COLONIZATION"))
        return false;
    if (!pol_afford(pri, cost))
        return false;
    if (!ui_found_colony(starters, n))
        return false;
    play_log("colony founded on %s %s at %s, %d credits", planet, moon ? moon : "", pol_date(),
             pol_credits());
    return true;
}

static const char *const DESERT_STARTERS[] = {"Builder Plant", "Windtrap", "Mine", "Building"};
static const char *const EARTH_STARTERS[] = {"Nuclear Gen.", "Builder Plant", "Mine", "Building"};

static bool goal_e18(void)
{
    return found_colony(PRI_E18, "New Earth", "Apollo", DESERT_STARTERS, 4, 142000);
}

static bool goal_e19(void)
{
    if (!pol_on_map("Amnesty 2", "Mir"))
        return false;  // not found, or its moons not explored yet
    return found_colony(PRI_E19, "Amnesty 2", "Mir", DESERT_STARTERS, 4, 142000);
}

static bool goal_e20(void)
{
    if (!pol_on_map("Zeus", "Penelope"))
        return false;
    return found_colony(PRI_E20, "Zeus", "Penelope", EARTH_STARTERS, 4, 167000);
}

// E21: New Earth to 9 mines with a droid each and 20 Derricks.
static bool goal_e21(void)
{
    pol_new_earth(SCR_SURFACE);
    UiBuildings m = ui_buildings("Mine"), d = ui_buildings("Derrick");
    int droids = ui_droids(), stock = ui_droid_stock();
    if (droids < m.finished && stock > 0) {
        ui_add_droid();
        return false;
    }
    if (m.finished > droids + stock && pol_stores("Miner droid") == 0) {
        int32_t have[ORES], need[ORES];
        scaled(need, DROID_ORES, 1);
        UiShopQueue q;
        ui_shop_select("Miner droid");
        if (ui_shop_queue(&q) && (!q.shown || q.queued == 0) && pol_ores("Miner droid", have)
            && pol_ores_cover(have, need))
            pol_buy(PRI_E21, "Miner droid", 1, DROID_PRICE);
        pol_new_earth(SCR_SURFACE);
    }
    if (m.total < 9 && m.building == 0) {
        pol_build(PRI_E21, "Mine", MINE_COST);
        return false;
    }
    if (d.total < 20 && d.building == 0)
        pol_build(PRI_E21, "Derrick", DERRICK_COST);
    return m.total >= 9 && droids >= 9 && d.active >= 20;
}

void act1_goals_s2(void)
{
    pol_goal("S2_new_earth", PRI_S2, goal_s2_new_earth, 0);
    pol_goal("S4_new_earth", PRI_S4, goal_s4_new_earth, 0);
}

void act1_goals_e09(void)
{
    pol_goal("E09_vehicle_plant", PRI_E9, goal_e09, 0);
}

void act1_goals_e12(void)
{
    pol_goal("E12_university", PRI_E12, goal_e12, 0);
}

void act1_goals_e14(void)
{
    pol_goal("S12_jack_university", PRI_S12, goal_s12_jack, 0);
}

void act1_goals_e16(void)
{
    pol_goal("E16_survey", PRI_E16, goal_e16, 0);
}

void act1_goals_e18(void)
{
    pol_goal("E18_apollo", PRI_E18, goal_e18, 0);
    pol_goal("colony_apollo", PRI_APOLLO, goal_colony_apollo, 0);
}

void act1_goals_e19(void)
{
    pol_goal("E19_mir", PRI_E19, goal_e19, 0);
    pol_goal("colony_mir", PRI_MIR, goal_colony_mir, 0);
}

void act1_goals_e20(void)
{
    pol_goal("E20_penelope", PRI_E20, goal_e20, 0);
    pol_goal("colony_penelope", PRI_PENELOPE, goal_colony_penelope, 0);
}

void act1_goals_russel(void)
{
    pol_goal("colony_russel", PRI_J11, goal_colony_russel, 0);
}

void act1_goals_e21(void)
{
    pol_goal("E21_new_earth_mines", PRI_E21, goal_e21, 0);
}

// --- Steps E9-E23 ---------------------------------------------------------------

void act1_e09_vehicle_plant(void)
{
    pol_orders();
}

void act1_e10_carrier(void)
{
    // KIT 4 comes 10-19 h after MSG 1; the research ~14 h.
    pol_research("Sat.carrier", 60);
}

void act1_e11_miner_station(void)
{
    // Apollo's satellite: "ready for mining" and MSG 2, KIT 5 20-59 h later.
    pol_research("Miner station", 10 * POL_DAY);
}

void act1_e12_university(void)
{
    pol_orders();
}

void act1_e13_transfer_ship(void)
{
    // KIT 6 30-39 developer hours after E11; 109 h at experience 20, 31 h
    // after the course.
    pol_research("Transfer ship", 12 * POL_DAY);
}

void act1_e14_pilot(void)
{
    ui_command_centre();
    pol_hire("PILOTS", "Jack Benson", PRI_E14, 25000);
}

typedef struct {
    int32_t need[ORES];
} OreWait;

static bool ores_there(void *arg)
{
    const OreWait *w = arg;
    int32_t have[ORES];
    if (!pol_ores("Satellite", have))
        return false;
    play_log("E15: ores D%d E%d K%d L%d", have[ORE_D], have[ORE_E], have[ORE_K], have[ORE_L]);
    return pol_ores_cover(have, w->need);
}

typedef struct {
    const char *item;
    int n;
} StoresWait;

static bool stores_reach(void *arg)
{
    const StoresWait *w = arg;
    return pol_stores(w->item) >= w->n;
}

static bool msg4_seen(void *unused)
{
    (void)unused;
    return pol_seen("This planet is suitable for living") >= 0;
}

void act1_e15_station_on_apollo(void)
{
    ui_open_body("New Earth", "Apollo", SCR_PLANET_INFO);
    bool placed = ui_body_lists("Miner Station");
    ui_ship_info();
    if (!placed && !ui_group_find("Trade 1")) {
        // The station and the ship, bought together (the shop takes the ore
        // at once).
        int stations = pol_stores("Miner station"), ships = pol_stores("Transfer ship");
        OreWait w = {};
        for (int k = 0; k < ORES; k++)
            w.need[k] = (stations ? 0 : MINER_STATION_ORES[k]) + (ships ? 0 : TRANSFER_SHIP_ORES[k]);
        int32_t cost = (stations ? 0 : MINER_STATION_PRICE) + (ships ? 0 : TRANSFER_SHIP_PRICE);
        if (cost > 0) {
            pol_poll_for("E15: ores for the station and the ship", ores_there, &w, 30 * POL_DAY, 6);
            pol_wait_money("E15: station and ship", PRI_E15, cost, 30 * POL_DAY);
            if (!stations && pol_buy(PRI_E15, "Miner station", 1, MINER_STATION_PRICE) != 1)
                play_fail("E15: the Miner station was not bought");
            if (!ships && pol_buy(PRI_E15, "Transfer ship", 1, TRANSFER_SHIP_PRICE) != 1)
                play_fail("E15: the Transfer ship was not bought");
            pol_step_claim(PRI_E15, 0);
        }
        pol_poll_for("E15: the station built", stores_reach, &(StoresWait){"Miner station", 1},
                     4 * POL_DAY, 6);
        pol_poll_for("E15: the ship built", stores_reach, &(StoresWait){"Transfer ship", 1},
                     4 * POL_DAY, 6);
        if (!ui_new_group("Trade", "Trade 1"))
            play_fail("E15: no Trade group type");
        // The station goes in the Mine column of the grid: it is not cargo.
        if (!ui_equip("Trade 1", "Transfer ship", nullptr, 1)
            || !ui_equip("Trade 1", "Transfer ship", "Miner station", 1))
            play_fail("E15: Trade 1 not equipped");
    }
    if (!placed) {
        if (!pol_ends_with(pol_where("Trade 1"), "New Earth Apollo")) {
            if (!ui_take_off("Trade 1") || !ui_launch("Trade 1", nullptr, "New Earth", "Apollo"))
                play_fail("E15: Trade 1 not launched to Apollo");
            if (!ui_wait_arrival("Trade 1", 2 * POL_DAY))
                play_fail("E15: Trade 1 did not reach Apollo");
        }
        ui_open_group("Trade 1", 17);
        if (!ui_body_action("ADD MINER STATION"))
            play_fail("E15: no ADD MINER STATION at Apollo");
        pol_wait_for("E15: MSG 4", msg4_seen, nullptr, 2);
    }
    // E23 from here on: Trade 1 stands by in orbit at New Earth.
    act1_e23_standby();
}

void act1_e16_survey(void)
{
    pol_orders();
}

void act1_e17_control_centre(void)
{
    // KIT 7 20-59 h after MSG 4; 1,550 / 70 = 22 h once Dempsey has Physics 4.
    // Dempsey's course (E12, 50-69 h) may still wait for money.
    pol_research("Control Centre", 30 * POL_DAY);
}

void act1_e18_apollo(void)
{
    pol_orders();
}

void act1_e19_mir(void)
{
    pol_orders();
}

void act1_e20_penelope(void)
{
    pol_orders();
}

void act1_e21_new_earth_mines(void)
{
    pol_orders();
}

// The rename prompt of Jade's discovery ("The new planet's name in Amnesty
// system? Jade").
static bool jade_seen(void *unused)
{
    (void)unused;
    return pol_seen("system? Jade") >= 0;
}

void act1_e22_jade(void)
{
    // The port reveals Jade by 2928-04-01 (game/fixes.h): a later date is a
    // port bug.
    UiDate due = {2928, 4, 2, 0};
    int hours = (int)(ui_date_hours(&due) - pol_now());
    // Found before this step (or this run): Jade is drawn on the Amnesty map.
    if (jade_seen(nullptr) || pol_on_map("Jade", nullptr))
        return;
    if (hours <= 0 || !pol_wait(jade_seen, nullptr, hours))
        play_fail("E22: Jade not revealed by 2928-04-02");
    play_log("E22: Jade revealed at %s", pol_date());
}

void act1_e23_standby(void)
{
    if (pol_ends_with(pol_where("Trade 1"), "New Earth") && ui_group_status("Trade 1") == SHIP_ORBIT)
        return;
    if (!ui_take_off("Trade 1"))
        play_fail("E23: Trade 1 did not take off");
    if (!pol_ends_with(pol_where("Trade 1"), "New Earth")) {
        if (!ui_launch("Trade 1", nullptr, "New Earth", nullptr))
            play_fail("E23: Trade 1 not launched home");
        if (!ui_wait_arrival("Trade 1", 2 * POL_DAY))
            play_fail("E23: Trade 1 did not reach New Earth");
    }
    play_log("E23: Trade 1 %s", pol_where("Trade 1"));
}
