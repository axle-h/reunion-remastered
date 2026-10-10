#include "play/army.h"

#include <SDL3/SDL.h>

#include "play/play.h"
#include "play/policy.h"
#include "play/run.h"
#include "play/ui.h"
#include "play/ui_office.h"
#include "play/ui_planet.h"
#include "play/ui_ships.h"

enum : int { SCR_SURFACE = 20 };

// The number of the act-3 step running ("M08_wave2_force": 8); later acts
// count as past every act-3 step.
static int step_no(void)
{
    const char *s = play_step_name();
    if (pol_act() > 3)
        return 99;
    return pol_act() < 3 || !s ? 0 : SDL_atoi(s + 1);
}

// The products of the army, as the shop and the group grid name them.
typedef struct {
    const char *name;
    int32_t price;
    int32_t ores[ORES];  // "Ore needed: (One piece)"
    int depth;           // units kept queued: about three days of its line
    int depth4;          // from act 4 on (0: the same): a pass of the goals takes ~3.5 days
    int depth5;          // from act 5 on (0: depth4)
    int act;             // the act from which the goal builds it (0: any)
} Product;

enum {
    P_HUNTER, P_SF, P_DESTROYER, P_TANK, P_LASER, P_TWIN, P_MISSILE, P_AIRCRAFT, P_PLASMA,
    P_CRUISER, P_LAUNCHER, PRODUCTS
};

static const Product PRODUCT[PRODUCTS] = {
    [P_HUNTER] = {"Hunter", 5700, {450, 1875, 406, 454, 78, 0}, 4},
    [P_SF] = {"Starfighter", 8400, {1490, 1730, 506, 490, 120, 515}, 3},
    [P_DESTROYER] = {"Destroyer", 38000, {7500, 13500, 2056, 8000, 2000, 1530}, 1},
    // depth4: act 4's gates, never idle: the tank line (Home Guard's 40,
    // below) and the Twin line (aircraft carry 4 each).
    [P_TANK] = {"Battle tank", 6000, {1300, 1215, 66, 95, 780, 150}, 2, 3},
    [P_LASER] = {"Laser cannon", 1550, {50, 35, 16, 15, 78, 0}, 9},
    [P_TWIN] = {"Twin laser gun", 2650, {85, 75, 35, 25, 118, 0}, 6, 10},
    // depth5: act 5's gate, the Missile line (5 per launcher, 6 per
    // Cruiser: ~340 missiles of 10 h each).
    [P_MISSILE] = {"Missile", 3000, {40, 50, 60, 50, 80, 300}, 8, 0, 16},
    [P_AIRCRAFT] = {"Aircraft", 17000, {700, 2455, 1455, 500, 200, 200}, 3},
    // Act 5 (Rigel 1, X2; Cruiser from the League's surrender). A Cruiser's
    // queue is capped at the Space stations in stock, like a Destroyer's.
    [P_PLASMA] = {"Plasma gun", 5000, {290, 35, 156, 335, 599, 2300}, 4, .act = 5},
    [P_CRUISER] = {"Cruiser", 340000, {10000, 18675, 12377, 6665, 1200, 2860}, 1, .act = 5},
    [P_LAUNCHER] = {"Missile launcher", 26000, {630, 510, 345, 48, 90, 0}, 3, .act = 5},
};

// Ships and ground units, and the weapons each carries (walkthrough.md
// "Weapons and loadouts"; a weapon not yet researched is left out), in the
// order the grid gets them, one row per ArmyForce unit. No Troopers: 90
// tanks are the measured Mirach force, aircraft the League's and launchers
// Earth's (playthrough-battles.md §6).
enum { WEAPONS = 3 };
static const struct {
    int unit;
    struct { int product, per; } weapon[WEAPONS];
} LOADOUT[ARMY_UNITS] = {
    [ARMY_HUNTER] = {P_HUNTER, {{P_LASER, 2}, {P_TWIN, 1}}},
    [ARMY_SF] = {P_SF, {{P_TWIN, 4}, {P_MISSILE, 1}}},
    [ARMY_DESTROYER] = {P_DESTROYER, {{P_TWIN, 2}, {P_MISSILE, 4}, {P_PLASMA, 1}}},
    [ARMY_TANK] = {P_TANK, {{P_LASER, 3}, {P_TWIN, 1}}},
    [ARMY_AIRCRAFT] = {P_AIRCRAFT, {{P_TWIN, 4}, {P_MISSILE, 1}}},
    [ARMY_CRUISER] = {P_CRUISER, {{P_MISSILE, 6}, {P_PLASMA, 3}}},
    [ARMY_LAUNCHER] = {P_LAUNCHER, {{P_MISSILE, 5}}},
};

