// Unit AlienTalk (2e4b): the alien visit cutscenes (screen 21h) and the
// conversation screen (22h) with its question menu, plus the scheme A
// verifier (see protect.h).
//
// Conversation state: the AlienTalk_* fields of DS (game/dseg.h). Texts:
// TEXT\KERDES<race>.AT (questions) and VALASZ<race>.AT.
#pragma once

#include <stdint.h>

#include "game/dseg.h"

// 2e4b:0000: CurrentScreen := 21h with ScreenSwitch, Scene := scene.
void AlienTalk_StartAlienScene(int16_t scene);
// 2e4b:0044: screen 21h handler. Plays PICS\PIC<scene> and the Command-room
// animation of scenes 1, 2, 9, 10.
void AlienTalk_PlayAlienScene(int16_t scene);
// 2e4b:03a5: CurrentScreen := 22h, Race := race, string at 78B8 := race.
void AlienTalk_StartAlienTalk(int16_t race);
// 2e4b:0400
void AlienTalk_DrawAlienTalkScreen(void);
// 2e4b:0622: one hotspot "Talking" (6,86h, D2h x 3Fh); hover state := -1.
void AlienTalk_SetupTalkHotspots(void);
// 2e4b:0675: 78C2 := value of the first two characters of line 78BE of KERDES<race>.AT.
void AlienTalk_LoadQuestionAnswerId(void);
// 2e4b:0700
void AlienTalk_DrawAlienReply(void);
// 2e4b:0780: loads CurMenu's questions and maps screen lines to questions.
void AlienTalk_BuildQuestionMenu(void);
// 2e4b:088d
void AlienTalk_DrawQuestionMenu(void);
// 2e4b:092e
void AlienTalk_HandleTalkMouse(void);
// 2e4b:09d3: scheme A verifier (called from the main loop).
void AlienTalk_VerifyManualAnswer(void);
