// Playthrough drivers for ships, groups, cargo and travel (thread B3), each
// used the way Act 1 of docs/playthrough-policy.md needs them. Needs the
// game data.
//
// 1. Proof run (seed 12345, input only, replayed): from a new game, hire
//    Dempsey and Jack Benson, research Satellite, buy two, deploy them on
//    Apollo and Klatoo (E4, E7, E8), research Satellite carrier when it is
//    invented (E10), buy carriers and satellites, form the carrier group
//    "Survey 1", equip it, take off, fly to Zeus (docking there is refused:
//    carriers land only on New Earth) and on to its moon Penelope, deploy a
//    satellite there, fly home and dock (E16). Jack is hired at once, not at
//    E14: without a pilot a carrier flies 1 leg an hour (80-199 h a hop).
// 2. Driver test (play_test_setup, not a proof): a Transfer ship and a Miner
//    station are put in stock (by play they need E11-E13 and ~1,700 t of
//    Detoxin: weeks of game time and other threads' screens); then by input:
//    a Trade group with the ship and the station, ore loaded on screen 13,
//    Move refused without a pilot, a pilot hired, a flight to Apollo and
//    back, docking and unloading.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/dseg.h"
#include "platform/fs.h"
#include "play/play.h"
#include "play/ui.h"
#include "play/ui_message.h"
#include "play/ui_office.h"
#include "play/ui_planet.h"
#include "play/ui_ships.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

enum : int {
    SCR_COMMAND = 1,
    SCR_RESEARCH = 3,
    SCR_PLANET_INFO = 8,
    SCR_MESSAGES = 11,
    SCR_GROUP = 17,
};

// --- Research, shop and satellites (ui_office.h, ui_planet.h) -----------------------

// The research screen shows the product's icon (it appears there when the
// product is invented).
static bool research_offered(void *name)
{
    return ui_research_icon(name) != UI_RS_UNKNOWN;
}

// The box "The <name> is ready for production", in the message log.
static bool research_done(void *name)
{
    char text[64];
    SDL_snprintf(text, sizeof text, "The %s is ready for production", (const char *)name);
    return ui_log_has(text);
}

// The shop panel's "Stores: n" for the product selected (redrawn when a unit
// is finished) reaches *n.
static bool stores_reach(void *n)
{
    UiShopQueue q;
    return ui_shop_queue(&q) && q.stores >= *(int *)n;
}

// Buys until `n` are in stock, retrying once a game day when credits or ore
// cut the order (policy §0 short-money fallback). Waits on the shop screen.
static void buy_stock(const char *name, int n, int hours)
{
    int64_t end = ui_clock_hours() + hours;
    for (;;) {
        UiShopQueue q;
        if (!ui_shop_select(name) || !ui_shop_queue(&q))
            play_fail("buy_stock: no %s in the shop", name);
        int queued = q.stores + (q.shown ? q.queued : 0);
        if (queued < n && ui_buy(name, n - queued) < 0)
            play_fail("buy_stock: %s can't be bought", name);
        int day = 24;
        if (ui_wait_hours(stores_reach, &n, day))
            return;
        if (ui_clock_hours() >= end) {
            ui_shop_queue(&q);
            play_fail("buy_stock: %s stores %d of %d", name, q.stores, n);
        }
    }
}

// The stock satellite on `planet`, or on its moon `moon`, from planet info.
static void deploy_stock_satellite(const char *planet, const char *moon)
{
    ui_open_body(planet, moon, SCR_PLANET_INFO);
    if (!ui_deploy_stock_satellite())
        play_fail("deploy: no satellite on %s", moon ? moon : planet);
    play_log("stock satellite deployed on %s", moon ? moon : planet);
}

static bool log_has(void *text)
{
    return ui_log_has(text);
}

// The idle screen of the policy (§0): messages, clock running.
static void idle_screen(void)
{
    ui_open_screen("MESSAGES", SCR_MESSAGES);
}