// The groups the goal fills, in that order: act 3's one (Home Guard, then
// Strike), act 4's and act 5's two.
enum { GROUPS = 2, TIERS = 2 };

static int army_groups(const char *names[GROUPS])
{
    if (pol_act() < 4) {
        names[0] = army_group();
        return 1;
    }
    // Act 5: the ground reserve first (X6 before X7).
    bool reserve_first = pol_act() >= 5;
    names[reserve_first] = "Strike";
    names[!reserve_first] = "Home Guard";
    return 2;
}

// The force each group is filled up to, by tier: every group gets its
// tier-1 force before any gets more. Act 3 (§1 M8, M12, M18): one tier.
// Act 4 (§1 L7, L13; measured with Destroyers without Plasma, which comes
// in act 5): Strike first to the Orionis 3 and Lyrae 5 force (40H 40SF 6D
// won 20/20 in space, 30T 20Air 20/20 on the ground; 40T 10Air lost 3/20 at
// Orionis), then to Lyrae 7's (50H 50SF 8D, 30T 30Air: 20/20 with the
// Lisonian wave's troops back home); Home Guard holds the reserve that
// beats the Lisonian landing on its own, 40T 10Air (10/10), while Strike is
// away (S9).
static ArmyForce tier_target(const char *group, int tier)
{
    // Act 5 (§1 X3, X6; one tier, no battle before Earth): Strike with
    // more ships than the guide's 50H 50SF 10D 10C (Earth's fleet: 10/10,
    // but the strike loses 40H 40SF and up to 8D; 60H 60SF 12D lost ~1D),
    // and 40T 20Air 40ML on the ground (20/20, lost 10T 20Air 30ML; 30ML
    // 9/10); Home Guard the ground reserve for Earth wave 1, 40T 10Air with
    // 10 launchers (20/20; 40T 30Air alone 8/20).
    if (pol_act() >= 5) {
        if (group && SDL_strcmp(group, "Home Guard") == 0)
            return (ArmyForce){{0, 0, 0, 40, 10, 0, 10}};
        return (ArmyForce){{60, 60, 12, 40, 20, 10, 40}};
    }
    if (pol_act() >= 4) {
        if (group && SDL_strcmp(group, "Home Guard") == 0)
            return (ArmyForce){{0, 0, 0, 40, 10}};
        return tier == 0 ? (ArmyForce){{40, 40, 6, 30, 20}} : (ArmyForce){{50, 50, 8, 30, 30}};
    }
    int k = step_no();
    // M8: 24 Hunters, 16 Starfighters and the ground reserve for wave 2;
    // the tank line runs on towards M18's 90 from the deal on (§5 R8).
    if (k < 10)
        return (ArmyForce){{24, 16, 0, 90}};
    // M12: 30 Hunters, 24 Starfighters, 30 tanks for wave 3.
    if (k < 13)
        return (ArmyForce){{30, 24, 0, 90}};
    // M18: 90 tanks for Mirach 3 (playthrough-battles.md §6) and, as the
    // tank line sets the date, more ships than the measured 20H 30SF 5D:
    // the waves' survivors went home there (55 ships against 20H 20SF 3D
    // measured; 30H 30SF 5D and the Kalls' 15H 10SF won but lost 31H 19SF
    // 3D in one run).
    return (ArmyForce){{40, 40, 6, 90}};
}

static int tiers(void)
{
    return pol_act() == 4 ? TIERS : 1;
}

ArmyForce army_target(const char *group)
{
    return tier_target(group, tiers() - 1);
}

// Weapons per unit of a row by the step running: the tanks' twins from M10
// on (M8 gives the twins to the ships first, §1 M8).
static int loadout_per(int row, int w)
{
    int p = LOADOUT[row].weapon[w].product;
    if (p == P_TWIN && LOADOUT[row].unit == P_TANK && step_no() < 10)
        return 0;
    return p < 0 ? 0 : LOADOUT[row].weapon[w].per;
}

const char *army_group(void)
{
    ui_ship_info();
    return ui_group_find("Strike") ? "Strike" : "Home Guard";
}

int army_count(const char *group, const char *ship, const char *weapon)
{
    ui_ship_info();
    return ui_group_units(group, ship, weapon);
}

