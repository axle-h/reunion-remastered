#include "game/gfx.h"

#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/blit.h"
#include "game/crt.h"
#include "game/dseg.h"
#include "game/mouse.h"
#include "game/music.h"
#include "game/sound.h"
#include "platform/fs.h"
#include "platform/timing.h"
#include "platform/video.h"

#include "game/system.h"

// Not ported yet: see stubs.c.
void RawText_SetPaletteRange(int16_t first, int16_t last, FarPtr pal);  // 4049:0160
void Sound_SbBreakLoop(void);                                           // 431a:017f
void GfxPath_FreeToolbarBuffer(void);                                   // 3a1b:09a3
void Ems_EmsReleaseGameData(void);                                      // 4719:0317

bool (*gfx_wait_retrace)(void) = timing_wait_retrace;

// Unnamed globals.
#define SAVED_EXIT_PROC (*(uint32_t *)ds_at(0x89BA))  // ExitProc before Gfx_Init

// Scanlines per frame in mode 13h (31.47 kHz / 70.086 Hz), for WaitScanlines.
enum : int32_t { SCANLINES_PER_FRAME = 449 };

// --- helpers -----------------------------------------------------------------

// Byte at p + off, the offset wrapping inside p's segment as on the 8086.
static inline uint8_t *at(FarPtr p, uint16_t off)
{
    return far_host(far_ptr((uint16_t)(p >> 16), (uint16_t)((uint16_t)p + off)));
}

static inline FarPtr ds_ptr(uint16_t off)
{
    return far_ptr(MEM_DS_SEG, off);
}

// Locals that the original kept on its stack and passed on by far pointer
// (temporary palettes) live in the emulated stack segment, which nothing
// else in the port uses.
static inline FarPtr stack_temp(void)
{
    return far_ptr(MEM_SS_SEG, 0x0A00);
}

FarPtr Gfx_PalettePtr(void)
{
    return ds_ptr(offsetof(DSeg, Gfx_Palette));
}

// Pascal string helpers (length byte + chars, at most 255).
static void pstr_copy(uint8_t *dst, const uint8_t *src)
{
    SDL_memmove(dst, src, (size_t)src[0] + 1);
}

static void pstr_cat(uint8_t *dst, const uint8_t *src)
{
    size_t n = SDL_min((size_t)src[0], (size_t)(255 - dst[0]));
    SDL_memcpy(dst + 1 + dst[0], src + 1, n);
    dst[0] = (uint8_t)(dst[0] + n);
}

static void pstr_cat_c(uint8_t *dst, const char *src)
{
    uint8_t tmp[256];
    size_t n = SDL_min(SDL_strlen(src), (size_t)255);
    tmp[0] = (uint8_t)n;
    SDL_memcpy(tmp + 1, src, n);
    pstr_cat(dst, tmp);
}

static void pstr_from_c(uint8_t *dst, const char *src)
{
    dst[0] = 0;
    pstr_cat_c(dst, src);
}

static void pstr_to_c(char *dst, const uint8_t *src)
{
    SDL_memcpy(dst, src + 1, src[0]);
    dst[src[0]] = '\0';
}

// Str(v:0, s).
static void str_long(uint8_t *dst, int64_t v)
{
    char tmp[32];
    SDL_snprintf(tmp, sizeof tmp, "%lld", (long long)v);
    pstr_from_c(dst, tmp);
}

// Pos(c, s) for a one-character substring: 1-based index or 0.
static uint16_t pos_char(uint8_t c, const PString *s)
{
    for (uint16_t i = 0; i < s->len; i++) {
        if ((uint8_t)s->s[i] == c)
            return i + 1;
    }
    return 0;
}

// Write(Output, s) / WriteLn. The text screen is gone; output goes to stdout.
static void out_write(const uint8_t *s)
{
    fwrite(s + 1, 1, s[0], stdout);
    fflush(stdout);
}

static void out_write_c(const char *s)
{
    fputs(s, stdout);
    fflush(stdout);
}

// --- Borland file variables ---------------------------------------------------
// The FileRec at DS:91EE shared by the ICON.ALL and data-file routines, and
// LoadFile's local one, as whole files in host memory. io_res plays the part of
// InOutRes: IOResult reads and clears it, CheckIO turns it into a run-time error.

typedef struct {
    uint8_t *data;
    size_t size;
    size_t pos;
    bool open;
} DosFile;

static DosFile data_file;  // DS:91EE
static uint16_t io_res;

static void file_reset(DosFile *f, const uint8_t *name)
{
    char path[256];
    pstr_to_c(path, name);
    SDL_free(f->data);
    *f = (DosFile){0};
    f->data = fs_load(path, &f->size);
    if (f->data)
        f->open = true;
    else if (io_res == 0)
        io_res = 2;  // file not found
}

static void file_block_read(DosFile *f, FarPtr dst, uint16_t count)
{
    if (io_res != 0)
        return;
    if (!f->open) {
        io_res = 103;  // file not open
        return;
    }
    if (f->pos + count > f->size) {
        io_res = 100;  // disk read error (short read with a nil result pointer)
        return;
    }
    for (uint16_t i = 0; i < count; i++)
        *at(dst, i) = f->data[f->pos + i];
    f->pos += count;
}

