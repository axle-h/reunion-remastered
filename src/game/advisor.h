// Unit Advisor (1991): the advisor dialogue (screen 24, "duma"). The player
// picks a question from a menu (rkerdes1.sp); the answer line comes from
// valasz1.sp, its first two characters naming the next menu.
//
// State: the Advisor_* fields of DS (game/dseg.h); Advisor_Training* is
// story state set by the answers (with the port's courses, game/courses.h).
#pragma once
#include <stdint.h>
#include "game/dseg.h"

// Port improvement (game/courses.h): the answer past valasz1.sp's 15, "no
// University to go to", which leads back to the main menu.
enum : int16_t { ADVISOR_NO_UNIVERSITY = 16 };

// 1991:0000
void Advisor_AdvisorScreen(void);
// 1991:02c9: last of the 9 hints for this advisor whose invention is in
// State 5 (flag) or 1..4 (not flag); 0 if none.
int16_t Advisor_FindAdvisorItem(bool flag);
// 1991:0355
void Advisor_LoadAdvisorPictures(void);
// 1991:0487
void Advisor_DrawAdvisorQuestion(void);
// 1991:0580
void Advisor_ShowAdvisorAnswer(void);
// 1991:08de
void Advisor_LoadAdvisorMenu(void);
// 1991:0946
void Advisor_DrawAdvisorMenu(void);
// 1991:09e8
void Advisor_PollAdvisorMenu(void);
// 1991:0a8b
void Advisor_SetupAdvisorHotspots(void);