// The shop's queue ("Bought items:") of `item`, 0 if none.
static int queued(const char *item)
{
    UiShopQueue q;
    if (!ui_shop_select(item) || !ui_shop_queue(&q) || !q.shown)
        return 0;
    return q.queued;
}

// What one pass of the army goal reads, each screen visited once (a screen
// switch costs game time while the clock runs): the research icons, the
// army groups' grids (ship info; the grid counts of screen 22) and, per
// product, the shop's stores and queue.
typedef struct {
    int state[PRODUCTS];  // UI_RS_*
    int groups;
    const char *name[GROUPS];
    int group[GROUPS][ARMY_UNITS][1 + WEAPONS];  // each group's grid per LOADOUT row: units, weapons
    int stores[PRODUCTS], queued[PRODUCTS];
    int32_t reserved[ORES];  // New Earth's ores kept for the Space station (M16)
} ArmyView;

static void read_group(ArmyView *v, int g)
{
    bool there = ui_group_find(v->name[g]) != 0;
    for (int r = 0; r < ARMY_UNITS; r++) {
        const char *ship = PRODUCT[LOADOUT[r].unit].name;
        v->group[g][r][0] = there ? ui_group_units(v->name[g], ship, nullptr) : 0;
        for (int w = 0; w < WEAPONS; w++) {
            int p = LOADOUT[r].weapon[w].product;
            v->group[g][r][1 + w] = there && p >= 0 ? ui_group_units(v->name[g], ship, PRODUCT[p].name) : 0;
        }
    }
}

static void read_groups(ArmyView *v)
{
    v->groups = army_groups(v->name);
    for (int g = 0; g < v->groups; g++)
        read_group(v, g);
}

// M16's Space station, researched and not yet in the stores: the army's
// Destroyers need it, so its ores are kept from the lines (seed 3: the
// tanks and ships took New Earth's Detoxin as it came in, the station's
// 60,000 t never gathered and M16 timed out).
static const int32_t STATION_ORES[ORES] = {60000, 48000, 6590, 2585, 2000, 3270};

static void read_states(ArmyView *v)
{
    pol_research_state("Satellite");  // the research screen, its icons drawn
    for (int p = 0; p < PRODUCTS; p++)
        v->state[p] = pol_act() < PRODUCT[p].act ? UI_RS_UNKNOWN : ui_research_icon(PRODUCT[p].name);
    if (pol_goal_pending("M16_station") && ui_research_icon("Space station") == UI_RS_DONE)
        SDL_memcpy(v->reserved, STATION_ORES, sizeof v->reserved);
}

static void read_army(ArmyView *v)
{
    *v = (ArmyView){};
    read_states(v);
    read_groups(v);
    for (int p = 0; p < PRODUCTS; p++) {
        if (v->state[p] == UI_RS_UNKNOWN)
            continue;
        v->stores[p] = pol_stores(PRODUCT[p].name);
        v->queued[p] = v->state[p] == UI_RS_DONE ? queued(PRODUCT[p].name) : 0;
    }
}

// How many of a product New Earth's ore stock allows (at most n), less
// the ores kept for the station.
static int ores_allow(const ArmyView *v, int p, int n)
{
    int32_t have[ORES];
    if (!pol_ores(PRODUCT[p].name, have))
        return 0;
    for (int k = 0; k < ORES; k++)
        if (PRODUCT[p].ores[k] > 0)
            n = SDL_min(n, (int)((have[k] - v->reserved[k]) / PRODUCT[p].ores[k]));
    return SDL_max(n, 0);
}

// Keeps product p's line busy towards `need` more units than the groups
// hold: up to its depth queued, as money (pol_buy) and New Earth's ores
// allow.
static void order(const ArmyView *v, int p, int need)
{
    if (v->state[p] != UI_RS_DONE)
        return;
    int q = v->queued[p];
    int depth = pol_act() >= 4 && PRODUCT[p].depth4 ? PRODUCT[p].depth4 : PRODUCT[p].depth;
    if (pol_act() >= 5 && PRODUCT[p].depth5)
        depth = PRODUCT[p].depth5;
    // A Destroyer's or Cruiser's queue is capped at the Space stations in
    // stock (M16; shop.c).
    if (p == P_DESTROYER || p == P_CRUISER) {
        int stations = pol_stores("Space station");
        if (stations == 0)
            return;
        depth = SDL_min(depth, stations);
    }
    int n = SDL_min(need - v->stores[p] - q, depth - q);
    if (n <= 0)
        return;
    int k = ores_allow(v, p, n);
    if (k < n)
        play_log("M8: the ores allow %d of %d %s", k, n, PRODUCT[p].name);
    if (k > 0)
        pol_buy(PRI_M8, PRODUCT[p].name, k, PRODUCT[p].price);
}