// --- 1. Proof run ----------------------------------------------------------------------

static struct {
    int carriers, satellites, left;  // Survey 1's grid
    bool zeus_dock_refused;
    char zeus_refusal[400];
    int status_home;
    bool penelope_logged;
} proof;

static void wait_or_fail(bool (*pred)(void *), void *arg, int hours, const char *what)
{
    if (!ui_wait_hours(pred, arg, hours))
        play_fail("timeout: %s", what);
    play_log("seen: %s", what);
}

static void proof_policy(void *unused)
{
    (void)unused;
    ui_new_game();
    // E2, and E14 early: every group, carriers too, moves pilot experience
    // + 1 legs an hour, so without a pilot a carrier takes 80-199 h a hop.
    ui_hire("DEVELOPERS", "Thomas Dempsey");
    ui_hire("PILOTS", "Jack Benson");

    // E4: Satellite.
    if (!ui_research_start("Satellite"))
        play_fail("Satellite not researched");
    idle_screen();
    wait_or_fail(research_done, "Satellite", 30, "Satellite ready for production");

    // E7, E8.
    buy_stock("Satellite", 2, 240);
    deploy_stock_satellite("New Earth", "Apollo");
    deploy_stock_satellite("Klatoo", nullptr);
    idle_screen();
    wait_or_fail(log_has, "satellites were destroyed", 40, "MSG 1 (Klatoo satellite lost)");

    // E10: Satellite carrier, once invented (KIT 4).
    ui_open_screen("RESEARCH-DESIGN", SCR_RESEARCH);
    wait_or_fail(research_offered, "Sat.carrier", 60, "Sat.carrier offered");
    if (!ui_research_start("Sat.carrier"))
        play_fail("Sat.carrier not researched");
    idle_screen();
    wait_or_fail(research_done, "Sat.carrier", 60, "Sat.carrier ready for production");

    // E16: "Survey 1", 2 carriers with one satellite each.
    buy_stock("Sat.carrier", 2, 240);
    buy_stock("Satellite", 2, 240);
    if (!ui_new_group("Carrier", "Survey 1"))
        play_fail("no Carrier type");
    if (!ui_equip("Survey 1", "Sat.carrier", nullptr, 2)
        || !ui_equip("Survey 1", "Sat.carrier", "Satellite", 2))
        play_fail("Survey 1 not equipped");
    proof.carriers = ui_group_units("Survey 1", "Sat.carrier", nullptr);
    proof.satellites = ui_group_units("Survey 1", "Sat.carrier", "Satellite");
    play_dump("equipped");
    if (!ui_take_off("Survey 1"))
        play_fail("Survey 1 did not take off");
    // Zeus first: arriving there reveals its moons.
    if (!ui_launch("Survey 1", nullptr, "Zeus", nullptr))
        play_fail("Survey 1 not launched to Zeus");
    if (!ui_wait_arrival("Survey 1", 24))
        play_fail("Survey 1 did not reach Zeus");
    // A refusal: carriers dock only at New Earth.
    proof.zeus_dock_refused = !ui_dock("Survey 1");
    SDL_strlcpy(proof.zeus_refusal, ui_last_box(), sizeof proof.zeus_refusal);
    if (!ui_launch("Survey 1", nullptr, "Zeus", "Penelope"))
        play_fail("Survey 1 not launched to Penelope");
    if (!ui_wait_arrival("Survey 1", 24))
        play_fail("Survey 1 did not reach Penelope");
    proof.penelope_logged = ui_log_has("Survey 1 arrived to Amnesty Zeus Penelope");
    if (!ui_deploy_carrier_satellite("Survey 1"))
        play_fail("no satellite deployed on Penelope");
    proof.left = ui_group_units("Survey 1", "Sat.carrier", "Satellite");
    // Home: carriers dock only at New Earth.
    if (!ui_launch("Survey 1", nullptr, "New Earth", nullptr))
        play_fail("Survey 1 not launched home");
    if (!ui_wait_arrival("Survey 1", 24) || !ui_dock("Survey 1"))
        play_fail("Survey 1 not docked at New Earth");
    proof.status_home = ui_group_status("Survey 1");
    ui_ship_info();
    play_dump("end");
    play_quit();
}

