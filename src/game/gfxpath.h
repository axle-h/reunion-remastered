// Unit GfxPath (3a1b): the 12-icon toolbar at the top of the screen and the
// ICON.ALL icon store.
//
// One block (ems.h's Ems_ToolbarBlock, DS:7DFA) holds ICON.ALL (DS:54CA
// bytes), then two 320x33 pages of the bar (page 1 at +1, page 2 at +2940h
// +1 past the icons) and the 48x32 cell backgrounds (240 wide). The slots
// are 21-byte records at DS:7C51 + 21 * slot (slot 1..12): a label string[17]
// at +0 and the command id (word) at +13h.
#pragma once

#include <stdint.h>

#include "game/mem.h"

// 3a1b:0000: Gfx's WaitVRetrace again.
void GfxPath_WaitVRetrace(void);
// 3a1b:0019
void GfxPath_SetToolbarAndShow(int16_t screen);
// 3a1b:0030
void GfxPath_SetToolbar(int16_t screen);
// Port addition: SetToolbar with an explicit list of `count` command ids
// (as for a screen other than 1).
void GfxPath_SetToolbarCommands(const int8_t *ids, int16_t count);
// 3a1b:00bf
void GfxPath_RedrawToolbar(void);
// 3a1b:0366
void GfxPath_AddToolbarIcon(int16_t id);
// 3a1b:0586 / 0643: 21-step scroll to the other page.
void GfxPath_ScrollToolbarNext(void);
void GfxPath_ScrollToolbarPrev(void);
// 3a1b:0704 / 0780
void GfxPath_ShowToolbar(void);
void GfxPath_ShowToolbarInstant(void);
// 3a1b:07ca
bool GfxPath_ToolbarHasIcon(int16_t id);
// 3a1b:0810: ds index (DS:7D72) of the 68 icons in the ICON.ALL copy.
void GfxPath_IconAllIndex(void);
// 3a1b:0864: draws icon number `icon` (an ICON.ALL index, not a command id)
// 40x24 into the 320-wide image at dest + 504h. Port addition: OPTIONS_ICON
// (game/options.h) draws the port's own icon.
void GfxPath_DrawIcon(int16_t icon, FarPtr dest);
// 3a1b:0932
void GfxPath_ShowScreenBackdrop(void);
// 3a1b:09a3 (= Game::GfxPathProc)
void GfxPath_FreeToolbarBuffer(void);
// 3a1b:09b2: allocates the block (needs Gfx_IconAllOpenSize for its size).
void GfxPath_Init(void);