// Group g, docked at a colony, takes what the shop's stores hold of the
// army's units up to the force `t`, then their weapons by LOADOUT's order
// (weapons are pooled per row, so the rows are filled one after another;
// S10). The stores were read before the orders, which add to the queue only.
static void equip_group(ArmyView *v, int g, const ArmyForce *t, bool last)
{
    const char *name = v->name[g];
    if (ui_group_status(name) != SHIP_DOCKED) {
        play_log("M8: %s is not docked (%d)", name, ui_group_status(name));
        return;  // the grid takes units only at the dock (Strike away)
    }
    int (*have)[1 + WEAPONS] = v->group[g];
    for (int r = 0; r < ARMY_UNITS; r++) {
        int p = LOADOUT[r].unit, add = SDL_min(t->n[r] - have[r][0], v->stores[p]);
        if (add <= 0 || v->state[p] == UI_RS_UNKNOWN)
            continue;
        ui_equip(name, PRODUCT[p].name, nullptr, have[r][0] + add);
        int now = ui_group_units(name, PRODUCT[p].name, nullptr);
        v->stores[p] -= now - have[r][0];
        have[r][0] = now;
    }
    for (int r = 0; r < ARMY_UNITS; r++) {
        for (int w = 0; w < WEAPONS; w++) {
            int p = LOADOUT[r].weapon[w].product, per = loadout_per(r, w);
            if (per == 0 || have[r][0] == 0 || v->state[p] != UI_RS_DONE)
                continue;
            const char *ship = PRODUCT[LOADOUT[r].unit].name;
            // A tier arms the units it counts; the last arms all there are.
            int units = last ? have[r][0] : SDL_min(have[r][0], t->n[r]);
            int add = SDL_min(per * units - have[r][1 + w], v->stores[p]);
            if (add <= 0)
                continue;
            ui_equip(name, ship, PRODUCT[p].name, have[r][1 + w] + add);
            int now = ui_group_units(name, ship, PRODUCT[p].name);
            v->stores[p] -= now - have[r][1 + w];
            have[r][1 + w] = now;
        }
    }
}

// Group g (docked) gives back to the stores what it holds of a unit beyond
// its target that another group (docked) lacks, with the units' share of
// the row's weapons (taken first: a row's weapons are capped by its units):
// Strike came home from Mirach with more tanks than act 4 gives it, Home
// Guard had none (S9).
static void release_excess(ArmyView *v, int g)
{
    const char *name = v->name[g];
    if (ui_group_status(name) != SHIP_DOCKED)
        return;
    ArmyForce t = army_target(name);
    int (*have)[1 + WEAPONS] = v->group[g];
    for (int r = 0; r < ARMY_UNITS; r++) {
        int lack = 0;
        for (int o = 0; o < v->groups; o++) {
            ArmyForce u = army_target(v->name[o]);
            if (o != g && ui_group_status(v->name[o]) == SHIP_DOCKED)
                lack += SDL_max(u.n[r] - v->group[o][r][0], 0);
        }
        int units = have[r][0], k = SDL_min(units - t.n[r], lack);
        if (k <= 0)
            continue;
        const char *ship = PRODUCT[LOADOUT[r].unit].name;
        play_log("M8: %s gives back %d %s", name, k, ship);
        for (int w = 0; w < WEAPONS; w++) {
            int p = LOADOUT[r].weapon[w].product, give = have[r][1 + w] * k / units;
            if (give <= 0)
                continue;
            ui_equip(name, ship, PRODUCT[p].name, have[r][1 + w] - give);
            int now = ui_group_units(name, ship, PRODUCT[p].name);
            v->stores[p] += have[r][1 + w] - now;
            have[r][1 + w] = now;
        }
        ui_equip(name, ship, nullptr, units - k);
        int now = ui_group_units(name, ship, nullptr);
        v->stores[LOADOUT[r].unit] += units - now;
        have[r][0] = now;
    }
}

