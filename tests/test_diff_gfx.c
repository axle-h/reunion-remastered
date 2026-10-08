// Differential test of unit Gfx (405f): palettes and fades, waits, files and
// pictures, text, lines and the exit paths, with random arguments from the
// states of a new game. The fades, LoadPic and the scanline waits only differ
// in timing (the port's retrace hooks), which is not compared.
// See docs/difftest.md. Needs the game data.
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/gfx.h"

#define P(i) ((FarPtr)a[i])
#define S(i) ((const uint8_t *)far_host(P(i)))
#define I(i) ((int16_t)a[i])
#define W(i) ((uint16_t)a[i])

DIFF_PROC(Gfx_GetPalette, Gfx_GetPalette(P(0)));
DIFF_PROC(Gfx_SetPalette, Gfx_SetPalette(P(0)));
DIFF_PROC(Gfx_FadeIn, Gfx_FadeIn(I(0), I(1), P(2)));
DIFF_PROC(Gfx_FadeOut, Gfx_FadeOut(I(0), I(1), P(2)));
DIFF_PROC(Gfx_FadeInSteps, Gfx_FadeInSteps(I(0), I(1), P(2), I(3)));
DIFF_PROC(Gfx_FadeOutSteps, Gfx_FadeOutSteps(I(0), I(1), P(2), I(3)));
DIFF_PROC(Gfx_FadeFromWhite, Gfx_FadeFromWhite(I(0), I(1), P(2), I(3)));
DIFF_PROC(Gfx_FadeToWhite, Gfx_FadeToWhite(I(0), I(1), P(2), I(3)));
DIFF_PROC(Gfx_WhitenPalette, Gfx_WhitenPalette(I(0), I(1), I(2), P(3)));
DIFF_PROC(Gfx_BlankPalette, Gfx_BlankPalette(P(0), I(1), I(2)));
DIFF_PROC(Gfx_WaitScanlines, Gfx_WaitScanlines(a[0]));
DIFF_PROC(Gfx_WaitVRetrace, Gfx_WaitVRetrace());
DIFF_PROC(Gfx_WaitFrames, Gfx_WaitFrames(a[0]));
DIFF_PROC(Gfx_WaitFramesOrClick, Gfx_WaitFramesOrClick(a[0]));
DIFF_PROC(Gfx_PaletteFlashTick, Gfx_PaletteFlashTick());

DIFF_PROC(Gfx_LoadFile, Gfx_LoadFile(S(0), far_host(P(1)), far_host(P(2))));
DIFF_PROC(Gfx_IconAllOpenSize, Gfx_IconAllOpenSize(far_host(P(0))));
DIFF_PROC(Gfx_OpenDataFile, Gfx_OpenDataFile(S(0)));
DIFF_PROC(Gfx_LoadPic, Gfx_LoadPic(S(0), P(1), W(2), W(3), W(4), W(5), W(6), (bool)a[7], P(8), W(9),
                                   W(10), W(11), W(12), W(13)));
DIFF_PROC(Gfx_LoadPicScreen, Gfx_LoadPicScreen(S(0), P(1), W(2), P(3)));
DIFF_PROC(Gfx_ShowGrafikaPic, Gfx_ShowGrafikaPic(S(0)));
DIFF_PROC(Gfx_SetVideoMode13, Gfx_SetVideoMode13());
DIFF_PROC(Gfx_SetTextMode, Gfx_SetTextMode());

