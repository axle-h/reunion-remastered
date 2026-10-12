// Importing the player's copy of the DOS game into data/ in the player's
// folder (platform/playerdir.h), from a folder, a ZIP or an ISO image
// (platform/source.h).
//
// The copy is checked against the list of game files first (size and
// CRC-32; a ZIP's own CRCs, so a bad archive is rejected before anything is
// extracted): if any file is missing or different, every such file is
// named and nothing is copied. Otherwise the files go to data.partial/,
// under their upper-case DOS names, with each CRC recalculated as it is
// copied. The marker file written last holds the list's version and the
// edition, and the folder is then renamed to data/, replacing an earlier
// import, so a half-finished import is never used.
//
// The DOS game's saves (SAVE\SPIDYSAV.n) are copied to save/spidysav.n for
// the slots that are empty there. SAVE\SETUP.SPD is not imported: the
// CD's says "no sound", and without one the game behaves as the floppy's.
#pragma once

#include <SDL3/SDL_atomic.h>

#include "platform/gamedata.h"

enum : int { IMPORT_SAVE_SLOTS = 12 };

typedef struct ImportReport {
    bool ok;
    Edition edition;
    char *root;           // where the game was in the copy ("REUNION.ENG"; "" at the top)
    char *error;          // why the copy couldn't be read or written, or nullptr
    char **problems;      // "Missing: GRAFIKA\MAIN.PIC", "Different: ..."
    int problem_count;
    int saves_copied;
    bool save_skipped[IMPORT_SAVE_SLOTS + 1];  // [1..12]: the slot was in use
} ImportReport;

// Files checked and copied so far, of total (the check and the copy each
// count every file), for a progress display on another thread.
typedef struct ImportProgress {
    SDL_AtomicInt done, total;
} ImportProgress;

// Imports the copy at `path` into `player_dir` (ending in a separator),
// checking it against `files`. `progress` may be null. Free the report with
// import_report_free.
bool import_game(const char *path, const char *player_dir, const GameFile *files, int count,
                 ImportReport *report, ImportProgress *progress);
void import_report_free(ImportReport *report);

// <player_dir>data if it holds a finished import made with this list
// version, else nullptr. Free with SDL_free. *edition (may be null)
// receives the edition the marker names.
char *import_data_dir(const char *player_dir, Edition *edition);
