// mouse: hotspot tables, hit tests, cursor drawing and the poll snapshot logic.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/crt.h"
#include "game/dos.h"
#include "game/dseg.h"
#include "game/mouse.h"
#include "platform/input.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static const uint8_t cap_a[] = {3, 'a', 'b', 'c'};
static const uint8_t cap_long[] = {20, 'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T'};

static void hotspots(void)
{
    mem_init();
    Mouse_ClearHotspots(1, 66);
    CHECK(ds.Mouse_Hotspots[0].W == 0 && ds.Mouse_Hotspots[65].Caption.len == 0);

    // Element 1 lives at DS:8A8C.
    Mouse_SetHotspot(1, 100, 50, 20, 10, cap_a, 1, 4);
    CHECK((uint8_t *)&ds.Mouse_Hotspots[0] == (uint8_t *)ds_at(0x8A8C));
    CHECK(ds.Mouse_Hotspots[0].X == 100 && ds.Mouse_Hotspots[0].Y == 50);
    CHECK(ds.Mouse_Hotspots[0].Caption.len == 3 && ds.Mouse_Hotspots[0].Repeat_ == 1
          && ds.Mouse_Hotspots[0]._1A == 4);
    // Overlapping hotspot 2 covers hotspot 1's right half; lowest index wins.
    Mouse_SetHotspot(2, 110, 50, 20, 10, cap_long, 0, 2);
    CHECK(ds.Mouse_Hotspots[1].Caption.len == 18);
    ds.Mouse_HotspotCount = 2;

    int16_t hit;
    struct { int x, y, want; } cases[] = {
        {100, 50, 21}, {119, 59, 21}, {120, 59, 22}, {129, 59, 22}, {130, 59, 0},
        {99, 55, 0}, {105, 49, 0}, {105, 60, 0}, {110, 50, 21},
    };
    for (size_t i = 0; i < SDL_arraysize(cases); i++) {
        ds.Mouse_X = (int16_t)cases[i].x;
        ds.Mouse_Y = (int16_t)cases[i].y;
        Mouse_HitTestCursor(&hit);
        CHECK(hit == cases[i].want);
        // HitTest uses the click position and also knows the fixed regions.
        ds.Mouse_ClickX = ds.Mouse_X;
        ds.Mouse_ClickY = ds.Mouse_Y;
        uint16_t h2;
        Mouse_HitTest(&h2);
        CHECK(h2 == cases[i].want);
    }
    // HotspotCount limits the scan.
    ds.Mouse_HotspotCount = 1;
    ds.Mouse_X = 125; ds.Mouse_Y = 55;
    Mouse_HitTestCursor(&hit);
    CHECK(hit == 0);
    ds.Mouse_HotspotCount = 2;

    // Fixed regions: menu bar 48x33 cells (1..6), top-right corner 13, clock 14.
    struct { int x, y, want; } fixed[] = {
        {0, 0, 1}, {47, 32, 1}, {48, 0, 2}, {287, 10, 6}, {200, 40, 0}, {0, 33, 0},
        {0x120, 0x20, 13}, {319, 0, 13}, {0x120, 0x21, 0}, {0xDA, 0x22, 14}, {0xDA, 0x2D, 14},
        {0xDA, 0x2E, 0}, {0xD9, 0x22, 0},
    };
    for (size_t i = 0; i < SDL_arraysize(fixed); i++) {
        ds.Mouse_ClickX = (int16_t)fixed[i].x;
        ds.Mouse_ClickY = (int16_t)fixed[i].y;
        uint16_t h;
        Mouse_HitTest(&h);
        CHECK(h == fixed[i].want);
    }

    int16_t col, row;
    ds.Mouse_ClickX = 75; ds.Mouse_ClickY = 40;
    Mouse_GridCell(&col, &row, 50, 20, 149, 119, 10, 25);
    CHECK(col == 3 && row == 1);
    ds.Mouse_ClickY = 100;
    Mouse_GridCell(&col, &row, 50, 20, 149, 119, 10, 25);
    CHECK(col == 3 && row == 4);
    ds.Mouse_ClickX = 150;
    Mouse_GridCell(&col, &row, 50, 20, 149, 119, 10, 25);
    CHECK(col == 0 && row == 0);

    Mouse_ClearHotspots(1, 2);
    CHECK(ds.Mouse_Hotspots[0].W == 0 && ds.Mouse_Hotspots[1].Caption.len == 0
          && ds.Mouse_Hotspots[1]._1A == 0);
}

