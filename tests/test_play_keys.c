// Improvement (game/improvements.h, keyboard_play): a new game played with
// key events alone, no mouse: the title's two choices, the command centre's
// rooms in Tab order, the surface map's cells and its scrolling at the
// edge, a menu (the disk screen's slots) reached by a toolbar key, the
// clock held, the star map's moving planets, and a ground battle's field.
// Then a click-free run with the switch off: the keys reach no screen.
// Needs the game data.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/dseg.h"
#include "game/improvements.h"
#include "game/intel.h"
#include "game/groups.h"
#include "platform/fs.h"
#include "play/play.h"
#include "play/ui_message.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

enum : int {
    CMD_DISK = 1,
    CMD_OK_ATTACK = 0x0B,
    CMD_END_BATTLE = 0x41,
    CMD_ZOOM_OUT = 0x21,
};

static struct {
    bool title, centre;
    int rooms_seen, toolbar_seen;
    bool previous_back;
    bool surface, cell_step, scrolled, pointer_kept, back;
    bool disk, slot_step;
    int64_t clock_hours;
    bool star_map, followed, zoomed;
    bool space_battle, assignment, ground, selected, attack_ordered, battle_over;
    bool off_ignored;
} seen;

// --- Keys -----------------------------------------------------------------------------

static void key_event(SDL_Keycode k, SDL_Keymod mod, bool down)
{
    SDL_Event e = {.key = {.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP,
                           .key = k, .mod = mod, .down = down}};
    SDL_PushEvent(&e);
}

static void hold(SDL_Keycode k, SDL_Keymod mod, int frames)
{
    key_event(k, mod, true);
    play_wait(frames);
    key_event(k, mod, false);
    play_wait(3);
}

static void tap(SDL_Keycode k)
{
    hold(k, 0, 1);
}

static void enter(void)
{
    hold(SDLK_RETURN, 0, 2);
    play_wait(6);
}

// The pointer, as the cursor shows it.
static int px(void) { return ds.Mouse_X; }
static int py(void) { return ds.Mouse_Y; }

static bool in_rect(int x, int y, int w, int h)
{
    return px() >= x && px() < x + w && py() >= y && py() < y + h;
}

// The hotspot under the pointer (its caption is the hint line), or 0.
static int hotspot_under(void)
{
    return ds.Mouse_HotspotUnder >= 20 ? ds.Mouse_HotspotUnder - 20 : 0;
}

// The main loop waits for a click.
static bool waiting(void)
{
    int32_t before = ds.Game_FrameCount;
    play_wait(2);
    return ds.Game_FrameCount != before;
}

// Waits until screen s waits for a click, closing boxes with Click.
static bool wait_screen(int s, int timeout)
{
    for (int t = 0; t < timeout; t += 4) {
        UiBox box;
        if (ui_box(&box)) {
            enter();
            continue;
        }
        if (ds.Game_CurrentScreen == s && waiting())
            return true;
        play_wait(2);
    }
    return false;
}

// Closes boxes until the main loop waits for a click.
static void settle(void)
{
    wait_screen(ds.Game_CurrentScreen, 2000);
}

// Tab until pred holds, at least once and at most n times.
static bool tab_to(bool (*pred)(void), int n)
{
    for (int i = 0; i < n; i++) {
        tap(SDLK_TAB);
        if (pred())
            return true;
    }
    return false;
}

static int16_t slot_command(int slot)
{
    return ds.GfxPath_MenuBar[slot - 1].Command;
}

// Presses the toolbar key showing command c: 1..6, after 7 for the other
// page. False if neither page has it.
static bool toolbar_key(int16_t c)
{
    settle();
    for (int turn = 0; turn < 2; turn++) {
        for (int h = 1; h <= 6; h++) {
            int slot = ds.GfxPath_PageFlagB == 0 ? h + 6 : h;
            if (slot_command(slot) == c) {
                tap(SDLK_1 + h - 1);
                play_wait(10);
                return true;
            }
        }
        int page = ds.GfxPath_PageFlagB;
        tap(SDLK_7);
        for (int i = 0; i < 200 && ds.GfxPath_PageFlagB == page; i++)
            play_wait(1);
        settle();
    }
    return false;
}

// --- The run --------------------------------------------------------------------------

static void switch_on(void *arg)
{
    improvements.keyboard_play = *(bool *)arg;
}

static bool on_new_game(void) { return in_rect(0x4B, 0x30, 0xB0, 0x1C); }
static bool on_map(void) { return in_rect(0x5D, 0x35, 0xE0, 0x90); }
static bool on_slots(void) { return in_rect(0x0B, 0x40, 0x82, 0x6C); }
static bool on_planet(void) { return ds.Game_CurrentScreen == 7 && hotspot_under() >= 0x14; }
static bool on_field(void) { return in_rect(0x40, 0x34, 0x100, 0x90); }

