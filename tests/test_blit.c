// blit: the Blit unit's copies, transparent/flipped blits, fills and cursor
// save/restore on small buffers in the emulated address space.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/blit.h"
#include "game/dseg.h"
#include "game/gfx.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

// Two off-screen 320-wide buffers.
static const FarPtr SRC = (FarPtr)0x50000000;   // 5000:0000
static const FarPtr DST = (FarPtr)0x60000000;   // 6000:0000

static uint8_t *px(FarPtr p, int x, int y)
{
    return (uint8_t *)far_host(p) + y * 320 + x;
}

static int retraces;
static bool count_retrace(void)
{
    retraces++;
    return true;
}

static void reset(void)
{
    mem_init();
    ds.System_SegA000 = 0xA000;
    ds.Gfx_ScreenSeg = 0xA000;
    gfx_wait_retrace = count_retrace;
    retraces = 0;
}

// src 3x3 = 1..9 row-major, with 0 as the transparent key in the centre.
static void pattern3(void)
{
    static const uint8_t p[3][3] = {{1, 2, 3}, {4, 0, 6}, {7, 8, 9}};
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 3; x++)
            *px(SRC, x, y) = p[y][x];
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 3; x++)
            *px(DST, x, y) = 0xEE;
}

static void copies(void)
{
    reset();
    for (int i = 0; i < 8; i++)
        *px(SRC, i, 0) = (uint8_t)(10 + i);
    Blit_CopyBytes(SRC, DST, 5);
    CHECK(SDL_memcmp(px(DST, 0, 0), px(SRC, 0, 0), 5) == 0 && *px(DST, 5, 0) == 0);

    // Word-wise forward copy onto itself shifted by one byte: words are read
    // before they are written, so the pattern repeats in pairs.
    reset();
    for (int i = 0; i < 6; i++)
        *px(SRC, i, 0) = (uint8_t)(1 + i);
    Blit_CopyBytes(SRC, SRC + 1, 4);
    static const uint8_t shifted[6] = {1, 1, 2, 2, 4, 6};
    CHECK(SDL_memcmp(px(SRC, 0, 0), shifted, 6) == 0);

    // CopyRect to a buffer and to the screen (stride 320, hook nil).
    reset();
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 4; x++)
            *px(SRC, x, y) = (uint8_t)(y * 16 + x + 1);
    Blit_CopyRect(SRC, DST, 4, 3);
    CHECK(*px(DST, 3, 2) == 0x24 && *px(DST, 4, 2) == 0 && *px(DST, 0, 3) == 0);
    Blit_CopyRect(SRC, far_ptr(0xA000, 10 * 320 + 5), 4, 3);
    CHECK(vid_screen[10][5] == 0x01 && vid_screen[12][8] == 0x24 && vid_screen[13][5] == 0);
    CHECK(ds.Blit_HookRow == 999);

    // Strided: 2x2 from a 4-wide source into an 8-wide destination.
    reset();
    uint8_t *s = far_host(SRC);
    for (int i = 0; i < 8; i++)
        s[i] = (uint8_t)(i + 1);
    Blit_CopyRectStrided(SRC, DST, 2, 2, 4, 8);
    uint8_t *d = far_host(DST);
    CHECK(d[0] == 1 && d[1] == 2 && d[8] == 5 && d[9] == 6 && d[2] == 0);

    // Copy16x16 to the screen.
    reset();
    for (int y = 0; y < 17; y++)
        for (int x = 0; x < 17; x++)
            *px(SRC, x, y) = (uint8_t)(x + y + 1);
    Blit_Copy16x16(SRC, far_ptr(0xA000, 0));
    CHECK(vid_screen[15][15] == 31 && vid_screen[15][16] == 0 && vid_screen[16][0] == 0);
}

static void transparent(void)
{
    reset();
    pattern3();
    Blit_BlitTransparent(SRC, DST, 3, 3, 0);
    CHECK(*px(DST, 0, 0) == 1 && *px(DST, 1, 1) == 0xEE && *px(DST, 2, 2) == 9);

    reset();
    pattern3();
    Blit_BlitTransparentStrided(SRC, DST, 2, 2, 320, 640, 4);
    CHECK(*px(DST, 0, 0) == 1 && *px(DST, 1, 0) == 2);
    CHECK(*px(DST, 0, 2) == 0xEE && *px(DST, 1, 2) == 0);  // key 4 skipped, 0 copied
    CHECK(*px(DST, 0, 1) == 0xEE);

    // BlitOverBackground: src 40h is transparent, dest >= 60h is protected.
    reset();
    *px(SRC, 0, 0) = 0x10;
    *px(SRC, 1, 0) = 0x40;
    *px(SRC, 2, 0) = 0x11;
    *px(SRC, 3, 0) = 0x12;
    *px(DST, 0, 0) = 0x5F;
    *px(DST, 1, 0) = 0x01;
    *px(DST, 2, 0) = 0x60;
    *px(DST, 3, 0) = 0xFF;
    Blit_BlitOverBackground(SRC, DST, 4, 1);
    CHECK(*px(DST, 0, 0) == 0x10 && *px(DST, 1, 0) == 0x01);
    CHECK(*px(DST, 2, 0) == 0x60 && *px(DST, 3, 0) == 0xFF);
}