static void cursor(void)
{
    mem_init();
    uint8_t *scr = &vid_screen[0][0];
    for (int i = 0; i < 320 * 200; i++)
        scr[i] = (uint8_t)(i % 251 + 1);
    // Shape 2: column x (0..10) of every row has colour x+1 except column 5 (transparent).
    for (int r = 0; r < 16; r++)
        for (int c = 0; c < 16; c++)
            ds.Mouse_CursorImages[2][r * 16 + c] = c == 5 ? 0 : (uint8_t)(100 + c);
    ds.Mouse_CursorShape = 2;
    static uint8_t orig[320 * 200];
    SDL_memcpy(orig, scr, sizeof orig);

    ds.Mouse_X = 20; ds.Mouse_Y = 30;
    ds.Mouse_CursorState = 1;
    Mouse_ShowCursor();
    CHECK(ds.Mouse_CursorState == 0);
    CHECK(scr[30 * 320 + 20] == 100 && scr[30 * 320 + 25] == orig[30 * 320 + 25]);
    CHECK(scr[40 * 320 + 30] == 110 && scr[41 * 320 + 20] == orig[41 * 320 + 20]);
    CHECK(ds.Mouse_CursorSaveX == 20 && ds.Mouse_CursorSaveY == 30);
    Mouse_HideCursor();
    CHECK(ds.Mouse_CursorState == 1);
    CHECK(SDL_memcmp(scr, orig, sizeof orig) == 0);

    // Clipped at the bottom right corner: box is 2 wide, 2 high.
    ds.Mouse_X = 318; ds.Mouse_Y = 198;
    Mouse_ShowCursor();
    CHECK(scr[198 * 320 + 318] == 100 && scr[199 * 320 + 319] == 101);
    Mouse_HideCursor();
    CHECK(SDL_memcmp(scr, orig, sizeof orig) == 0);

    // Frozen cursor is neither shown nor hidden.
    ds.Mouse_CursorFrozen = 1;
    ds.Mouse_X = 20; ds.Mouse_Y = 30;
    Mouse_ShowCursor();
    CHECK(ds.Mouse_CursorState == 1 && SDL_memcmp(scr, orig, sizeof orig) == 0);
    ds.Mouse_CursorFrozen = 0;

    // Blit hook: a blit row inside Y..Y+11 hides the cursor, 999 brings it back.
    Mouse_ShowCursor();
    ds.Blit_HookRow = 35;
    Mouse_CursorBlitHook();
    CHECK(ds.Mouse_CursorFrozen == 1 && ds.Mouse_CursorState == 0);
    CHECK(SDL_memcmp(scr, orig, sizeof orig) == 0);
    ds.Blit_HookRow = 999;
    Mouse_CursorBlitHook();
    CHECK(ds.Mouse_CursorFrozen == 0 && scr[30 * 320 + 20] == 100);
    Mouse_HideCursor();

    // InitMouse puts the pointer at 160,70 and installs the hook.
    Mouse_InitMouse();
    CHECK(ds.Mouse_Present == 1 && ds.Mouse_X == 160 && ds.Mouse_Y == 70);
    CHECK(ds.Blit_CursorHook == far_ptr(0x4758, 0x012D) && ds.Mouse_CursorState == 1);
}

static void button(int b, bool down, float x, float y)
{
    SDL_Event e = {.button = {.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP,
                             .button = b, .down = down, .x = x, .y = y}};
    SDL_PushEvent(&e);
}

