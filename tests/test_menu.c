// menu: the port menu's model driven by key events, without a window:
// sections and rows, changes to the config, requests to the game, the
// languages, the improvements' and the fixes' rows, and the System
// section's Restart / Quit.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "platform/config.h"
#include "platform/input.h"
#include "platform/menu.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

enum { SEC_CHEATS, SEC_SOUND, SEC_VIDEO, SEC_GAME, SEC_IMPROVEMENTS, SEC_FIXES, SEC_SYSTEM };

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

static unsigned langs;

static unsigned languages(void)
{
    return langs;
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
    static const MenuSwitch fixes[] = {
        {"find_jade", "Jade found in time", "help", false},
        {"mine_all_systems", "All systems mine", "help", false},
    };
    static const MenuSwitch improvements[] = {
        {"more_stations", "More Miner stations", "help", false},
        {"trade_routes", "Trade routes", "help", true},
    };
    MenuHooks hooks = {
        .game_running = game_running,
        .changed = changed,
        .add_money = 100000,
        .add_ore = 10000,
        .fixes = fixes,
        .fix_count = 2,
        .improvements = improvements,
        .improvement_count = 2,
        .languages = languages,
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

    // Game: the colony stats and the Detoxin rate are on by default.
    go_to(SEC_GAME);
    press(SDLK_RETURN);
    CHECK(config.skip_intro);
    CHECK(menu_row_count() == 4 && row_is(1, "[x] Colony stats") && row_is(2, "[x] Detoxin rate"));
    press(SDLK_DOWN);
    press(SDLK_RETURN);
    CHECK(!config.colony_stats && row_is(1, "[ ] Colony stats"));
    press(SDLK_RETURN);
    CHECK(config.colony_stats);
    press(SDLK_DOWN);
    press(SDLK_RETURN);
    CHECK(!config.detoxin_rate && row_is(2, "[ ] Detoxin rate"));
    press(SDLK_RETURN);
    CHECK(config.detoxin_rate);

    // Language: only the game's own (English) until the data has more;
    // then it steps through those, both ways. One the data lacks shows the
    // one the game falls back on.
    press(SDLK_DOWN);
    CHECK(row_is(3, "Language <English>"));
    press(SDLK_RIGHT);
    CHECK(config.language == LANGUAGE_ENGLISH);
    langs = IN_ENGLISH | IN_GERMAN | IN_ITALIAN;
    press(SDLK_RIGHT);
    CHECK(config.language == LANGUAGE_GERMAN && row_is(3, "Language <German>"));
    press(SDLK_RIGHT);
    CHECK(config.language == LANGUAGE_ITALIAN);
    press(SDLK_RIGHT);
    CHECK(config.language == LANGUAGE_ENGLISH);
    press(SDLK_LEFT);
    CHECK(config.language == LANGUAGE_ITALIAN);
    langs = IN_GERMAN | IN_FRENCH;
    CHECK(row_is(3, "Language <German>"));
    press(SDLK_RIGHT);
    CHECK(config.language == LANGUAGE_FRENCH);
    langs = 0;
    config.language = LANGUAGE_ENGLISH;

    // Improvements: a row each, on; off and on again, all off and all on.
    // While Trade routes is on, two rows say that saves won't load in DOS.
    go_to(SEC_IMPROVEMENTS);
    CHECK(menu_row_count() == 6 && row_is(2, "[x] More Miner stations") && row_is(3, "[x] Trade routes"));
    CHECK(row_is(4, "Saves made now can't be") && row_is(5, "loaded by the DOS game."));
    press(SDLK_DOWN);
    press(SDLK_DOWN);
    press(SDLK_RETURN);
    CHECK(!config_improvement("more_stations") && changes[MENU_CHANGED_SWITCHES] == 1);
    press(SDLK_RETURN);
    CHECK(config_improvement("more_stations"));
    press(SDLK_UP);
    press(SDLK_RETURN);  // All off
    CHECK(!config_improvement("more_stations") && config_fix("find_jade"));
    CHECK(menu_row_count() == 4);
    press(SDLK_UP);
    press(SDLK_RETURN);  // All on
    CHECK(config_improvement("more_stations") && menu_row_count() == 6);
    changes[MENU_CHANGED_SWITCHES] = 0;

    // Fixes: a row per fix, all on; one off, then all on again. All off
    // (original) turns the improvements off too, All on leaves them.
    go_to(SEC_FIXES);
    CHECK(menu_row_count() == 4);
    CHECK(row_is(2, "[x] Jade found in time") && row_is(3, "[x] All systems mine"));
    press(SDLK_DOWN);
    press(SDLK_DOWN);
    press(SDLK_RETURN);
    CHECK(!config_fix("find_jade") && config_fix("mine_all_systems"));
    CHECK(changes[MENU_CHANGED_SWITCHES] == 1);
    press(SDLK_UP);
    press(SDLK_RETURN);  // All off (original)
    CHECK(!config_fix("find_jade") && !config_fix("mine_all_systems"));
    CHECK(!config_improvement("more_stations"));
    press(SDLK_UP);
    press(SDLK_RETURN);  // All on
    CHECK(config_fix("find_jade") && config_fix("mine_all_systems"));
    CHECK(!config_improvement("more_stations"));

    // Tab moves between sections and keeps the focus on the rows.
    press(SDLK_TAB);
    CHECK(menu_section() == SEC_SYSTEM && menu_focus() == 0);
    CHECK(menu_row_count() == 3 && row_is(0, "> Import game data...") && row_is(1, "> Restart now")
          && row_is(2, "> Quit now"));
    press(SDLK_TAB);
    CHECK(menu_section() == SEC_CHEATS);
    menu_handle_event(&(SDL_Event){.key = {.type = SDL_EVENT_KEY_DOWN, .key = SDLK_TAB,
                                           .mod = SDL_KMOD_SHIFT, .down = true}});
    CHECK(menu_section() == SEC_SYSTEM);
    SDL_Event shift_tab = {.key = {.type = SDL_EVENT_KEY_DOWN, .key = SDLK_TAB, .mod = SDL_KMOD_SHIFT, .down = true}};
    menu_handle_event(&shift_tab);
    CHECK(menu_section() == SEC_FIXES);

    // Escape closes.
    press(SDLK_ESCAPE);
    CHECK(!menu_is_open());

    // Restart and Quit close the menu with their request.
    menu_open();
    go_to(SEC_SYSTEM);
    press(SDLK_DOWN);
    press(SDLK_RETURN);
    CHECK(!menu_is_open() && input_restart_requested() && !input_quit_requested());
    input_reset();
    menu_open();
    go_to(SEC_SYSTEM);
    press(SDLK_DOWN);
    press(SDLK_DOWN);
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