DIFF_PROC(Gfx_DrawText6x8, Gfx_DrawText6x8(S(0), I(1), I(2), I(3), (uint8_t)a[4]));
DIFF_PROC(Gfx_DrawStatusText, Gfx_DrawStatusText(S(0)));
DIFF_PROC(Gfx_DrawTextF9, Gfx_DrawTextF9(S(0), I(1), I(2), I(3)));
DIFF_PROC(Gfx_DrawTextFC, Gfx_DrawTextFC(S(0), I(1), I(2), I(3)));
DIFF_PROC(Gfx_DrawTextF1, Gfx_DrawTextF1(S(0), I(1), I(2), I(3)));
DIFF_PROC(Gfx_DrawTextF4, Gfx_DrawTextF4(S(0), I(1), I(2), I(3)));
DIFF_PROC(Gfx_DrawTextB0, Gfx_DrawTextB0(S(0), I(1), I(2), I(3)));
DIFF_PROC(Gfx_DrawTextB3, Gfx_DrawTextB3(S(0), I(1), I(2), I(3)));
DIFF_PROC(Gfx_DrawGameDate, Gfx_DrawGameDate());
DIFF_PROC(Gfx_DrawCredits, Gfx_DrawCredits());
DIFF_PROC(Gfx_FlashCredits, Gfx_FlashCredits());
DIFF_PROC(Gfx_TypeText, Gfx_TypeText(S(0), I(1), I(2), I(3), (uint8_t)a[4]));
DIFF_PROC(Gfx_TypeTextF9, Gfx_TypeTextF9(S(0), I(1), I(2), I(3)));
DIFF_PROC(Gfx_TypeTextFC, Gfx_TypeTextFC(S(0), I(1), I(2), I(3)));

DIFF_PROC(Gfx_HLine, Gfx_HLine(I(0), I(1), I(2), (uint8_t)a[3]));
DIFF_PROC(Gfx_VLine, Gfx_VLine(I(0), I(1), I(2), (uint8_t)a[3]));
DIFF_PROC(Gfx_DrawRectOutline, Gfx_DrawRectOutline(I(0), I(1), I(2), I(3), (uint8_t)a[4]));
DIFF_PROC(Gfx_DrawDitherRect, Gfx_DrawDitherRect(I(0), I(1), I(2), I(3), (uint8_t)a[4]));
DIFF_PROC(Gfx_DrawLine, Gfx_DrawLine(W(0), I(1), I(2), I(3), I(4), (uint8_t)a[5]));
DIFF_PROC(Gfx_DrawLineCopy, Gfx_DrawLineCopy(W(0), I(1), I(2), I(3), I(4), W(5)));

DIFF_PROC(Gfx_FatalExit, Gfx_FatalExit());
DIFF_PROC(Gfx_OutOfMemoryExit, Gfx_OutOfMemoryExit());
DIFF_PROC(Gfx_ExitHandler, Gfx_ExitHandler());

// --- Scratch ---------------------------------------------------------------------

// Pointers into the scratch buffer, normalised (offset < 16) like GetMem's.
static FarPtr scratch(uint16_t ofs)
{
    uint32_t lin = far_linear(DIFF_SCRATCH) + ofs;
    return far_ptr((uint16_t)(lin >> 4), (uint16_t)(lin & 15));
}

// A Pascal string at scratch + ofs.
static FarPtr put_str(uint16_t ofs, const char *s)
{
    uint8_t *p = far_host(scratch(ofs));
    size_t n = SDL_min(strlen(s), (size_t)255);
    p[0] = (uint8_t)n;
    memcpy(p + 1, s, n);
    return scratch(ofs);
}

// Random text: mostly characters of the game's font, some that are not.
static FarPtr random_text(DiffRng *r, uint16_t ofs, int maxlen)
{
    uint8_t *p = far_host(scratch(ofs));
    int n = diff_pick(r, 0, 1, diff_range(r, 0, maxlen), diff_range(r, 0, maxlen));
    p[0] = (uint8_t)n;
    const PString *cs = &ds.Gfx_CharsetChars;
    for (int i = 1; i <= n; i++) {
        if (cs->len && !diff_chance(r, 8))
            p[i] = (uint8_t)cs->s[diff_rand(r, cs->len)];
        else
            p[i] = (uint8_t)diff_pick(r, ' ', '~', 0, '@', 0xE9, diff_rand(r, 256));
    }
    return scratch(ofs);
}

// --- Palettes --------------------------------------------------------------------------

