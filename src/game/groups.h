// Unit Groups (2b8d): the group lists (fleets and planet forces), the group
// detail screen (0x16) and the New Group screen (0x15).
//
// Game_Groups is a far pointer to array[0..1] of array[1..32] of TGroup in the
// game data block (game/ems.h): list 0 = fleets, list 1 = planet forces.
#pragma once

#include <stdint.h>

#include "game/dseg.h"
#include "game/mem.h"

// Group `group` (1..32) of list `list` (0..1): the original's Groups^[list][group].
static inline TGroup *Groups_At(int16_t list, int16_t group)
{
    return (TGroup *)far_host(ds.Game_Groups) + list * 32 + (group - 1);
}

// The selected group: Groups^[GroupList][SelGroup].
static inline TGroup *Groups_Selected(void)
{
    return Groups_At(ds.Game_GroupList, ds.Game_SelGroup);
}

// Per-kind tables, read by raw DS offset like the original (kind 0 or a
// category outside 1..4 reads the neighbouring bytes instead of faulting).
// TypeSlots[kind].Used[cat]: does the kind show slot category cat (1..4)?
static inline bool Groups_SlotUsed(uint8_t kind, int16_t cat)
{
    return *(uint8_t *)ds_at(0x0B3B + kind * 0x14 + cat) != 0;
}
// TypeSlots[kind].Layout[cat]: far pointer to the slot layout blob (see groups.c)
static inline FarPtr Groups_SlotLayout(uint8_t kind, int16_t cat)
{
    return *(uint32_t *)ds_at(0x0B3C + kind * 0x14 + cat * 4);
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
