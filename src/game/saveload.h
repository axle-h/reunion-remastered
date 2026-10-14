// Unit SaveLoad (1a3e): saved games, the new-game file and the disk/options
// screen. A save is 41,670 bytes of raw memory blocks in a fixed order
// (saveload.c, the block list), with no magic, version or checksum. SAVE\INIT
// holds only BuildingCount and the Buildings table (the new game's 23
// buildings; the slots past them are stale bytes, never read).
//
// Where the port keeps files: saves are save/spidysav.<n> in the player's
// folder (platform/playerdir.h), where the import puts the DOS game's saves
// too. save\init comes from the game data.
//
// The port's format. While an improvement keeps state the DOS save has no
// room for (Improvements_PortSaves), or the game holds some (a trade route,
// game/routes.h, more than 1,000 buildings, or more than one university
// course, game/courses.h), a save is written as
// save/reunion.<n> instead: the DOS game never opens it (its slot shows
// "Empty" there and can be saved over), where a different layout under its
// own name would load as garbage. The
// file: "REUNION" 1Ah, a version word, the slot name, the DOS blocks in
// their order with the building table cut to BuildingCount records (up to
// BUILDINGS_MAX, game/main_program.h; the DOS format can't hold more than
// 1,000: the original would read past its table into the charset), then
// the port's tables, each as its count and record size, then the records:
// the routes and, from version 2, the courses (version 1 has none). Writing a
// slot in one format deletes its file in the other. The disk screen lists
// both and loads either, whatever the switches say; a version newer than
// the port knows is listed but refused.
//
// The autosave (game/autosave.h) is save/reunion.auto, always in the port's
// format. While the switch is on the disk screen lists it as a 13th row
// under the 12 slots, which move up to make room; it only loads.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "game/dseg.h"

enum : uint32_t { SAVELOAD_FILE_SIZE = 41670 };

enum : int16_t {
    AUTOSAVE_SLOT = 13,      // the disk screen's row of the autosave
    AUTOSAVE_ROWS_Y = 0x3A,  // the first row's y with it (the original's 0x40)
};

// Port addition (test hook): directory for the player's saves, with or without
// a trailing slash. nullptr restores the default (the player's folder).
void SaveLoad_SetSaveDir(const char *dir);

// Port addition: SaveLoadGame without the redraw of date and credits. Returns
// false if the file could not be opened or a block failed (the original stops
// with I/O error 100 and leaves the state half-loaded).
bool SaveLoad_Transfer(bool save);
// Port addition (test hook): the same with the file at `path` in the DOS
// format; `name` holds the header (the slot name as string[19]).
bool SaveLoad_TransferFile(const char *path, uint8_t name[0x14], bool save);
// Port addition: a save now is written in the port's format.
bool SaveLoad_PortFormat(void);
// Port improvement (game/autosave.h): the game as a save in the port's
// format named `name` (string[19]), in memory the caller frees with
// SDL_free; nullptr on failure.
void *SaveLoad_SaveToMemory(const uint8_t name[0x14], size_t *size);
// Writes such a save as the autosave's file.
bool SaveLoad_WriteAutosave(const void *data, size_t size);
// The disk screen's slot rows: the first's y and how many (keyboard play).
void SaveLoad_SlotArea(int16_t *y, int16_t *rows);

// 1a3e:0008
void SaveLoad_DiskScreen(void);
// 1a3e:029c: Save = true writes slot DS:7732, else reads it (if the slot is used).
void SaveLoad_SaveLoadGame(bool save);
// 1a3e:0b93 / 0b9d
void SaveLoad_SaveGame(void);
void SaveLoad_LoadGame(void);
// 1a3e:0bb1: save\init (BuildingCount + Buildings).
void SaveLoad_LoadInit(void);
// 1a3e:0c40: fills the 12-entry slot table at DS:7734^ (Used byte + string[20]).
void SaveLoad_ReadSlotNames(void);
// 1a3e:146e
void SaveLoad_PressButton(int16_t which);
// 1a3e:14b5
void SaveLoad_Init(void);