// A palette: Gfx_Palette or one in the scratch buffer, 6-bit values or
// (sometimes) any byte, so the signed products of the fades overflow.
static FarPtr random_palette(DiffRng *r)
{
    FarPtr pal = diff_chance(r, 50) ? Gfx_PalettePtr() : scratch(0x400);
    uint8_t *p = far_host(pal);
    int mode = diff_rand(r, 4);
    if (mode == 0 && pal == Gfx_PalettePtr())
        return pal;
    for (int i = 0; i < 768; i++)
        p[i] = (uint8_t)(mode == 3 ? diff_rand(r, 256) : diff_rand(r, 64));
    return pal;
}

// first..last. An empty range (last < first) makes SetPaletteRange send
// about 64 KB, most of it from beyond the fades' temporary palette, a stack
// local in the original and elsewhere in the port: only FadeInSteps (which
// ends with the whole palette) and BlankPalette (no temporary) get one.
static void range(DiffRng *r, int32_t *first, int32_t *last, bool empty)
{
    *first = diff_pick(r, 0, 0, 1, 0x40, diff_range(r, 0, 255));
    *last = diff_pick(r, 255, 0xFE, 0xBE, *first, diff_range(r, *first, 255));
    if (*last < *first)
        *last = *first;
    if (empty && diff_chance(r, 8))
        *last = *first - 1;
}

static void setup_palette_ptr(DiffRng *r, int32_t *a)
{
    a[0] = (int32_t)random_palette(r);
}

static void setup_fade(DiffRng *r, int32_t *a)
{
    range(r, &a[0], &a[1], false);
    a[2] = (int32_t)random_palette(r);
}

static void setup_fade_steps(DiffRng *r, int32_t *a)
{
    setup_fade(r, a);
    a[3] = diff_pick(r, 1, 2, 5, 5, 40, 100, 150, -1, diff_range(r, 0, 200));
}

static void setup_fade_in_steps(DiffRng *r, int32_t *a)
{
    range(r, &a[0], &a[1], true);
    a[2] = (int32_t)random_palette(r);
    a[3] = diff_pick(r, 1, 2, 5, 5, 40, 100, 150, -1, diff_range(r, 0, 200));
}

static void setup_whiten(DiffRng *r, int32_t *a)
{
    range(r, &a[0], &a[1], false);
    a[2] = diff_pick(r, 0, 15, diff_range(r, 0, 15), diff_range(r, 0, 15), diff_range(r, -20, 200));
    a[3] = (int32_t)random_palette(r);
}

static void setup_blank(DiffRng *r, int32_t *a)
{
    a[0] = (int32_t)random_palette(r);
    range(r, &a[1], &a[2], true);
}

// --- Waits --------------------------------------------------------------------------------

static void setup_scanlines(DiffRng *r, int32_t *a)
{
    a[0] = diff_pick(r, 0, -1, 1, 40, 300, 449, diff_range(r, 0, 3000), (int32_t)0x80000000);
}

static void setup_frames(DiffRng *r, int32_t *a)
{
    a[0] = diff_pick(r, 0, -5, 1, 6, diff_range(r, 0, 70));
}

static void setup_frames_click(DiffRng *r, int32_t *a)
{
    setup_frames(r, a);
    ds.Gfx_KeyAbort = (uint8_t)diff_pick(r, 0, 0, 1);
    if (diff_chance(r, 60)) {  // no key: waits the frames out
        static const int esc[] = {27, 'a'};
        diff_set_keys(esc, diff_pick(r, 0, 0, 0, 1, 2));
    }
}

static void setup_flash(DiffRng *r, int32_t *a)
{
    (void)a;
    ds.Gfx_FlashStep = (int16_t)diff_pick(r, 0, 1, 31, 61, -1, diff_range(r, -3, 61), diff_range(r, 0, 61));
    ds.Gfx_FlashCount = (int16_t)diff_pick(r, 0, 1, 3, -1);
    for (int c = 0; c < 3; c++)
        ds.Gfx_Palette[0][c] = (uint8_t)diff_rand(r, 64);
}