static void title(void)
{
    play_wait(40);
    tab_to(on_new_game, 4);
    enter();
    play_wait(80);
    tap(SDLK_TAB);  // a hero
    enter();
    seen.title = true;
    seen.centre = wait_screen(1, 3000);
}

static void command_centre(void)
{
    unsigned rooms = 0;
    bool toolbar = false;
    for (int i = 0; i < 24; i++) {
        tap(SDLK_TAB);
        int j = hotspot_under();
        if (j >= 5 && j <= 12)
            rooms |= 1u << j;
        toolbar |= py() < 0x21;
    }
    seen.rooms_seen = __builtin_popcount(rooms);
    seen.toolbar_seen = toolbar;
    int a = hotspot_under();
    tap(SDLK_TAB);
    hold(SDLK_TAB, SDL_KMOD_LSHIFT, 1);
    seen.previous_back = hotspot_under() == a;
}

static void surface(void)
{
    settle();
    hold(SDLK_F4, 0, 1);  // PLANET MAIN
    seen.surface = wait_screen(20, 2000);
    if (!seen.surface)
        return;
    tab_to(on_map, 20);
    int x = px();
    tap(SDLK_RIGHT);
    seen.cell_step = on_map() && px() == x + 16;
    // To the right edge, then once more: the view moves a column.
    while (on_map() && px() < 0x5D + 13 * 16)
        tap(SDLK_RIGHT);
    int scroll = ds.Surface_ScrollX, ex = px(), ey = py();
    tap(SDLK_RIGHT);
    play_wait(20);
    seen.scrolled = ds.Surface_ScrollX == scroll + 1;
    seen.pointer_kept = px() == ex && py() == ey;
    hold(SDLK_ESCAPE, 0, 1);
    seen.back = wait_screen(1, 2000);
}

static void disk(void)
{
    if (!toolbar_key(CMD_DISK))
        return;
    seen.disk = wait_screen(12, 2000);
    if (!seen.disk)
        return;
    tab_to(on_slots, 20);
    int y = py();
    tap(SDLK_DOWN);
    seen.slot_step = on_slots() && py() == y + 9;
    hold(SDLK_ESCAPE, 0, 1);
    wait_screen(1, 2000);
}

static void clock(void)
{
    int64_t before = ui_clock_hours();
    hold(SDLK_H, 0, 200);
    play_wait(20);
    seen.clock_hours = ui_clock_hours() - before;
}

static void star_map(void)
{
    settle();
    hold(SDLK_F3, 0, 1);
    seen.star_map = wait_screen(7, 2000);
    play_dump("star-map");
    if (!seen.star_map || !tab_to(on_planet, 30))
        return;
    int j = hotspot_under();
    int x = px(), y = py();
    play_wait(200);  // it moves on its orbit, and the pointer with it
    seen.followed = hotspot_under() == j && (px() != x || py() != y);
    enter();
    play_wait(30);
    for (int slot = 1; slot <= 12; slot++)
        seen.zoomed |= slot_command(slot) == CMD_ZOOM_OUT;
    hold(SDLK_F2, 0, 1);
    wait_screen(1, 2000);
}

// The Morgruls' first wave lands on New Earth, which has no ships and ten
// tanks (tests/test_play_battle.c, "wave1-ground").
static void attack(void *unused)
{
    (void)unused;
    Intel_Race(3)->Status = 2;
    TRaceForce *fc = Intel_Force(3, 1);
    fc->System = 1;
    fc->Body = 5;
    fc->Moon = 0;
    fc->Active = 1;
    fc->TravelTime = 0;
    fc->Mission = 2;
    fc->NextMission = 2;
    fc->Timer = 1;
    TGroup *g = Groups_At(1, 1);
    g->Units[1][1].Count = 10;
    g->Units[1][1].Weapons[0] = 30;
    g->Units[1][1].Weapons[1] = 10;
}

// Arrows to the cell of the stack at (x, y), the top left of its sprite.
static void arrows_to(int x, int y)
{
    for (int i = 0; i < 40 && on_field(); i++) {
        int c = (px() - 0x40) / 16, r = (py() - 0x34) / 16;
        int tc = (x + 8 - 0x40) / 16, tr = (y + 8 - 0x34) / 16;
        if (c == tc && r == tr)
            return;
        tap(c < tc ? SDLK_RIGHT : c > tc ? SDLK_LEFT : r < tr ? SDLK_DOWN : SDLK_UP);
    }
}

