// Unit Anim (3c64): loading and drawing .ANI animations (docs/formats/ani.md)
// and one shaped screen copy.
//
// The loaded animation lives in the 841-byte record that the far pointer at
// DS:7E40 points to: a loaded flag, 140 far pointers to the frame blocks
// (Gfx_ReadChunk blocks: 9-byte magic, width, height, stream) and their 140
// block sizes. Only one animation is loaded at a time; the loaders do nothing
// while the flag is set. Frame counts come from the tables at DS:555B (SA,
// 5-byte records), DS:55D7 (MAIN, 7-byte) and DS:5651 (GRANIM, 5-byte),
// indexed by the file number.
#pragma once

#include <stdint.h>

#include "game/mem.h"

// 3c64:0010 / 011e: war\war\sa<id>.ani. Frames that didn't fit in the heap
// keep a nil pointer.
void Anim_LoadWarAnim(uint16_t id);
void Anim_FreeWarAnim(uint16_t id);
// 3c64:019c / 028d: anim\main<id>.ani
void Anim_LoadMainAnim(uint16_t id);
void Anim_FreeMainAnim(uint16_t id);
// 3c64:031d / 0415: war\grwar\granim<id>.ani
void Anim_LoadGrAnim(uint16_t id);
void Anim_FreeGrAnim(uint16_t id);

// 3c64:0496: decodes frame 1 into dest, then frame `frame` (or the nearest
// earlier one that was loaded), both with colour offset 40h.
void Anim_DrawAnimKeyed(uint16_t frame, FarPtr dest);
// 3c64:0516: decodes frame `frame` over dest; *ok is false if that frame
// isn't loaded (and stays true when no animation is loaded at all).
void Anim_DrawAnimFrame(uint16_t frame, FarPtr dest, bool *ok);
// 3c64:057c: decodes one frame block into dest (linear, no stride).
void Anim_DecodeAnimFrame(FarPtr frame, FarPtr dest, uint8_t colourOffset);

// 3c64:0666: copies a 132-row disc-shaped area (rows of 7..160 pixels) from
// src + C28h (a 160-wide buffer) to the screen at row 59 (Gfx_ScreenSeg,
// 320 wide).
void Anim_BlitWedgeToScreen(FarPtr src);
