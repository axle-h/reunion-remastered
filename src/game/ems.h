// Unit Ems (4719): the game keeps Groups, Buildings and the charset in one
// 8770h-byte block that the original moved into EMS when available, and
// graphics code keeps its icon/toolbar buffer the same way (GfxPath).
//
// With an EMM (game/emm.h, installed by mem_init) a block moves to EMS
// (state 2): Data points into the page frame and the block must be mapped
// (EmsMapAll) before use; the blocks share the frame. Without one it stays
// a heap copy (state 1) and the map calls do nothing.
#pragma once

#include <stdint.h>

#include "game/dseg.h"

// The TEmsBlock variables the game uses.
static inline TEmsBlock *Ems_GameDataBlock(void)
{
    return (TEmsBlock *)ds_at(0x8A4E);
}

static inline TEmsBlock *Ems_ToolbarBlock(void)
{
    return (TEmsBlock *)ds_at(0x7DFA);
}

// 4719:0009: is an EMM installed (INT 67h vector -> "EMMXXXX0")?
bool Ems_DetectEms(TEmsBlock *blk);
// 4719:00a5: unallocated EMS pages. This and the next three store the EMM
// status in DS:8A4C (Ems_Status).
uint16_t Ems_EmsFreePages(TEmsBlock *blk);
// 4719:00ce: FrameSeg := page frame; allocates `pages` pages -> Handle.
void Ems_EmsAlloc(TEmsBlock *blk, uint16_t pages);
// 4719:0128: maps logical page logPage of blk's handle to physPage.
void Ems_EmsMapPage(TEmsBlock *blk, uint8_t physPage, uint16_t logPage);
// 4719:015e: releases blk's handle.
void Ems_EmsFree(TEmsBlock *blk);
// 4719:0188: makes blk hold the size bytes at src: in EMS if possible (src
// copied into the frame and freed, state 2), else src itself (state 1).
void Ems_EmsMoveToEms(TEmsBlock *blk, uint16_t size, FarPtr src);
// 4719:0253: frees the heap copy (state 1) or the EMS handle (state 2);
// state := 0.
void Ems_EmsRelease(TEmsBlock *blk);
// 4719:0290: maps a block in EMS to physical pages 0..n-1. 4719:02cf: empty.
void Ems_EmsMapAll(TEmsBlock *blk);
void Ems_EmsNop(TEmsBlock *blk);
// 4719:02d6: points Game_Groups, Game_Buildings and Gfx_Charset into the
// game data block (Buildings = Groups + 2840h, Charset = Groups + 5EF0h).
void Ems_EmsMapGameData(void);
// 4719:0317 (= Ems::RestoreProc)
void Ems_EmsReleaseGameData(void);
// 4719:0325: allocates Ems_Buffer (8770h bytes) and moves it to EMS as the
// game data block.
void Ems_Init(void);