static void file_close(DosFile *f)
{
    if (!f->open && io_res == 0)
        io_res = 103;
    SDL_free(f->data);
    *f = (DosFile){0};
}

static uint16_t io_result(void)
{
    uint16_t r = io_res;
    io_res = 0;
    return r;
}

static void check_io(void)
{
    if (io_res != 0)
        System_RunError(io_result());
}

// SetTextMode; Write(Output, msg); Halt.
static void cannot_open(const uint8_t *msg)
{
    Gfx_SetTextMode();
    out_write(msg);
    System_Halt(0);
}

// --- palette -------------------------------------------------------------------

// 405f:002f
void Gfx_GetPalette(FarPtr pal)
{
    for (uint16_t i = 0; i <= 0xFF; i++) {
        for (uint16_t c = 0; c < 3; c++)
            *at(pal, (uint16_t)(i * 3 + c)) = vid_palette[i][c];
    }
}

// 405f:0074
void Gfx_SetPalette(FarPtr pal)
{
    RawText_SetPaletteRange(0, 0xFF, pal);
}

// 405f:0089
void Gfx_FadeIn(int16_t first, int16_t last, FarPtr pal)
{
    Gfx_FadeInSteps(first, last, pal, 5);
}

// 405f:00a2
void Gfx_FadeOut(int16_t first, int16_t last, FarPtr pal)
{
    Gfx_FadeOutSteps(first, last, pal, 5);
}

// The four stepped fades: entry := f(pal[i], s) for s = 0..steps, each step
// written to the DAC after a retrace. The original multiplies with MUL (16-bit
// result) and divides with CWD/IDIV, so the product is taken as signed.
typedef enum { FADE_IN, FADE_OUT, FROM_WHITE, TO_WHITE } FadeKind;

static uint8_t fade_value(FadeKind kind, uint8_t v, int16_t s, int16_t steps)
{
    if (steps == 0)
        System_RunError(200);  // IDIV by zero
    switch (kind) {
    case FADE_IN:
        return (uint8_t)((int16_t)(uint16_t)(v * s) / steps);
    case FADE_OUT:
        return (uint8_t)((int16_t)(uint16_t)(v * (uint16_t)(steps - s)) / steps);
    case FROM_WHITE:
        return (uint8_t)(63 - (int16_t)(uint16_t)((uint16_t)(63 - v) * s) / steps);
    case TO_WHITE:
        return (uint8_t)(63 - (int16_t)(uint16_t)((uint16_t)(63 - v) * (uint16_t)(steps - s)) / steps);
    }
    return 0;
}

static void fade_steps(FadeKind kind, int16_t first, int16_t last, FarPtr pal, int16_t steps)
{
    FarPtr tmp = stack_temp();
    Gfx_GetPalette(tmp);
    if (steps < 0)
        return;
    for (int16_t s = 0;; s++) {
        if (first <= last) {
            for (int16_t i = first;; i++) {
                for (uint16_t c = 0; c < 3; c++) {
                    uint16_t o = (uint16_t)(i * 3 + c);
                    *at(tmp, o) = fade_value(kind, *at(pal, o), s, steps);
                }
                if (i == last)
                    break;
            }
        }
        Gfx_WaitVRetrace();
        RawText_SetPaletteRange(first, last, tmp);
        if (s == steps)
            break;
    }
}

// 405f:00bb: ends with the whole of pal in the DAC.
void Gfx_FadeInSteps(int16_t first, int16_t last, FarPtr pal, int16_t steps)
{
    fade_steps(FADE_IN, first, last, pal, steps);
    Gfx_SetPalette(pal);
}

// 405f:01c2
void Gfx_FadeOutSteps(int16_t first, int16_t last, FarPtr pal, int16_t steps)
{
    fade_steps(FADE_OUT, first, last, pal, steps);
}

// 405f:02d5
void Gfx_FadeFromWhite(int16_t first, int16_t last, FarPtr pal, int16_t steps)
{
    fade_steps(FROM_WHITE, first, last, pal, steps);
}

// 405f:03fd
void Gfx_FadeToWhite(int16_t first, int16_t last, FarPtr pal, int16_t steps)
{
    fade_steps(TO_WHITE, first, last, pal, steps);
}

// 405f:053a
void Gfx_WhitenPalette(int16_t first, int16_t last, int16_t level, FarPtr pal)
{
    FarPtr tmp = stack_temp();
    Gfx_GetPalette(tmp);
    if (first <= last) {
        for (int16_t i = first;; i++) {
            for (uint16_t c = 0; c < 3; c++) {
                uint16_t o = (uint16_t)(i * 3 + c);
                *at(tmp, o) = (uint8_t)(63 - (int16_t)(uint16_t)((uint16_t)(63 - *at(pal, o)) * level) / 15);
            }
            if (i == last)
                break;
        }
    }
    RawText_SetPaletteRange(first, last, tmp);
}