// --- 2. Trade group and cargo (test setup) ----------------------------------------

// Test setup only: the product record to change.
static TProduct *product(const char *name)
{
    for (int k = 0; k < 35; k++) {
        TProduct *p = &ds.Research_Products[k];
        if (p->Name.len == SDL_strlen(name) && SDL_strncmp(p->Name.s, name, p->Name.len) == 0)
            return p;
    }
    play_fail("no product %s", name);
}

static void grant_transfer_ship(void *unused)
{
    (void)unused;
    // As if E11 and E13 were done by play: Transfer ship and Miner station
    // researched, one of each bought; New Earth has some ore.
    product("Transfer ship")->State = 5;
    product("Transfer ship")->Stock = 1;
    product("Miner station")->State = 5;
    product("Miner station")->Stock = 1;
    ds.Game_GroupsEnabled = 1;
    ds.Game_GroupKindAllowed[1] = 1;  // Trade
    ds.Game_HomeStock[0] = 300;       // Detoxin
    ds.Game_HomeStock[1] = 1000;      // Energon
}

static struct {
    int ships, stations;
    bool no_pilot_box;
    int32_t loaded_energon, planet_detoxin, loaded_detoxin, used, cap;
    int32_t at_apollo;
    char apollo[128];
    int32_t unloaded, ship_energon_end, planet_energon_end;
    int status_end;
} trade;

static void trade_policy(void *unused)
{
    (void)unused;
    ui_new_game();
    play_test_setup(grant_transfer_ship, nullptr);

    // E15 as the code has it: the Miner station rides in the trade group's
    // grid ("Mine" column), not as cargo.
    if (!ui_new_group("Trade", "Trade 1"))
        play_fail("no Trade type");
    ui_equip("Trade 1", "Transfer ship", nullptr, 1);
    ui_equip("Trade 1", "Transfer ship", "Miner station", 1);
    trade.ships = ui_group_units("Trade 1", "Transfer ship", nullptr);
    trade.stations = ui_group_units("Trade 1", "Transfer ship", "Miner station");

    // Cargo at New Earth: 100 t of Energon, then all the Detoxin that fits.
    trade.loaded_energon = ui_cargo("Trade 1", "Energon", 100);
    trade.planet_detoxin = ui_cargo_planet("Detoxin");
    trade.loaded_detoxin = ui_cargo("Trade 1", "Detoxin", SHIP_CARGO_ALL);
    int32_t pcap;
    ui_cargo_space(&trade.used, &trade.cap, &pcap);
    play_dump("cargo");

    // No pilot yet: Move is refused with a box.
    if (!ui_take_off("Trade 1"))
        play_fail("Trade 1 did not take off");
    ui_open_group("Trade 1", SCR_GROUP);
    uint64_t since = play_frame();
    ui_hotspot("Move");
    ui_settle();  // closes the box
    trade.no_pilot_box = ui_box_seen("You haven't got a pilot!", since) != nullptr;
    ui_open_screen("BACK TO M.SCREEN", SCR_COMMAND);
    ui_hire("PILOTS", "Jack Benson");

    // To Apollo and back; Apollo is no colony, so no transfer there.
    if (!ui_launch("Trade 1", nullptr, "New Earth", "Apollo"))
        play_fail("Trade 1 not launched to Apollo");
    if (!ui_wait_arrival("Trade 1", 24))
        play_fail("Trade 1 did not reach Apollo");
    ui_group_location("Trade 1", trade.apollo, sizeof trade.apollo);
    if (!ui_dock("Trade 1"))
        play_fail("Trade 1 did not dock at Apollo");
    trade.at_apollo = ui_cargo("Trade 1", "Energon", -SHIP_CARGO_ALL);
    if (!ui_take_off("Trade 1") || !ui_launch("Trade 1", nullptr, "New Earth", nullptr))
        play_fail("Trade 1 not launched home");
    if (!ui_wait_arrival("Trade 1", 24) || !ui_dock("Trade 1"))
        play_fail("Trade 1 not docked at New Earth");
    trade.unloaded = ui_cargo("Trade 1", "Energon", -SHIP_CARGO_ALL);
    trade.unloaded += ui_cargo("Trade 1", "Detoxin", -SHIP_CARGO_ALL);
    trade.ship_energon_end = ui_cargo_ship("Energon");
    trade.planet_energon_end = ui_cargo_planet("Energon");
    trade.status_end = ui_group_status("Trade 1");
    play_dump("unloaded");
    play_quit();
}

