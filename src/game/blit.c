#include "game/blit.h"

#include "game/dseg.h"
#include "game/gfx.h"
#include "game/mouse.h"

// The original's offsets wrap inside the 64 KB segment; the port's
// pointers don't. Blits stay inside their buffers (and video memory is 64 KB,
// gfx_screen), so the two agree.

// call far [Blit_CursorHook]. The only value the game stores there is
// Mouse's CursorBlitHook (Mouse_InitMouse); before that the pointer is nil
// and the original would jump to 0000:0000, so there is nothing to call.
static void call_cursor_hook(void)
{
    if (ds.Blit_CursorHook)
        ds.Blit_CursorHook();
}

// 47da:0000
void Blit_CopyBytes(const uint8_t *src, uint8_t *dest, uint16_t count)
{
    if (count & 1)
        *dest++ = *src++;
    // rep movsw: each word is read before it is written, which matters
    // (like the original) when the ranges overlap.
    for (uint16_t n = count >> 1; n != 0; n--) {
        uint8_t lo = src[0], hi = src[1];
        dest[0] = lo;
        dest[1] = hi;
        src += 2;
        dest += 2;
    }
}

// 47da:0025
void Blit_CopyRect(const uint8_t *src, uint8_t *dest, int16_t width, int16_t rows)
{
    uint32_t ofs = 0;
    if (gfx_on_screen(dest)) {
        // The cursor hook gets the screen row about to be overwritten.
        uint16_t row = (uint16_t)((dest - gfx_screen()) / 320);
        uint16_t last = (uint16_t)(row + rows - 1);
        if (row <= last) {
            for (;;) {
                ds.Blit_HookRow = (int16_t)row;
                call_cursor_hook();
                Blit_CopyBytes(src + ofs, dest + ofs, (uint16_t)width);
                ofs += 320;
                if (row == last)
                    break;
                row++;
            }
        }
    } else {
        for (uint16_t i = 0;; i++) {
            Blit_CopyBytes(src + ofs, dest + ofs, (uint16_t)width);
            ofs += 320;
            if (i == (uint16_t)(rows - 1))
                break;
        }
    }
    ds.Blit_HookRow = 999;
    call_cursor_hook();
}

// 47da:00de
void Blit_CopyRectStrided(const uint8_t *src, uint8_t *dest, int16_t width, int16_t rows,
                          int16_t srcStride, int16_t destStride)
{
    for (uint16_t i = 0;; i++) {
        Blit_CopyBytes(src, dest, (uint16_t)width);
        src += srcStride;
        dest += destStride;
        if (i == (uint16_t)(rows - 1))
            break;
    }
}