// --- Files and pictures ------------------------------------------------------------------

static const char *const pics[] = {
    "grafika\\main", "grafika\\options", "grafika\\choise", "grafika\\disk", "grafika\\diskbr",
    "grafika\\text", "grafika\\charset1", "grafika\\szatelit", "grafika\\sat1", "grafika\\hero1",
    "icon\\iconmain", "alien\\alien3", "epulet\\epulet7", "credits\\a1", "grafika\\bolygo",
};
enum { PICS = sizeof pics / sizeof pics[0] };

static void setup_load_file(DiffRng *r, int32_t *a)
{
    static const char *const files[] = {"text\\sz_face.raw", "text\\message.txt", "grafika\\main.pic",
                                        "icon\\icon.all", "text\\nosuch.txt"};
    a[0] = (int32_t)put_str(0, files[diff_rand(r, 5)]);
    a[1] = (int32_t)scratch(0x100);
    a[2] = (int32_t)scratch(0x108);
}

static void setup_icon_size(DiffRng *r, int32_t *a)
{
    (void)r;
    a[0] = (int32_t)scratch(0x100);
}

static void setup_open_data(DiffRng *r, int32_t *a)
{
    static const char *const files[] = {"vectors\\v10.vec", "icon\\icon.all", "text\\nosuch.txt",
                                        "anim\\atv1.spd"};
    a[0] = (int32_t)put_str(0, files[diff_rand(r, 4)]);
}

// A destination buffer of 64 KB in unused memory (B000h) or the screen.
static FarPtr pic_dest(DiffRng *r)
{
    return diff_chance(r, 50) ? far_ptr(0xA000, 0) : far_ptr((uint16_t)diff_pick(r, 0xB000, 0xB800), 0);
}

static void setup_load_pic(DiffRng *r, int32_t *a)
{
    a[0] = (int32_t)put_str(0, pics[diff_rand(r, PICS)]);
    a[1] = (int32_t)pic_dest(r);
    a[2] = diff_pick(r, 0, 1);
    a[3] = diff_pick(r, 200, 0x97, 0x20, 1, diff_range(r, 1, 220));
    a[4] = diff_pick(r, 0, 0, diff_range(r, 0, 100));
    a[5] = diff_pick(r, 0, 0, diff_range(r, 0, 30));
    a[6] = diff_pick(r, 320, 320, 0xF0, diff_range(r, 200, 400));
    a[7] = diff_chance(r, 50);
    a[8] = (int32_t)random_palette(r);
    a[9] = diff_pick(r, 0, 0, diff_range(r, 0, 255));
    a[10] = diff_pick(r, 0xFF, 0xB0, 2, 0x21, diff_range(r, a[9], 255));
    // The fade range, shifted by colourOffset, must stay in 0..255 and not
    // be empty: the temporary palette is a stack local (see range()).
    a[11] = diff_pick(r, 0, 0x40, 0x24, 2, diff_range(r, 0, 0x40));
    a[12] = diff_pick(r, 0, 0, diff_range(r, 0, 0x80));
    a[13] = diff_pick(r, 0xBE, 0xFF - a[11], 2, diff_range(r, a[12], 0xBE));
    a[13] = SDL_min(a[13], 0xFF - a[11]);
    a[12] = SDL_min(a[12], a[13]);
    ds.Game_MinMemAvail = diff_pick(r, ds.Game_MinMemAvail, 0x7FFFFFFF, 0);
}

static void setup_load_pic_screen(DiffRng *r, int32_t *a)
{
    a[0] = (int32_t)put_str(0, pics[diff_rand(r, PICS)]);
    a[1] = (int32_t)pic_dest(r);
    a[2] = diff_pick(r, 0x97, 200, diff_range(r, 1, 200));
    a[3] = (int32_t)random_palette(r);
}

