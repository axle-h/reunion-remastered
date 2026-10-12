// Playthrough drivers for research, production and messages
// (docs/playthrough.md): a policy plays the start of act 1 (E1, E2, E4,
// E7) through clicks only: hires the
// builder and the developer, researches Satellite, starts Miner droid, waits
// for New Earth's ores and buys two satellites, servicing the message boxes
// that pop up meanwhile. Every decision reads the screen. The recorded input
// then replays without the policy to the same end state. Needs the game data.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/alientalk.h"
#include "game/dseg.h"
#include "platform/fs.h"
#include "play/play.h"
#include "play/ui.h"
#include "play/ui_message.h"
#include "play/ui_office.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

enum : int {
    HOUR = 101,  // retraces per game hour while the clock runs (main_program.c)
    SATELLITE_TIMEOUT = 20 * HOUR,  // policy E4: 140 / exp 20 = 7 h, timeout 20 h
    ORE_TIMEOUT = 200,              // policy E7, in game hours
};

// What the policy saw, for the checks after the run.
static struct {
    int icon_satellite, icon_droid;       // icons before starting
    int percent_start, percent_mid;       // "Completed: n%" while developing
    int icon_satellite_done;
    char status_done[20], status_droid[20];
    bool ready_box;
    int32_t ores[6];
    int bought;
    UiShopQueue queue, stores;
    int32_t credits_before, credits_after;
    char last_message[UI_LOG_COLS + 1];
    int boxes;
} seen;

static void count_box(const char *text, void *unused)
{
    (void)text;
    (void)unused;
    seen.boxes++;
}

static bool satellite_ready(void *unused)
{
    (void)unused;
    UiResearchPanel p;
    if (seen.percent_mid < 0 && ui_research_panel(&p) && p.percent >= 0 && p.percent < 70)
        seen.percent_mid = p.percent;
    return ui_box_seen("The Satellite is ready for production", 0)
           && ui_research_icon("Satellite") == UI_RS_DONE;
}

static bool date_reached(void *arg)
{
    UiDate d;
    return ui_date(&d) && ui_date_hours(&d) >= *(int64_t *)arg;
}

// Satellite's ores per piece (shop info panel "Ore needed: (One piece)";
// walkthrough.md product table): Detoxin 37, Energon 31, Kremir 12, Lepitium 5.
static const int32_t SATELLITE_ORES[6] = {37, 31, 12, 5, 0, 0};

static bool ores_for(const int32_t ores[6], int n)
{
    for (int k = 0; k < 6; k++)
        if (ores[k] < SATELLITE_ORES[k] * n)
            return false;
    return true;
}

static bool stores_reached(void *arg)
{
    UiShopQueue q;
    return ui_shop_queue(&q) && q.stores >= *(int *)arg;
}

static void policy(void *unused)
{
    (void)unused;
    ui_on_box(count_box, nullptr);
    ui_new_game();
    // E1, E2.
    ui_hire("BUILDERS", "William Forbs");
    ui_hire("DEVELOPERS", "Thomas Dempsey");

    // E4: Satellite shows "Can be developed".
    ui_goto(3);
    seen.icon_satellite = ui_research_icon("Satellite");
    seen.icon_droid = ui_research_icon("Miner droid");
    play_dump("research");
    if (seen.icon_satellite != UI_RS_CAN_DEVELOP || !ui_research_start("Satellite"))
        play_fail("Satellite can't be developed (icon %d)", seen.icon_satellite);
    UiResearchPanel p;
    ui_research_panel(&p);
    seen.percent_start = p.percent;
    seen.percent_mid = -1;
    if (!ui_wait_until(satellite_ready, nullptr, SATELLITE_TIMEOUT))
        play_fail("Satellite not ready in time");
    seen.ready_box = true;
    seen.icon_satellite_done = ui_research_icon("Satellite");
    ui_research_panel(&p);
    SDL_strlcpy(seen.status_done, p.status, sizeof seen.status_done);
    play_dump("satellite-done");
    // ‖ Miner droid ("Can be analysed").
    if (!ui_research_start("Miner droid"))
        play_fail("Miner droid can't be analysed");
    ui_research_panel(&p);
    SDL_strlcpy(seen.status_droid, p.status, sizeof seen.status_droid);

    // E7: buy two satellites once New Earth has the ores, checked every
    // three game hours while idling on the message log (S13).
    UiDate start, now;
    if (!ui_date(&start))
        play_fail("no date on the status bar");
    for (;;) {
        if (!ui_shop_ores("Satellite", seen.ores))
            play_fail("can't open the Satellite order");
        play_log("ores in stock D%d E%d K%d L%d R%d T%d", seen.ores[0], seen.ores[1], seen.ores[2],
                 seen.ores[3], seen.ores[4], seen.ores[5]);
        if (ores_for(seen.ores, 2))
            break;
        ui_goto(11);
        ui_date(&now);
        if (ui_date_hours(&now) - ui_date_hours(&start) > ORE_TIMEOUT)
            play_fail("New Earth's ores never covered two satellites");
        int64_t next = ui_date_hours(&now) + 3;
        if (!ui_wait_until(date_reached, &next, 5 * HOUR))
            play_fail("the clock stopped");
    }
    ui_credits(&seen.credits_before);
    seen.bought = ui_buy("Satellite", 2);
    ui_credits(&seen.credits_after);
    ui_shop_queue(&seen.queue);
    play_dump("bought");
    int want = 2;
    if (!ui_wait_until(stores_reached, &want, 15 * HOUR))
        play_fail("the satellites were not built");
    ui_shop_queue(&seen.stores);
    play_dump("stores");

    ui_last_message(seen.last_message, sizeof seen.last_message);
    play_dump("messages");
    play_log("last message: %s", seen.last_message);
    play_quit();
}

