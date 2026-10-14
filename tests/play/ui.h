// UI drivers for playthrough policies: find what the screen offers (toolbar
// buttons by label, hotspots by caption, both of which the game shows as the
// hint line under the mouse) and click it like a player would.
//
// The shared machinery is in two modules (docs/playthrough.md,
// "The fixture"): this one acts (waits, clicks, the toolbar, hotspots, the
// clock, navigation) and ui_message.h reads the screen and handles the
// interrupts. Per-screen drivers build on both.
//
// Interrupt rule: every wait and click here services the interrupts while it
// waits (ui_handle_interrupts: closes message boxes, accepts rename prompts,
// clicks through story scenes and holograms), so a box that pops up just
// before a click is closed instead of stalling it. Use play_wait_until only
// to watch a box or a hover yourself.
//
// Clicks and the main loop (src/game/main_program.c): a press on a hotspot runs
// the screen's handler at once. On a toolbar button or a hotspot flagged as
// a button (THotspot.Repeat_) that is all; on any other hotspot the handler
// runs again on every retrace the button stays down over it (auto-repeat:
// the clock button, the group grid) and once more on the release over it.
// So UI_CLICK runs such a handler about 5 times (fewer if the handler itself
// waits, as cargo's 4-retrace flash does); UI_PRESS runs it once.
#pragma once

#include <stdint.h>

typedef enum {
    UI_CLICK,        // press 4 retraces, release in place, 2 more
    UI_CLICK_RIGHT,  // the same with the right button
    UI_PRESS,        // press 1 retrace, slide off to an inert point, hold 4,
    UI_PRESS_RIGHT,  // release there, 2 more
    UI_CLICK_SHIFT,  // UI_CLICK with Shift held until the release
} UiClick;
// Either holds the press until the game's input poll has taken it (ui.c
// hold_until_polled): longer than 1 or 4 retraces only if the game turned
// busy as the button went down. If it stays busy (a box), the button is
// released unseen and the click starts again once the game takes input.

// --- Waits (all service interrupts) ---

// The screen shown (Game_CurrentScreen; the screen list is in
// src/game/main_program.h).
int ui_screen(void);
// Waits until pred(arg) holds, checking once per retrace after servicing
// interrupts, for at most `timeout` retraces. Returns whether it holds.
bool ui_wait_until(bool (*pred)(void *arg), void *arg, int timeout);
// ui_wait_until bounded by game hours on the status bar clock (the clock
// stops in boxes and on some screens): gives up after `hours` hours, or
// after three times their retraces if the clock stands. Returns pred(arg).
bool ui_wait_hours(bool (*pred)(void *arg), void *arg, int hours);
// Waits until the main loop has been waiting for input for a few retraces
// (not fading, typing text, animating or held by a box).
bool ui_wait_idle(int timeout);
// ui_wait_idle that fails the run after 5000 retraces.
void ui_settle(void);
// Waits until `screen` is shown and its handler has run its setup, then a
// few retraces more for the fades. False after `timeout` retraces.
bool ui_wait_screen(int screen, int timeout);
// Waits until a hotspot captioned `caption` is on screen.
bool ui_wait_hotspot(const char *caption, int timeout);

// --- Clicks ---

// Waits for idle, then clicks (UI_CLICK). Fails the run if the game stays
// busy, or if an alien talk, battle or ending replaced the screen meanwhile
// (the click would land there).
void ui_click(int x, int y);
void ui_click_as(int x, int y, UiClick how);

// --- Toolbar ---

// Clicks the toolbar button labelled `label` (case-insensitive) or with
// command `command`, turning the toolbar page first if needed. False if no
// button has it.
bool ui_toolbar(const char *label);
bool ui_toolbar_command(int command);
// The toolbar (either page) has the button.
bool ui_toolbar_has(const char *label);
bool ui_toolbar_has_command(int command);

// --- Hotspots ---

// Index of the k-th (1-based) hotspot captioned `caption` (case-insensitive,
// blanks around the caption ignored; hotspots of zero size skipped), 0 if
// none. Captions are string[18]: a longer `caption` matches its first 18.
int ui_find_hotspot(const char *caption, int k);
// Hotspot j is on screen (not of zero size) and captioned `caption`.
bool ui_hotspot_captioned(int j, const char *caption);
// The caption of hotspot j, blanks at both ends trimmed ("" if it has none
// or is not on screen).
const char *ui_hotspot_caption(int j);
// Clicks the first hotspot captioned `caption` (UI_CLICK); false if none.
bool ui_hotspot(const char *caption);
// Clicks the k-th hotspot captioned `caption` (same-caption hotspots, e.g.
// two Strangers in the pub); false if none.
bool ui_hotspot_as(const char *caption, int k, UiClick how);
// Clicks hotspot j at the point nearest its middle that no lower-numbered
// hotspot covers: the hit test takes the lowest index under the mouse, so
// overlapping hotspots (orbiting planets on the maps) can steal a click.
// Waits a while for such a point if the hotspots overlap.
void ui_click_hotspot(int j, UiClick how);

// --- Clock ---

// Clicks the clock button (one game hour per click) until
// pred(arg) holds, for at most `hours` hours. Only on screens whose clock
// runs. Returns the hours it took, -1 if pred still fails.
int ui_fast_forward(bool (*pred)(void *arg), void *arg, int hours);

// --- Navigation ---

// Goes to `screen` with the toolbar button `label`, through the command
// centre if the toolbar lacks it, and waits until it takes input. Does
// nothing but settle if `screen` is shown. Fails the run if it can't.
void ui_open_screen(const char *label, int screen);
// Hires `name` (as the hire screen captions them) from the command centre and
// returns to it. Fails the run if the candidate won't come.
void ui_hire(const char *kind, const char *name);
// From power-on: "new game", the left hero, and waits for the command
// centre with the clock running.
void ui_new_game(void);
// Logs the toolbar labels and the hotspot captions on screen.
void ui_log_screen(void);
