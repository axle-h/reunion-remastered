// Unit Command (3abd): the command-centre room (screen 1), its door and
// panel animations, the ambient lights, the staff icons, the hologram and the
// two ending sequences.
//
// Data segment state without names in dseg.h (all in the 7E08..7E44 block
// except where noted):
//   7E08 far  mainface sprite sheet (E600h)     7E0C far  heroes sheet (4C40h)
//   7E10 far  64000-byte off-screen copy        7E14 int  light sprite frame
//   7E16 int  light countdown                   7E18 far  maina5 sheet (5280h)
//   7E1C int  light 2 frame, 7E1E its countdown 7E20 int  door timer
//   7E22 int  door frame countdown              7E24 far  maina4 door strips
//   7E28 byte door direction                    7E29 byte show the hologram on entry
//   7E2A int  current anim id, 7E2C frame, 7E2E delay, 7E30 tick counter,
//   7E32 repeat count, 7E34 speed               7E36 far  war anim buffer
//   7E3A byte first backdrop already shown      7E3C int  staff kind picked
//   7E40 far  anim record, 7E44 far second buffer
//   89A0[1..4] int  staff kinds behind hotspots 1..4, 89AA int their count
#pragma once

#include <stdint.h>

#include "game/dseg.h"

// 3abd:005c: per-frame handler of screen 1.
void Command_HandleCommandCentre(void);
// 3abd:0275
void Command_DrawCommandBackdrop(void);
// 3abd:0352
void Command_LoadCommandGfx(void);
// 3abd:04b4: plays the hologram panel with wireframe model `vec` until a
// key or click.
void Command_ShowHologram(int16_t vec);
// 3abd:08e0
void Command_SetupCommandHotspots(void);
// 3abd:0ba1 / 0bdc
void Command_InitRoomAnim(void);
void Command_FreeRoomAnim(void);
// 3abd:0c05 / 0c55 / 0c70 / 0e5d
void Command_StartMainAnim(int16_t anim);
void Command_StopMainAnim(void);
void Command_TickRoomAnim(void);
void Command_StepMainAnim(void);
// 3abd:0ed5 / 0efe / 0f19
void Command_FreeGrRoomAnim(void);
void Command_StopGrAnim(void);
void Command_TickGrAnim(void);
// 3abd:102c / 1122 / 1221: door panels sliding open.
void Command_OpenPanelMaina1(void);
void Command_OpenPanelMaina2(void);
void Command_OpenPanelMaina3And4(void);
// 3abd:1395 / 14c5: called every frame (and from GfxPath's toolbar scroll).
void Command_TickPanelDoorAnim(void);
void Command_TickAmbientLights(void);
// 3abd:163f / 167d
void Command_DrawHeroPortrait(void);
void Command_DrawHeroPortraitToBuffer(void);
// 3abd:16bc / 1794
void Command_DrawStaffIcon(int16_t slot);
void Command_DrawAllStaffIcons(void);
// 3abd:17dd / 1a47 (neither returns)
[[noreturn]] void Command_ShowDeathEnding(void);
[[noreturn]] void Command_FadeOutAndHalt(void);