// --- Interrupts run: driver test of the handlers in ui_message.c, with
// play_test_setup bringing the rare ones on (tainted: not a proof). ---

static struct {
    int hologram_bits, scene_bits[3];
    int screen_after_hologram, screen_after_scene[3];
    bool ready_box, jade_box, rename_jade;
    int rename_bits;
} irq;

static const int SCENES[3] = {3, 1, 9};

static bool on_screen_1(void *unused)
{
    (void)unused;
    return ui_screen() == 1;
}

// Idles on the command centre for a while and returns the screen then: a
// press that leaked into the room would have opened another one.
static int idle_on_command_centre(void)
{
    ui_wait_until(on_screen_1, nullptr, 3000);
    ui_wait_idle(3000);
    play_wait(100);
    ui_wait_idle(3000);
    return ui_screen();
}

static struct { int *bits, want; } collect;

static bool interrupts_seen(void *unused)
{
    (void)unused;
    *collect.bits |= ui_interrupts_handled();
    return (*collect.bits & collect.want) == collect.want;
}

// Waits until the interrupts `want` have been handled; *bits gets all seen.
static void collect_interrupts(int *bits, int want, int frames)
{
    ui_interrupts_handled();
    collect.bits = bits;
    collect.want = want;
    ui_wait_until(interrupts_seen, nullptr, frames);
    *bits |= ui_interrupts_handled();
}

static void jade_due(void *unused)
{
    (void)unused;
    // The day before the port reveals Jade (game/fixes.h), 22:00.
    ds.Game_Year = 2928;
    ds.Game_Month = 3;
    ds.Game_Day = 30;
    ds.Game_Hour = 22;
}

static void start_scene(void *arg)
{
    AlienTalk_StartAlienScene((int16_t)*(const int *)arg);
}