static void poll(void)
{
    mem_init();
    ds.Mouse_CursorState = 1;  // hidden: the poll must not draw
    Mouse_SetHotspot(1, 150, 90, 20, 20, cap_a, 0, 3);
    ds.Mouse_HotspotCount = 1;

    int ww, wh;
    SDL_GetWindowSize(SDL_GetRenderWindow(vid_renderer()), &ww, &wh);
    float cx = ww / 2.0f, cy = wh / 2.0f;  // game pixel 160,100

    button(SDL_BUTTON_LEFT, true, cx, cy);
    input_pump();
    Mouse_Poll();
    CHECK(SDL_abs(ds.Mouse_X - 160) <= 2 && SDL_abs(ds.Mouse_Y - 100) <= 2);
    CHECK(ds.Mouse_BtnDown == 1 && ds.Mouse_BtnLeft == 1 && ds.Mouse_BtnRight == 0);
    CHECK(ds.Mouse_Buttons == 1 && ds.Mouse_ButtonsChanged == 1);
    CHECK(ds.Mouse_ClickX == ds.Mouse_X && ds.Mouse_ClickY == ds.Mouse_Y);
    CHECK(ds.Mouse_HotspotUnder == 21 && ds.Mouse_CursorShape == 3);
    CHECK(*(uint8_t *)ds_at(0x8A86) == 1 && *(uint8_t *)ds_at(0x8A85) == 0);
    CHECK(*(uint8_t *)ds_at(0x8A7A) == 1);

    Mouse_Poll();  // held: no change
    CHECK(ds.Mouse_ButtonsChanged == 0 && *(uint8_t *)ds_at(0x8A86) == 0);

    button(SDL_BUTTON_LEFT, false, cx, cy);
    button(SDL_BUTTON_RIGHT, true, 0, 0);
    input_pump();
    Mouse_Poll();  // left up, right down: still "a button down", no change
    CHECK(ds.Mouse_BtnDown == 1 && ds.Mouse_BtnRight == 1 && ds.Mouse_BtnLeft == 0);
    CHECK(ds.Mouse_ButtonsChanged == 0 && *(uint8_t *)ds_at(0x8A7B) == 1);
    CHECK(ds.Mouse_HotspotUnder == 0 && ds.Mouse_CursorShape == 0);

    button(SDL_BUTTON_RIGHT, false, 0, 0);
    input_pump();
    Mouse_Poll();
    CHECK(ds.Mouse_Buttons == 0 && ds.Mouse_ButtonsChanged == 1);
    CHECK(*(uint8_t *)ds_at(0x8A85) == 1 && *(uint8_t *)ds_at(0x8A86) == 0);

    // While the cursor is being redrawn (state 2) the position is not updated.
    ds.Mouse_CursorState = 2;
    ds.Mouse_X = 77;
    button(SDL_BUTTON_LEFT, true, cx, cy);
    input_pump();
    Mouse_Update();
    CHECK(ds.Mouse_X == 77);

    // Poll swallows one key (an extended key takes two reads).
    ds.Mouse_CursorState = 1;
    SDL_Event k = {.key = {.type = SDL_EVENT_KEY_DOWN, .key = SDLK_F2, .down = true}};
    SDL_PushEvent(&k);
    input_pump();
    CHECK(Crt_KeyPressed());
    Mouse_Poll();
    CHECK(!Crt_KeyPressed());
}

int main(void)
{
    hotspots();
    cursor();

    uint8_t res[256], name[] = {10, 'r', 'e', 'u', 'n', 'i', 'o', 'n', '_', 't', 'x'};
    SDL_setenv_unsafe("REUNION_TX", "hello", 1);
    Dos_GetEnv(res, name);
    CHECK(res[0] == 5 && SDL_memcmp(res + 1, "hello", 5) == 0);
    uint8_t none[] = {6, 'N', 'O', 'S', 'U', 'C', 'H'};
    Dos_GetEnv(res, none);
    CHECK(res[0] == 0);

    SDL_setenv_unsafe("SDL_VIDEO_DRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_VIDEO) && vid_init("test")) {
        input_init();
        poll();
        vid_shutdown();
    } else {
        printf("no video, poll test skipped\n");
    }
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
