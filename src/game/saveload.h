// Unit SaveLoad (1a3e): saved games, the new-game file and the disk/options
// screen. A save is 41,670 bytes of raw memory blocks in a fixed order
// (saveload.c, the block list), with no magic, version or checksum. SAVE\INIT
// holds only BuildingCount and the Buildings table (the new game's 23
// buildings; the slots past them are stale bytes, never read).
//
// Where the port keeps files: saves are save/spidysav.<n> in the player's
// folder (platform/playerdir.h), where the import puts the DOS game's saves
// too. save\init comes from the game data.
#pragma once

#include <stdint.h>

#include "game/dseg.h"

enum : uint32_t { SAVELOAD_FILE_SIZE = 41670 };

// Port addition (test hook): directory for the player's saves, with or without
// a trailing slash. nullptr restores the default (the player's folder).
void SaveLoad_SetSaveDir(const char *dir);

// Port addition: SaveLoadGame without the redraw of date and credits. Returns
// false if the file could not be opened or a block failed (the original stops
// with I/O error 100 and leaves the state half-loaded).
bool SaveLoad_Transfer(bool save);
// Port addition (test hook): the same with the file at `path`; `name` holds
// the header (the slot name as string[19]).
bool SaveLoad_TransferFile(const char *path, uint8_t name[0x14], bool save);

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