// 405f:0638
void Gfx_BlankPalette(FarPtr pal, int16_t first, int16_t last)
{
    if (first <= last) {
        for (int16_t i = first;; i++) {
            for (uint16_t c = 0; c < 3; c++)
                *at(pal, (uint16_t)(i * 3 + c)) = 0;
            if (i == last)
                break;
        }
    }
    Gfx_WaitVRetrace();
    RawText_SetPaletteRange(first, last, pal);
}

// --- waits ---------------------------------------------------------------------

// Scanlines waited for that don't make a whole retrace yet.
static int32_t scanlines_pending;

// 405f:068d: the original counts display-enable toggles (one per scanline);
// whole frames' worth become retrace waits and the rest carries over.
void Gfx_WaitScanlines(int32_t n)
{
    if (n <= 0)
        return;
    scanlines_pending += n;
    while (scanlines_pending >= SCANLINES_PER_FRAME) {
        scanlines_pending -= SCANLINES_PER_FRAME;
        gfx_wait_retrace();
    }
}

// 405f:06e6
void Gfx_WaitVRetrace(void)
{
    gfx_wait_retrace();
}

// 405f:06ff
void Gfx_WaitFrames(int32_t n)
{
    for (int32_t i = 1; i <= n; i++)
        gfx_wait_retrace();
}

// 405f:0758 (nested in WaitFramesOrClick)
static void poll_key_abort(void)
{
    if (Crt_KeyPressed())
        ds.Gfx_KeyAbort = 1;
    if (Crt_KeyPressed())
        Crt_ReadKey();
}

// 405f:077f. The original read Mouse_BtnDown, which the INT 33h callback kept
// current; Mouse_Update does that job here.
void Gfx_WaitFramesOrClick(int32_t n)
{
    poll_key_abort();
    if (ds.Gfx_KeyAbort)
        return;
    int32_t i = 0;
    for (;;) {
        Mouse_Update();
        if (!(i < n && ds.Mouse_BtnDown))
            break;
        gfx_wait_retrace();
        poll_key_abort();
        i++;
    }
    for (;;) {
        Mouse_Update();
        if (!(i < n && !ds.Mouse_BtnDown))
            break;
        gfx_wait_retrace();
        poll_key_abort();
        i++;
    }
}

// --- files ---------------------------------------------------------------------

// 405f:0856
void Gfx_LoadFile(const uint8_t *name, FarPtr *buf, uint16_t *size)
{
    uint8_t nm[256];
    pstr_copy(nm, name);
    DosFile f = {0};
    file_reset(&f, nm);
    if (io_result() != 0) {
        uint8_t msg[256];
        pstr_from_c(msg, "Cannot Open ");
        pstr_cat(msg, nm);
        cannot_open(msg);
        return;
    }
    // FileSize is a longint; only its low word is kept.
    *size = (uint16_t)f.size;
    *buf = System_GetMem(*size);
    file_block_read(&f, *buf, *size);
    check_io();
    file_close(&f);
    check_io();
}

// 405f:095a
void Gfx_IconAllOpenSize(uint16_t *size)
{
    uint8_t name[32];
    pstr_from_c(name, "icon\\icon.all");
    file_reset(&data_file, name);
    if (io_result() != 0) {
        uint8_t msg[64];
        pstr_from_c(msg, "Cannot Open ICON\\ICON.ALL");
        cannot_open(msg);
        return;
    }
    *size = (uint16_t)data_file.size;
}

// 405f:09ce: no IOResult check, so a missing file fails at the first read.
void Gfx_IconAllOpen(void)
{
    uint8_t name[32];
    pstr_from_c(name, "icon\\icon.all");
    file_reset(&data_file, name);
    ds.Gfx_ReadPos = 0;
}

// 405f:09f3
void Gfx_IconAllReadChunk(const FarPtr *buf, uint16_t *len)
{
    uint8_t word[2] = {0, 0};
    if (io_res == 0 && data_file.open && data_file.pos + 2 <= data_file.size) {
        SDL_memcpy(word, data_file.data + data_file.pos, 2);
        data_file.pos += 2;
    } else if (io_res == 0) {
        io_res = data_file.open ? 100 : 103;
    }
    check_io();
    *len = (uint16_t)(word[0] | word[1] << 8);
    uint16_t ofs = ds.Gfx_ReadPos;
    ds.Gfx_ReadPos = (uint16_t)(ds.Gfx_ReadPos + *len);
    file_block_read(&data_file, far_ptr((uint16_t)(*buf >> 16), (uint16_t)((uint16_t)*buf + ofs)),
                    *len);
    check_io();
}

// 405f:0a57
void Gfx_IconAllClose(void)
{
    file_close(&data_file);
    check_io();
}

// 405f:0a78
void Gfx_OpenDataFile(const uint8_t *name)
{
    uint8_t nm[256];
    pstr_copy(nm, name);
    file_reset(&data_file, nm);
    if (io_result() != 0) {
        uint8_t msg[256];
        pstr_from_c(msg, "Cannot Open ");
        pstr_cat(msg, nm);
        cannot_open(msg);
    }
}

// Game_MinMemAvail := min(Game_MinMemAvail, MemAvail).
static void track_min_mem(void)
{
    if (System_MemAvail() < ds.Game_MinMemAvail)
        ds.Game_MinMemAvail = System_MemAvail();
}

