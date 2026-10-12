// The import screen: asks for the player's copy of the DOS game and imports
// it (platform/import.h). Shown at start-up when no game data is found, and
// from the port menu's System section. Drawn like the port menu, with SDL's
// debug font, since no game data may be loaded yet.
//
// It offers "Choose ZIP or ISO...", "Choose folder..." (SDL's file dialogs:
// on Linux they need xdg-desktop-portal or zenity) and Quit, or Back from
// the menu; a file or folder dropped onto the window is imported as well.
// It shows the edition found, or every problem with the copy.
#pragma once

#include "platform/import.h"

// Runs until the player leaves: at start-up by playing an imported copy
// or quitting, from the menu by going back. True if a copy was imported.
// Closing the window requests a quit (input_quit_requested).
bool importscreen_run(bool from_menu);

// The import's result as text, for the screen and --import: what was
// imported, or why not, with at most max_problems of the problems (0: all)
// on lines of their own. Free with SDL_free.
char *importscreen_describe(const ImportReport *report, int max_problems);
