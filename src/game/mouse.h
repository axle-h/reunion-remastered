// Unit Mouse (4758): mouse state, hotspot hit tests and the software cursor.
//
// The original read the mouse through an INT 33h user callback that wrote
// Mouse::X/Y and the raw button bytes asynchronously. The port does the same
// work in Mouse_Update(), which Mouse_Poll() calls first, and at every
// retrace wait once the mouse moved or a button changed (so the cursor
// follows the mouse during waits that never poll it). Code that polled
// Mouse::BtnDown in a loop without a retrace wait (e.g. 405f:077f) should
// call Mouse_Update() first.
#pragma once

#include <stdint.h>

#include "game/dseg.h"

// 4758:0000 MouseCallback (INT 33h handler), driven from input_mouse().
void Mouse_Update(void);
// 4758:012d CursorBlitHook: hides/redraws the cursor around blits (Blit_HookRow).
void Mouse_CursorBlitHook(void);
// 4758:01aa / 0231 / 0294
void Mouse_CursorDraw(void);
void Mouse_CursorSaveBackground(void);
void Mouse_CursorRestoreBackground(void);
// 4758:0314
void Mouse_InitMouse(void);
// 4758:03e4 / 040a
void Mouse_ShowCursor(void);
void Mouse_HideCursor(void);
// 4758:042d: snapshots buttons and position (Buttons, ClickX/Y, ButtonsChanged).
void Mouse_Poll(void);
// 4758:04c0 (INT 33h AX=4). input.h cannot warp the pointer, so this only
// sets Mouse_X/Y until the next Mouse_Update.
void Mouse_SetMousePos(uint16_t x, uint16_t y);
// 4758:04e5
void Mouse_GridCell(int16_t *col, int16_t *row, uint16_t left, uint16_t top, uint16_t right,
                    uint16_t bottom, uint16_t cellW, uint16_t cellH);
// 4758:05bc, result in *hit
void Mouse_HitTest(uint16_t *hit);
// 4758:06a5, result in *hit
void Mouse_HitTestCursor(int16_t *hit);
// 4758:0749
void Mouse_ClearHotspots(int16_t first, int16_t last);
// 4758:079d: caption is cut to 18 characters (the hotspot's string[18]).
void Mouse_SetHotspot(int16_t index, int16_t x, uint8_t y, int16_t w, uint8_t h,
                      const char *caption, uint8_t repeat_, uint8_t flag);

// Hotspot j (1..66, as in Pascal).
static inline THotspot *Mouse_Hotspot(int16_t j)
{
    return &ds.Mouse_Hotspots[j - 1];
}