// 405f:0af8
void Gfx_ReadChunk(FarPtr *buf, uint16_t *size)
{
    uint8_t word[2] = {0, 0};
    if (io_res == 0 && data_file.open && data_file.pos + 2 <= data_file.size) {
        SDL_memcpy(word, data_file.data + data_file.pos, 2);
        data_file.pos += 2;
    } else if (io_res == 0) {
        io_res = data_file.open ? 100 : 103;
    }
    check_io();
    *size = (uint16_t)((word[0] | word[1] << 8) + 13);
    if (System_MaxAvail() < (int32_t)*size) {
        // Skips the chunk. BUG? It seeks length + 13 bytes, 13 more than the
        // chunk (consistent with the read below, which also takes 13 extra).
        data_file.pos += *size;
        *size = 0;
        *buf = 0;
    } else {
        *buf = System_GetMem(*size);
        file_block_read(&data_file, *buf, *size);
        check_io();
    }
    track_min_mem();
}

// 405f:0bd6
void Gfx_CloseDataFile(void)
{
    file_close(&data_file);
    check_io();
}

// --- pictures --------------------------------------------------------------------

// Retraces the fade in LoadPic takes: one per this many of its 40 steps.
// The original never waits there: the decoding, 40 DAC loads (573 bytes for
// 40h..FEh) and a MUL and DIV per component paced it, roughly 90 ms (6
// retraces) on a 486DX-33. Without the waits the old picture cut to black.
enum : uint16_t { LOADPIC_FADE_STEPS_PER_RETRACE = 8 };

// 405f:0bef. Same RLE loop as pic_decode_into (assets/pic.h), but kept as the
// original runs it: the header width is used as is, a run may overshoot a row
// (the stream stays linear), and the palette index colourOffset + i is not
// clipped. The inline-asm prologue (0c97..0cd8, which the decompile drops)
// shifts the fade range by colourOffset and clips rows to the header height.
void Gfx_LoadPic(const uint8_t *name, FarPtr dest, uint16_t unused, uint16_t rows, uint16_t x,
                 uint16_t y, uint16_t stride, bool fade, FarPtr pal, uint16_t firstColour,
                 uint16_t lastColour, uint16_t colourOffset, uint16_t fadeFirst, uint16_t fadeLast)
{
    (void)unused;
    uint8_t nm[256], path[256];
    pstr_copy(nm, name);
    Sound_FreeSampleIfIdle();
    pstr_copy(path, nm);
    pstr_cat_c(path, ".pic");
    FarPtr buf;
    uint16_t size;
    Gfx_LoadFile(path, &buf, &size);

    uint16_t width = (uint16_t)(*at(buf, 8) | *at(buf, 9) << 8);
    uint16_t height = (uint16_t)(*at(buf, 10) | *at(buf, 11) << 8);
    // 405f:0c97: the fade range is in picture colours, like firstColour..
    // lastColour, so LoadPicScreen fades 40h..FEh and leaves the toolbar's
    // colours 0..3Fh alone.
    fadeFirst = (uint16_t)(fadeFirst + colourOffset);
    fadeLast = (uint16_t)(fadeLast + colourOffset);
    if (rows > height)  // 405f:0ccb
        rows = height;
    const uint16_t steps = 40;
    uint16_t step = 0, prev_step = 0;
    uint16_t next_row = 1;
    uint16_t src = 12;
    uint16_t dst = (uint16_t)(y * stride + x);
    uint8_t add = (uint8_t)colourOffset;
    FarPtr tmp = stack_temp();

    uint16_t col = 1, row = 1;
    do {
        do {
            uint8_t b = *at(buf, src++);
            if (b < 0xC0) {
                *at(dest, dst++) = (uint8_t)(b + add);
                col++;
            } else {
                uint16_t n = b & 0x3F;
                uint8_t v = *at(buf, src++);
                col = (uint16_t)(col + n);
                for (; n != 0; n--)
                    *at(dest, dst++) = (uint8_t)(v + add);
            }
        } while (col <= width);

        if (fade && row == next_row) {
            if (step < steps)
                step++;
            next_row = (uint16_t)(step * rows) / steps;
            if (fadeFirst <= fadeLast) {
                for (uint16_t i = fadeFirst;; i++) {
                    for (uint16_t c = 0; c < 3; c++) {
                        uint16_t o = (uint16_t)(i * 3 + c);
                        *at(tmp, o) = (uint8_t)((uint16_t)(*at(pal, o) * (steps - step)) / steps);
                    }
                    if (i == fadeLast)
                        break;
                }
            }
            RawText_SetPaletteRange((int16_t)fadeFirst, (int16_t)fadeLast, tmp);
            if (step % LOADPIC_FADE_STEPS_PER_RETRACE == 0 && step != prev_step)
                gfx_wait_retrace();  // port: shows the fade (see above)
            prev_step = step;
        }
        col = 1;
        row++;
    } while (row <= rows);

    // The file palette is the last 768 bytes (8-bit values).
    uint16_t raw = (uint16_t)(size - 0x300);
    for (uint16_t i = 0; i <= 0xFF; i++) {
        if (firstColour <= i && i <= lastColour) {
            for (uint16_t c = 0; c < 3; c++)
                *at(pal, (uint16_t)((i + colourOffset) * 3 + c)) = *at(buf, (uint16_t)(raw + i * 3 + c)) >> 2;
        }
    }
    track_min_mem();
    System_FreeMem(buf, size);
    Sound_FreeSampleIfIdle();
}

