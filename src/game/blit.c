#include "game/blit.h"

#include "game/dseg.h"
#include "game/gfx.h"
#include "game/mouse.h"

// Byte at p + off, with the offset wrapping inside p's segment like an 8086
// 16-bit address computation.
static inline uint8_t *at(FarPtr p, uint16_t off)
{
    return far_host(far_ptr((uint16_t)(p >> 16), (uint16_t)((uint16_t)p + off)));
}

// Far pointer p advanced by off bytes within its segment.
static inline FarPtr add(FarPtr p, uint16_t off)
{
    return far_ptr((uint16_t)(p >> 16), (uint16_t)((uint16_t)p + off));
}

static inline uint16_t seg_of(FarPtr p)
{
    return (uint16_t)(p >> 16);
}

// call far [Blit_CursorHook]. The only value the game stores there is
// Mouse's CursorBlitHook (Mouse_InitMouse); before that the pointer is nil
// and the original would jump to 0000:0000, so there is nothing to call.
static void call_cursor_hook(void)
{
    if (ds.Blit_CursorHook == far_ptr(0x4758, 0x012D))
        Mouse_CursorBlitHook();
}

// 47da:0000
void Blit_CopyBytes(FarPtr src, FarPtr dest, uint16_t count)
{
    uint16_t s = 0, d = 0;
    if (count & 1) {
        *at(dest, d++) = *at(src, s++);
    }
    // rep movsw: each word is read before it is written, which matters
    // (like the original) when the ranges overlap.
    for (uint16_t n = count >> 1; n != 0; n--) {
        uint8_t lo = *at(src, s), hi = *at(src, (uint16_t)(s + 1));
        *at(dest, d) = lo;
        *at(dest, (uint16_t)(d + 1)) = hi;
        s += 2;
        d += 2;
    }
}

// 47da:0025
void Blit_CopyRect(FarPtr src, FarPtr dest, int16_t width, int16_t rows)
{
    uint16_t ofs = 0;
    if (seg_of(dest) == ds.System_SegA000) {
        // The cursor hook gets the screen row about to be overwritten.
        uint16_t row = (uint16_t)dest / 320;
        uint16_t last = (uint16_t)(row + rows - 1);
        if (row <= last) {
            for (;;) {
                ds.Blit_HookRow = (int16_t)row;
                call_cursor_hook();
                Blit_CopyBytes(add(src, ofs), add(dest, ofs), (uint16_t)width);
                ofs += 320;
                if (row == last)
                    break;
                row++;
            }
        }
    } else {
        for (uint16_t i = 0;; i++) {
            Blit_CopyBytes(add(src, ofs), add(dest, ofs), (uint16_t)width);
            ofs += 320;
            if (i == (uint16_t)(rows - 1))
                break;
        }
    }
    ds.Blit_HookRow = 999;
    call_cursor_hook();
}

// 47da:00de
void Blit_CopyRectStrided(FarPtr src, FarPtr dest, int16_t width, int16_t rows,
                          int16_t srcStride, int16_t destStride)
{
    uint16_t s = 0, d = 0;
    for (uint16_t i = 0;; i++) {
        Blit_CopyBytes(add(src, s), add(dest, d), (uint16_t)width);
        s += (uint16_t)srcStride;
        d += (uint16_t)destStride;
        if (i == (uint16_t)(rows - 1))
            break;
    }
}

// 47da:0135: dest is assumed to be on screen (no segment test).
void Blit_Copy16x16(FarPtr src, FarPtr dest)
{
    uint16_t row = (uint16_t)dest / 320;
    uint16_t last = (uint16_t)(row + 15);
    if (row <= last) {
        for (;;) {
            ds.Blit_HookRow = (int16_t)row;
            call_cursor_hook();
            Blit_CopyBytes(src, dest, 16);
            src = add(src, 320);
            dest = add(dest, 320);
            if (row == last)
                break;
            row++;
        }
    }
    ds.Blit_HookRow = 999;
    call_cursor_hook();
}

