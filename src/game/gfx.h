// Unit Gfx (405f): VGA palette and fades, retrace waits, file and .PIC
// loading, ICON.ALL access, 6x8 text, lines, and the fatal-error exit.
//
// Untyped Pascal pointers (palettes, buffers), Pascal strings (a length
// byte followed by the characters) and `var` parameters are host pointers;
// heap blocks the unit allocates are still far pointers (game/mem.h).
#pragma once

#include <stdint.h>

#include "game/dseg.h"
#include "platform/video.h"

// The drawing target: video memory, 64 KB as the VGA's segment A000h, of
// which rows 0..199 (64,000 bytes) are shown.
static inline uint8_t *gfx_screen(void)
{
    return &vid_screen[0][0];
}

static inline bool gfx_on_screen(const uint8_t *p)
{
    return p >= gfx_screen() && p < gfx_screen() + 0x10000;
}

// Every vertical-retrace wait in Gfx and Blit goes through this; it is
// timing_wait_retrace by default. Tests replace it to run without a window.
extern bool (*gfx_wait_retrace)(void);

// ds.Gfx_Palette (what the game passes as @Gfx::Palette).
uint8_t *Gfx_PalettePtr(void);

// 405f:002f: reads the 256 DAC entries into pal.
void Gfx_GetPalette(uint8_t *pal);
// 405f:0074
void Gfx_SetPalette(uint8_t *pal);
// 405f:0089 / 00a2: 5-step fades of first..last from black to pal / pal to black.
void Gfx_FadeIn(int16_t first, int16_t last, uint8_t *pal);
void Gfx_FadeOut(int16_t first, int16_t last, uint8_t *pal);
// 405f:00bb / 01c2 / 02d5 / 03fd: steps + 1 DAC writes, one retrace before each.
void Gfx_FadeInSteps(int16_t first, int16_t last, uint8_t *pal, int16_t steps);
void Gfx_FadeOutSteps(int16_t first, int16_t last, uint8_t *pal, int16_t steps);
void Gfx_FadeFromWhite(int16_t first, int16_t last, uint8_t *pal, int16_t steps);
void Gfx_FadeToWhite(int16_t first, int16_t last, uint8_t *pal, int16_t steps);
// 405f:053a: level 15 = pal, 0 = white; no wait.
void Gfx_WhitenPalette(int16_t first, int16_t last, int16_t level, uint8_t *pal);
// 405f:0638: zeroes pal[first..last] and writes it to the DAC.
void Gfx_BlankPalette(uint8_t *pal, int16_t first, int16_t last);

// 405f:068d: waits n scanlines (accumulated into whole retraces here).
void Gfx_WaitScanlines(int32_t n);
// 405f:06e6
void Gfx_WaitVRetrace(void);
// 405f:06ff
void Gfx_WaitFrames(int32_t n);
// 405f:077f: waits for a button release, then a press, at most n frames
// in all; a key press sets ds.Gfx_KeyAbort, which skips later waits.
void Gfx_WaitFramesOrClick(int32_t n);

// 405f:0856: reads a whole file into a new heap block (halts if missing).
void Gfx_LoadFile(const uint8_t *name, FarPtr *buf, uint16_t *size);
// 405f:095a / 09ce / 09f3 / 0a57: ICON\ICON.ALL through the shared file
// variable. ReadChunk appends the next chunk at *buf + Gfx_ReadPos and puts
// its length in *len. (symbols.txt has the two parameter names swapped.)
void Gfx_IconAllOpenSize(uint16_t *size);
void Gfx_IconAllOpen(void);
void Gfx_IconAllReadChunk(const FarPtr *buf, uint16_t *len);
void Gfx_IconAllClose(void);
// 405f:0a78 / 0af8 / 0bd6: chunked data files (word length + data). ReadChunk
// allocates length + 13 bytes, or returns nil/0 when the heap is too small.
void Gfx_OpenDataFile(const uint8_t *name);
void Gfx_ReadChunk(FarPtr *buf, uint16_t *size);
void Gfx_CloseDataFile(void);

