// Unit GrWarUnits (1430): the unit assignment screen (0x13) where the player
// picks the troops for a ground battle.
//
// Forces are kept in tables of four longints (unit types 1..4) at DS:
//   7656+4t  attackers' total of type t        7666+4t  defenders' total
//   7676+4t  attackers' type t not yet assigned to a slot
//   7686+4t  attackers' attack sum             7696+4t  defenders' attack sum
//   76A6+4t  attackers' defence sum            76B6+4t  defenders' defence sum
// (all are 7646 + 16*s + 4*t, which is how GrWar addresses them) and the
// slots are two lists of up to 20 39-byte records at the far pointers DS:7646
// (attackers) and DS:764A (defenders).
#pragma once

#include <stdint.h>

#include "game/dseg.h"

// One slot of a unit list (the ground battle also uses 21 for the commander/base).
typedef struct [[gnu::packed]] GrUnit {
    uint8_t type;      // +0 unit type 1..4
    int16_t count;     // +1 units left (the slot's strength)
    uint8_t rest[36];  // +3 battle state (see GrWar)
} GrUnit;
static_assert(sizeof(GrUnit) == 0x27);

// 1430:000f: screen 0x13 handler
void GrWarUnits_UnitAssignScreen(void);
// 1430:033c
void GrWarUnits_UnitAssignDrawScreen(void);
// 1430:0479
void GrWarUnits_CollectAvailableForces(void);
// 1430:0c14
void GrWarUnits_SetupUnitHotspots(void);
// 1430:0d3c
void GrWarUnits_DrawUnitList(void);
