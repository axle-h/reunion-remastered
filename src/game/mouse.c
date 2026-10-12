#include "game/mouse.h"

#include "game/crt.h"
#include "platform/input.h"
#include "platform/timing.h"

#if __has_include("game/blit.h")
#include "game/blit.h"
#else
// Routines missing from their units' headers.
void Blit_DrawCursorImage(uint16_t screenOfs, int16_t width, int16_t rows, uint16_t imageOfs);
void Blit_SaveCursorBackground(uint16_t screenOfs, int16_t width, int16_t rows);
void Blit_RestoreCursorBackground(uint16_t screenOfs, int16_t width, int16_t rows);
#endif

// Does the blit row Blit_HookRow touch the cursor's rows Y..Y+11? The original
// compares the signed row with Y as an unsigned word, so negative rows never match.
static bool hook_row_overlaps(void)
{
    int32_t row = ds.Blit_HookRow;
    uint16_t y = (uint16_t)ds.Mouse_Y;
    return row <= (int32_t)(uint16_t)(y + 11) && (int32_t)y <= row;
}

// 4758:0000 MouseCallback. In the original this ran from INT 33h with
// CX/DX = position and BX = buttons (bit 0 left, bit 1 right).
static void callback(void)
{
    InputMouse m = input_mouse();

    if (ds.Mouse_CursorState != 2) {
        ds.Mouse_X = (int16_t)m.x;
        ds.Mouse_Y = (int16_t)m.y;
    }
    // BX bit 0 is the left button, bit 1 the right one.
    ds.Mouse_BtnDown = m.left || m.right;
    ds.Mouse_BtnLeft = m.left;
    ds.Mouse_BtnRight = m.right;
    if (!ds.Mouse_BtnDown)
        ds.Mouse_LastPushable = 0;

    int16_t under;
    Mouse_HitTestCursor(&under);
    ds.Mouse_HotspotUnder = (uint16_t)under;
    if (ds.Mouse_HotspotUnder < 20)
        ds.Mouse_CursorShape = 0;
    else
        ds.Mouse_CursorShape = Mouse_Hotspot((int16_t)(ds.Mouse_HotspotUnder - 20))->_1A;

    // 999 (end of blit) never overlaps, so this also unfreezes the cursor.
    if (hook_row_overlaps()) {
        if (!ds.Mouse_CursorFrozen && ds.Mouse_CursorState == 0)
            Mouse_CursorRestoreBackground();
        ds.Mouse_CursorFrozen = true;
    } else {
        if (ds.Mouse_CursorFrozen && ds.Mouse_CursorState == 0) {
            Mouse_CursorSaveBackground();
            Mouse_CursorDraw();
        }
        ds.Mouse_CursorFrozen = false;
    }
    if (ds.Mouse_CursorState == 0 && !ds.Mouse_CursorFrozen
        && (ds.Mouse_X != ds.Mouse_CursorSaveX || ds.Mouse_Y != ds.Mouse_CursorSaveY)) {
        Mouse_CursorRestoreBackground();
        Mouse_CursorSaveBackground();
        Mouse_CursorDraw();
    }
}

void Mouse_Update(void)
{
    // The port's mouse state only changes when events are pumped (at
    // retrace); busy polling loops get a retrace every few calls.
    timing_idle_poll();
    callback();
}

// Every retrace: the driver called back on a move or a button change, also
// while the game was busy elsewhere (typing text, fading, waiting), so the
// cursor kept following the mouse. Only once Mouse_InitMouse installed it.
// A change is against what the callback last wrote to DS, so a mouse at
// rest where DS has it does nothing (as the diff harness sets it up).
static void retrace_hook(void)
{
    if (!ds.Mouse_Present)
        return;
    InputMouse m = input_mouse();
    if (m.x != ds.Mouse_X || m.y != ds.Mouse_Y || m.left != ds.Mouse_BtnLeft
        || m.right != ds.Mouse_BtnRight)
        callback();
}

// 4758:012d CursorBlitHook (Blit_CursorHook)
void Mouse_CursorBlitHook(void)
{
    uint16_t state = ds.Mouse_CursorState;
    ds.Mouse_CursorState = 2;
    if (hook_row_overlaps()) {
        if (!ds.Mouse_CursorFrozen && state == 0)
            Mouse_CursorRestoreBackground();
        ds.Mouse_CursorState = state;
        ds.Mouse_CursorFrozen = true;
        return;
    }
    if (ds.Mouse_CursorFrozen && state == 0) {
        Mouse_CursorSaveBackground();
        Mouse_CursorDraw();
    }
    ds.Mouse_CursorFrozen = false;
    ds.Mouse_CursorState = state;
}

