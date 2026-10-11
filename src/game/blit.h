// Unit Blit (47da): rectangle copies, transparent and flipped/rotated blits,
// fills and the software cursor's save/restore.
//
// Untyped Pascal pointers (`const src; var dest`) are host pointers into a
// buffer or video memory (gfx_screen; CopyRect tells the two apart). Row
// strides are 320 unless a routine takes them as parameters. Counts behave
// like the original's loops: a do-while on a 16-bit counter, so 0 means
// 65536.
#pragma once

#include <stdint.h>


// 47da:0000: like Move, forward and word-wise (odd byte first).
void Blit_CopyBytes(const uint8_t *src, uint8_t *dest, uint16_t count);
// 47da:0025: calls Blit_CursorHook around every row when dest is on screen.
void Blit_CopyRect(const uint8_t *src, uint8_t *dest, int16_t width, int16_t rows);
// 47da:00de
void Blit_CopyRectStrided(const uint8_t *src, uint8_t *dest, int16_t width, int16_t rows,
                          int16_t srcStride, int16_t destStride);
// 47da:0135
void Blit_Copy16x16(const uint8_t *src, uint8_t *dest);
// 47da:01b6: two-pass interlaced reveal (waits for retraces).
void Blit_InterlaceReveal(const uint8_t *src, uint8_t *dest, int16_t width, int16_t rows);
// 47da:02ae
void Blit_CopyRowTransparent(const uint8_t *src, uint8_t *dest, uint16_t count, uint8_t key);
// 47da:02da
void Blit_BlitTransparent(const uint8_t *src, uint8_t *dest, int16_t width, int16_t rows, uint8_t key);
// 47da:0329
void Blit_BlitTransparentStrided(const uint8_t *src, uint8_t *dest, int16_t width, int16_t rows,
                                 int16_t srcStride, int16_t destStride, uint8_t key);
// 47da:0384
void Blit_BlitOverBackground(const uint8_t *src, uint8_t *dest, int16_t width, int16_t rows);
// 47da:0427 (near helper)
void Blit_CopyColumnTransparentDown(const uint8_t *src, uint8_t *dest, uint16_t count, uint8_t key);
// 47da:0456
void Blit_BlitTransposed(const uint8_t *src, uint8_t *dest, int16_t size, uint8_t key);
// 47da:04ac (near helper)
void Blit_CopyRowTransparentReverse(const uint8_t *src, uint8_t *dest, uint16_t count, uint8_t key);
// 47da:04d8
void Blit_BlitRotate180(const uint8_t *src, uint8_t *dest, int16_t size, uint8_t key);
// 47da:053e (near helper)
void Blit_CopyColumnTransparentUp(const uint8_t *src, uint8_t *dest, uint16_t count, uint8_t key);
// 47da:056d
void Blit_BlitAntiTranspose(const uint8_t *src, uint8_t *dest, int16_t size, uint8_t key);
// 47da:05d1
void Blit_FillBytes(uint8_t *dest, uint16_t count, uint8_t value);
// 47da:05f2: on Gfx_ScreenSeg.
void Blit_FillRect(int16_t x, int16_t y, int16_t width, int16_t height, uint8_t colour);
// 47da:0630
void Blit_FillRectAt(uint8_t *dest, int16_t width, int16_t height, uint8_t colour);
// 47da:065f / 06a1 / 06cf: the cursor sprite and the pixels under it
// (screen offsets on Gfx_ScreenSeg; imageOfs indexes Mouse_CursorImages).
void Blit_DrawCursorImage(uint16_t screenOfs, int16_t width, int16_t rows, uint16_t imageOfs);
void Blit_SaveCursorBackground(uint16_t screenOfs, int16_t width, int16_t rows);
void Blit_RestoreCursorBackground(uint16_t screenOfs, int16_t width, int16_t rows);