static void interrupts(void *unused)
{
    (void)unused;
    ui_new_game();
    ui_hire("DEVELOPERS", "Thomas Dempsey");
    // A research finishing on the command centre: box, then the hologram.
    if (!ui_research_start("Satellite"))
        play_fail("Satellite can't be developed");
    ui_goto(1);
    uint64_t since = play_frame();
    collect_interrupts(&irq.hologram_bits, UI_INT_BOX | UI_INT_HOLOGRAM, 12 * HOUR);
    irq.ready_box = ui_box_seen("The Satellite is ready for production", since);
    irq.screen_after_hologram = idle_on_command_centre();

    // The observatory finds Jade: box, then the rename prompt (Enter).
    ui_goto(11);
    play_test_setup(jade_due, nullptr);
    since = play_frame();
    collect_interrupts(&irq.rename_bits, UI_INT_BOX | UI_INT_RENAME, 4 * HOUR);
    irq.jade_box = ui_box_seen("Your observatory found a new planet", since);
    irq.rename_jade = ui_box_seen("new planet's name in Amnesty system? Jade", since);

    // Story scenes: one click (3), click-release-click (1), a box and no
    // click (9); each returns to the command centre.
    for (int i = 0; i < 3; i++) {
        ui_goto(11);
        play_test_setup(start_scene, (void *)&SCENES[i]);
        collect_interrupts(&irq.scene_bits[i], UI_INT_SCENE, 30 * HOUR);
        irq.screen_after_scene[i] = idle_on_command_centre();
    }
    play_quit();
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    const char *audio = SDL_getenv("PLAY_AUDIO");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, audio ? audio : "dummy");
    if (!SDL_Init(SDL_INIT_EVENTS)) {
        printf("SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    char record[1024];
    char *cwd = SDL_GetCurrentDirectory();
    SDL_snprintf(record, sizeof record, "%s/play_office.txt", cwd);
    SDL_free(cwd);

    PlayResult r = play_run(12345, policy, nullptr, record);
    printf("run: exit %d after %llu frames, hash %016llx%s%s\n", r.exit_code,
           (unsigned long long)r.frames, (unsigned long long)r.state_hash,
           r.failed ? ", FAILED: " : "", r.failure);
    CHECK(!r.failed);
    CHECK(r.exit_code == -1);

    printf("icons: Satellite %d, Miner droid %d, Satellite after %d\n", seen.icon_satellite,
           seen.icon_droid, seen.icon_satellite_done);
    CHECK(seen.icon_satellite == UI_RS_CAN_DEVELOP);
    CHECK(seen.icon_droid == UI_RS_CAN_ANALYSE);
    CHECK(seen.icon_satellite_done == UI_RS_DONE);
    printf("completed: %d%% at the start, %d%% later; done '%s', droid '%s'\n", seen.percent_start,
           seen.percent_mid, seen.status_done, seen.status_droid);
    CHECK(seen.percent_start == 100);  // counts down: the part still to do
    CHECK(seen.percent_mid >= 0 && seen.percent_mid < 70);
    CHECK(SDL_strcmp(seen.status_done, "Done") == 0);
    CHECK(SDL_strcmp(seen.status_droid, "Under analysis") == 0);
    CHECK(seen.ready_box);
    printf("ores D%d E%d K%d L%d; bought %d, queue %d (%d h, %d credits), credits %d -> %d, "
           "stores %d\n",
           seen.ores[0], seen.ores[1], seen.ores[2], seen.ores[3], seen.bought, seen.queue.queued,
           seen.queue.hours, seen.queue.price, seen.credits_before, seen.credits_after,
           seen.stores.stores);
    CHECK(ores_for(seen.ores, 2));
    CHECK(seen.bought == 2);
    CHECK(seen.queue.shown && seen.queue.queued == 2 && seen.queue.price == 2 * 780);
    CHECK(seen.credits_before - seen.credits_after == 2 * 780);
    CHECK(seen.stores.stores == 2);
    printf("boxes: %d; last message: '%s'\n", seen.boxes, seen.last_message);
    CHECK(seen.boxes >= 2);
    CHECK(SDL_strstr(seen.last_message, "ready for production") != nullptr);

    char script[1100];
    SDL_snprintf(script, sizeof script, "@%s", record);
    PlayResult q = play_replay(script);
    printf("replay: exit %d after %llu frames, hash %016llx\n", q.exit_code,
           (unsigned long long)q.frames, (unsigned long long)q.state_hash);
    CHECK(q.exit_code == r.exit_code);
    CHECK(q.frames == r.frames);
    CHECK(q.state_hash == r.state_hash);

    PlayResult t = play_run(12345, interrupts, nullptr, nullptr);
    printf("interrupts run: exit %d after %llu frames%s%s%s\n", t.exit_code,
           (unsigned long long)t.frames, t.tainted ? " (test setup)" : "",
           t.failed ? ", FAILED: " : "", t.failure);
    CHECK(!t.failed);
    printf("hologram: bits %x, ready box %d, then screen %d\n", irq.hologram_bits, irq.ready_box,
           irq.screen_after_hologram);
    CHECK((irq.hologram_bits & (UI_INT_BOX | UI_INT_HOLOGRAM)) == (UI_INT_BOX | UI_INT_HOLOGRAM));
    CHECK(irq.ready_box);
    CHECK(irq.screen_after_hologram == 1);
    printf("Jade: bits %x, box %d, rename prompt %d, planet status %d\n", irq.rename_bits,
           irq.jade_box, irq.rename_jade, ds.Galaxy_PlanetStatus[1 - 1][7 - 1]);
    CHECK((irq.rename_bits & UI_INT_RENAME) && irq.jade_box && irq.rename_jade);
    CHECK(ds.Galaxy_PlanetStatus[1 - 1][7 - 1] == 0);  // Jade (Amnesty planet 7) known
    for (int i = 0; i < 3; i++) {
        printf("scene %d: bits %x, then screen %d\n", SCENES[i], irq.scene_bits[i],
               irq.screen_after_scene[i]);
        CHECK(irq.scene_bits[i] & UI_INT_SCENE);
        CHECK(SCENES[i] != 9 || (irq.scene_bits[i] & UI_INT_BOX));  // MSG 1 first
        CHECK(irq.screen_after_scene[i] == 1);
    }

    SDL_Quit();
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
