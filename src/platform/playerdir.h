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
