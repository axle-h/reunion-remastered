// The files of the player's copy that the port loads, and the edition they
// come from (docs/versions.md). The import checks a copy against this list
// (platform/import.h); fs.c warns in debug builds when the game opens a file
// that is not on it.
#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum : uint8_t {
    EDITION_UNKNOWN,
    EDITION_FLOPPY,  // the English floppy release
    EDITION_CD,      // REUNION.ENG of the European CD: the floppy's files but the font sheet
} Edition;

typedef struct GameFile {
    const char *path;  // DOS path, upper case
    uint32_t size;
    uint32_t crc;      // CRC-32
    Edition edition;   // the edition this version of the file names, or unknown
} GameFile;

// A path may be listed once per accepted version (GRAFIKA\CHARSET1.PIC).
// The list is sorted by path.
extern const GameFile gamedata_files[];
extern const int gamedata_file_count;
// Goes up whenever the list changes; an import made with another list is
// not used (import.h).
enum : int { GAMEDATA_LIST_VERSION = 1 };

const char *gamedata_edition_name(Edition edition);  // "floppy", "CD"
Edition gamedata_edition_from_name(const char *name);

// The edition named by GRAFIKA\CHARSET1.PIC in the fs root. A copy not
// checked by the import may have another sheet: then the floppy's.
Edition gamedata_edition(void);

// Is `dos_path` on the list (any case, either slash)? SAVE\SETUP.SPD is
// read when present but never imported, and counts as listed.
bool gamedata_listed(const char *dos_path);

uint32_t gamedata_crc32(uint32_t crc, const void *data, size_t size);