// Width/height of the part of the 11x11 cursor box at (x, y) that is on screen.
// Draw and restore share the original's (odd) test for negative coordinates;
// save has none. The pointer is clamped to 0..318 / 0..198, so only the
// right/bottom clipping matters in practice.
static void clip_box(int16_t x, int16_t y, bool negative_test, int16_t *w, int16_t *h)
{
    uint16_t ux = (uint16_t)x, uy = (uint16_t)y;
    *w = 11;
    if ((negative_test && ux > 0xFFF4) || (uint16_t)(ux + 11) > 320)
        *w = (int16_t)(320 - x);
    *h = 11;
    if (negative_test) {
        int iy = (uy > 0xFFF4) - (y == -11);
        if (iy > 0 || (iy >= 0 && (uint16_t)(uy + 10) > 200))
            *h = (int16_t)(200 - y);
    } else if ((uint16_t)(uy + 10) > 200) {
        *h = (int16_t)(200 - y);
    }
}

// 4758:01aa: draws the cursor sprite clipped to the screen.
void Mouse_CursorDraw(void)
{
    int16_t w, h;
    ds.Mouse_DrawState = 1;
    clip_box(ds.Mouse_X, ds.Mouse_Y, true, &w, &h);
    Blit_DrawCursorImage((uint16_t)(ds.Mouse_Y * 320 + ds.Mouse_X), w, h,
                         (uint16_t)(ds.Mouse_CursorShape << 8));
}

// 4758:0231: saves the screen under the cursor.
void Mouse_CursorSaveBackground(void)
{
    int16_t w, h;
    ds.Mouse_DrawState = 2;
    clip_box(ds.Mouse_X, ds.Mouse_Y, false, &w, &h);
    Blit_SaveCursorBackground((uint16_t)(ds.Mouse_Y * 320 + ds.Mouse_X), w, h);
    ds.Mouse_CursorSaveX = ds.Mouse_X;
    ds.Mouse_CursorSaveY = ds.Mouse_Y;
}

// 4758:0294: restores the background saved at CursorSaveX/Y.
void Mouse_CursorRestoreBackground(void)
{
    int16_t w, h;
    ds.Mouse_DrawState = 3;
    clip_box(ds.Mouse_CursorSaveX, ds.Mouse_CursorSaveY, true, &w, &h);
    Blit_RestoreCursorBackground((uint16_t)(ds.Mouse_CursorSaveY * 320 + ds.Mouse_CursorSaveX), w, h);
}

// 4758:0314: the INT 33h setup (reset, mickeys, range 0..318 x 0..198) is
// input.h's job; SDL always has a mouse. The callback runs from the retrace
// wait (retrace_hook).
void Mouse_InitMouse(void)
{
    ds.Mouse_Present = 1;
    ds.Mouse_CursorState = 1;
    Mouse_SetMousePos(160, 70);
    ds.Mouse_X = 160;
    ds.Mouse_Y = 70;
    Mouse_CursorSaveBackground();
    ds.Blit_CursorHook = Mouse_CursorBlitHook;
    ds.Mouse_CursorHidden = 0;
    timing_set_mouse_hook(retrace_hook);
}

// 4758:03e4
void Mouse_ShowCursor(void)
{
    if (ds.Mouse_CursorState != 0 && !ds.Mouse_CursorFrozen) {
        ds.Mouse_CursorState = 2;
        Mouse_CursorSaveBackground();
        Mouse_CursorDraw();
        ds.Mouse_CursorState = 0;
    }
}

// 4758:040a
void Mouse_HideCursor(void)
{
    if (ds.Mouse_CursorState == 0 && !ds.Mouse_CursorFrozen) {
        ds.Mouse_CursorState = 2;
        Mouse_CursorRestoreBackground();
        ds.Mouse_CursorState = 1;
    }
}

