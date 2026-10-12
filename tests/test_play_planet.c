// Planet drivers (docs/playthrough.md): from a new game a policy plays the
// first planet steps of act 1 (E1-E3, E5, E6) through clicks only, checks them on what the screens show, and the
// recorded input replays to the same end. Needs the game data.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/dseg.h"
#include "game/fixes.h"
#include "game/gfx.h"
#include "platform/config.h"
#include "platform/fs.h"
#include "play/play.h"
#include "play/ui.h"
#include "play/ui_message.h"
#include "play/ui_planet.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

enum : int {
    RESERVE = 20000,   // act 1 reserve (docs/playthrough.md, "Conventions")
    MINE_COST = 6000,  // "Cost" in the building info
    DERRICK_COST = 7000,
    E6_HOURS = 10 * 24,
};

static struct {
    bool listed_droids;
    bool stats_shown, stats_clean, stats_closed;
    int tax, mood, droids_before, droids_after, mines, derricks_wanted, derricks;
    int32_t population;
    char kind_first[20];
} seen;

// The credits on the status bar.
static bool credits_at_least(void *arg)
{
    return ds.Game_Credits >= *(int32_t *)arg;
}

static void act1(void *unused)
{
    (void)unused;
    ui_new_game();
    // E1, E2: builder and developer.
    ui_hire("BUILDERS", "William Forbs");
    ui_hire("DEVELOPERS", "Thomas Dempsey");

    // E3: tax New Earth "Very difficult".
    ui_planet_main();
    SDL_strlcpy(seen.kind_first, ui_surface_kind(), sizeof seen.kind_first);
    ui_planet_info();
    play_log("planet info: '%s', pop %d, mood %d, tax %d", ui_body_name(), ui_population(), ui_mood(),
             ui_tax_level());
    ui_set_tax(TAX_VERY_DIFFICULT);
    seen.tax = ui_tax_level();
    seen.population = ui_population();
    seen.mood = ui_mood();
    play_dump("tax");

    // The colony stats (port addition): the POP row opens them, a click on
    // them goes back. Clicked above the Food bar, so the cursor stands on
    // the bars while they are drawn; once it has moved away, no background
    // (colour F9h) may be left in the bar.
    ui_click(100, 0x48);
    ui_wait_idle(200);
    seen.stats_shown = ds.PlanetInfo_AlienMode == 4 && ds.Mouse_HotspotCount == 1;
    play_mouse(300, 190, 0);
    play_wait(5);
    seen.stats_clean = true;
    for (int x = 100; x < 116; x++)
        seen.stats_clean &= gfx_screen()[0x4D * 320 + x] != 0xF9;
    play_dump("colony-stats");
    ui_hotspot("SEE PLANET INFO");
    ui_wait_idle(200);
    seen.stats_closed = ds.PlanetInfo_AlienMode == 0 && ds.Mouse_HotspotCount == 4;

    // E5: a third mine; when it works, a droid from stock.
    ui_planet_main();
    if (ds.Game_Credits < MINE_COST + RESERVE)
        play_fail("E5: not enough credits for a mine");
    if (ui_build("Mine") != UI_BUILD_OK)
        play_fail("E5: the mine was not built (%s)", ui_last_box());
    if (!ui_wait_buildings_active("Mine", 3, 72))
        play_fail("E5: the mine did not start working");
    seen.mines = ui_buildings("Mine").active;
    seen.droids_before = ui_droids();
    if (!ui_add_droid())
        play_fail("E5: no droid added");
    seen.droids_after = ui_droids();

    // E6: 4 Derricks; short money: build what the credits allow, the rest
    // as the tax brings it in.
    seen.derricks_wanted = ui_buildings("Derrick").total + 4;
    int hours_left = E6_HOURS;
    while (ui_buildings("Derrick").total < seen.derricks_wanted) {
        int32_t need = DERRICK_COST + RESERVE;
        int spent = ui_fast_forward(credits_at_least, &need, hours_left);
        if (spent < 0)
            play_fail("E6: no money for derrick %d", ui_buildings("Derrick").total + 1);
        if (ui_build("Derrick") != UI_BUILD_OK)
            play_fail("E6: a derrick was not built (%s)", ui_last_box());
        hours_left -= spent;
    }
    if (!ui_wait_buildings_active("Derrick", seen.derricks_wanted, hours_left))
        play_fail("E6: the derricks did not start working");
    seen.derricks = ui_buildings("Derrick").active;
    ui_planet_info();
    play_log("New Earth: pop %d, mood %d, tax %d, droids listed %d", ui_population(), ui_mood(),
             ui_tax_level(), ui_body_lists("3 Miner Droid"));
    seen.listed_droids = ui_body_lists("3 Miner Droid");
    play_dump("end");
    play_quit();
}