// 47da:01b6: pass 1 copies the even rows top-down and paints the odd rows
// colour 40h; pass 2 fills in the odd rows bottom-up. One retrace per two rows.
void Blit_InterlaceReveal(FarPtr src, FarPtr dest, int16_t width, int16_t rows)
{
    uint16_t ofs = 0;
    uint16_t half = (uint16_t)((uint16_t)(rows - 1) >> 1);
    if (half != 0xFFFF) {
        for (uint16_t i = 1;; i++) {
            Blit_CopyBytes(add(src, ofs), add(dest, ofs), (uint16_t)width);
            Blit_FillBytes(add(dest, (uint16_t)(ofs + 320)), (uint16_t)width, 0x40);
            ofs += 640;
            if (i & 1)
                gfx_wait_retrace();
            if (i == (uint16_t)(half + 1))
                break;
        }
    }
    uint16_t pairs = (uint16_t)rows >> 1;
    ofs = (uint16_t)(pairs * 640 - 320);
    if (pairs != 0) {
        for (uint16_t i = 1;; i++) {
            Blit_CopyBytes(add(src, ofs), add(dest, ofs), (uint16_t)width);
            ofs -= 640;
            if (i & 1)
                gfx_wait_retrace();
            if (i == pairs)
                break;
        }
    }
}

// The four transparent copy loops differ only in the destination step.
static void copy_transparent(FarPtr src, FarPtr dest, uint16_t count, uint8_t key, uint16_t step)
{
    uint16_t s = 0, d = 0;
    do {
        uint8_t b = *at(src, s++);
        if (b != key)
            *at(dest, d) = b;
        d += step;
        count--;
    } while (count != 0);
}

// 47da:02ae
void Blit_CopyRowTransparent(FarPtr src, FarPtr dest, uint16_t count, uint8_t key)
{
    copy_transparent(src, dest, count, key, 1);
}

// 47da:02da
void Blit_BlitTransparent(FarPtr src, FarPtr dest, int16_t width, int16_t rows, uint8_t key)
{
    uint16_t ofs = 0;
    for (uint16_t i = 0;; i++) {
        Blit_CopyRowTransparent(add(src, ofs), add(dest, ofs), (uint16_t)width, key);
        ofs += 320;
        if (i == (uint16_t)(rows - 1))
            break;
    }
}

// 47da:0329
void Blit_BlitTransparentStrided(FarPtr src, FarPtr dest, int16_t width, int16_t rows,
                                 int16_t srcStride, int16_t destStride, uint8_t key)
{
    uint16_t s = 0, d = 0;
    for (uint16_t i = 0;; i++) {
        Blit_CopyRowTransparent(add(src, s), add(dest, d), (uint16_t)width, key);
        s += (uint16_t)srcStride;
        d += (uint16_t)destStride;
        if (i == (uint16_t)(rows - 1))
            break;
    }
}

// 47da:0384: skips src colour 40h and never overwrites dest colours >= 60h
// (the interface colours).
void Blit_BlitOverBackground(FarPtr src, FarPtr dest, int16_t width, int16_t rows)
{
    uint16_t row_ofs = 0;
    for (uint16_t y = 0;; y++) {
        for (uint16_t x = 0;; x++) {
            uint16_t o = (uint16_t)(x + row_ofs);
            if (*at(dest, o) < 0x60 && *at(src, o) != 0x40)
                *at(dest, o) = *at(src, o);
            if (x == (uint16_t)(width - 1))
                break;
        }
        row_ofs += 320;
        if (y == (uint16_t)(rows - 1))
            break;
    }
}

// 47da:0427
void Blit_CopyColumnTransparentDown(FarPtr src, FarPtr dest, uint16_t count, uint8_t key)
{
    copy_transparent(src, dest, count, key, 320);
}

// 47da:0456: src row i becomes dest column i.
void Blit_BlitTransposed(FarPtr src, FarPtr dest, int16_t size, uint8_t key)
{
    uint16_t s = 0, d = 0;
    for (uint16_t i = 0;; i++) {
        Blit_CopyColumnTransparentDown(add(src, s), add(dest, d), (uint16_t)size, key);
        s += 320;
        d += 1;
        if (i == (uint16_t)(size - 1))
            break;
    }
}

// 47da:04ac
void Blit_CopyRowTransparentReverse(FarPtr src, FarPtr dest, uint16_t count, uint8_t key)
{
    copy_transparent(src, dest, count, key, (uint16_t)-1);
}

// 47da:04d8
void Blit_BlitRotate180(FarPtr src, FarPtr dest, int16_t size, uint8_t key)
{
    uint16_t s = 0;
    uint16_t d = (uint16_t)(size * 0x141 - 0x141);  // bottom-right pixel
    for (uint16_t i = 0;; i++) {
        Blit_CopyRowTransparentReverse(add(src, s), add(dest, d), (uint16_t)size, key);
        s += 320;
        d -= 320;
        if (i == (uint16_t)(size - 1))
            break;
    }
}

// 47da:053e
void Blit_CopyColumnTransparentUp(FarPtr src, FarPtr dest, uint16_t count, uint8_t key)
{
    copy_transparent(src, dest, count, key, (uint16_t)-320);
}

