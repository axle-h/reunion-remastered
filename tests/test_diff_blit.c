// Differential test of unit Blit (47da): every copy, transparent blit, fill
// and cursor-buffer routine, with random rectangles in video memory (the
// cursor hook path) and in unused memory, random pixels (key colours
// included) and random cursor states. See docs/difftest.md. Needs the game data.
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/blit.h"
#include "game/dseg.h"

#define P(i) ((FarPtr)a[i])

DIFF_PROC(Blit_CopyBytes, Blit_CopyBytes(P(0), P(1), (uint16_t)a[2]));
DIFF_PROC(Blit_CopyRect, Blit_CopyRect(P(0), P(1), (int16_t)a[2], (int16_t)a[3]));
DIFF_PROC(Blit_CopyRectStrided,
          Blit_CopyRectStrided(P(0), P(1), (int16_t)a[2], (int16_t)a[3], (int16_t)a[4], (int16_t)a[5]));
DIFF_PROC(Blit_Copy16x16, Blit_Copy16x16(P(0), P(1)));
DIFF_PROC(Blit_InterlaceReveal, Blit_InterlaceReveal(P(0), P(1), (int16_t)a[2], (int16_t)a[3]));
DIFF_PROC(Blit_CopyRowTransparent, Blit_CopyRowTransparent(P(0), P(1), (uint16_t)a[2], (uint8_t)a[3]));
DIFF_PROC(Blit_BlitTransparent,
          Blit_BlitTransparent(P(0), P(1), (int16_t)a[2], (int16_t)a[3], (uint8_t)a[4]));
DIFF_PROC(Blit_BlitTransparentStrided,
          Blit_BlitTransparentStrided(P(0), P(1), (int16_t)a[2], (int16_t)a[3], (int16_t)a[4],
                                      (int16_t)a[5], (uint8_t)a[6]));
DIFF_PROC(Blit_BlitOverBackground, Blit_BlitOverBackground(P(0), P(1), (int16_t)a[2], (int16_t)a[3]));
DIFF_PROC(Blit_CopyColumnTransparentDown,
          Blit_CopyColumnTransparentDown(P(0), P(1), (uint16_t)a[2], (uint8_t)a[3]));
DIFF_PROC(Blit_BlitTransposed, Blit_BlitTransposed(P(0), P(1), (int16_t)a[2], (uint8_t)a[3]));
DIFF_PROC(Blit_CopyRowTransparentReverse,
          Blit_CopyRowTransparentReverse(P(0), P(1), (uint16_t)a[2], (uint8_t)a[3]));
DIFF_PROC(Blit_BlitRotate180, Blit_BlitRotate180(P(0), P(1), (int16_t)a[2], (uint8_t)a[3]));
DIFF_PROC(Blit_CopyColumnTransparentUp,
          Blit_CopyColumnTransparentUp(P(0), P(1), (uint16_t)a[2], (uint8_t)a[3]));
DIFF_PROC(Blit_BlitAntiTranspose, Blit_BlitAntiTranspose(P(0), P(1), (int16_t)a[2], (uint8_t)a[3]));
DIFF_PROC(Blit_FillBytes, Blit_FillBytes(P(0), (uint16_t)a[1], (uint8_t)a[2]));
DIFF_PROC(Blit_FillRect,
          Blit_FillRect((int16_t)a[0], (int16_t)a[1], (int16_t)a[2], (int16_t)a[3], (uint8_t)a[4]));
DIFF_PROC(Blit_FillRectAt, Blit_FillRectAt(P(0), (int16_t)a[1], (int16_t)a[2], (uint8_t)a[3]));
DIFF_PROC(Blit_DrawCursorImage,
          Blit_DrawCursorImage((uint16_t)a[0], (int16_t)a[1], (int16_t)a[2], (uint16_t)a[3]));