// Driver checks for the steps that act 1 reaches only after research and
// buying (E8, E18): the game state is set up directly, so this run is no
// proof and is not replayed.
static struct {
    char refusal[128], row1[64], klatoo[16], colony_box[128];
    bool satellite, colony, under_colonization;
} drv;

static void give_satellite(void *unused)
{
    (void)unused;
    ds.Research_Products[2].Stock = 1;  // Satellite
}

static void give_control_centre(void *unused)
{
    (void)unused;
    ds.Research_Products[6].State = 5;  // Control Centre developed
    TBody *apollo = ds.Game_CurBody;
    apollo->InfoLevel = 40;
    apollo->Surveyed = 1;
    ds.Game_Credits = 400000;
}

static bool colony_box(void *arg)
{
    const char *text = ui_box_seen("colony started", *(uint64_t *)arg);
    if (text)
        SDL_strlcpy(drv.colony_box, text, sizeof drv.colony_box);
    return text != nullptr;
}

static void drivers(void *unused)
{
    (void)unused;
    ui_new_game();
    // Building without a builder: the refusal box.
    ui_planet_main();
    if (ui_build("Mine") == UI_BUILD_REFUSED)
        SDL_strlcpy(drv.refusal, ui_last_box(), sizeof drv.refusal);
    ui_command_centre();
    ui_hire("BUILDERS", "William Forbs");

    // The colony list.
    ui_planet_list("YOUR PLANETS");
    SDL_strlcpy(drv.row1, ui_planet_list_row(1), sizeof drv.row1);
    for (int row = 1; row <= 3; row++)
        play_log("your planets %d: '%s'", row, ui_planet_list_row(row));
    ui_planet_list("USEFUL PLANETS");
    for (int row = 1; row <= 3; row++)
        play_log("useful planets %d: '%s'", row, ui_planet_list_row(row));
    ui_command_centre();

    // A planet without zooming in.
    ui_open_body("Klatoo", nullptr, 8);
    SDL_strlcpy(drv.klatoo, ui_body_name(), sizeof drv.klatoo);

    // E8: a stock satellite on Apollo.
    play_test_setup(give_satellite, nullptr);
    ui_open_body("New Earth", "Apollo", 8);
    ui_log_screen();
    drv.satellite = ui_deploy_stock_satellite();
    play_dump("apollo-satellite");

    // E18: found the colony on Apollo.
    play_test_setup(give_control_centre, nullptr);
    ui_open_body("New Earth", "Apollo", 8);
    ui_log_screen();
    static const char *const starters[] = {"Builder Plant", "Windtrap", "Mine", "Building"};
    uint64_t since = play_frame();
    drv.colony = ui_found_colony(starters, 4);
    ui_planet_info();
    drv.under_colonization = ui_body_type_says("under colonization");
    play_dump("apollo-colonization");
    ui_fast_forward(colony_box, &since, 72);
    play_log("box: '%s'", drv.colony_box);
    ui_settle();
    play_quit();
}

// Fixes on the surface (game/fixes.h), run with the two switches on and
// then off: demolishing a Mine (demolish_droids) and building with 1,000
// buildings in the game (building_limit). Test setup fills the building
// table, so this run is no proof.
static struct {
    int droids_before, droids_after, stock_before, stock_after;
    bool demolished;
    int32_t credits_before, credits_after;
    int buildings;
    UiBuildResult build;
} fx;

// Copies of New Earth's command centre on top of it, up to 1,000 buildings.
static void fill_buildings(void *unused)
{
    (void)unused;
    for (int i = ds.Game_BuildingCount + 1; i <= 1000; i++)
        ds.Game_Buildings[i - 1] = ds.Game_Buildings[0];
    ds.Game_BuildingCount = 1000;
}

static void fix_drivers(void *unused)
{
    (void)unused;
    ui_new_game();
    ui_hire("BUILDERS", "William Forbs");
    ui_planet_main();
    fx.droids_before = ui_droids();
    fx.stock_before = ui_droid_stock();
    fx.demolished = ui_demolish("Mine");
    fx.droids_after = ui_droids();
    fx.stock_after = ui_droid_stock();
    play_dump("demolished");

    play_test_setup(fill_buildings, nullptr);
    fx.credits_before = ds.Game_Credits;
    fx.build = ui_build("Mine");
    fx.credits_after = ds.Game_Credits;
    fx.buildings = ds.Game_BuildingCount;
    play_dump("building-limit");
    play_quit();
}