// 405f:0f23
void Gfx_LoadPicScreen(const uint8_t *name, FarPtr dest, uint16_t rows, FarPtr pal)
{
    uint8_t nm[256];
    pstr_copy(nm, name);
    Gfx_LoadPic(nm, dest, 0, rows, 0, 0, 320, true, pal, 0, 0xB0, 0x40, 0, 0xBE);
}

// 405f:0f7a
void Gfx_ShowGrafikaPic(const uint8_t *name)
{
    uint8_t path[256];
    FarPtr buf = System_GetMem(0xBCC0);
    pstr_from_c(path, "grafika\\");
    pstr_cat(path, name);
    Gfx_LoadPicScreen(path, buf, 151, Gfx_PalettePtr());
    Blit_CopyRect(buf, far_ptr(ds.Gfx_ScreenSeg, 49 * 320), 320, 151);
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
    System_FreeMem(buf, 0xBCC0);
}

// --- video mode ------------------------------------------------------------------

// 405f:1014: INT 10h mode 13h clears video memory. The 3C2h write (sync
// polarity) has no equivalent; the BIOS default palette is not loaded.
void Gfx_SetVideoMode13(void)
{
    SDL_memset(far_host(far_ptr(MEM_VIDEO_SEG, 0)), 0, 0x10000);
}

// 405f:103d: INT 10h mode 3. The window stays; text output goes to stdout.
void Gfx_SetTextMode(void)
{
}

// --- text ----------------------------------------------------------------------

// Does a width-cell text line at x, y touch the 16x16 cursor box? Word compares.
static bool text_under_cursor(int16_t x, int16_t y, int16_t width)
{
    bool hit = (uint16_t)x < (uint16_t)(ds.Mouse_X + 16)
               && (uint16_t)ds.Mouse_X < (uint16_t)(width * 6 + x);
    return hit && (uint16_t)y < (uint16_t)(ds.Mouse_Y + 16)
           && (uint16_t)ds.Mouse_Y < (uint16_t)(y + 8);
}

// Draws glyph g (cell in the 20-per-row charset sheet) at screen offset ofs.
static void draw_glyph(uint16_t ofs, uint8_t g, uint8_t colour)
{
    FarPtr screen = far_ptr(ds.Gfx_ScreenSeg, 0);
    uint16_t src = (uint16_t)((g / 20) * 0xA00 + (g % 20) * 16 + 0x80);
    for (uint16_t r = 0; r < 8; r++) {
        for (uint16_t c = 0; c < 6; c++) {
            uint16_t o = (uint16_t)(r * 320 + c);
            *at(screen, (uint16_t)(ofs + o)) = (uint8_t)(*at(ds.Gfx_Charset, (uint16_t)(src + o)) + colour);
        }
    }
}

// 405f:1056. Characters not in Gfx_CharsetChars get glyph 78 (the scan
// gives up after 79 characters).
void Gfx_DrawText6x8(const uint8_t *s, int16_t x, int16_t y, int16_t width, uint8_t colour)
{
    // The original pads in a 256-byte stack copy; widths over 255 would run
    // off its end. BUG? Here the padding is clipped to the buffer.
    uint8_t str[512] = {0};
    pstr_copy(str, s);
    bool hide = text_under_cursor(x, y, width);
    if (hide)
        Mouse_HideCursor();
    uint8_t blank = (uint8_t)(pos_char(' ', &ds.Gfx_CharsetChars) - 1);
    if (str[0] == 0) {
        str[0] = 1;
        str[1] = ' ';
    }
    uint16_t len = str[0];
    for (uint16_t i = 1; i <= len; i++) {
        uint16_t k = 0;
        while (k < 78 && (uint8_t)ds.Gfx_CharsetChars.s[k] != str[i])
            k++;
        str[i] = (uint8_t)k;
    }
    if ((uint16_t)width >= len) {
        for (uint16_t i = len + 1; i <= (uint16_t)width && i < sizeof str; i++)
            str[i] = blank;
    }
    uint16_t ofs = (uint16_t)(y * 320 + x - 6);
    if ((uint16_t)width >= 1) {
        for (uint16_t i = 1;; i++) {
            ofs += 6;
            draw_glyph(ofs, i < sizeof str ? str[i] : blank, colour);
            if (i == (uint16_t)width)
                break;
        }
    }
    if (hide)
        Mouse_ShowCursor();
}

// 405f:11e8
void Gfx_DrawStatusText(const uint8_t *s)
{
    Gfx_DrawText6x8(s, 16, 37, 18, 0x34);
}

