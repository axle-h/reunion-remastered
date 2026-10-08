// Unit Advisor (1991): the advisor dialogue (screen 24, "duma"). The player
// picks a question from a menu (rkerdes1.sp); the answer line comes from
// valasz1.sp, its first two characters naming the next menu.
//
// Unnamed state (DS): 771C menu number (0 = leave), 771E question id, 7720
// answer id, 7722 far ^array[1..5] of string[60] (the menu lines), 7E3C
// advisor kind 1..4 (set where the screen is opened), 7E3E highlighted menu
// line; 5D9C/5D9E/5DA0/5DA2 story state set by the answers (5D9E = kind that
// is on strike).
#pragma once
#include <stdint.h>
#include "game/dseg.h"

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