// 405f:0bef: decodes <name>.pic into dest + y * stride + x as one linear
// stream of rows lines (see docs/formats/pic.md), adding colourOffset to each
// pixel; rows is clipped to the picture height. With fade, pal[colourOffset +
// fadeFirst..colourOffset + fadeLast] fades to black in the DAC while
// decoding. Afterwards file colours firstColour..lastColour go to
// pal[colourOffset + i] (not written to the DAC).
void Gfx_LoadPic(const uint8_t *name, uint8_t *dest, uint16_t unused, uint16_t rows, uint16_t x,
                 uint16_t y, uint16_t stride, bool fade, uint8_t *pal, uint16_t firstColour,
                 uint16_t lastColour, uint16_t colourOffset, uint16_t fadeFirst, uint16_t fadeLast);
// 405f:0f23: the game's usual options: stride 320, colours 0..B0h at +40h,
// fading picture colours 0..BEh (DAC 40h..FEh).
void Gfx_LoadPicScreen(const uint8_t *name, uint8_t *dest, uint16_t rows, uint8_t *pal);
// 405f:0f7a: grafika\<name> (320x151) to screen row 49, then fades it in.
void Gfx_ShowGrafikaPic(const uint8_t *name);

// 405f:1014 / 103d
void Gfx_SetVideoMode13(void);
void Gfx_SetTextMode(void);

// 405f:1056: 6x8 glyphs from Gfx_Charset, padded with blanks to width
// cells; each pixel is written as glyph pixel + colour.
void Gfx_DrawText6x8(const uint8_t *s, int16_t x, int16_t y, int16_t width, uint8_t colour);
// 405f:11e8 .. 1317: DrawText6x8 with fixed colours (and position).
void Gfx_DrawStatusText(const uint8_t *s);
void Gfx_DrawTextF9(const uint8_t *s, int16_t x, int16_t y, int16_t width);
void Gfx_DrawTextFC(const uint8_t *s, int16_t x, int16_t y, int16_t width);
void Gfx_DrawTextF1(const uint8_t *s, int16_t x, int16_t y, int16_t width);
void Gfx_DrawTextF4(const uint8_t *s, int16_t x, int16_t y, int16_t width);
void Gfx_DrawTextB0(const uint8_t *s, int16_t x, int16_t y, int16_t width);
void Gfx_DrawTextB3(const uint8_t *s, int16_t x, int16_t y, int16_t width);
// 405f:134c / 140a / 143a: status bar date and credits.
void Gfx_DrawGameDate(void);
void Gfx_DrawCredits(void);
void Gfx_FlashCredits(void);
// 405f:14c1 / 17a2 / 17d5: typewriter text with click sounds.
void Gfx_TypeText(const uint8_t *s, int16_t x, int16_t y, int16_t width, uint8_t colour);
void Gfx_TypeTextF9(const uint8_t *s, int16_t x, int16_t y, int16_t width);
void Gfx_TypeTextFC(const uint8_t *s, int16_t x, int16_t y, int16_t width);

// 405f:1808 / 1836 / 1869: 1-based screen coordinates.
void Gfx_HLine(int16_t x1, int16_t x2, int16_t y, uint8_t colour);
void Gfx_VLine(int16_t x, int16_t y1, int16_t y2, uint8_t colour);
void Gfx_DrawRectOutline(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t colour);
// 405f:18be: 0-based; pixels with even x + y.
void Gfx_DrawDitherRect(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t colour);

// 405f:1933: fade out, text mode, Halt.
void Gfx_FatalExit(void);
// 405f:1999
void Gfx_OutOfMemoryExit(void);
// 405f:1a62 (no symbol yet): the unit's ExitProc. Stops sound, releases
// buffers and, after a run-time error, shows the error screen and waits
// for a key.
void Gfx_ExitHandler(void);
// 405f:1c21: one frame of the red colour-0 alert flash.
void Gfx_PaletteFlashTick(void);
// 405f:1c82: Bresenham line in the segment starting at seg (stride
// Gfx_LineStride; 16-bit offsets from seg, as in the original).
void Gfx_DrawLine(uint8_t *seg, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t colour);
// 405f:1d7c: the same line, copying pixels from srcSeg into destSeg.
void Gfx_DrawLineCopy(uint8_t *destSeg, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                      uint8_t *srcSeg);
// 405f:1e78
void Gfx_Init(void);
