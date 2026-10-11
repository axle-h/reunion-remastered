// Unit Pub (1fd2): the pub (screen 14 and the talk screen): ten people to
// talk to, their question menus and replies, pub deals and the idle
// animations. The screen handlers themselves are in Main (1000:1271); Sim
// (1b8a:006e) uses the deal windows.
//
// State: the Pub_* fields of DS (game/dseg.h).
#pragma once
#include <stdint.h>
#include "game/dseg.h"

// 1fd2:0000 (near): person idx 1..10.
void Pub_DrawPubSprite(int16_t idx);
// 1fd2:00d9
void Pub_LoadPubGraphics(void);
// 1fd2:01ef
void Pub_SetupPubHotspots(void);
// 1fd2:02f4
void Pub_LoadPersonPortrait(void);
// 1fd2:04f0
void Pub_SetupTalkHotspot(void);
// 1fd2:0544
void Pub_ReadQuestionCode(void);
// 1fd2:05fe
void Pub_ShowReply(void);
// 1fd2:0788
void Pub_BuildQuestionMenu(void);
// 1fd2:09ea
void Pub_DrawQuestionMenu(void);
// 1fd2:0a8b
void Pub_UpdateQuestionHover(void);
// 1fd2:0b30
void Pub_UpdateTopicAvailability(void);
// 1fd2:0c2b: *year/month/day/hour += the given amounts (the first four are
// added with 8-bit wrap for hour/day/month), then carried 24h, 30d, 12m.
void Pub_AddToDate(int8_t hour, int8_t day, int8_t month, int16_t year, uint8_t *pHour,
                   uint8_t *pDay, uint8_t *pMonth, int16_t *pYear);
// 1fd2:0c95
void Pub_StartPubDealWindows(void);
// 1fd2:0d2b (near): ((year * 20 + month) * 40 + day) * 30 + hour
int32_t Pub_DateToHours(int32_t year, int32_t month, int32_t day, int32_t hour);
// 1fd2:0d75
int16_t Pub_FindActiveDeal(void);
// 1fd2:0e5f
void Pub_PlayRandomPubSound(void);
// 1fd2:0ec1 (near): animation idx 1..9
void Pub_AnimatePubPerson(int16_t idx);
// 1fd2:0fc2
void Pub_UpdatePubAnimations(void);