// 47da:0135: dest is assumed to be on screen (no segment test).
void Blit_Copy16x16(const uint8_t *src, uint8_t *dest)
{
    uint16_t row = (uint16_t)((dest - gfx_screen()) / 320);
    uint16_t last = (uint16_t)(row + 15);
    if (row <= last) {
        for (;;) {
            ds.Blit_HookRow = (int16_t)row;
            call_cursor_hook();
            Blit_CopyBytes(src, dest, 16);
            src += 320;
            dest += 320;
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
void Blit_InterlaceReveal(const uint8_t *src, uint8_t *dest, int16_t width, int16_t rows)
{
    int32_t ofs = 0;
    uint16_t half = (uint16_t)((uint16_t)(rows - 1) >> 1);
    if (half != 0xFFFF) {
        for (uint16_t i = 1;; i++) {
            Blit_CopyBytes(src + ofs, dest + ofs, (uint16_t)width);
            Blit_FillBytes(dest + ofs + 320, (uint16_t)width, 0x40);
            ofs += 640;
            if (i & 1)
                gfx_wait_retrace();
            if (i == (uint16_t)(half + 1))
                break;
        }
    }
    uint16_t pairs = (uint16_t)rows >> 1;
    ofs = pairs * 640 - 320;
    if (pairs != 0) {
        for (uint16_t i = 1;; i++) {
            Blit_CopyBytes(src + ofs, dest + ofs, (uint16_t)width);
            ofs -= 640;
            if (i & 1)
                gfx_wait_retrace();
            if (i == pairs)
                break;
        }
    }
}

// The four transparent copy loops differ only in the destination step.
static void copy_transparent(const uint8_t *src, uint8_t *dest, uint16_t count, uint8_t key,
                             int16_t step)
{
    do {
        uint8_t b = *src++;
        if (b != key)
            *dest = b;
        dest += step;
        count--;
    } while (count != 0);
}

// 47da:02ae
void Blit_CopyRowTransparent(const uint8_t *src, uint8_t *dest, uint16_t count, uint8_t key)
{
    copy_transparent(src, dest, count, key, 1);
}

// 47da:02da
void Blit_BlitTransparent(const uint8_t *src, uint8_t *dest, int16_t width, int16_t rows, uint8_t key)
{
    uint32_t ofs = 0;
    for (uint16_t i = 0;; i++) {
        Blit_CopyRowTransparent(src + ofs, dest + ofs, (uint16_t)width, key);
        ofs += 320;
        if (i == (uint16_t)(rows - 1))
            break;
    }
}

// 47da:0329
void Blit_BlitTransparentStrided(const uint8_t *src, uint8_t *dest, int16_t width, int16_t rows,
                                 int16_t srcStride, int16_t destStride, uint8_t key)
{
    for (uint16_t i = 0;; i++) {
        Blit_CopyRowTransparent(src, dest, (uint16_t)width, key);
        src += srcStride;
        dest += destStride;
        if (i == (uint16_t)(rows - 1))
            break;
    }
}

// 47da:0384: skips src colour 40h and never overwrites dest colours >= 60h
// (the interface colours).
void Blit_BlitOverBackground(const uint8_t *src, uint8_t *dest, int16_t width, int16_t rows)
{
    uint32_t row_ofs = 0;
    for (uint16_t y = 0;; y++) {
        for (uint16_t x = 0;; x++) {
            uint32_t o = x + row_ofs;
            if (dest[o] < 0x60 && src[o] != 0x40)
                dest[o] = src[o];
            if (x == (uint16_t)(width - 1))
                break;
        }
        row_ofs += 320;
        if (y == (uint16_t)(rows - 1))
            break;
    }
}

// 47da:0427
void Blit_CopyColumnTransparentDown(const uint8_t *src, uint8_t *dest, uint16_t count, uint8_t key)
{
    copy_transparent(src, dest, count, key, 320);
}

// 47da:0456: src row i becomes dest column i.
void Blit_BlitTransposed(const uint8_t *src, uint8_t *dest, int16_t size, uint8_t key)
{
    for (uint16_t i = 0;; i++) {
        Blit_CopyColumnTransparentDown(src, dest, (uint16_t)size, key);
        src += 320;
        dest += 1;
        if (i == (uint16_t)(size - 1))
            break;
    }
}

// 47da:04ac
void Blit_CopyRowTransparentReverse(const uint8_t *src, uint8_t *dest, uint16_t count, uint8_t key)
{
    copy_transparent(src, dest, count, key, -1);
}

// 47da:04d8
void Blit_BlitRotate180(const uint8_t *src, uint8_t *dest, int16_t size, uint8_t key)
{
    dest += size * 0x141 - 0x141;  // bottom-right pixel
    for (uint16_t i = 0;; i++) {
        Blit_CopyRowTransparentReverse(src, dest, (uint16_t)size, key);
        src += 320;
        dest -= 320;
        if (i == (uint16_t)(size - 1))
            break;
    }
}

// 47da:053e
void Blit_CopyColumnTransparentUp(const uint8_t *src, uint8_t *dest, uint16_t count, uint8_t key)
{
    copy_transparent(src, dest, count, key, -320);
}

// 47da:056d: src row i becomes dest column size-1-i, read bottom-up.
void Blit_BlitAntiTranspose(const uint8_t *src, uint8_t *dest, int16_t size, uint8_t key)
{
    dest += size * 0x141 - 0x140;  // one past the bottom-right pixel
    for (uint16_t i = 0;; i++) {
        dest -= 1;
        Blit_CopyColumnTransparentUp(src, dest, (uint16_t)size, key);
        src += 320;
        if (i == (uint16_t)(size - 1))
            break;
    }
}

// 47da:05d1
void Blit_FillBytes(uint8_t *dest, uint16_t count, uint8_t value)
{
    uint16_t words = count >> 1;
    if (count & 1) {
        *dest++ = value;
        // BUG? `inc cx` after the odd byte: an odd count fills count + 2 bytes.
        words++;
    }
    for (; words != 0; words--) {
        *dest++ = value;
        *dest++ = value;
    }
}

// Shared by the two rectangle fills: rows of width bytes, stride 320.
static void fill_rect(uint8_t *dest, int16_t width, int16_t height, uint8_t colour)
{
    uint16_t rows = (uint16_t)height;
    do {
        uint16_t n = (uint16_t)width;
        if (n & 1)
            *dest++ = colour;
        for (n >>= 1; n != 0; n--) {
            *dest++ = colour;
            *dest++ = colour;
        }
        dest += 320 - width;
        rows--;
    } while (rows != 0);
}

// 47da:05f2: y * 320 is computed as (y byte-swapped) + that / 4, which is
// y * 256 + y * 64 only for y < 256.
void Blit_FillRect(int16_t x, int16_t y, int16_t width, int16_t height, uint8_t colour)
{
    uint16_t sw = (uint16_t)((uint16_t)y << 8 | (uint16_t)y >> 8);
    uint16_t ofs = (uint16_t)(x + sw + (sw >> 2));
    fill_rect(gfx_screen() + ofs, width, height, colour);
}

// 47da:0630
void Blit_FillRectAt(uint8_t *dest, int16_t width, int16_t height, uint8_t colour)
{
    fill_rect(dest, width, height, colour);
}

// 47da:065f: colour 0 is transparent; image rows are 16 bytes apart.
void Blit_DrawCursorImage(uint16_t screenOfs, int16_t width, int16_t rows, uint16_t imageOfs)
{
    uint8_t *screen = gfx_screen() + screenOfs;
    const uint8_t *image = &ds.Mouse_CursorImages[0][0] + imageOfs;
    uint16_t r = (uint16_t)rows;
    do {
        uint16_t n = (uint16_t)width;
        do {
            uint8_t b = *image++;
            if (b != 0)
                *screen = b;
            screen++;
            n--;
        } while (n != 0);
        image += 16 - width;
        screen += 320 - width;
        r--;
    } while (r != 0);
}

// Copies between the screen and the packed Blit_CursorBackground buffer.
static void cursor_background(uint16_t screenOfs, int16_t width, int16_t rows, bool save)
{
    uint8_t *screen = gfx_screen() + screenOfs;
    uint8_t *buf = (uint8_t *)&ds.Blit_CursorBackground;
    uint16_t r = (uint16_t)rows;
    do {
        // rep movsb: a width of 0 copies nothing.
        for (uint16_t n = (uint16_t)width; n != 0; n--) {
            if (save)
                *buf++ = *screen++;
            else
                *screen++ = *buf++;
        }
        screen += 320 - width;
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
