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

// Routines missing from their units' headers.
void RawText_SetPaletteRange(int16_t first, int16_t last, const uint8_t *pal);  // 4049:0160
void GfxPath_FreeToolbarBuffer(void);                                   // 3a1b:09a3

bool (*gfx_wait_retrace)(void) = timing_wait_retrace;

// Scanlines per frame in mode 13h (31.47 kHz / 70.086 Hz), for WaitScanlines.
enum : int32_t { SCANLINES_PER_FRAME = 449 };

// --- helpers -----------------------------------------------------------------

// Byte at p + off. The original's offsets wrap inside p's segment; with p
// at the segment's start (video memory, the line routines' buffers) the
// 16-bit off does the same.
static inline uint8_t *at(uint8_t *p, uint16_t off)
{
    return p + off;
}

// The temporary palette the original kept on its stack.
static uint8_t temp_palette[0x300];

uint8_t *Gfx_PalettePtr(void)
{
    return (uint8_t *)&ds.Gfx_Palette;
}

// Pos(c, Gfx_CharsetChars): 1-based index or 0. The charset holds neither
// NUL nor PSTR_NUL, so text from game data draws as the original's does.
static uint16_t charset_pos(char c)
{
    char chars[256];
    const char *p = SDL_strchr(pstr_get(chars, sizeof chars, &ds.Gfx_CharsetChars), c);
    return p && c != '\0' ? (uint16_t)(p - chars + 1) : 0;
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
    char path[256];  // for error reports
} DosFile;

static DosFile data_file;  // DS:91EE
static uint16_t io_res;

static void file_reset(DosFile *f, const char *name)
{
    SDL_free(f->data);
    *f = (DosFile){0};
    SDL_strlcpy(f->path, name, sizeof f->path);
    f->data = fs_load(f->path, &f->size);
    if (f->data)
        f->open = true;
    else if (io_res == 0)
        io_res = 2;  // file not found
}

static void file_block_read(DosFile *f, uint8_t *dst, uint16_t count)
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
    SDL_memcpy(dst, f->data + f->pos, count);
    f->pos += count;
}

static void file_close(DosFile *f)
{
    if (!f->open && io_res == 0)
        io_res = 103;
    SDL_free(f->data);
    f->data = nullptr;
    f->size = f->pos = 0;
    f->open = false;
}

static uint16_t io_result(void)
{
    uint16_t r = io_res;
    io_res = 0;
    return r;
}

static void check_io(const DosFile *f)
{
    if (io_res != 0)
        System_RunError(io_result(), f->path);
}

// "Cannot Open <name>": the original switches to text mode, writes it and
// halts (exit code 0); the port reports it as a fatal error.
static void cannot_open(const char *name)
{
    char text[300];
    SDL_snprintf(text, sizeof text, "Cannot Open %s", name);
    system_fatal(0, text);
}

// --- palette -------------------------------------------------------------------

// 405f:002f
void Gfx_GetPalette(uint8_t *pal)
{
    for (uint16_t i = 0; i <= 0xFF; i++) {
        for (uint16_t c = 0; c < 3; c++)
            *at(pal, (uint16_t)(i * 3 + c)) = vid_palette[i][c];
    }
}

// 405f:0074
void Gfx_SetPalette(uint8_t *pal)
{
    RawText_SetPaletteRange(0, 0xFF, pal);
}

// 405f:0089
void Gfx_FadeIn(int16_t first, int16_t last, uint8_t *pal)
{
    Gfx_FadeInSteps(first, last, pal, 5);
}

// 405f:00a2
void Gfx_FadeOut(int16_t first, int16_t last, uint8_t *pal)
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
        System_RunError(200, nullptr);  // IDIV by zero
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

