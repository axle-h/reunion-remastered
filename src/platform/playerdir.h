// The player's folder: SDL_GetPrefPath("Reunion", "Reunion")
// (~/.local/share/Reunion/Reunion/ on Linux). It holds reunion.ini, save/,
// screenshots/ and the imported game in data/ (platform/import.h).
#pragma once

// The folder, ending in a path separator, or nullptr if SDL has no pref
// path. Created on first use.
const char *playerdir(void);
// `name` inside the folder ("save/"). Free with SDL_free; nullptr without a
// folder.
char *playerdir_path(const char *name);
// Test hook: use `dir` (created if missing) instead; nullptr restores the
// pref path.
void playerdir_set(const char *dir);

// Earlier builds kept the saves in SDL_GetPrefPath("Amnesty Design",
// "Reunion")/save/. Moves every file there into save/ unless save/ already
// has one of that name, then removes the old folders if they are empty.
void playerdir_migrate(void);
