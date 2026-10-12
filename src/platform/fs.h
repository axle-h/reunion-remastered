// Access to the original game data, addressed with the game's own DOS paths
// (e.g. "GRAFIKA\\MAIN.PIC"): either slash, any letter case. Debug builds
// warn when the game loads a file that is not on the list of game files
// (platform/gamedata.h).
#pragma once

#include <stddef.h>
#include <stdint.h>

// Sets the directory holding the game data (the one with GRAFIKA, PICS, ...).
bool fs_init(const char *data_dir);
void fs_shutdown(void);
// The directory fs_init set, or nullptr.
const char *fs_root(void);

// Returns the host path of an existing file or directory, or nullptr.
// The caller frees the result with SDL_free().
char *fs_resolve(const char *dos_path);

// Loads a whole file. Returns a buffer to free with SDL_free(), or nullptr
// if the file doesn't exist. *size receives the length (may be null).
uint8_t *fs_load(const char *dos_path, size_t *size);