static void setup_grafika(DiffRng *r, int32_t *a)
{
    static const char *const names[] = {"main", "bolygo", "disk", "szatelit", "options"};
    a[0] = (int32_t)put_str(0, names[diff_rand(r, 5)]);
}

// --- Text ------------------------------------------------------------------------------------

// The cursor over the text or elsewhere, shown or hidden.
static void cursor_near(DiffRng *r, int x, int y)
{
    if (diff_chance(r, 40)) {
        int mx = x + diff_range(r, -20, 60), my = y + diff_range(r, -18, 10);
        ds.Mouse_X = (int16_t)SDL_clamp(mx, 0, 318);  // as Mouse clamps it
        ds.Mouse_Y = (int16_t)SDL_clamp(my, 0, 198);
    }
    if (diff_chance(r, 30))
        ds.Mouse_CursorState = (uint16_t)diff_pick(r, 0, 1);
}

static void text_args(DiffRng *r, int32_t *a, int maxw)
{
    a[0] = (int32_t)random_text(r, 0, 60);
    int w = diff_pick(r, 0, 1, 18, diff_range(r, 0, maxw), diff_range(r, 0, maxw));
    a[3] = w;
    a[1] = diff_range(r, 0, SDL_max(0, 320 - w * 6));
    a[2] = diff_range(r, 0, 192);
    cursor_near(r, a[1], a[2]);
}

static void setup_text(DiffRng *r, int32_t *a)
{
    text_args(r, a, 53);
    a[4] = (int32_t)diff_pick(r, 0x34, 0xF9, 0xFC, 0xB0, diff_rand(r, 256));
}

static void setup_text_fixed(DiffRng *r, int32_t *a)
{
    text_args(r, a, 53);
}

static void setup_status(DiffRng *r, int32_t *a)
{
    a[0] = (int32_t)random_text(r, 0, 30);
    cursor_near(r, 16, 37);
}

static void setup_type(DiffRng *r, int32_t *a)
{
    text_args(r, a, 40);
    a[4] = (int32_t)diff_pick(r, 0xF9, 0xFC, diff_rand(r, 256));
}

static void setup_type_fixed(DiffRng *r, int32_t *a)
{
    text_args(r, a, 40);
}

static void setup_date(DiffRng *r, int32_t *a)
{
    (void)a;
    ds.Game_Year = (uint16_t)diff_pick(r, 2927, 0, 65535, diff_rand(r, 65536));
    ds.Game_Month = (uint16_t)diff_pick(r, 1, 12, 0, diff_rand(r, 65536));
    ds.Game_Day = (uint16_t)diff_pick(r, 1, 30, 9, diff_rand(r, 65536));
    ds.Game_Hour = (uint16_t)diff_pick(r, 0, 23, 9, diff_rand(r, 65536));
    cursor_near(r, 224, 37);
}

static void setup_credits(DiffRng *r, int32_t *a)
{
    (void)a;
    ds.Game_Credits = diff_pick(r, 0, 1000000, -1, 999999999, (int32_t)0x80000000,
                                (int32_t)diff_rand(r, 0xFFFFFFFFu), diff_range(r, -1000, 100000));
    cursor_near(r, 144, 37);
}

// --- Lines and rectangles ------------------------------------------------------------------

static void setup_hline(DiffRng *r, int32_t *a)
{
    a[0] = diff_range(r, 1, 320);
    a[1] = diff_pick(r, a[0], 320, diff_range(r, a[0], 320));
    a[2] = diff_range(r, 1, 200);
    a[3] = (int32_t)diff_rand(r, 256);
}

static void setup_vline(DiffRng *r, int32_t *a)
{
    a[0] = diff_range(r, 1, 320);
    a[1] = diff_range(r, 1, 200);
    a[2] = diff_pick(r, a[1], 200, diff_range(r, a[1], 200));
    a[3] = (int32_t)diff_rand(r, 256);
}

