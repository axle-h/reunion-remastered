// Unit Hire (3fab): the staff hiring screen (screen 2, "faces"). Four kinds
// of staff (pilot, builder, fighter, developer), three candidates each. A
// candidate's price is Staff_Price, the "scholarly level" Staff_Level; the
// hired one's current experience lives in Game_StaffExp.
//
// State: Hire_Candidate (0 = none), Hire_StaffKind 1..4 and the other
// Hire_* fields of DS (game/dseg.h).
#pragma once
#include <stdint.h>
#include "game/dseg.h"

// 3fab:0032
void Hire_HireScreen(void);
// 3fab:0321
void Hire_LoadHireBackground(void);
// 3fab:03ba
void Hire_DrawCandidateFrames(void);
// 3fab:048f
void Hire_DrawCandidateInfo(void);
// 3fab:092b
void Hire_SetupHireHotspots(void);

// Outcome of clicking the hire button for candidate `c` (1..3) of `kind`.
typedef enum HireResult {
    HIRE_OK,
    HIRE_NO_CREDITS,     // price above Credits
    HIRE_ALREADY_HIRED,  // c is the hired candidate
    HIRE_NOT_SKILLED,    // the hired candidate is better than c
} HireResult;
// Pure part of HireScreen's hire branch: compares the hired candidate
// (Game_StaffHired[kind - 1]) and Credits with candidate c's rank and price.
HireResult Hire_CheckHire(int16_t kind, int16_t c);