// Group g holds the force `t` of a tier with its weapons (rows of units
// not yet researched apart).
static bool group_holds(const ArmyView *v, int g, const ArmyForce *t)
{
    for (int r = 0; r < ARMY_UNITS; r++) {
        if (v->state[LOADOUT[r].unit] != UI_RS_DONE)
            continue;
        if (v->group[g][r][0] < t->n[r])
            return false;
        for (int w = 0; w < WEAPONS; w++) {
            int p = LOADOUT[r].weapon[w].product;
            if (v->state[p] == UI_RS_DONE && v->group[g][r][1 + w] < loadout_per(r, w) * t->n[r])
                return false;
        }
    }
    return true;
}

// The groups (created where missing, docked at New Earth) take what the
// stores hold, tier by tier: a tier's units only once every group holds
// the tier before with its weapons (more units would take the weapons the
// tier before still lacks, S10).
static void equip_groups(ArmyView *v)
{
    ui_ship_info();
    for (int g = 0; g < v->groups; g++) {
        if (ui_group_find(v->name[g]))
            continue;
        if (!ui_new_group("Army", v->name[g]))
            play_fail("M8: no Army group type");
        ui_ship_info();
        read_group(v, g);
    }
    for (int g = 0; g < v->groups && v->groups > 1; g++)
        release_excess(v, g);
    for (int tier = 0; tier < tiers(); tier++) {
        bool held = true;
        for (int g = 0; g < v->groups; g++) {
            ArmyForce t = tier_target(v->name[g], tier);
            equip_group(v, g, &t, tier == tiers() - 1);
            held = held && group_holds(v, g, &t);
        }
        if (!held)
            break;
    }
}

// The army (§1 M8): every line its units need kept busy towards the
// targets, and the groups filled from the stores.
static bool goal_army(void)
{
    ArmyView v;
    read_army(&v);
    // What each group lacks of its target, its units' weapons included
    // (what one group holds beyond its target does not make up for what
    // another lacks: the weapons are not handed over with release_excess's
    // units to the last one).
    int want[PRODUCTS] = {}, need[PRODUCTS] = {};
    for (int g = 0; g < v.groups; g++) {
        ArmyForce t = army_target(v.name[g]);
        for (int r = 0; r < ARMY_UNITS; r++) {
            int units = SDL_max(t.n[r], v.group[g][r][0]);
            want[LOADOUT[r].unit] += t.n[r];
            need[LOADOUT[r].unit] += units - v.group[g][r][0];
            for (int w = 0; w < WEAPONS; w++) {
                int p = LOADOUT[r].weapon[w].product, per = loadout_per(r, w);
                want[p] += t.n[r] * per;
                need[p] += SDL_max(units * per - v.group[g][r][1 + w], 0);
            }
        }
    }
    // Tanks first: their line set M18's date, and when an ore runs short
    // the ships get what is left. Act 5: the Missile line first (the gate,
    // Texon), then the launchers and Cruisers.
    static const int ORDER[PRODUCTS] = {P_TANK, P_LASER, P_TWIN, P_MISSILE, P_HUNTER, P_SF,
                                        P_DESTROYER, P_AIRCRAFT, P_PLASMA, P_CRUISER, P_LAUNCHER};
    static const int ORDER5[PRODUCTS] = {P_MISSILE, P_LAUNCHER, P_CRUISER, P_PLASMA, P_TWIN,
                                         P_TANK, P_LASER, P_HUNTER, P_SF, P_DESTROYER, P_AIRCRAFT};
    const int *ord = pol_act() >= 5 ? ORDER5 : ORDER;
    for (int i = 0; i < PRODUCTS; i++)
        if (want[ord[i]] > 0)
            order(&v, ord[i], need[ord[i]]);
    equip_groups(&v);
    pol_new_earth(SCR_SURFACE);  // S1
    return false;  // a standing order
}

void army_goals(void)
{
    pol_goal("M8_army", PRI_M8, goal_army, 0);
}

bool army_holds(const char *group, const ArmyForce *f, bool tanks)
{
    ArmyView v = {};
    read_states(&v);
    v.groups = 1;
    v.name[0] = group ? group : army_group();
    read_group(&v, 0);
    for (int r = 0; r < ARMY_UNITS; r++) {
        if (LOADOUT[r].unit == P_TANK && !tanks)
            continue;
        if (v.group[0][r][0] < f->n[r])
            return false;
        for (int w = 0; w < WEAPONS; w++) {
            int q = LOADOUT[r].weapon[w].product, per = loadout_per(r, w);
            if (per > 0 && v.state[q] == UI_RS_DONE && v.group[0][r][1 + w] < per * f->n[r])
                return false;
        }
    }
    return true;
}
