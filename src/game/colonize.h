// Unit Colonize (2841): screen 15h, setting up a new colony. The player picks
// up to six starter buildings (the total cost starts at 100000 credits) and
// pays; a planet force group is created for the body.
#pragma once

#include <stdint.h>

#include "game/dseg.h"

// 2841:0060
void Colonize_ColonizeScreen(void);
// 2841:04c6
void Colonize_SetupColonizeHotspots(void);
// 2841:062d
void Colonize_DrawColonizeScreen(void);

// The two checks the screen makes before it lets the player pick a starter
// building (not separate routines in the original).
//
// Whether the player may build the type at all: no required invention and
// enough builder experience (a negative requirement is always met), or the
// required invention is developed (State 5).
bool Colonize_TypeAvailable(const TBuildingType *bt);
// ... and the surface type of the body allows it (TBuildingType._12 holds one
// flag per surface type, from byte 13h on).
bool Colonize_SuitsSurface(const TBuildingType *bt, int8_t surfaceType);