static void setup_rect(DiffRng *r, int32_t *a)
{
    a[0] = diff_range(r, 1, 320);
    a[1] = diff_range(r, 1, 200);
    a[2] = diff_pick(r, a[0], diff_range(r, a[0], 320));
    a[3] = diff_pick(r, a[1], diff_range(r, a[1], 200));
    a[4] = (int32_t)diff_rand(r, 256);
    cursor_near(r, a[0], a[1]);
}

static void setup_dither(DiffRng *r, int32_t *a)
{
    a[0] = diff_range(r, 0, 319);
    a[1] = diff_range(r, 0, 199);
    a[2] = diff_pick(r, a[0], a[0] - 1, diff_range(r, a[0], 319));
    a[3] = diff_pick(r, a[1], a[1] - 1, diff_range(r, a[1], 199));
    a[4] = (int32_t)diff_rand(r, 256);
    cursor_near(r, a[0], a[1]);
}

static void line_args(DiffRng *r, int32_t *a)
{
    a[1] = diff_range(r, 0, 319);
    a[2] = diff_range(r, 0, 199);
    switch (diff_rand(r, 5)) {
    case 0:  // vertical
        a[3] = a[1];
        a[4] = diff_range(r, 0, 199);
        break;
    case 1:  // horizontal
        a[3] = diff_range(r, 0, 319);
        a[4] = a[2];
        break;
    case 2:  // a point
        a[3] = a[1];
        a[4] = a[2];
        break;
    default:
        a[3] = diff_range(r, 0, 319);
        a[4] = diff_range(r, 0, 199);
        break;
    }
    if (diff_chance(r, 20)) {  // another row stride
        ds.Gfx_LineStride = (int16_t)diff_pick(r, 320, 240, 256, 100);
        ds.Gfx_LineStrideCopy = (int16_t)diff_pick(r, 320, 240, 256, 100);
    }
}

static void setup_line(DiffRng *r, int32_t *a)
{
    a[0] = diff_pick(r, 0xA000, 0xB000);
    line_args(r, a);
    a[5] = (int32_t)diff_rand(r, 256);
}

static void setup_line_copy(DiffRng *r, int32_t *a)
{
    a[0] = diff_pick(r, 0xA000, 0xB000);
    line_args(r, a);
    a[5] = a[0] == 0xA000 ? 0xB800 : 0xA000;
    uint8_t *src = far_host(far_ptr((uint16_t)a[5], 0));
    for (int i = 0; i < 64000; i += 1 + (int)diff_rand(r, 7))
        src[i] = (uint8_t)diff_rand(r, 256);
}

// --- Exit paths --------------------------------------------------------------------------------

static void setup_exit_handler(DiffRng *r, int32_t *a)
{
    (void)a;
    ds.System_ErrorAddr = (uint32_t)diff_pick(r, 0, 0, (int32_t)far_ptr(0x1234, 0x0042));
    ds.System_ExitCode = (int16_t)diff_pick(r, 0, 203, 200, 2);
}

