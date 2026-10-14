// The files of the player's copy that the port loads, the edition they
// come from and the CD's four languages (docs/versions.md). The import
// checks a copy against this list (platform/import.h); fs.c warns in debug
// builds when the game opens a file that is not on it.
#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum : uint8_t {
    EDITION_UNKNOWN,
    EDITION_FLOPPY,  // the English floppy release
    EDITION_CD,      // the European CD: the floppy's files but the font sheet, in four languages
} Edition;

// The CD's games. The floppy release is English.
typedef enum : uint8_t {
    LANGUAGE_ENGLISH,
    LANGUAGE_GERMAN,
    LANGUAGE_FRENCH,
    LANGUAGE_ITALIAN,
    LANGUAGE_COUNT,
} Language;

// GameFile.languages: the languages a version of a file is, as bits.
enum : uint8_t {
    IN_ENGLISH = 1 << LANGUAGE_ENGLISH,
    IN_GERMAN = 1 << LANGUAGE_GERMAN,
    IN_FRENCH = 1 << LANGUAGE_FRENCH,
    IN_ITALIAN = 1 << LANGUAGE_ITALIAN,
};

typedef struct GameFile {
    const char *path;   // DOS path, upper case
    uint32_t size;
    uint32_t crc;       // CRC-32
    Edition edition;    // the edition this version of the file names, or unknown
    uint8_t languages;  // IN_* of the languages this version is; 0: all four share it
} GameFile;

// A path may be listed once per accepted version (GRAFIKA\CHARSET1.PIC, the
// files with text or speech, which differ by language). The list is sorted
// by path.
extern const GameFile gamedata_files[];
extern const int gamedata_file_count;
// Goes up whenever the list changes; an import made with another list is
// not used (import.h).
enum : int { GAMEDATA_LIST_VERSION = 2 };

const char *gamedata_edition_name(Edition edition);  // "floppy", "CD"
Edition gamedata_edition_from_name(const char *name);

// "english", "german", ... as reunion.ini and the import's marker write them.
const char *gamedata_language_name(Language language);
// The name, or the CD's folder suffix ("DEU"); LANGUAGE_COUNT if neither.
Language gamedata_language_from_name(const char *name);
// The CD's folder suffix, "ENG", "DEU", "FRA" or "ITA" (REUNION.DEU on the
// CD, data/DEU in an import).
const char *gamedata_language_folder(Language language);
// "English", "German", ... for the port menu.
const char *gamedata_language_label(Language language);

// Is `f` the version of its path that `language` has?
bool gamedata_in_language(const GameFile *f, Language language);

// The edition named by GRAFIKA\CHARSET1.PIC in the fs root. A copy not
// checked by the import may have another sheet: then the floppy's.
Edition gamedata_edition(void);
// The language of the game fs_load reads, named by TEXT\MESSAGE.TXT;
// English if the file is not on the list.
Language gamedata_language(void);

// Is `dos_path` on the list (any case, either slash)? SAVE\SETUP.SPD is
// read when present but never imported, and counts as listed.
bool gamedata_listed(const char *dos_path);

uint32_t gamedata_crc32(uint32_t crc, const void *data, size_t size);