// 405f:1218
void Gfx_DrawTextF9(const uint8_t *s, int16_t x, int16_t y, int16_t width)
{
    Gfx_DrawText6x8(s, x, y, width, 0xF9);
}

// 405f:124b
void Gfx_DrawTextFC(const uint8_t *s, int16_t x, int16_t y, int16_t width)
{
    Gfx_DrawText6x8(s, x, y, width, 0xFC);
}

// 405f:127e
void Gfx_DrawTextF1(const uint8_t *s, int16_t x, int16_t y, int16_t width)
{
    Gfx_DrawText6x8(s, x, y, width, 0xF1);
}

// 405f:12b1
void Gfx_DrawTextF4(const uint8_t *s, int16_t x, int16_t y, int16_t width)
{
    Gfx_DrawText6x8(s, x, y, width, 0xF4);
}

// 405f:12e4
void Gfx_DrawTextB0(const uint8_t *s, int16_t x, int16_t y, int16_t width)
{
    Gfx_DrawText6x8(s, x, y, width, 0xB0);
}

// 405f:1317
void Gfx_DrawTextB3(const uint8_t *s, int16_t x, int16_t y, int16_t width)
{
    Gfx_DrawText6x8(s, x, y, width, 0xB3);
}

// 405f:134c: "Year-Month-Day-Hour".
void Gfx_DrawGameDate(void)
{
    uint8_t t[256], part[256];
    str_long(t, ds.Game_Year);
    pstr_cat_c(t, "-");
    str_long(part, ds.Game_Month);
    pstr_cat(t, part);
    pstr_cat_c(t, "-");
    str_long(part, ds.Game_Day);
    pstr_cat(t, part);
    pstr_cat_c(t, "-");
    str_long(part, ds.Game_Hour);
    pstr_cat(t, part);
    Gfx_DrawText6x8(t, 224, 37, 13, 0x34);
}

// 405f:140a
void Gfx_DrawCredits(void)
{
    uint8_t t[256];
    str_long(t, ds.Game_Credits);
    Gfx_DrawText6x8(t, 144, 37, 10, 0x34);
}

// 405f:143a
void Gfx_FlashCredits(void)
{
    uint8_t t[256];
    str_long(t, ds.Game_Credits);
    Gfx_DrawText6x8(t, 144, 37, 10, 0xFC);
    Gfx_WaitFrames(6);
    Gfx_DrawText6x8(t, 144, 37, 10, 0x34);
}

// TypeText's glyph loop: like draw_glyph, but the index is Pos - 1 as a
// byte, so a character missing from the charset gives glyph 255.
static void type_glyph(int16_t x, int16_t y, uint16_t cell, uint8_t g, uint8_t colour)
{
    FarPtr screen = far_ptr(ds.Gfx_ScreenSeg, 0);
    uint16_t base = (uint16_t)(y * 320 + x + (cell - 1) * 6);
    uint16_t src = (uint16_t)((g / 20) * 0xA00 + (g % 20) * 16 + 0x80);
    for (uint16_t r = 0; r <= 7; r++) {
        for (uint16_t c = 0; c <= 5; c++) {
            uint16_t o = (uint16_t)(r * 320 + c);
            *at(screen, (uint16_t)(base + o)) = (uint8_t)(colour + *at(ds.Gfx_Charset, (uint16_t)(src + o)));
        }
    }
}

// 405f:14c1. Clears the cells, then types the text one character at a time
// with a click and a 300-scanline pause per non-blank character.
void Gfx_TypeText(const uint8_t *s, int16_t x, int16_t y, int16_t width, uint8_t colour)
{
    uint8_t str[256] = {0};
    pstr_copy(str, s);
    uint8_t kiiro[8];
    pstr_from_c(kiiro, "kiiro");
    Sound_PlaySfx(kiiro);
    // The original also works out whether the cursor overlaps the text, but
    // never uses it: it hides the cursor around every character instead.
    if ((uint16_t)width >= 1) {
        for (uint16_t i = 1;; i++) {
            uint8_t g = (uint8_t)(pos_char(' ', &ds.Gfx_CharsetChars) - 1);
            type_glyph(x, y, i, g, colour);
            if (i == (uint16_t)width)
                break;
        }
    }
    // s := s + 52 spaces, then Delete(s, width + 1, 255). The bytes past the
    // new length keep the spaces, which the loop below may read.
    pstr_cat_c(str, "                                                    ");
    int32_t idx = (int16_t)(width + 1);
    if (idx >= 1 && idx <= str[0])
        str[0] = (uint8_t)(idx - 1);
    if ((uint16_t)width >= 1) {
        for (uint16_t i = 1;; i++) {
            uint8_t ch = str[(uint8_t)i];  // BUG? the original reads past the 256-byte buffer for width > 255
            uint8_t g = (uint8_t)(pos_char(ch, &ds.Gfx_CharsetChars) - 1);
            Mouse_HideCursor();
            type_glyph(x, y, i, g, colour);
            Mouse_ShowCursor();
            if (ch != ' ')
                Sound_StartSample();
            if (ch != ' ')
                Gfx_WaitScanlines(300);
            if (i == (uint16_t)width)
                break;
        }
    }
}