DIFF_PROC(Blit_SaveCursorBackground,
          Blit_SaveCursorBackground((uint16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_PROC(Blit_RestoreCursorBackground,
          Blit_RestoreCursorBackground((uint16_t)a[0], (int16_t)a[1], (int16_t)a[2]));

// --- Inputs ---------------------------------------------------------------------

static uint8_t key_colour(DiffRng *r)
{
    return (uint8_t)diff_pick(r, 0, 0x40, 0x40, 2, 0xFF, diff_rand(r, 256));
}

// Random pixels in [p, p + n), drawn from a few colours (so the key and the
// 40h/60h thresholds come up often) or from all 256.
static void fill_random(DiffRng *r, FarPtr p, uint32_t n, uint8_t key)
{
    bool few = diff_chance(r, 60);
    uint8_t pal[4] = {key, 0x40, (uint8_t)diff_range(r, 0x50, 0x6F), (uint8_t)diff_rand(r, 256)};
    for (uint32_t i = 0; i < n; i++) {
        uint8_t v = few ? pal[diff_rand(r, 4)] : (uint8_t)diff_rand(r, 256);
        *(uint8_t *)far_host(far_ptr((uint16_t)(p >> 16), (uint16_t)((uint16_t)p + i))) = v;
    }
}

// A block of `extent` bytes that stays inside one segment: on the screen
// (A000h, where CopyRect calls the cursor hook) or in unused memory.
static FarPtr region(DiffRng *r, uint32_t extent, bool screen)
{
    uint16_t seg = screen ? 0xA000 : (uint16_t)diff_pick(r, 0xB000, 0xB800, 0xC000);
    uint32_t room = extent >= 0x10000 ? 0 : 0x10000 - extent;
    if (screen && extent <= 64000 && diff_chance(r, 70))
        room = 64000 - extent;  // on the visible screen
    return far_ptr(seg, (uint16_t)diff_rand(r, room + 1));
}

static bool screen_dest(DiffRng *r)
{
    return diff_chance(r, 60);
}

// The software cursor: shown or hidden, frozen, anywhere.
static void cursor_state(DiffRng *r)
{
    if (!diff_chance(r, 50))
        return;
    ds.Mouse_CursorState = (uint16_t)diff_pick(r, 0, 0, 1);
    ds.Mouse_CursorFrozen = (uint8_t)diff_pick(r, 0, 0, 0, 1);
    if (diff_chance(r, 50)) {
        ds.Mouse_X = (int16_t)diff_range(r, 0, 318);
        ds.Mouse_Y = (int16_t)diff_range(r, 0, 198);
    }
}

static int16_t dim(DiffRng *r, int lo, int hi)
{
    return (int16_t)diff_pick(r, lo, lo + 1, hi, diff_range(r, lo, hi), diff_range(r, lo, hi));
}

// --- Copies --------------------------------------------------------------------------

static void setup_copy_bytes(DiffRng *r, int32_t *a)
{
    uint16_t n = (uint16_t)diff_pick(r, 0, 1, 2, 3, diff_range(r, 0, 400), diff_range(r, 0, 20000));
    FarPtr src = region(r, n + 2u, diff_chance(r, 30));
    FarPtr dst;
    if (diff_chance(r, 20)) {  // overlapping, either way
        int d = diff_pick(r, -3, -2, -1, 1, 2, 3, 7);
        dst = far_ptr((uint16_t)(src >> 16), (uint16_t)((uint16_t)src + d));
        if ((uint16_t)src < 8 || (uint32_t)(uint16_t)src + n + 8 > 0xFFFF)
            dst = src;
    } else {
        dst = region(r, n + 2u, screen_dest(r));
    }
    fill_random(r, src, n, 0);
    a[0] = (int32_t)src;
    a[1] = (int32_t)dst;
    a[2] = n;
}

static void setup_copy_rect(DiffRng *r, int32_t *a)
{
    cursor_state(r);
    int16_t w = dim(r, 0, 320), rows = dim(r, 1, 200);
    uint32_t extent = (uint32_t)(rows - 1) * 320 + (uint32_t)w + 1;
    FarPtr src = region(r, extent, diff_chance(r, 20));
    FarPtr dst = region(r, extent, screen_dest(r));
    fill_random(r, src, extent, 0);
    a[0] = (int32_t)src;
    a[1] = (int32_t)dst;
    a[2] = w;
    a[3] = rows;
}

static void setup_copy_rect_strided(DiffRng *r, int32_t *a)
{
    cursor_state(r);
    int16_t w = dim(r, 0, 320), rows = dim(r, 1, 100);
    int16_t ss = (int16_t)diff_pick(r, w, 320, diff_range(r, w, 400));
    int16_t dsd = (int16_t)diff_pick(r, w, 320, diff_range(r, w, 400));
    FarPtr src = region(r, (uint32_t)(rows - 1) * (uint16_t)ss + (uint32_t)w + 1, false);
    FarPtr dst = region(r, (uint32_t)(rows - 1) * (uint16_t)dsd + (uint32_t)w + 1, screen_dest(r));
    fill_random(r, src, (uint32_t)(rows - 1) * (uint16_t)ss + (uint32_t)w, 0);
    a[0] = (int32_t)src;
    a[1] = (int32_t)dst;
    a[2] = w;
    a[3] = rows;
    a[4] = ss;
    a[5] = dsd;
}

static void setup_copy16(DiffRng *r, int32_t *a)
{
    cursor_state(r);
    uint32_t extent = 15 * 320 + 16;
    FarPtr src = region(r, extent, false);
    FarPtr dst = region(r, extent, diff_chance(r, 85));
    if (diff_chance(r, 40)) {  // where the cursor is
        int y = ds.Mouse_Y - diff_range(r, 0, 20), x = ds.Mouse_X - diff_range(r, 0, 20);
        dst = far_ptr(0xA000, (uint16_t)(SDL_clamp(y, 0, 184) * 320 + SDL_clamp(x, 0, 304)));
    }
    fill_random(r, src, extent, 0);
    a[0] = (int32_t)src;
    a[1] = (int32_t)dst;
}

static void setup_interlace(DiffRng *r, int32_t *a)
{
    int16_t w = dim(r, 1, 320), rows = dim(r, 1, 120);
    uint32_t extent = (uint32_t)(rows + 1) * 320 + (uint32_t)w + 1;
    FarPtr src = region(r, extent, false);
    FarPtr dst = region(r, extent, screen_dest(r));
    fill_random(r, src, extent, 0);
    a[0] = (int32_t)src;
    a[1] = (int32_t)dst;
    a[2] = w;
    a[3] = rows;
}

// --- Transparent blits ------------------------------------------------------------------

static void setup_row_transparent(DiffRng *r, int32_t *a)
{
    uint16_t n = (uint16_t)dim(r, 1, 640);
    uint8_t key = key_colour(r);
    FarPtr src = region(r, n, false), dst = region(r, n, screen_dest(r));
    fill_random(r, src, n, key);
    a[0] = (int32_t)src;
    a[1] = (int32_t)dst;
    a[2] = n;
    a[3] = key;
}

static void setup_blit_transparent(DiffRng *r, int32_t *a)
{
    int16_t w = dim(r, 1, 320), rows = dim(r, 1, 150);
    uint32_t extent = (uint32_t)(rows - 1) * 320 + (uint32_t)w;
    uint8_t key = key_colour(r);
    FarPtr src = region(r, extent, false), dst = region(r, extent, screen_dest(r));
    fill_random(r, src, extent, key);
    a[0] = (int32_t)src;
    a[1] = (int32_t)dst;
    a[2] = w;
    a[3] = rows;
    a[4] = key;
}

static void setup_blit_transparent_strided(DiffRng *r, int32_t *a)
{
    int16_t w = dim(r, 1, 320), rows = dim(r, 1, 100);
    int16_t ss = (int16_t)diff_pick(r, w, 320, diff_range(r, w, 400));
    int16_t dsd = (int16_t)diff_pick(r, w, 320, diff_range(r, w, 400));
    uint8_t key = key_colour(r);
    uint32_t se = (uint32_t)(rows - 1) * (uint16_t)ss + (uint32_t)w;
    FarPtr src = region(r, se, false);
    FarPtr dst = region(r, (uint32_t)(rows - 1) * (uint16_t)dsd + (uint32_t)w, screen_dest(r));
    fill_random(r, src, se, key);
    a[0] = (int32_t)src;
    a[1] = (int32_t)dst;
    a[2] = w;
    a[3] = rows;
    a[4] = ss;
    a[5] = dsd;
    a[6] = key;
}

static void setup_over_background(DiffRng *r, int32_t *a)
{
    int16_t w = dim(r, 1, 320), rows = dim(r, 1, 150);
    uint32_t extent = (uint32_t)(rows - 1) * 320 + (uint32_t)w;
    FarPtr src = region(r, extent, false), dst = region(r, extent, screen_dest(r));
    fill_random(r, src, extent, 0x40);
    if (diff_chance(r, 50))
        fill_random(r, dst, extent, 0x5F);
    a[0] = (int32_t)src;
    a[1] = (int32_t)dst;
    a[2] = w;
    a[3] = rows;
}

// Down: dest steps +320. Up: -320. Reverse: -1.
static void setup_column(DiffRng *r, int32_t *a, int step)
{
    uint16_t n = (uint16_t)dim(r, 1, 200);
    uint8_t key = key_colour(r);
    uint32_t extent = (uint32_t)(n - 1) * (uint32_t)SDL_abs(step) + 1;
    FarPtr src = region(r, n, false), dst = region(r, extent, screen_dest(r));
    if (step < 0)
        dst = far_ptr((uint16_t)(dst >> 16), (uint16_t)((uint16_t)dst + extent - 1));
    fill_random(r, src, n, key);
    a[0] = (int32_t)src;
    a[1] = (int32_t)dst;
    a[2] = n;
    a[3] = key;
}

static void setup_column_down(DiffRng *r, int32_t *a) { setup_column(r, a, 320); }
static void setup_column_up(DiffRng *r, int32_t *a) { setup_column(r, a, -320); }
static void setup_row_reverse(DiffRng *r, int32_t *a) { setup_column(r, a, -1); }

// The square blits: size x size, stride 320 on both sides.
static void setup_square(DiffRng *r, int32_t *a)
{
    int16_t n = dim(r, 1, 100);
    uint8_t key = key_colour(r);
    uint32_t extent = (uint32_t)(n - 1) * 321 + 1;
    FarPtr src = region(r, extent, false), dst = region(r, extent, screen_dest(r));
    fill_random(r, src, extent, key);
    a[0] = (int32_t)src;
    a[1] = (int32_t)dst;
    a[2] = n;
    a[3] = key;
}

// --- Fills ---------------------------------------------------------------------------------

static void setup_fill_bytes(DiffRng *r, int32_t *a)
{
    uint16_t n = (uint16_t)diff_pick(r, 0, 1, 2, 3, 4, 5, diff_range(r, 0, 400), diff_range(r, 0, 30000));
    a[0] = (int32_t)region(r, n + 4u, screen_dest(r));
    a[1] = n;
    a[2] = (int32_t)diff_rand(r, 256);
}

static void setup_fill_rect(DiffRng *r, int32_t *a)
{
    int16_t x = (int16_t)diff_range(r, 0, 319), y = (int16_t)diff_range(r, 0, 199);
    if (diff_chance(r, 10))
        y = (int16_t)diff_range(r, 200, 203);  // past the screen, still in the segment
    a[0] = x;
    a[1] = y;
    a[2] = dim(r, 1, 320 - x);
    a[3] = dim(r, 1, SDL_max(1, 200 - y));
    a[4] = (int32_t)diff_rand(r, 256);
}

static void setup_fill_rect_at(DiffRng *r, int32_t *a)
{
    int16_t w = dim(r, 1, 320), h = dim(r, 1, 150);
    a[0] = (int32_t)region(r, (uint32_t)(h - 1) * 320 + (uint32_t)w + 2, screen_dest(r));
    a[1] = w;
    a[2] = h;
    a[3] = (int32_t)diff_rand(r, 256);
}

// --- The cursor's buffers ----------------------------------------------------------------

static void setup_cursor_image(DiffRng *r, int32_t *a)
{
    int16_t w = dim(r, 1, 16), rows = dim(r, 1, 16);
    for (int k = 0; k < 6; k++)
        if (diff_chance(r, 30))
            fill_random(r, far_ptr(MEM_DS_SEG, (uint16_t)(offsetof(DSeg, Mouse_CursorImages) + k * 256)), 256, 0);
    a[0] = diff_range(r, 0, 199 - rows) * 320 + diff_range(r, 0, 320 - w);
    a[1] = w;
    a[2] = rows;
    a[3] = diff_range(r, 0, 5) * 256 + diff_pick(r, 0, 0, diff_range(r, 0, 16 * (16 - rows) + 16 - w));
}

static void setup_cursor_background(DiffRng *r, int32_t *a)
{
    int16_t w = dim(r, 0, 16), rows = dim(r, 1, 16);
    fill_random(r, far_ptr(MEM_DS_SEG, offsetof(DSeg, Blit_CursorBackground)), 256, 0);
    a[0] = diff_range(r, 0, 199 - rows) * 320 + diff_range(r, 0, 320 - w);
    a[1] = w;
    a[2] = rows;
}

// Base states: a new game on the command centre (cursor shown) and on the
// research screen.
static const char *const scripts[] = {"1500:quit", "900:click:72,16;1300:quit"};
enum { STATES = sizeof scripts / sizeof scripts[0] };

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    DiffState *bases[STATES];
    for (int i = 0; i < STATES; i++) {
        if (!diff_boot(scripts[i])) {
            printf("FAILED to boot %s\n", scripts[i]);
            return 1;
        }
        bases[i] = diff_save();
    }
    diff_Blit_CopyColumnTransparentDown.near = true;
    diff_Blit_CopyRowTransparentReverse.near = true;
    diff_Blit_CopyColumnTransparentUp.near = true;

    const struct {
        DiffFn *fn;
        DiffSetup setup;
        int cases;
    } fns[] = {
        {&diff_Blit_CopyBytes, setup_copy_bytes, 60},
        {&diff_Blit_CopyRect, setup_copy_rect, 60},
        {&diff_Blit_CopyRectStrided, setup_copy_rect_strided, 40},
        {&diff_Blit_Copy16x16, setup_copy16, 40},
        {&diff_Blit_InterlaceReveal, setup_interlace, 30},
        {&diff_Blit_CopyRowTransparent, setup_row_transparent, 40},
        {&diff_Blit_BlitTransparent, setup_blit_transparent, 40},
        {&diff_Blit_BlitTransparentStrided, setup_blit_transparent_strided, 40},
        {&diff_Blit_BlitOverBackground, setup_over_background, 40},
        {&diff_Blit_CopyColumnTransparentDown, setup_column_down, 30},
        {&diff_Blit_BlitTransposed, setup_square, 30},
        {&diff_Blit_CopyRowTransparentReverse, setup_row_reverse, 30},
        {&diff_Blit_BlitRotate180, setup_square, 30},
        {&diff_Blit_CopyColumnTransparentUp, setup_column_up, 30},
        {&diff_Blit_BlitAntiTranspose, setup_square, 30},
        {&diff_Blit_FillBytes, setup_fill_bytes, 40},
        {&diff_Blit_FillRect, setup_fill_rect, 40},
        {&diff_Blit_FillRectAt, setup_fill_rect_at, 40},
        {&diff_Blit_DrawCursorImage, setup_cursor_image, 30},
        {&diff_Blit_SaveCursorBackground, setup_cursor_background, 30},
        {&diff_Blit_RestoreCursorBackground, setup_cursor_background, 30},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, bases, STATES, fns[i].setup, diff_cases(fns[i].cases));
    for (int i = 0; i < STATES; i++)
        diff_free(bases[i]);
    return diff_finish();
}
