// Unit Ems (4719): the game keeps Groups, Buildings and the charset in one
// 8770h-byte block, and graphics code its icon/toolbar buffer (GfxPath).
// The original moved both into EMS when an EMM was installed (state 2: Data
// in the page frame, which the blocks shared, so each had to be mapped
// before use). The port has no EMS: blocks stay heap copies (state 1), as
// on a DOS machine without an EMM, and need no mapping.
#pragma once

#include <stdint.h>

#include "game/dseg.h"

// The TEmsBlock variables the game uses.
static inline TEmsBlock *Ems_GameDataBlock(void)
{
    return &ds.Ems_GameData;
}

static inline TEmsBlock *Ems_ToolbarBlock(void)
{
    return &ds.GfxPath_ToolbarBlock;
}

// 4719:0188: makes blk hold the size bytes at src (state 1). The original
// moved them into EMS when it could.
void Ems_EmsMoveToEms(TEmsBlock *blk, uint16_t size, uint8_t *src);
// 4719:0253: frees the heap copy; state := 0.
void Ems_EmsRelease(TEmsBlock *blk);
// 4719:0317 (= Ems::RestoreProc)
void Ems_EmsReleaseGameData(void);
// 4719:0325: allocates Ems_Buffer (8770h bytes) as the game data block and
// points Game_Groups, Game_Buildings and Gfx_Charset into it (Buildings =
// Groups + 2840h, Charset = Groups + 5EF0h).
void Ems_Init(void);