// 405f:17a2
void Gfx_TypeTextF9(const uint8_t *s, int16_t x, int16_t y, int16_t width)
{
    Gfx_TypeText(s, x, y, width, 0xF9);
}

// 405f:17d5
void Gfx_TypeTextFC(const uint8_t *s, int16_t x, int16_t y, int16_t width)
{
    Gfx_TypeText(s, x, y, width, 0xFC);
}

// --- lines and rectangles ----------------------------------------------------------

// 405f:1808: rep stosb, so x2 < x1 draws a (very long) wrapped line.
void Gfx_HLine(int16_t x1, int16_t x2, int16_t y, uint8_t colour)
{
    FarPtr screen = far_ptr(ds.Gfx_ScreenSeg, 0);
    uint16_t d = (uint16_t)((y - 1) * 320 + x1 - 1);
    for (uint16_t n = (uint16_t)(x2 - x1 + 1); n != 0; n--)
        *at(screen, d++) = colour;
}

// 405f:1836
void Gfx_VLine(int16_t x, int16_t y1, int16_t y2, uint8_t colour)
{
    FarPtr screen = far_ptr(ds.Gfx_ScreenSeg, 0);
    uint16_t d = (uint16_t)((y1 - 1) * 320 + x - 1);
    uint16_t n = (uint16_t)(y2 - y1 + 1);
    do {
        *at(screen, d) = colour;
        d += 320;
        n--;
    } while (n != 0);
}

// 405f:1869
void Gfx_DrawRectOutline(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t colour)
{
    Mouse_HideCursor();
    Gfx_HLine(x1, x2, y1, colour);
    Gfx_HLine(x1, x2, y2, colour);
    Gfx_VLine(x1, y1, y2, colour);
    Gfx_VLine(x2, y1, y2, colour);
    Mouse_ShowCursor();
}

// 405f:18be
void Gfx_DrawDitherRect(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t colour)
{
    FarPtr screen = far_ptr(ds.Gfx_ScreenSeg, 0);
    Mouse_HideCursor();
    if ((uint16_t)x1 <= (uint16_t)x2) {
        for (uint16_t x = (uint16_t)x1;; x++) {
            if ((uint16_t)y1 <= (uint16_t)y2) {
                for (uint16_t y = (uint16_t)y1;; y++) {
                    if ((uint16_t)(x + y) % 2 == 0)
                        *at(screen, (uint16_t)(y * 320 + x)) = colour;
                    if (y == (uint16_t)y2)
                        break;
                }
            }
            if (x == (uint16_t)x2)
                break;
        }
    }
    Mouse_ShowCursor();
}

// y * 320 as the line routines compute it: (y byte-swapped) + that / 4.
static uint16_t line_offset(int16_t x, int16_t y)
{
    uint16_t sw = (uint16_t)((uint16_t)y << 8 | (uint16_t)y >> 8);
    return (uint16_t)(x + sw + (sw >> 2));
}

// The Bresenham loop shared by DrawLine and DrawLineCopy: either colour, or
// (copy) the source segment's pixel at the same offset.
static void line(uint16_t destSeg, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                 uint16_t stride, bool copy, uint8_t colour, uint16_t srcSeg)
{
    FarPtr dest = far_ptr(destSeg, 0);
    FarPtr src = far_ptr(srcSeg, 0);
#define PUT(o) (*at(dest, (o)) = copy ? *at(src, (o)) : colour)

    uint16_t si = stride;
    uint16_t cx = (uint16_t)(x2 - x1);
    if (cx == 0) {
        // Vertical: from the smaller y down.
        int16_t ytop = y1;
        int16_t n = (int16_t)(y2 - y1);
        if (n < 0) {
            n = (int16_t)-n;
            ytop = y2;
        }
        uint16_t count = (uint16_t)(n + 1);
        uint16_t di = line_offset(x1, ytop);
        si--;
        do {
            PUT(di);
            di = (uint16_t)(di + 1 + si);
            count--;
        } while (count != 0);
        return;
    }
    if ((int16_t)cx < 0) {
        cx = (uint16_t)-cx;
        int16_t t = x1;
        x1 = x2;
        x2 = t;
        t = y1;
        y1 = y2;
        y2 = t;
    }
    uint16_t bx = (uint16_t)(y2 - y1);
    if (bx == 0) {
        // Horizontal (rep stosb / a byte loop).
        uint16_t di = line_offset(x1, y1);
        uint16_t count = (uint16_t)(cx + 1);
        do {
            PUT(di);
            di++;
            count--;
        } while (count != 0);
        return;
    }
    if ((int16_t)bx < 0) {
        bx = (uint16_t)-bx;
        si = (uint16_t)-si;
    }
    uint16_t step = si;
    bool ymajor = false;
    if ((int16_t)bx > (int16_t)cx) {
        ymajor = true;
        uint16_t t = bx;
        bx = cx;
        cx = t;
    }
    uint16_t inc_minor = (uint16_t)(bx * 2);                 // [bp-6]
    uint16_t err = (uint16_t)(inc_minor - cx);               // si
    uint16_t inc_both = (uint16_t)(inc_minor - cx - cx);     // [bp-4]
    uint16_t di = line_offset(x1, y1);
    uint16_t count = (uint16_t)(cx + 1);
    do {
        PUT(di);
        di++;
        if (ymajor) {
            di += step;
            if ((int16_t)err < 0) {
                err += inc_minor;
                di--;
            } else {
                err += inc_both;
            }
        } else {
            if ((int16_t)err < 0) {
                err += inc_minor;
            } else {
                err += inc_both;
                di += step;
            }
        }
        count--;
    } while (count != 0);
#undef PUT
}