static void flips(void)
{
    // Transposed: dest[j][i] = src[i][j].
    reset();
    pattern3();
    Blit_BlitTransposed(SRC, DST, 3, 0);
    static const uint8_t transposed[3][3] = {{1, 4, 7}, {2, 0xEE, 8}, {3, 6, 9}};
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 3; x++)
            CHECK(*px(DST, x, y) == transposed[y][x]);

    // Rotated 180 degrees.
    reset();
    pattern3();
    Blit_BlitRotate180(SRC, DST, 3, 0);
    static const uint8_t rotated[3][3] = {{9, 8, 7}, {6, 0xEE, 4}, {3, 2, 1}};
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 3; x++)
            CHECK(*px(DST, x, y) == rotated[y][x]);

    // Anti-transposed: dest[n-1-j][n-1-i] = src[i][j].
    reset();
    pattern3();
    Blit_BlitAntiTranspose(SRC, DST, 3, 0);
    static const uint8_t anti[3][3] = {{9, 6, 3}, {8, 0xEE, 2}, {7, 4, 1}};
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 3; x++)
            CHECK(*px(DST, x, y) == anti[y][x]);
    CHECK(*px(DST, 3, 0) == 0 && *px(DST, 0, 3) == 0);

    // The helpers on their own: reverse row and upward column.
    reset();
    pattern3();
    Blit_CopyRowTransparentReverse(SRC, DST + 2, 3, 0xFF);
    CHECK(*px(DST, 0, 0) == 3 && *px(DST, 1, 0) == 2 && *px(DST, 2, 0) == 1);
    Blit_CopyColumnTransparentUp(SRC, DST + 2 * 320 + 5, 3, 2);
    CHECK(*px(DST, 5, 2) == 1 && *px(DST, 5, 1) == 0 && *px(DST, 5, 0) == 3);
}

static void fills(void)
{
    reset();
    Blit_FillBytes(DST, 4, 7);
    CHECK(*px(DST, 3, 0) == 7 && *px(DST, 4, 0) == 0);
    // Odd counts fill two bytes too many (inc cx after the odd byte).
    reset();
    Blit_FillBytes(DST, 5, 7);
    CHECK(*px(DST, 6, 0) == 7 && *px(DST, 7, 0) == 0);

    reset();
    Blit_FillRect(10, 20, 3, 2, 0x33);
    CHECK(vid_screen[20][10] == 0x33 && vid_screen[21][12] == 0x33);
    CHECK(vid_screen[20][13] == 0 && vid_screen[22][10] == 0 && vid_screen[19][10] == 0);

    reset();
    Blit_FillRectAt(DST + 1, 2, 3, 0x44);
    CHECK(*px(DST, 1, 0) == 0x44 && *px(DST, 2, 2) == 0x44);
    CHECK(*px(DST, 0, 0) == 0 && *px(DST, 3, 0) == 0 && *px(DST, 1, 3) == 0);
}

static void interlace(void)
{
    reset();
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 4; x++)
            *px(SRC, x, y) = (uint8_t)(y + 1);
    Blit_InterlaceReveal(SRC, DST, 4, 4);
    for (int y = 0; y < 4; y++)
        CHECK(*px(DST, 0, y) == y + 1 && *px(DST, 3, y) == y + 1);
    CHECK(*px(DST, 0, 4) == 0 && *px(DST, 4, 0) == 0);
    CHECK(retraces == 2);  // one per two rows in each pass

    // Odd height: pass 1 also paints the row below the area.
    reset();
    Blit_InterlaceReveal(SRC, DST, 2, 3);
    CHECK(*px(DST, 0, 3) == 0x40);
}

static void cursor(void)
{
    reset();
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 3; x++)
            vid_screen[50 + y][100 + x] = (uint8_t)(0x80 + y * 3 + x);
    Blit_SaveCursorBackground(50 * 320 + 100, 3, 3);
    uint8_t *bg = &ds.Blit_CursorBackground;
    CHECK(bg[0] == 0x80 && bg[4] == 0x84 && bg[8] == 0x88);

    // Image 1, rows 16 bytes apart; 0 stays transparent.
    ds.Mouse_CursorImages[1][0] = 5;
    ds.Mouse_CursorImages[1][1] = 0;
    ds.Mouse_CursorImages[1][16] = 6;
    ds.Mouse_CursorImages[1][17] = 7;
    Blit_DrawCursorImage(50 * 320 + 100, 2, 2, 256);
    CHECK(vid_screen[50][100] == 5 && vid_screen[50][101] == 0x81);
    CHECK(vid_screen[51][100] == 6 && vid_screen[51][101] == 7 && vid_screen[51][102] == 0x85);

    Blit_RestoreCursorBackground(50 * 320 + 100, 3, 3);
    CHECK(vid_screen[50][100] == 0x80 && vid_screen[51][101] == 0x84);
}

int main(void)
{
    copies();
    transparent();
    flips();
    fills();
    interlace();
    cursor();
    return ok ? 0 : 1;
}
