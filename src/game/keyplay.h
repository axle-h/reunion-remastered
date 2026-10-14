// Improvement (game/improvements.h, keyboard_play): the game played with
// keys alone. The keys move and click the mouse the game already reads, so
// every screen, box and wait behaves as it does with the mouse, and the
// mouse can be used alongside.
//
// - Targets: the toolbar's buttons and page arrow, the screen's hotspots
//   (Mouse_SetHotspot) and the areas some screens take clicks in without
//   one (keyplay.c, screen_targets). Next and Previous put the pointer on
//   the next or previous one in screen order, top to bottom and left to
//   right; a frame is drawn around it and the game's hint line shows its
//   caption as for the mouse.
// - Grids: a target made of cells (the surface map, the star map, the
//   cargo rows, the battle maps, lists) takes the arrow keys one cell at a
//   time; elsewhere an arrow goes to the nearest target that way. Shift+
//   arrows move the pointer a few pixels, for anything else.
// - Click and Right click press that mouse button while the key is held
//   (Shift+Click is a Shift+click). In a box, any key goes on.
// - Shortcuts: the toolbar's six buttons and its page arrow, Back (the
//   toolbar's way out of the screen), the clock (held, the hours run as
//   with the button held), and the command centre and its rooms. They work
//   while the main loop waits for a click; in a box or a wait the key goes
//   to the game, which takes any key as a click.
//
// The quick-save key (game/autosave.h) works whenever the autosave
// improvement is on, keyboard play or not.
//
// The keys are remappable: [keys] in reunion.ini, an action = SDL key
// names with Shift+ / Ctrl+ / Alt+ in front, two keys apart by a comma (so
// ',' and ';' can't be bound). While text is typed (a name) every key goes
// to the editor.
#pragma once

#include <SDL3/SDL_events.h>

typedef enum : int {
    KEY_NEXT,
    KEY_PREVIOUS,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_NUDGE_UP,
    KEY_NUDGE_DOWN,
    KEY_NUDGE_LEFT,
    KEY_NUDGE_RIGHT,
    KEY_CLICK,
    KEY_RIGHT_CLICK,
    KEY_BACK,
    KEY_CLOCK,
    KEY_TOOLBAR_1,  // .. KEY_TOOLBAR_6, left to right
    KEY_TOOLBAR_PAGE = KEY_TOOLBAR_1 + 6,
    // The command centre, and its rooms through it.
    KEY_COMMAND_CENTRE,
    KEY_STAR_MAP,
    KEY_PLANET,
    KEY_SHIP_INFO,
    KEY_SPACE_LOCAL,
    KEY_RESEARCH,
    KEY_COMMANDERS,
    KEY_INFO_BUY,
    KEY_MESSAGES,
    // Improvement autosave (game/autosave.h): with or without keyboard play.
    KEY_QUICK_SAVE,
    KEY_ACTION_COUNT,
} KeyAction;

typedef struct KeyActionInfo {
    const char *key;       // in [keys]
    const char *label;     // port menu, at most 20 characters
    const char *defaults;  // as written in [keys]
} KeyActionInfo;

extern const KeyActionInfo KeyPlay_Actions[KEY_ACTION_COUNT];

// Where the targets come from: the screens of the main loop, or the
// title's two choices, which have no hotspots (game/title.c).
typedef enum : int {
    KEYPLAY_SCREENS,
    KEYPLAY_START_MENU,
    KEYPLAY_HERO,
} KeyPlayContext;

// Takes the keys from the config ([keys], the defaults where it has none)
// and hooks the input. Scripted runs have keyboard_play off unless a test
// switches it on.
void KeyPlay_Init(void);
// The keys of action a as the config has them (the defaults if it doesn't).
const char *KeyPlay_Keys(KeyAction a);
// The port menu's help for action a, nullptr for keyboard play's own.
const char *KeyPlay_Help(KeyAction a);
// Every retrace (Mouse's retrace hook): finishes clicks and shortcuts and
// draws the focus frame.
void KeyPlay_Frame(void);
// Forgets clicks and shortcuts in progress and the focus.
void KeyPlay_Reset(void);
// The game's mouse is set up (Mouse_InitMouse) / the game has ended: keys
// are taken only in between, so the cut-scenes get theirs.
void KeyPlay_Start(void);
void KeyPlay_Stop(void);
void KeyPlay_SetContext(KeyPlayContext c);