// 405f:1c82
void Gfx_DrawLine(uint16_t seg, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t colour)
{
    line(seg, x1, y1, x2, y2, (uint16_t)ds.Gfx_LineStride, false, colour, 0);
}

// 405f:1d7c
void Gfx_DrawLineCopy(uint16_t destSeg, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                      uint16_t srcSeg)
{
    line(destSeg, x1, y1, x2, y2, (uint16_t)ds.Gfx_LineStrideCopy, true, 0, srcSeg);
}

// --- exit --------------------------------------------------------------------------

// 405f:1933
void Gfx_FatalExit(void)
{
    Gfx_FadeOut(0, 0xFF, Gfx_PalettePtr());
    Gfx_SetTextMode();
    System_Halt(0);
}

// 405f:1999
void Gfx_OutOfMemoryExit(void)
{
    Gfx_SetTextMode();
    out_write_c("Not enough memory !  Please run with more main memory or EMS (max 256k)\n");
    System_Halt(0);
}

// call far [p] for the procedure variables the exit handler calls.
static void call_far_proc(uint32_t p)
{
    if (p == far_ptr(0x3A1B, 0x09A3))
        GfxPath_FreeToolbarBuffer();
    else if (p == far_ptr(0x4719, 0x0317))
        Ems_EmsReleaseGameData();
    // nil (unit not initialised): the original would crash; nothing to do.
}

// 405f:1a62, installed as ExitProc by Gfx_Init.
void Gfx_ExitHandler(void)
{
    ds.System_ExitProc = SAVED_EXIT_PROC;
    if (ds.Sound_Enabled && ds.Sound_DeviceType != 1)
        Sound_SbBreakLoop();
    Music_StopMusic();
    call_far_proc(ds.Game_GfxPathProc);
    call_far_proc(ds.Ems_RestoreProc);
    if (ds.System_ErrorAddr == 0)
        return;

    Gfx_FadeOut(0, 0xFF, Gfx_PalettePtr());
    Gfx_SetTextMode();
    uint8_t t[256];
    if (ds.System_ExitCode == 203) {  // heap overflow
        out_write_c("Not enough memory!\n");
        out_write_c("Please run REUNION with more conventional memory or EMS (max 256k) !\n");
    } else {
        // The strings are verbatim, odd "103" and "23" included.
        out_write_c("External or internal error!\n");
        out_write_c("Error code:      103");
        str_long(t, ds.System_ExitCode);
        out_write(t);
        out_write_c("23\n");
        out_write_c("Error address:   ");
        str_long(t, (uint16_t)(ds.System_ErrorAddr >> 16));
        out_write(t);
        out_write_c(":");
        str_long(t, (uint16_t)ds.System_ErrorAddr);
        out_write(t);
        out_write_c("\n");
    }
    // repeat until KeyPressed. One retrace per poll keeps events flowing and
    // lets a closed window end the wait.
    while (!Crt_KeyPressed()) {
        if (!gfx_wait_retrace())
            break;
    }
    ds.System_ErrorAddr = 0;
}

// 405f:1c21: colour 0 goes R = 60 - 2 * |FlashStep - 31| (byte arithmetic)
// straight to the DAC, while Gfx_Palette[0] is set to black.
void Gfx_PaletteFlashTick(void)
{
    if (ds.Gfx_FlashStep > 0) {
        int16_t d = (int16_t)(ds.Gfx_FlashStep - 31);
        uint8_t sign = d < 0 ? 0xFF : 0x00;
        uint8_t mag = (uint8_t)(((uint8_t)d ^ sign) - sign);
        vid_palette[0][0] = (uint8_t)(mag * -2 + 60) & 0x3F;  // the DAC keeps 6 bits
        vid_palette[0][1] = 0;
        vid_palette[0][2] = 0;
        ds.Gfx_Palette[0][0] = 0;
        ds.Gfx_Palette[0][1] = 0;
        ds.Gfx_Palette[0][2] = 0;
        ds.Gfx_FlashStep--;
    }
    if (ds.Gfx_FlashStep == 0 && ds.Gfx_FlashCount > 0) {
        ds.Gfx_FlashCount--;
        ds.Gfx_FlashStep = 61;
    }
}

// 405f:1e78
void Gfx_Init(void)
{
    scanlines_pending = 0;  // port: every boot starts the same (tests replay boots)
    ds.Game_MinMemAvail = 0x009BF9B0;
    SAVED_EXIT_PROC = ds.System_ExitProc;
    ds.System_ExitProc = far_ptr(0x405F, 0x1A62);  // Gfx_ExitHandler
}
