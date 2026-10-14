// Importing the player's copy of the DOS game into data/ in the player's
// folder (platform/playerdir.h), from a folder, a ZIP or an ISO image
// (platform/source.h).
//
// The copy is checked against the list of game files first (size and
// CRC-32; a ZIP's own CRCs, so a bad archive is rejected before anything is
// extracted): if any file is missing or different, every such file is
// named and nothing is copied. Otherwise the files go to data.partial/,
// under their upper-case DOS names, with each CRC recalculated as it is
// copied. The marker file written last holds the list's version, the
// edition and the languages, and the folder is then renamed to data/,
// replacing an earlier import, so a half-finished import is never used.
//
// A copy of the CD has a game per language (platform/source.h). The
// English one, or else the first, goes to data/ whole; each other
// language's versions of the files that differ from it go to data/DEU,
// data/FRA or data/ITA, which the game reads before data/ in that language
// (fs_set_language_dir, platform/fs.h). A game's language is its CD
// folder's (REUNION.DEU), else its TEXT\MESSAGE.TXT's.
//
// The DOS game's saves (SAVE\SPIDYSAV.n, data/'s game first) are copied to
// save/spidysav.n for the slots that are empty there. SAVE\SETUP.SPD is
// not imported: the CD's says "no sound", and without one the game behaves
// as the floppy's.
#pragma once

#include <SDL3/SDL_atomic.h>

#include "platform/gamedata.h"

enum : int { IMPORT_SAVE_SLOTS = 12 };

typedef struct ImportReport {
    bool ok;
    Edition edition;
    Language language;    // data/'s
    uint8_t languages;    // IN_* of every language imported, data/'s included
    char *root;           // where data/'s game was in the copy ("REUNION.ENG"; "" at the top)
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

typedef struct ImportedData {
    Edition edition;
    Language language;  // data/'s
    uint8_t languages;  // IN_* of every language imported, data/'s included
} ImportedData;

// <player_dir>data if it holds a finished import made with this list
// version, else nullptr. Free with SDL_free. *info (may be null) receives
// what the marker says.
char *import_data_dir(const char *player_dir, ImportedData *info);
