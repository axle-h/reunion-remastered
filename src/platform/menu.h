// The port menu (F1): settings drawn over the game, which waits inside
// timing_wait_retrace while it is open. Sections: Cheats, Sound, Video,
// Game (with the language of the CD's games), Improvements, Fixes and
// System (import game data, restart, quit). Every change goes to the config file at once.
//
// The menu knows nothing of the game. Settings of the platform layer
// (volumes, display) apply at once; for the rest it calls hooks.changed and
// the game carries the change out at a point of its own choosing (the music
// at once, the rest at the main loop, since the menu may have been opened
// anywhere).
#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_render.h>

typedef enum : int {
    MENU_CHANGED_CHEATS,  // config.freeze_money / freeze_ore
    MENU_ADD_MONEY,
    MENU_ADD_ORE,
    MENU_CHANGED_MUSIC,   // config.music
    MENU_CHANGED_SWITCHES,  // config_fix(), config_improvement()
} MenuChange;

// A fix or an improvement.
typedef struct MenuSwitch {
    const char *key;    // config key
    const char *label;  // at most 20 characters
    const char *help;   // at most 2 lines of 36 characters
} MenuSwitch;

typedef struct MenuHooks {
    // A game is being played (the add cheats are usable).
    bool (*game_running)(void);
    void (*changed)(MenuChange change);
    int add_money, add_ore;  // the add cheats' amounts, for their labels
    const MenuSwitch *fixes;
    int fix_count;
    const MenuSwitch *improvements;
    int improvement_count;
    // IN_* of the languages the game data has (platform/gamedata.h).
    unsigned (*languages)(void);
} MenuHooks;

void menu_set_hooks(const MenuHooks *hooks);

// Shows the menu until it is closed (F1, Esc, Restart, Quit or closing the
// window), then drops the input it saw. Import game data runs the import
// screen (platform/importscreen.h) from it.
void menu_run(void);

// Draws the menu over the frame in render output pixels (menu_run makes it
// the video overlay).
void menu_draw(SDL_Renderer *r);

// The model, without a window (menu_run and tests).
void menu_open(void);
bool menu_is_open(void);
void menu_handle_event(const SDL_Event *e);
// The focused section's rows as drawn ("[x] Freeze money", "Music <UltraSound>").
int menu_row_count(void);
const char *menu_row_text(int row);
int menu_section(void);
int menu_focus(void);  // row, or -1 on the section list