// 4758:042d
void Mouse_Poll(void)
{
    Mouse_Update();

    uint8_t old = ds.Mouse_Buttons;
    ds.Mouse_Buttons = ds.Mouse_BtnDown;
    ds.Mouse_ClickX = ds.Mouse_X;
    ds.Mouse_ClickY = ds.Mouse_Y;
    ds.Mouse_QuitFlag = 0;
    if (ds.Mouse_BtnDown) {
        ds.Mouse_HeldLeft = ds.Mouse_BtnLeft;
        ds.Mouse_HeldRight = ds.Mouse_BtnRight;
    }
    // Swallows one key press (an extended key takes two reads).
    if (Crt_KeyPressed() && Crt_ReadKey() == 0)
        Crt_ReadKey();
    ds.Mouse_ButtonsChanged = old != ds.Mouse_Buttons;
    ds.Mouse_Released = ds.Mouse_ButtonsChanged && ds.Mouse_Buttons == 0;
    ds.Mouse_Pressed = ds.Mouse_ButtonsChanged && ds.Mouse_Buttons != 0;
}

// 4758:04c0
void Mouse_SetMousePos(uint16_t x, uint16_t y)
{
    ds.Mouse_X = (int16_t)x;
    ds.Mouse_Y = (int16_t)y;
}

// 4758:04e5: 1-based cell of the last click inside the rect, else 0, 0.
void Mouse_GridCell(int16_t *col, int16_t *row, uint16_t left, uint16_t top, uint16_t right,
                    uint16_t bottom, uint16_t cellW, uint16_t cellH)
{
    int x = ds.Mouse_ClickX, y = ds.Mouse_ClickY;
    if (x >= left && x <= right && y >= top && y <= bottom) {
        *col = (int16_t)((x - left) / cellW + 1);
        *row = (int16_t)((y - top) / cellH + 1);
        return;
    }
    *col = 0;
    *row = 0;
}

// Is (x, y) inside hotspot j? End coordinates wrap at 16 bits like the original.
static bool in_hotspot(int16_t j, int x, int y)
{
    const THotspot *h = Mouse_Hotspot(j);
    return h->X <= x && x <= (int16_t)(h->X + h->W - 1)
           && h->Y <= (uint16_t)y && (uint16_t)y <= (uint16_t)(h->Y + h->H - 1);
}

// 4758:05bc: what is under the last click.
void Mouse_HitTest(uint16_t *hit)
{
    int16_t col, row;
    Mouse_GridCell(&col, &row, 0, 0, 0x11F, 0x20, 0x30, 0x21);
    if (col != 0 && col <= 6 && row == 1) {
        *hit = (uint16_t)col;
    } else if (!(ds.Mouse_ClickX < 0x120 || ds.Mouse_ClickY > 0x20)) {
        *hit = 13;
    } else if (!(ds.Mouse_ClickX < 0xDA || ds.Mouse_ClickY < 0x22 || ds.Mouse_ClickY > 0x2D)) {
        *hit = 14;
    } else {
        *hit = 0;
        // Scans downwards, so the lowest matching index wins.
        for (int16_t j = ds.Mouse_HotspotCount; j >= 1; j--)
            if (in_hotspot(j, ds.Mouse_ClickX, ds.Mouse_ClickY))
                *hit = (uint16_t)(j + 20);
    }
}

// 4758:06a5
void Mouse_HitTestCursor(int16_t *hit)
{
    *hit = 0;
    for (int16_t j = ds.Mouse_HotspotCount; j >= 1; j--)
        if (in_hotspot(j, ds.Mouse_X, ds.Mouse_Y))
            *hit = (int16_t)(j + 20);
}

// 4758:0749
void Mouse_ClearHotspots(int16_t first, int16_t last)
{
    for (int16_t j = first; j <= last; j++) {
        THotspot *h = Mouse_Hotspot(j);
        h->X = 0;
        h->Y = 0;
        h->W = 0;
        h->H = 0;
        h->Caption.len = 0;
        h->Repeat_ = 0;
        h->_1A = 0;
    }
}

// 4758:079d
void Mouse_SetHotspot(int16_t index, int16_t x, uint8_t y, int16_t w, uint8_t h,
                      const uint8_t *caption, uint8_t repeat_, uint8_t flag)
{
    THotspot *s = Mouse_Hotspot(index);
    s->X = x;
    s->Y = y;
    s->W = w;
    s->H = h;
    // StrStore to string[18]: truncates.
    uint8_t n = caption[0] > 18 ? 18 : caption[0];
    s->Caption.len = n;
    for (int i = 0; i < n; i++)
        s->Caption.s[i] = (char)caption[1 + i];
    s->Repeat_ = repeat_;
    s->_1A = flag;
}
