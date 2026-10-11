// What the screen shows, and the interrupts every playthrough policy meets
// (docs/playthrough-policy.md §3): the screen text reader the observers use,
// the status bar, message boxes that pop up anywhere, the planet rename
// prompt, story scenes (screen 33) and the research hologram on the command
// centre. ui.c's waits and clicks call ui_handle_interrupts while they wait,
// so drivers rarely call the interrupt functions themselves.
//
// What is read, and where the player sees it:
// - Text: the pixels of video memory (A000:0000) matched against the game's
//   6x8 font, i.e. exactly what is drawn. Pixels under the mouse cursor are
//   skipped.
// - Message box: the bordered panel centred on (160,124) and its lines
//   (message.c:144-162); "Press mouse button" blinking in the status bar
//   (alone, when the game had no memory to draw the panel).
// - Rename prompt: a box reading "The new planet's name in <sys> system?"
//   with the name being edited (message.c:304-315).
// - Story scene: the full-screen picture of screen 33 (ui_screen()).
// - Hologram: the command centre stops taking input after a research
//   finished there (command.c:291-299); seen as the wireframe in the room,
//   detected as the command centre holding with no box for 200 retraces.
// "Waits in it" / "holding" means the main loop has stopped waiting for
// input (Game_EventWaitFrames, as ui.c's idle test): the screen doesn't
// react to the mouse.
#pragma once

#include <stddef.h>
#include <stdint.h>

// --- Screen text ---

enum : int { UI_ANY_COLOUR = -1 };

// Reads `width` glyph cells drawn with the game font at (x, y) (the
// coordinates Gfx_DrawText6x8 takes) into out, without trailing blanks.
// Cells that match no glyph read '?'; returns false if there was one.
bool ui_read_text(int x, int y, int width, char *out, size_t size);
// ui_read_text that takes only cells drawn in `colour` (the text routines
// add it to the glyph: 0xF9 plain, 0xFC highlighted, 0x34 the status line);
// cells in other colours read '?'.
bool ui_read_text_in(int x, int y, int width, int colour, char *out, size_t size);
// The colour a cell at (x, y) was drawn with, or -1 if no glyph matches.
int ui_text_colour(int x, int y);
// The screen shows `s` at (x, y) in any colour. Cells under the cursor
// match anything, but at least one pixel must be seen.
bool ui_text_at(int x, int y, const char *s);
// Reads a number drawn at (x, y) in `width` cells; false if there is none.
bool ui_read_number(int x, int y, int width, int32_t *value);
// Pixel (x, y) shows `colour`, or the mouse cursor hides it.
bool ui_pixel_is(int x, int y, int colour);

// --- Status bar (every screen with the toolbar) ---

typedef struct { int year, month, day, hour; } UiDate;

// The status line under the toolbar (hint line, "Press mouse button").
void ui_status_text(char *out, size_t size);
// Credits as the status bar shows them; false while they are not drawn
// (the first hour of a new game).
bool ui_credits(int32_t *credits);
// The clock "Y-M-D-H" on the status bar.
bool ui_date(UiDate *date);
// Hours since 2900-01-01 (the game's months have 30 days, sim.c).
int64_t ui_date_hours(const UiDate *date);
// ui_date_hours of the clock on the status bar, read from the date it
// draws (Game_Year..Game_Hour), so it also works where the bar is covered.
int64_t ui_clock_hours(void);

// --- Message boxes ---

enum : int {
    UI_BOX_LINES = 6,
    UI_BOX_COLS = 54,
    UI_BOX_TEXT = UI_BOX_LINES * (UI_BOX_COLS + 1) + 1,
};

typedef struct {
    int lines, cols;   // size in text cells (0: no panel drawn)
    int x, y;          // first text cell
    bool rename;       // the planet rename prompt (edit field, Enter closes it)
    char line[UI_BOX_LINES][UI_BOX_COLS + 1];
    char text[UI_BOX_TEXT];  // non-blank lines joined with '|', leading blanks dropped
} UiBox;

// A message box (or rename prompt) is drawn and the game waits in it (box
// may be nullptr). May give the game a retrace to see that it waits.
bool ui_box(UiBox *box);
// Closes the message box shown: release, press, release on an inert point.
// (A box that keeps coming back fails the run in ui_handle_interrupts.)
void ui_close_box(void);
// Accepts the rename prompt shown with Enter (the only key the fixture
// presses there; the name is never typed). Fails the run if it stays.
void ui_accept_name(void);
// Text of the last box closed ("" if none), and the text of the last box
// containing `text` (case-insensitive; "" matches any) closed at or after
// frame `since`, or nullptr.
const char *ui_last_box(void);
const char *ui_box_seen(const char *text, uint64_t since);
// Calls fn(text, arg) for each box before it is closed (nullptr: none), so a
// policy can note T(x) or react to a box it is waiting for.
void ui_on_box(void (*fn)(const char *text, void *arg), void *arg);

// A point where a click does nothing on the screen shown: not on the
// toolbar, its toggle, the clock or any hotspot. Story scenes and holograms
// end on a press that the command centre then sees: after_screen 1 gives a
// point inert there; 0 one inert on the screen shown.
void ui_inert_point(int after_screen, int *x, int *y);

// --- Story scenes (screen 33) ---

// Screen 33 is up or about to be: the number of the picture it shows
// (pics\pic<n>), 0 if none.
int ui_story_scene_shown(void);
// Waits for a story scene (closing the box some open with) and plays it
// out as ui_handle_interrupts does, until the command centre is back.
// Returns the scene's picture number.
int ui_story_scene(void);
// Story scenes played out so far in this run.
int ui_scenes_played(void);

// --- Interrupts ---

enum : int {
    UI_INT_BOX = 1,       // closed a message box (text in ui_last_box())
    UI_INT_RENAME = 2,    // accepted a planet rename prompt
    UI_INT_SCENE = 4,     // clicked through a story scene (screen 33)
    UI_INT_HOLOGRAM = 8,  // pressed to end a hologram on the command centre
    UI_INT_SCREEN = 16,   // not handled: an alien talk (34), battle (29, 19,
                          // 32) or ending (35, 36) is shown; the caller's job
};

// Services what interrupts the game now: closes boxes, accepts rename
// prompts, clicks through scenes and holograms, until none is left. Returns
// the UI_INT_* bits of what it did or found. May give the game retraces.
int ui_handle_interrupts(void);
// The UI_INT_* bits handled since the last call (by any of the waits).
int ui_interrupts_handled(void);
// An alien talk, battle or ending screen is shown (UI_INT_SCREEN): the
// screens a policy must handle itself.
bool ui_interrupt_screen(void);
// Calls fn() (nullptr: none) whenever ui_handle_interrupts finds such a
// screen, i.e. from any wait or click: a policy can leave a driver
// sequence there (longjmp) to handle the screen and take it up again.
void ui_on_interrupt_screen(void (*fn)(void));
// For a driver that finds its screen gone (a story scene, e.g. the
// supernova's, ends on the command centre): leaves the sequence as for an
// interrupt screen (the hook above), else fails the run.
[[noreturn]] void ui_screen_lost(const char *what);

// --- Game data block ---

// The game keeps its Groups and Buildings in one block (game/ems.h).
// Observers that stand for the group and surface screens read the records
// here.
struct TGroup;
struct TBuilding;
const struct TGroup *ui_group_record(int list, int k);  // Groups^[list][k]
const struct TBuilding *ui_building_record(int i);      // Buildings^[i]