static void check_fixes(bool on)
{
    config_set_fix("demolish_droids", on);
    config_set_fix("building_limit", on);
    fx = (typeof(fx)){};
    PlayResult f = play_run(12345, fix_drivers, nullptr, nullptr);
    printf("fixes %s: exit %d after %llu frames%s%s\n", on ? "on" : "off", f.exit_code,
           (unsigned long long)f.frames, f.failed ? ", FAILED: " : "", f.failure);
    printf("  demolished %d, droids %d -> %d, stock %d -> %d; build %d, credits %d -> %d, %d buildings\n",
           fx.demolished, fx.droids_before, fx.droids_after, fx.stock_before, fx.stock_after, fx.build,
           fx.credits_before, fx.credits_after, fx.buildings);
    CHECK(!f.failed);
    CHECK(fx.demolished);
    CHECK(fx.droids_before == 2);
    // On, the droid without a mine goes back into stock; off, it keeps working.
    CHECK(fx.droids_after == (on ? 1 : 2));
    CHECK(fx.stock_after == fx.stock_before + (on ? 1 : 0));
    // On, the full table refuses the Mine before it is paid for; off, it
    // is paid for and never built.
    CHECK(fx.buildings == 1000);
    if (on)
        CHECK(fx.build != UI_BUILD_OK && fx.credits_after == fx.credits_before);
    else
        CHECK(fx.build == UI_BUILD_OK && fx.credits_after == fx.credits_before - MINE_COST);
    config_set_fix("demolish_droids", true);
    config_set_fix("building_limit", true);
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
    char *cwd = SDL_GetCurrentDirectory();
    SDL_snprintf(record, sizeof record, "%s/play_planet.txt", cwd);
    SDL_free(cwd);

    PlayResult r = play_run(12345, act1, nullptr, record);
    printf("run: exit %d after %llu frames, hash %016llx%s%s\n", r.exit_code,
           (unsigned long long)r.frames, (unsigned long long)r.state_hash,
           r.failed ? ", FAILED: " : "", r.failure);
    CHECK(!r.failed);
    CHECK(!r.tainted);
    CHECK(r.exit_code == -1);
    printf("first kind '%s', tax %d, pop %d, mood %d, mines %d, droids %d -> %d, derricks %d/%d\n",
           seen.kind_first, seen.tax, seen.population, seen.mood, seen.mines, seen.droids_before,
           seen.droids_after, seen.derricks, seen.derricks_wanted);
    CHECK(SDL_strcmp(seen.kind_first, "Command centre") == 0);
    CHECK(seen.tax == TAX_VERY_DIFFICULT);
    CHECK(seen.population > 0);
    CHECK(seen.mines == 3);
    CHECK(seen.droids_after == seen.droids_before + 1);
    CHECK(seen.derricks == seen.derricks_wanted);
    CHECK(seen.listed_droids);
    CHECK(seen.stats_shown && seen.stats_clean && seen.stats_closed);

    char script[1100];
    SDL_snprintf(script, sizeof script, "@%s", record);
    PlayResult p = play_replay(script);
    printf("replay: exit %d after %llu frames, hash %016llx\n", p.exit_code,
           (unsigned long long)p.frames, (unsigned long long)p.state_hash);
    CHECK(p.exit_code == r.exit_code);
    CHECK(p.frames == r.frames);
    CHECK(p.state_hash == r.state_hash);

    PlayResult d = play_run(12345, drivers, nullptr, nullptr);
    printf("drivers: exit %d after %llu frames%s%s\n", d.exit_code, (unsigned long long)d.frames,
           d.failed ? ", FAILED: " : "", d.failure);
    CHECK(!d.failed);
    CHECK(d.tainted);
    printf("refusal '%s', list '%s', klatoo '%s', satellite %d, colony %d, under colonization %d, box '%s'\n",
           drv.refusal, drv.row1, drv.klatoo, drv.satellite, drv.colony, drv.under_colonization, drv.colony_box);
    CHECK(SDL_strcmp(drv.refusal, "You have no builder!") == 0);
    CHECK(SDL_strcmp(drv.row1, "Colony on Amnesty system New Earth") == 0);
    CHECK(SDL_strcmp(drv.klatoo, "Klatoo") == 0);
    CHECK(drv.satellite);
    CHECK(drv.colony);
    CHECK(drv.under_colonization);
    CHECK(SDL_strstr(drv.colony_box, "colony started") != nullptr);

    check_fixes(true);
    check_fixes(false);

    SDL_Quit();
    return ok ? 0 : 1;
}
