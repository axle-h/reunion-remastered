// Unit PlanetList (1390): the scrolling list of bodies on screen 37 (colonies
// and miner stations, accommodable and mineable bodies, alien bodies).
//
// PlanetList_Entries (far) holds 8-byte entries {system, planet, moon, kind},
// 1-based like the Pascal array. A kind 4 entry is a moon's own row; it follows
// the entry of the moon's planet.
//
// Pascal strings are host pointers to a length byte plus characters.
#pragma once

#include <stdint.h>

#include "game/dseg.h"

typedef struct PlanetListEntry {
    int16_t System;
    int16_t Planet;
    int16_t Moon;
    int16_t Kind;
} PlanetListEntry;
static_assert(sizeof(PlanetListEntry) == 8);

// 1390:000b
void PlanetList_SetupScrollBarHotspot(void);
// 1390:0047: appends the entry (and, for a moon, its row) when the body's
// state fits ds.PlanetList_Mode. Nested in BuildList in the original.
void PlanetList_AddListEntry(int16_t sys, int16_t planet, int16_t moon, int16_t bodyIdx);
// 1390:02b6
void PlanetList_BuildList(void);
// 1390:03e9: "Colony on " ... depending on mode (1..3) and kind.
void PlanetList_ListKindCaption(PString *result, int16_t mode, int16_t kind);
// 1390:04cc
void PlanetList_DrawList(void);
// 1390:0735
void PlanetList_RefreshList(void);
// 1390:0741
void PlanetList_DrawScrollBarThumb(void);
// 1390:07c0
void PlanetList_DragScrollBar(void);
