// menu: the port menu's model driven by key events, without a window:
// sections and rows, changes to the config, requests to the game, the
// fixes' rows, and Restart / Quit.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "platform/config.h"
#include "platform/input.h"
#include "platform/menu.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

enum { SEC_CHEATS, SEC_SOUND, SEC_VIDEO, SEC_GAME, SEC_FIXES, SEC_RESTART, SEC_QUIT };

static int changes[8];
static bool running;

static void changed(MenuChange c)
{
    changes[c]++;
}

static bool game_running(void)
{
    return running;
}

static void press(SDL_Keycode key)
{
    SDL_Event e = {.key = {.type = SDL_EVENT_KEY_DOWN, .key = key, .down = true}};
    menu_handle_event(&e);
}

// To a section's first row.
static void go_to(int section)
{
    while (menu_section() != section)
        press(SDLK_TAB);
    if (menu_focus() < 0)
        press(SDLK_RIGHT);
    while (menu_focus() > 0)
        press(SDLK_UP);
}

static bool row_is(int row, const char *text)
{
    bool same = SDL_strcmp(menu_row_text(row), text) == 0;
    if (!same)
        printf("row %d: \"%s\", expected \"%s\"\n", row, menu_row_text(row), text);
    return same;
}

int main(void)
{
    static const MenuFix fixes[] = {
        {"find_jade", "Jade found in time", "help"},
        {"mine_all_systems", "All systems mine", "help"},
    };
    MenuHooks hooks = {
        .game_running = game_running,
        .changed = changed,
        .add_money = 100000,
        .add_ore = 10000,
        .fixes = fixes,
        .fix_count = 2,
    };
    menu_set_hooks(&hooks);
    config_reset();
    config.music = CONFIG_MUSIC_SOUND_BLASTER;

    menu_open();
    CHECK(menu_is_open() && menu_section() == SEC_CHEATS && menu_focus() == -1);
    CHECK(menu_row_count() == 4);
    CHECK(row_is(0, "[ ] Freeze money"));
    CHECK(row_is(2, "> Add 100000 credits"));

    // Cheats: a toggle changes the config and asks the game; the add rows
    // only work during a game.
    press(SDLK_RIGHT);
    CHECK(menu_focus() == 0);
    press(SDLK_RETURN);
    CHECK(config.freeze_money && changes[MENU_CHANGED_CHEATS] == 1);
    CHECK(row_is(0, "[x] Freeze money"));
    press(SDLK_DOWN);
    press(SDLK_DOWN);
    press(SDLK_RETURN);
    CHECK(changes[MENU_ADD_MONEY] == 0);
    running = true;
    press(SDLK_RETURN);
    press(SDLK_RETURN);
    CHECK(changes[MENU_ADD_MONEY] == 2);

    // Sound: the music cycles None, Sound Blaster, UltraSound; volumes step 10.
    go_to(SEC_SOUND);
    CHECK(row_is(0, "Music <Sound Blaster>"));
    press(SDLK_RIGHT);
    CHECK(config.music == CONFIG_MUSIC_GUS && changes[MENU_CHANGED_MUSIC] == 1);
    CHECK(row_is(0, "Music <UltraSound>"));
    press(SDLK_RIGHT);
    CHECK(config.music == CONFIG_MUSIC_NONE);
    press(SDLK_LEFT);
    CHECK(config.music == CONFIG_MUSIC_GUS);
    press(SDLK_DOWN);
    press(SDLK_RIGHT);  // already at 100%
    press(SDLK_LEFT);
    press(SDLK_LEFT);
    CHECK(config.music_volume == 80 && row_is(1, "Music volume <80%>"));
    press(SDLK_DOWN);
    press(SDLK_SPACE);
    CHECK(!config.effects && row_is(2, "[ ] Sound effects"));

    // Video: Desktop has no size row.
    go_to(SEC_VIDEO);
    CHECK(menu_row_count() == 2 && row_is(1, "Size <1280x960>"));
    press(SDLK_LEFT);
    CHECK(config.display == CONFIG_DESKTOP && menu_row_count() == 1);
    press(SDLK_RIGHT);
    CHECK(config.display == CONFIG_WINDOWED);

    // Game: the colony stats are on by default.
    go_to(SEC_GAME);
    press(SDLK_RETURN);
    CHECK(config.skip_intro);
    CHECK(menu_row_count() == 2 && row_is(1, "[x] Colony stats"));
    press(SDLK_DOWN);
    press(SDLK_RETURN);
    CHECK(!config.colony_stats && row_is(1, "[ ] Colony stats"));
    press(SDLK_RETURN);
    CHECK(config.colony_stats);

    // Fixes: a row per fix, all on; one off, then all on again.
    go_to(SEC_FIXES);
    CHECK(menu_row_count() == 4);
    CHECK(row_is(2, "[x] Jade found in time") && row_is(3, "[x] All systems mine"));
    press(SDLK_DOWN);
    press(SDLK_DOWN);
    press(SDLK_RETURN);
    CHECK(!config_fix("find_jade") && config_fix("mine_all_systems"));
    CHECK(changes[MENU_CHANGED_FIXES] == 1);
    press(SDLK_UP);
    press(SDLK_RETURN);  // All off (original)
    CHECK(!config_fix("find_jade") && !config_fix("mine_all_systems"));
    press(SDLK_UP);
    press(SDLK_RETURN);  // All on
    CHECK(config_fix("find_jade") && config_fix("mine_all_systems"));

    // Tab moves between sections and keeps the focus on the rows.
    press(SDLK_TAB);
    CHECK(menu_section() == SEC_RESTART && menu_focus() == 0);
    SDL_Event shift_tab = {.key = {.type = SDL_EVENT_KEY_DOWN, .key = SDLK_TAB, .mod = SDL_KMOD_SHIFT, .down = true}};
    menu_handle_event(&shift_tab);
    CHECK(menu_section() == SEC_FIXES);

    // Escape closes.
    press(SDLK_ESCAPE);
    CHECK(!menu_is_open());

    // Restart and Quit close the menu with their request.
    menu_open();
    go_to(SEC_RESTART);
    press(SDLK_RETURN);
    CHECK(!menu_is_open() && input_restart_requested() && !input_quit_requested());
    input_reset();
    menu_open();
    go_to(SEC_QUIT);
    press(SDLK_RETURN);
    CHECK(!menu_is_open() && input_quit_requested() && !input_restart_requested());
    input_reset();

    // Closing the window while it is open quits too.
    menu_open();
    SDL_Event quit = {.type = SDL_EVENT_QUIT};
    menu_handle_event(&quit);
    CHECK(!menu_is_open() && input_quit_requested());

    config_reset();
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
