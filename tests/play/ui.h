// UI drivers for playthrough policies: find what the screen offers (toolbar
// buttons by label, hotspots by caption, both of which the game shows as the
// hint line under the mouse) and click it like a player would.
#pragma once

#include <stdint.h>

// The screen shown (Game::CurrentScreen; docs/screens.md §3).
int ui_screen(void);
// Waits until `screen` is shown and its handler has run its setup, then a
// few retraces more for the fades. False after `timeout` retraces.
bool ui_wait_screen(int screen, int timeout);
// Waits until the main loop has been waiting for input for a few retraces
// (not fading, typing text or animating).
bool ui_wait_idle(int timeout);
// Waits for idle, then clicks. All UI clicks go through here.
void ui_click(int x, int y);
// Clicks the toolbar button labelled `label` (case-insensitive), turning
// the toolbar page first if needed. False if no button has that label.
bool ui_toolbar(const char *label);
// Clicks the middle of the hotspot captioned `caption` (case-insensitive).
bool ui_hotspot(const char *caption);
// Waits until a hotspot captioned `caption` is on screen.
bool ui_wait_hotspot(const char *caption, int timeout);
// Hires `name` (as the hire screen captions them) from the command centre and
// returns to it. Fails the run if the candidate won't come.
void ui_hire(const char *kind, const char *name);
// From power-on: "new game", the left hero, and waits for the command
// centre with the clock running.
void ui_new_game(void);
// Logs the toolbar labels and the hotspot captions on screen.
void ui_log_screen(void);
