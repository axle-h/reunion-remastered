// Unit RawText (4049): loads the three fixed-slot string tables
// (TEXT\SZ_*.RAW, see assets/text.h for their layout) into heap blocks, and
// the palette-range DAC writer used by the fades.
#pragma once

#include <stdint.h>

#include "game/mem.h"

// 4049:0011 / 0083 / 00f3: GetMem + read the whole file into
// ds.RawText_SzFace (7B0h bytes) / SzTalal (1DABh) / SzFaj (1440h).
void RawText_LoadSzFace(void);
void RawText_LoadSzTalal(void);
void RawText_LoadSzFaj(void);
// 4049:0153 / 0159: empty.
void RawText_Nop(void);
void RawText_Init(void);
// 4049:0160: writes pal[first..last] (colour-number indexed) to the DAC from
// index first, 3 bytes per entry, wrapping at 256; the DAC keeps 6 bits.
void RawText_SetPaletteRange(int16_t first, int16_t last, FarPtr pal);