// Base states: a new game on the command centre, the research screen and
// the system view.
static const char *const scripts[] = {"1500:quit", "900:click:72,16;1300:quit", "900:click:168,16;1300:quit"};
enum { STATES = sizeof scripts / sizeof scripts[0] };

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    DiffState *bases[STATES];
    for (int i = 0; i < STATES; i++) {
        if (!diff_boot(scripts[i]) || !diff_open_output()) {
            printf("FAILED to boot %s\n", scripts[i]);
            return 1;
        }
        bases[i] = diff_save();
    }
    diff_allow_port(0x3CC);  // SetVideoMode13: misc output register
    diff_allow_port(0x3C2);
    diff_Gfx_DrawText6x8.near = true;
    diff_Gfx_TypeText.near = true;
    // The file variable at DS:91EE: the port keeps the open file on the host.
    static const DiffRange file_rec[] = {DIFF_DS(0x91EE, 0x80), {0, 0}};
    diff_Gfx_IconAllOpenSize.ignore = file_rec;
    diff_Gfx_OpenDataFile.ignore = file_rec;
    // The error report written to Output (the port writes to stdout).
    static const DiffRange output[] = {DIFF_DS(offsetof(DSeg, System_Output), sizeof(TextRec)), {0, 0}};
    diff_Gfx_ExitHandler.ignore = output;

    const struct {
        DiffFn *fn;
        DiffSetup setup;
        int cases;
    } fns[] = {
        {&diff_Gfx_GetPalette, setup_palette_ptr, 6},
        {&diff_Gfx_SetPalette, setup_palette_ptr, 12},
        {&diff_Gfx_FadeIn, setup_fade, 15},
        {&diff_Gfx_FadeOut, setup_fade, 15},
        {&diff_Gfx_FadeInSteps, setup_fade_in_steps, 30},
        {&diff_Gfx_FadeOutSteps, setup_fade_steps, 30},
        {&diff_Gfx_FadeFromWhite, setup_fade_steps, 30},
        {&diff_Gfx_FadeToWhite, setup_fade_steps, 30},
        {&diff_Gfx_WhitenPalette, setup_whiten, 30},
        {&diff_Gfx_BlankPalette, setup_blank, 20},
        {&diff_Gfx_WaitScanlines, setup_scanlines, 16},
        {&diff_Gfx_WaitVRetrace, nullptr, 3},
        {&diff_Gfx_WaitFrames, setup_frames, 12},
        {&diff_Gfx_WaitFramesOrClick, setup_frames_click, 12},
        {&diff_Gfx_PaletteFlashTick, setup_flash, 30},
        {&diff_Gfx_LoadFile, setup_load_file, 15},
        {&diff_Gfx_IconAllOpenSize, setup_icon_size, 3},
        {&diff_Gfx_OpenDataFile, setup_open_data, 8},
        {&diff_Gfx_LoadPic, setup_load_pic, 40},
        {&diff_Gfx_LoadPicScreen, setup_load_pic_screen, 12},
        {&diff_Gfx_ShowGrafikaPic, setup_grafika, 6},
        {&diff_Gfx_SetVideoMode13, nullptr, 3},
        {&diff_Gfx_SetTextMode, nullptr, 3},
        {&diff_Gfx_DrawText6x8, setup_text, 40},
        {&diff_Gfx_DrawStatusText, setup_status, 12},
        {&diff_Gfx_DrawTextF9, setup_text_fixed, 9},
        {&diff_Gfx_DrawTextFC, setup_text_fixed, 9},
        {&diff_Gfx_DrawTextF1, setup_text_fixed, 9},
        {&diff_Gfx_DrawTextF4, setup_text_fixed, 9},
        {&diff_Gfx_DrawTextB0, setup_text_fixed, 9},
        {&diff_Gfx_DrawTextB3, setup_text_fixed, 9},
        {&diff_Gfx_DrawGameDate, setup_date, 15},
        {&diff_Gfx_DrawCredits, setup_credits, 15},
        {&diff_Gfx_FlashCredits, setup_credits, 6},
        {&diff_Gfx_TypeText, setup_type, 20},
        {&diff_Gfx_TypeTextF9, setup_type_fixed, 6},
        {&diff_Gfx_TypeTextFC, setup_type_fixed, 6},
        {&diff_Gfx_HLine, setup_hline, 20},
        {&diff_Gfx_VLine, setup_vline, 20},
        {&diff_Gfx_DrawRectOutline, setup_rect, 20},
        {&diff_Gfx_DrawDitherRect, setup_dither, 20},
        {&diff_Gfx_DrawLine, setup_line, 60},
        {&diff_Gfx_DrawLineCopy, setup_line_copy, 30},
        {&diff_Gfx_FatalExit, nullptr, 3},
        {&diff_Gfx_OutOfMemoryExit, nullptr, 3},
        {&diff_Gfx_ExitHandler, setup_exit_handler, 9},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, bases, STATES, fns[i].setup, diff_cases(fns[i].cases));
    for (int i = 0; i < STATES; i++)
        diff_free(bases[i]);
    return diff_finish();
}
