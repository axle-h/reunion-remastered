// Unit Groups (2b8d): the group lists (fleets and planet forces), the group
// detail screen (0x16) and the New Group screen (0x15).
//
// Game_Groups points to array[0..1] of array[1..32] of TGroup in the game
// data block (game/ems.h): list 0 = fleets, list 1 = planet forces.
#pragma once

#include <stdint.h>

#include "game/dseg.h"

// Group `group` (1..32) of list `list` (0..1): the original's Groups^[list][group].
// Group 0 (no group selected) is the record before the list, as the
// original addresses it.
static inline TGroup *Groups_At(int16_t list, int16_t group)
{
    return &ds.Game_Groups[0][0] + list * 32 + (group - 1);
}

// The selected group: Groups^[GroupList][SelGroup].
static inline TGroup *Groups_Selected(void)
{
    return Groups_At(ds.Game_GroupList, ds.Game_SelGroup);
}

// Per-kind tables (kind 1..5, slot category cat 1..4).
// TypeSlots[kind].Used[cat]: does the kind show slot category cat?
static inline bool Groups_SlotUsed(uint8_t kind, int16_t cat)
{
    return ds.Groups_TypeSlots[kind - 1].Used[cat - 1] != 0;
}
// TypeSlots[kind].Layout[cat]: the slot layout blob (see groups.c)
static inline uint8_t *Groups_SlotLayout(uint8_t kind, int16_t cat)
{
    return ds.Groups_TypeSlots[kind - 1].Layout[cat - 1];
}

// 2b8d:0002 / 0773: screen handlers (screens 0x16 and 0x15)
void Groups_GroupsScreen(void);
void Groups_NewGroupScreen(void);
// 2b8d:08bc
void Groups_LoadGroupsGfx(void);
// 2b8d:092b
void Groups_DrawGroupPanel(bool full);
// 2b8d:11bf
void Groups_SetupGroupsHotspots(void);
// 2b8d:15ec / 1659
void Groups_DrawNewGroupNameAndType(void);
void Groups_SetupNewGroupHotspots(void);
// 2b8d:16d0 / 17fd: reset / initialise Groups_Selected()
void Groups_ClearGroup(void);
void Groups_InitGroupOfKind(void);
// 2b8d:1996 / 19f7 / 1a62
void Groups_NextSlotCategory(void);
void Groups_PrevSlotCategory(void);
void Groups_FirstSlotCategory(void);
// 2b8d:1abb / 1bad
bool Groups_IsGroupEmpty(int16_t list, int16_t group);
bool Groups_IsGroupEmptyOfCargo(int16_t list, int16_t group);
// 2b8d:1c9f: index of the planet force at that location, 0 if none (the last match wins)
int16_t Groups_FindPlanetForce(int16_t system, int16_t planet, int16_t moon);
// 2b8d:1d09
void Groups_InitPlanetForce(void);
// 2b8d:1d6f
void Groups_Init(void);