static PlayResult show(const char *what, PlayResult r)
{
    printf("%s: exit %d after %llu frames, hash %016llx%s%s%s\n", what, r.exit_code,
           (unsigned long long)r.frames, (unsigned long long)r.state_hash, r.tainted ? ", tainted" : "",
           r.failed ? ", FAILED: " : "", r.failure);
    return r;
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

    char record[1024];
    SDL_snprintf(record, sizeof record, "%s/play_ships.txt", SDL_GetCurrentDirectory());
    PlayResult r = show("proof run", play_run(12345, proof_policy, nullptr, record));
    CHECK(!r.failed && !r.tainted);
    CHECK(r.exit_code == -1);
    printf("Survey 1: %d carriers, %d satellites, %d left after Penelope, status %d at home\n",
           proof.carriers, proof.satellites, proof.left, proof.status_home);
    printf("dock at Zeus: %s ('%s')\n", proof.zeus_dock_refused ? "refused" : "allowed", proof.zeus_refusal);
    CHECK(proof.carriers == 2 && proof.satellites == 2 && proof.left == 1);
    CHECK(proof.zeus_dock_refused && SDL_strstr(proof.zeus_refusal, "can only land"));
    CHECK(proof.penelope_logged);
    CHECK(proof.status_home == SHIP_DOCKED);

    char script[1100];
    SDL_snprintf(script, sizeof script, "@%s", record);
    PlayResult p = show("replay", play_replay(script));
    CHECK(p.exit_code == r.exit_code);
    CHECK(p.frames == r.frames);
    CHECK(p.state_hash == r.state_hash);

    PlayResult t = show("trade run", play_run(12345, trade_policy, nullptr, nullptr));
    CHECK(!t.failed && t.tainted);
    printf("Trade 1: %d ships, %d stations; loaded %d Energon, %d Detoxin, space %d/%d\n", trade.ships,
           trade.stations, trade.loaded_energon, trade.loaded_detoxin, trade.used, trade.cap);
    printf("at Apollo: '%s', moved %d; home: unloaded %d, ship %d / planet %d Energon, status %d\n",
           trade.apollo, trade.at_apollo, trade.unloaded, trade.ship_energon_end,
           trade.planet_energon_end, trade.status_end);
    CHECK(trade.ships == 1 && trade.stations == 1);
    CHECK(trade.no_pilot_box);
    CHECK(trade.loaded_energon == 100);
    CHECK(trade.planet_detoxin >= 300);  // granted, plus what the Derrick mined
    CHECK(trade.loaded_detoxin == SDL_min(trade.planet_detoxin, 400));
    CHECK(trade.used == 100 + trade.loaded_detoxin && trade.cap == 500);
    CHECK(SDL_strcmp(trade.apollo, "In orbit Amnesty system New Earth Apollo") == 0);
    CHECK(trade.at_apollo == 0);
    CHECK(trade.unloaded == trade.used && trade.ship_energon_end == 0);
    CHECK(trade.status_end == SHIP_DOCKED);

    SDL_Quit();
    return ok ? 0 : 1;
}