static const uint8_t *unit(int side, int i)
{
    return ds.GrWarUnits_UnitLists[side - 1] + (i - 1) * 0x27;
}

static void battle(void)
{
    play_test_setup(attack, nullptr);
    settle();
    hold(SDLK_F10, 0, 1);  // the message log, with the clock running
    seen.space_battle = wait_screen(29, 4000);
    if (!seen.space_battle)
        return;
    for (int i = 0; i < 400 && !ds.GrWar_BattleOver; i++) {
        bool over = false;
        for (int slot = 1; slot <= 12; slot++)
            over |= slot_command(slot) == CMD_END_BATTLE;
        if (over)
            break;
        play_wait(10);
    }
    toolbar_key(CMD_END_BATTLE);
    seen.assignment = wait_screen(19, 3000);
    if (!seen.assignment || !toolbar_key(CMD_OK_ATTACK))
        return;
    seen.ground = wait_screen(32, 3000);
    if (!seen.ground || !tab_to(on_field, 20))
        return;
    // The first own stack: select it, Attack, the first enemy stack.
    for (int i = 1; i <= ds.GrWarUnits_AttackerSlotCount && !seen.selected; i++) {
        const uint8_t *u = unit(1, i);
        int16_t count;
        SDL_memcpy(&count, u + 1, 2);
        if (count <= 0)
            continue;
        arrows_to(0x40 + u[4] * 16, 0x34 + u[5] * 16);
        enter();
        seen.selected = ds.GrWar_Selected == i && ds.GrWar_SelectedOwn;
    }
    if (!seen.selected)
        return;
    tap(SDLK_TAB);
    while (hotspot_under() != 3 && py() < 0xC8)
        tap(SDLK_TAB);
    enter();
    seen.attack_ordered = ds.GrWar_Order == 4;
    const uint8_t *e = unit(2, 1);
    tab_to(on_field, 20);
    arrows_to(0x40 + e[4] * 16, 0x34 + e[5] * 16);
    enter();
    for (int i = 0; i < 600 && !ds.GrWar_BattleOver; i++)
        play_wait(10);
    seen.battle_over = ds.GrWar_BattleOver != 0;
    play_wait(100);
    toolbar_key(CMD_END_BATTLE);
    play_wait(100);
}

static void keys(void *unused)
{
    (void)unused;
    bool on = true;
    play_test_setup(switch_on, &on);
    title();
    if (!seen.centre)
        play_fail("no command centre");
    command_centre();
    surface();
    disk();
    clock();
    star_map();
    battle();
    play_quit();
}

// With the switch off the keys do nothing: the start menu waits.
static void keys_off(void *unused)
{
    (void)unused;
    play_wait(40);
    int x = px(), y = py();
    tap(SDLK_TAB);
    enter();
    play_wait(80);
    seen.off_ignored = px() == x && py() == y && ds.Game_StartChoice == 0;
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
    PlayResult r = play_run(12345, keys, nullptr, nullptr);
    printf("keys: exit %d after %llu frames%s%s\n", r.exit_code, (unsigned long long)r.frames,
           r.failed ? ", FAILED: " : "", r.failure);
    printf("title %d centre %d, rooms %d toolbar %d previous %d; surface %d step %d scrolled %d kept %d "
           "back %d; disk %d step %d; clock %lld h; star map %d followed %d zoomed %d; space %d "
           "assignment %d ground %d selected %d attack %d over %d\n",
           seen.title, seen.centre, seen.rooms_seen, seen.toolbar_seen, seen.previous_back, seen.surface,
           seen.cell_step, seen.scrolled, seen.pointer_kept, seen.back, seen.disk, seen.slot_step,
           (long long)seen.clock_hours, seen.star_map, seen.followed, seen.zoomed, seen.space_battle,
           seen.assignment, seen.ground, seen.selected, seen.attack_ordered, seen.battle_over);
    CHECK(!r.failed);
    CHECK(seen.centre);
    CHECK(seen.rooms_seen == 8);
    CHECK(seen.toolbar_seen);
    CHECK(seen.previous_back);
    CHECK(seen.surface && seen.cell_step && seen.scrolled && seen.pointer_kept && seen.back);
    CHECK(seen.disk && seen.slot_step);
    CHECK(seen.clock_hours >= 20);
    CHECK(seen.star_map && seen.followed && seen.zoomed);
    CHECK(seen.space_battle && seen.assignment && seen.ground);
    CHECK(seen.selected && seen.attack_ordered && seen.battle_over);

    PlayResult o = play_run(12345, keys_off, nullptr, nullptr);
    CHECK(!o.failed);
    CHECK(seen.off_ignored);

    SDL_Quit();
    return ok ? 0 : 1;
}