// 47da:056d: src row i becomes dest column size-1-i, read bottom-up.
void Blit_BlitAntiTranspose(FarPtr src, FarPtr dest, int16_t size, uint8_t key)
{
    uint16_t s = 0;
    uint16_t d = (uint16_t)(size * 0x141 - 0x140);  // one past the bottom-right pixel
    for (uint16_t i = 0;; i++) {
        d -= 1;
        Blit_CopyColumnTransparentUp(add(src, s), add(dest, d), (uint16_t)size, key);
        s += 320;
        if (i == (uint16_t)(size - 1))
            break;
    }
}

// 47da:05d1
void Blit_FillBytes(FarPtr dest, uint16_t count, uint8_t value)
{
    uint16_t d = 0;
    uint16_t words = count >> 1;
    if (count & 1) {
        *at(dest, d++) = value;
        // BUG? `inc cx` after the odd byte: an odd count fills count + 2 bytes.
        words++;
    }
    for (; words != 0; words--) {
        *at(dest, d++) = value;
        *at(dest, d++) = value;
    }
}

// Shared by the two rectangle fills: rows of width bytes, stride 320.
static void fill_rect(FarPtr dest, int16_t width, int16_t height, uint8_t colour)
{
    uint16_t d = 0;
    uint16_t rows = (uint16_t)height;
    do {
        uint16_t n = (uint16_t)width;
        if (n & 1)
            *at(dest, d++) = colour;
        for (n >>= 1; n != 0; n--) {
            *at(dest, d++) = colour;
            *at(dest, d++) = colour;
        }
        d += (uint16_t)(320 - width);
        rows--;
    } while (rows != 0);
}

// 47da:05f2: y * 320 is computed as (y byte-swapped) + that / 4, which is
// y * 256 + y * 64 only for y < 256.
void Blit_FillRect(int16_t x, int16_t y, int16_t width, int16_t height, uint8_t colour)
{
    uint16_t sw = (uint16_t)((uint16_t)y << 8 | (uint16_t)y >> 8);
    uint16_t ofs = (uint16_t)(x + sw + (sw >> 2));
    fill_rect(far_ptr(ds.Gfx_ScreenSeg, ofs), width, height, colour);
}

// 47da:0630
void Blit_FillRectAt(FarPtr dest, int16_t width, int16_t height, uint8_t colour)
{
    fill_rect(dest, width, height, colour);
}

// 47da:065f: colour 0 is transparent; image rows are 16 bytes apart.
void Blit_DrawCursorImage(uint16_t screenOfs, int16_t width, int16_t rows, uint16_t imageOfs)
{
    FarPtr screen = far_ptr(ds.Gfx_ScreenSeg, screenOfs);
    FarPtr image = far_ptr(MEM_DS_SEG, (uint16_t)(offsetof(DSeg, Mouse_CursorImages) + imageOfs));
    uint16_t s = 0, d = 0;
    uint16_t r = (uint16_t)rows;
    do {
        uint16_t n = (uint16_t)width;
        do {
            uint8_t b = *at(image, s++);
            if (b != 0)
                *at(screen, d) = b;
            d++;
            n--;
        } while (n != 0);
        s += (uint16_t)(16 - width);
        d += (uint16_t)(320 - width);
        r--;
    } while (r != 0);
}

// Copies between the screen and the packed Blit_CursorBackground buffer.
static void cursor_background(uint16_t screenOfs, int16_t width, int16_t rows, bool save)
{
    FarPtr screen = far_ptr(ds.Gfx_ScreenSeg, screenOfs);
    FarPtr buf = far_ptr(MEM_DS_SEG, offsetof(DSeg, Blit_CursorBackground));
    uint16_t s = 0, b = 0;
    uint16_t r = (uint16_t)rows;
    do {
        // rep movsb: a width of 0 copies nothing.
        for (uint16_t n = (uint16_t)width; n != 0; n--) {
            if (save)
                *at(buf, b++) = *at(screen, s++);
            else
                *at(screen, s++) = *at(buf, b++);
        }
        s += (uint16_t)(320 - width);
        r--;
    } while (r != 0);
}

// 47da:06a1
void Blit_SaveCursorBackground(uint16_t screenOfs, int16_t width, int16_t rows)
{
    cursor_background(screenOfs, width, rows, true);
}

// 47da:06cf
void Blit_RestoreCursorBackground(uint16_t screenOfs, int16_t width, int16_t rows)
{
    cursor_background(screenOfs, width, rows, false);
}
