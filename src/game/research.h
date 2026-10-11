// Unit Research (2d66): the research and design screen (screen 3). 35
// inventions in a 5-column grid of 32x16 icons; clicking one starts or stops
// its development (State 1/2) or analysis (State 3/4), State 5 = done.
#pragma once
#include <stdint.h>
#include "game/dseg.h"

// 2d66:0029
void Research_ResearchScreen(void);
// 2d66:0299
void Research_SetupResearchHotspots(void);
// 2d66:03e9
void Research_DrawInventionInfo(void);
// 2d66:074c
void Research_DrawDeveloperSkills(void);
// 2d66:0891 / 0966: blink the selection frame / reset it to steady colours.
void Research_AnimateSelectionFrame(void);
void Research_ResetSelectionFrame(void);
// 2d66:0a1a
void Research_DrawInventionIcons(void);
// 2d66:0b5c: 31x14 icon `frame` of the CDS picture to dest + 140h in the
// segment starting at seg.
void Research_DrawIconFrame(uint8_t frame, uint16_t dest, uint8_t *seg);
// 2d66:0bae
void Research_AnimateCurrentResearch(void);
// 2d66:0caa / 0d7b: frames 0..10 and 10..20 of the state-change animation
// of invention `inv`; kind is the icon row (1 done, 2 develop, 3 ...).
void Research_PlayIconTransitionOut(int16_t kind, int16_t inv);
void Research_PlayIconTransitionIn(int16_t kind, int16_t inv);

// Skill shortfall of the developer for invention `inv` (AnimateCurrentResearch):
// sum of Skills[k] - DevSkill[k] over the skills where the developer is behind.
int16_t Research_SkillShortfall(int16_t inv);
// True when invention `inv` has progressed far enough for the "working" frames:
// Progress > 10000 - 10000 / (shortfall + 1).
bool Research_ProgressVisible(int16_t inv);