static void fade_steps(FadeKind kind, int16_t first, int16_t last, uint8_t *pal, int16_t steps)
{
    uint8_t *tmp = temp_palette;
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
void Gfx_FadeInSteps(int16_t first, int16_t last, uint8_t *pal, int16_t steps)
{
    fade_steps(FADE_IN, first, last, pal, steps);
    Gfx_SetPalette(pal);
}

// 405f:01c2
void Gfx_FadeOutSteps(int16_t first, int16_t last, uint8_t *pal, int16_t steps)
{
    fade_steps(FADE_OUT, first, last, pal, steps);
}

// 405f:02d5
void Gfx_FadeFromWhite(int16_t first, int16_t last, uint8_t *pal, int16_t steps)
{
    fade_steps(FROM_WHITE, first, last, pal, steps);
}

// 405f:03fd
void Gfx_FadeToWhite(int16_t first, int16_t last, uint8_t *pal, int16_t steps)
{
    fade_steps(TO_WHITE, first, last, pal, steps);
}

// 405f:053a
void Gfx_WhitenPalette(int16_t first, int16_t last, int16_t level, uint8_t *pal)
{
    uint8_t *tmp = temp_palette;
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
void Gfx_BlankPalette(uint8_t *pal, int16_t first, int16_t last)
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
void Gfx_LoadFile(const char *name, uint8_t **buf, uint16_t *size)
{
    DosFile f = {0};
    file_reset(&f, name);
    if (io_result() != 0) {
        cannot_open(name);
        return;
    }
    // FileSize is a longint; only its low word is kept.
    *size = (uint16_t)f.size;
    *buf = game_alloc(*size);
    file_block_read(&f, *buf, *size);
    check_io(&f);
    file_close(&f);
    check_io(&f);
}

// 405f:095a
void Gfx_IconAllOpenSize(uint16_t *size)
{
    file_reset(&data_file, "icon\\icon.all");
    if (io_result() != 0) {
        cannot_open("ICON\\ICON.ALL");
        return;
    }
    *size = (uint16_t)data_file.size;
}

// 405f:09ce: no IOResult check, so a missing file fails at the first read.
void Gfx_IconAllOpen(void)
{
    file_reset(&data_file, "icon\\icon.all");
    ds.Gfx_ReadPos = 0;
}

// 405f:09f3
void Gfx_IconAllReadChunk(uint8_t *buf, uint16_t *len)
{
    uint8_t word[2] = {0, 0};
    if (io_res == 0 && data_file.open && data_file.pos + 2 <= data_file.size) {
        SDL_memcpy(word, data_file.data + data_file.pos, 2);
        data_file.pos += 2;
    } else if (io_res == 0) {
        io_res = data_file.open ? 100 : 103;
    }
    check_io(&data_file);
    *len = (uint16_t)(word[0] | word[1] << 8);
    uint16_t ofs = ds.Gfx_ReadPos;
    ds.Gfx_ReadPos = (uint16_t)(ds.Gfx_ReadPos + *len);
    file_block_read(&data_file, buf + ofs, *len);
    check_io(&data_file);
}

// 405f:0a57
void Gfx_IconAllClose(void)
{
    file_close(&data_file);
    check_io(&data_file);
}

// 405f:0a78
void Gfx_OpenDataFile(const char *name)
{
    file_reset(&data_file, name);
    if (io_result() != 0)
        cannot_open(name);
}

// 405f:0af8
void Gfx_ReadChunk(uint8_t **buf, uint16_t *size)
{
    uint8_t word[2] = {0, 0};
    if (io_res == 0 && data_file.open && data_file.pos + 2 <= data_file.size) {
        SDL_memcpy(word, data_file.data + data_file.pos, 2);
        data_file.pos += 2;
    } else if (io_res == 0) {
        io_res = data_file.open ? 100 : 103;
    }
    check_io(&data_file);
    *size = (uint16_t)((word[0] | word[1] << 8) + 13);
    // The original skips the chunk when MaxAvail is smaller (the port's heap
    // never is).
    *buf = game_alloc(*size);
    file_block_read(&data_file, *buf, *size);
    check_io(&data_file);
}

// 405f:0bd6
void Gfx_CloseDataFile(void)
{
    file_close(&data_file);
    check_io(&data_file);
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
void Gfx_LoadPic(const char *name, uint8_t *dest, uint16_t unused, uint16_t rows, uint16_t x,
                 uint16_t y, uint16_t stride, bool fade, uint8_t *pal, uint16_t firstColour,
                 uint16_t lastColour, uint16_t colourOffset, uint16_t fadeFirst, uint16_t fadeLast)
{
    (void)unused;
    Sound_FreeSampleIfIdle();
    char path[256];  // name + '.pic', cut to 255 characters
    SDL_snprintf(path, sizeof path, "%s.pic", name);
    uint8_t *buf;
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
    uint8_t *tmp = temp_palette;

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
    game_free(buf);
    Sound_FreeSampleIfIdle();
}

// 405f:0f23
void Gfx_LoadPicScreen(const char *name, uint8_t *dest, uint16_t rows, uint8_t *pal)
{
    Gfx_LoadPic(name, dest, 0, rows, 0, 0, 320, true, pal, 0, 0xB0, 0x40, 0, 0xBE);
}

// 405f:0f7a
void Gfx_ShowGrafikaPic(const char *name)
{
    char path[256];
    uint8_t *buf = game_alloc(0xBCC0);
    SDL_snprintf(path, sizeof path, "grafika\\%s", name);
    Gfx_LoadPicScreen(path, buf, 151, Gfx_PalettePtr());
    Blit_CopyRect(buf, gfx_screen() + 49 * 320, 320, 151);
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
    game_free(buf);
}

// --- video mode ------------------------------------------------------------------

// 405f:1014: INT 10h mode 13h clears video memory. The 3C2h write (sync
// polarity) has no equivalent; the BIOS default palette is not loaded.
void Gfx_SetVideoMode13(void)
{
    SDL_memset(gfx_screen(), 0, 0x10000);
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

static bool cd_charset;

void gfx_set_cd_charset(bool cd)
{
    cd_charset = cd;
}

// Past the 78 glyphs the CD's sheet keeps the floppy's addresses: they fall
// on its blank fourth row, as the floppy's glyph 78 (a character not in the
// charset) is blank.
uint16_t gfx_glyph_offset(uint8_t g)
{
    if (cd_charset && g < 78)
        return (uint16_t)((g / 40) * 0xA00 + (g % 40) * 8 + 0x80);
    return (uint16_t)((g / 20) * 0xA00 + (g % 20) * 16 + 0x80);
}

// Draws glyph g (cell in the charset sheet) at screen offset ofs.
static void draw_glyph(uint16_t ofs, uint8_t g, uint8_t colour)
{
    uint8_t *screen = gfx_screen();
    uint16_t src = gfx_glyph_offset(g);
    for (uint16_t r = 0; r < 8; r++) {
        for (uint16_t c = 0; c < 6; c++) {
            uint16_t o = (uint16_t)(r * 320 + c);
            *at(screen, (uint16_t)(ofs + o)) =
                (uint8_t)(*at(ds.Gfx_Charset, (uint16_t)(src + o)) + colour);
        }
    }
}

// 405f:1056. Characters not in Gfx_CharsetChars get glyph 78 (the scan
// gives up after 79 characters).
void Gfx_DrawText6x8(const char *s, int16_t x, int16_t y, int16_t width, uint8_t colour)
{
    // The original pads in a 256-byte stack copy of the string (length byte
    // first); widths over 255 would run off its end. No caller passes one;
    // here the padding is clipped to the buffer.
    uint8_t str[512] = {0};
    str[0] = (uint8_t)SDL_min(SDL_strlen(s), (size_t)255);
    SDL_memcpy(str + 1, s, str[0]);
    bool hide = text_under_cursor(x, y, width);
    if (hide)
        Mouse_HideCursor();
    uint8_t blank = (uint8_t)(charset_pos(' ') - 1);
    char chars[256] = {};
    pstr_get(chars, sizeof chars, &ds.Gfx_CharsetChars);
    if (str[0] == 0) {
        str[0] = 1;
        str[1] = ' ';
    }
    uint16_t len = str[0];
    for (uint16_t i = 1; i <= len; i++) {
        uint16_t k = 0;
        while (k < 78 && (uint8_t)chars[k] != str[i])
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
void Gfx_DrawStatusText(const char *s)
{
    Gfx_DrawText6x8(s, 16, 37, 18, 0x34);
}

// 405f:1218
void Gfx_DrawTextF9(const char *s, int16_t x, int16_t y, int16_t width)
{
    Gfx_DrawText6x8(s, x, y, width, 0xF9);
}

// 405f:124b
void Gfx_DrawTextFC(const char *s, int16_t x, int16_t y, int16_t width)
{
    Gfx_DrawText6x8(s, x, y, width, 0xFC);
}

// 405f:127e
void Gfx_DrawTextF1(const char *s, int16_t x, int16_t y, int16_t width)
{
    Gfx_DrawText6x8(s, x, y, width, 0xF1);
}

// 405f:12b1
void Gfx_DrawTextF4(const char *s, int16_t x, int16_t y, int16_t width)
{
    Gfx_DrawText6x8(s, x, y, width, 0xF4);
}

// 405f:12e4
void Gfx_DrawTextB0(const char *s, int16_t x, int16_t y, int16_t width)
{
    Gfx_DrawText6x8(s, x, y, width, 0xB0);
}

// 405f:1317
void Gfx_DrawTextB3(const char *s, int16_t x, int16_t y, int16_t width)
{
    Gfx_DrawText6x8(s, x, y, width, 0xB3);
}

// 405f:134c: "Year-Month-Day-Hour".
void Gfx_DrawGameDate(void)
{
    char t[32];
    SDL_snprintf(t, sizeof t, "%u-%u-%u-%u", ds.Game_Year, ds.Game_Month, ds.Game_Day,
                 ds.Game_Hour);
    Gfx_DrawText6x8(t, 224, 37, 13, 0x34);
}

// 405f:140a
void Gfx_DrawCredits(void)
{
    char t[16];
    SDL_snprintf(t, sizeof t, "%d", (int)ds.Game_Credits);
    Gfx_DrawText6x8(t, 144, 37, 10, 0x34);
}

// 405f:143a
void Gfx_FlashCredits(void)
{
    char t[16];
    SDL_snprintf(t, sizeof t, "%d", (int)ds.Game_Credits);
    Gfx_DrawText6x8(t, 144, 37, 10, 0xFC);
    Gfx_WaitFrames(6);
    Gfx_DrawText6x8(t, 144, 37, 10, 0x34);
}

// TypeText's glyph loop: like draw_glyph, but the index is Pos - 1 as a
// byte, so a character missing from the charset gives glyph 255.
static void type_glyph(int16_t x, int16_t y, uint16_t cell, uint8_t g, uint8_t colour)
{
    uint8_t *screen = gfx_screen();
    uint16_t base = (uint16_t)(y * 320 + x + (cell - 1) * 6);
    uint16_t src = gfx_glyph_offset(g);
    for (uint16_t r = 0; r <= 7; r++) {
        for (uint16_t c = 0; c <= 5; c++) {
            uint16_t o = (uint16_t)(r * 320 + c);
            *at(screen, (uint16_t)(base + o)) =
                (uint8_t)(colour + *at(ds.Gfx_Charset, (uint16_t)(src + o)));
        }
    }
}

// 405f:14c1. Clears the cells, then types the text one character at a time
// with a click and a 300-scanline pause per non-blank character.
void Gfx_TypeText(const char *s, int16_t x, int16_t y, int16_t width, uint8_t colour)
{
    // The original's 256-byte copy of the string, length byte first.
    uint8_t str[256] = {0};
    str[0] = (uint8_t)SDL_min(SDL_strlen(s), (size_t)255);
    SDL_memcpy(str + 1, s, str[0]);
    Sound_PlaySfx("kiiro");
    // The original also works out whether the cursor overlaps the text, but
    // never uses it: it hides the cursor around every character instead.
    if ((uint16_t)width >= 1) {
        for (uint16_t i = 1;; i++) {
            uint8_t g = (uint8_t)(charset_pos(' ') - 1);
            type_glyph(x, y, i, g, colour);
            if (i == (uint16_t)width)
                break;
        }
    }
    // s := s + 52 spaces, then Delete(s, width + 1, 255). The bytes past the
    // new length keep the spaces, which the loop below may read.
    uint8_t pad = (uint8_t)SDL_min(52, 255 - str[0]);
    SDL_memset(str + 1 + str[0], ' ', pad);
    str[0] = (uint8_t)(str[0] + pad);
    int32_t idx = (int16_t)(width + 1);
    if (idx >= 1 && idx <= str[0])
        str[0] = (uint8_t)(idx - 1);
    if ((uint16_t)width >= 1) {
        for (uint16_t i = 1;; i++) {
            uint8_t ch = str[(uint8_t)i];  // the original reads past its 256-byte buffer for width > 255; none is
            uint8_t g = (uint8_t)(charset_pos((char)ch) - 1);
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
void Gfx_TypeTextF9(const char *s, int16_t x, int16_t y, int16_t width)
{
    Gfx_TypeText(s, x, y, width, 0xF9);
}

// 405f:17d5
void Gfx_TypeTextFC(const char *s, int16_t x, int16_t y, int16_t width)
{
    Gfx_TypeText(s, x, y, width, 0xFC);
}

// --- lines and rectangles ----------------------------------------------------------

// 405f:1808: rep stosb, so x2 < x1 draws a (very long) wrapped line.
void Gfx_HLine(int16_t x1, int16_t x2, int16_t y, uint8_t colour)
{
    uint8_t *screen = gfx_screen();
    uint16_t d = (uint16_t)((y - 1) * 320 + x1 - 1);
    for (uint16_t n = (uint16_t)(x2 - x1 + 1); n != 0; n--)
        *at(screen, d++) = colour;
}

// 405f:1836
void Gfx_VLine(int16_t x, int16_t y1, int16_t y2, uint8_t colour)
{
    uint8_t *screen = gfx_screen();
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
    uint8_t *screen = gfx_screen();
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
static void line(uint8_t *dest, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                 uint16_t stride, bool copy, uint8_t colour, uint8_t *src)
{
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
void Gfx_DrawLine(uint8_t *seg, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t colour)
{
    line(seg, x1, y1, x2, y2, (uint16_t)ds.Gfx_LineStride, false, colour, nullptr);
}

// 405f:1d7c
void Gfx_DrawLineCopy(uint8_t *destSeg, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                      uint8_t *srcSeg)
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

// call far [p] for the procedure variables the exit handler calls. nil
// (unit not initialised): the original would crash; nothing to do.
static void call_proc(TProc p)
{
    if (p)
        p();
}

// 405f:1a62, installed as ExitProc by Gfx_Init (the port's exit hook,
// game/system.h). After a run-time error the original then fades out,
// switches to text mode, prints "External or internal error!" with the code
// and address and waits for a key, or for error 203 asks for more
// conventional memory; the port reports fatal errors itself (system_fatal).
void Gfx_ExitHandler(void)
{
    // The original ends a looping SB voice here (Sound::SbBreakLoop,
    // 431a:017f); the port's audio stops with the program.
    Music_StopMusic();
    call_proc(ds.Game_GfxPathProc);
    call_proc(ds.Ems_RestoreProc);
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
    system_exit_hook = Gfx_ExitHandler;
}
