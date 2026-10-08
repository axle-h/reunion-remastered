// Differential test of unit Anim (3c64): the .ANI loaders, their Free
// routines, the frame decoders and the wedge blit, original (under Unicorn)
// against the port, from the command centre (where DS:7E40 points to an
// animation record). See docs/difftest.md. Needs the game data.
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/anim.h"
#include "game/dseg.h"

DIFF_PROC(Anim_LoadWarAnim, Anim_LoadWarAnim((uint16_t)a[0]));
DIFF_PROC(Anim_FreeWarAnim, Anim_FreeWarAnim((uint16_t)a[0]));
DIFF_PROC(Anim_LoadMainAnim, Anim_LoadMainAnim((uint16_t)a[0]));
DIFF_PROC(Anim_FreeMainAnim, Anim_FreeMainAnim((uint16_t)a[0]));
DIFF_PROC(Anim_LoadGrAnim, Anim_LoadGrAnim((uint16_t)a[0]));
DIFF_PROC(Anim_FreeGrAnim, Anim_FreeGrAnim((uint16_t)a[0]));
DIFF_PROC(Anim_DrawAnimKeyed, Anim_DrawAnimKeyed((uint16_t)a[0], (FarPtr)a[1]));
DIFF_PROC(Anim_DrawAnimFrame, Anim_DrawAnimFrame((uint16_t)a[0], (FarPtr)a[1], far_host((FarPtr)a[2])));
DIFF_PROC(Anim_DecodeAnimFrame, Anim_DecodeAnimFrame((FarPtr)a[0], (FarPtr)a[1], (uint8_t)a[2]));
DIFF_PROC(Anim_BlitWedgeToScreen, Anim_BlitWedgeToScreen((FarPtr)a[0]));

enum { REC_PTRS = 0x001, REC_SIZES = 0x231 };

static FarPtr rec(void) { return *(FarPtr *)ds_at(0x7E40); }
static uint8_t *rec_at(uint16_t off) { return (uint8_t *)far_host(rec()) + off; }
static FarPtr frame_ptr(int f)
{
    FarPtr p;
    memcpy(&p, rec_at((uint16_t)(REC_PTRS + (f - 1) * 4)), 4);
    return p;
}
static uint16_t frame_size(int f)
{
    uint16_t v;
    memcpy(&v, rec_at((uint16_t)(REC_SIZES + (f - 1) * 2)), 2);
    return v;
}
static uint8_t main_frames(int id) { return *(uint8_t *)ds_at((uint16_t)(0x55D7 + 7 * id)); }

static FarPtr screen(void) { return far_ptr(ds.Gfx_ScreenSeg, 0); }

// Sometimes above MemAvail, so the routines lower it.
static void min_mem(DiffRng *r)
{
    if (diff_chance(r, 50))
        ds.Game_MinMemAvail = diff_pick(r, 0x7FFFFFFF, 0x100000, 0);
}

// Main animations that exist (no MAIN7) and are small enough for the heap
// next to the command centre's buffers.
static int pick_main(DiffRng *r)
{
    return diff_pick(r, 1, 2, 3, 4, 5, 6, 8, 10, 11, 12, 13, 15, 17, diff_range(r, 1, 6));
}

// --- Loaders --------------------------------------------------------------------------

static void setup_load_war(DiffRng *r, int32_t *a)
{
    min_mem(r);
    *rec_at(0) = (uint8_t)diff_pick(r, 0, 0, 0, 1);
    a[0] = diff_range(r, 1, 25);
}

static void setup_load_main(DiffRng *r, int32_t *a)
{
    min_mem(r);
    *rec_at(0) = (uint8_t)diff_pick(r, 0, 0, 0, 1);
    a[0] = pick_main(r);
}

static void setup_load_gr(DiffRng *r, int32_t *a)
{
    min_mem(r);
    *rec_at(0) = (uint8_t)diff_pick(r, 0, 0, 0, 1);
    a[0] = diff_range(r, 1, 14);
}

// --- Free: load first (port), then free ---------------------------------------------

static void setup_free_war(DiffRng *r, int32_t *a)
{
    a[0] = diff_range(r, 1, 25);
    *rec_at(0) = 0;
    Anim_LoadWarAnim((uint16_t)a[0]);
    *rec_at(0) = (uint8_t)diff_pick(r, 1, 1, 1, 0);
}

static void setup_free_main(DiffRng *r, int32_t *a)
{
    a[0] = pick_main(r);
    *rec_at(0) = 0;
    Anim_LoadMainAnim((uint16_t)a[0]);
    *rec_at(0) = (uint8_t)diff_pick(r, 1, 1, 1, 0);
}

static void setup_free_gr(DiffRng *r, int32_t *a)
{
    a[0] = diff_range(r, 1, 14);
    *rec_at(0) = 0;
    Anim_LoadGrAnim((uint16_t)a[0]);
    *rec_at(0) = (uint8_t)diff_pick(r, 1, 1, 1, 0);
}

// --- Drawing ----------------------------------------------------------------------------

// Loads main animation `id` into the record; returns its frame count.
static int load_main(int id)
{
    *rec_at(0) = 0;
    Anim_LoadMainAnim((uint16_t)id);
    return main_frames(id);
}

static void setup_keyed(DiffRng *r, int32_t *a)
{
    int n = load_main(pick_main(r));
    a[0] = diff_pick(r, 1, 2, n, diff_range(r, 1, n));
    a[1] = (int32_t)screen();
    // Some frames empty (size 0): the keyed draw falls back to an earlier one.
    for (int f = 2; f <= n; f++)
        if (diff_chance(r, 15))
            memset(rec_at((uint16_t)(REC_SIZES + (f - 1) * 2)), 0, 2);
    if (diff_chance(r, 10))
        *rec_at(0) = 0;
}

static void setup_frame(DiffRng *r, int32_t *a)
{
    int n = load_main(pick_main(r));
    a[0] = diff_pick(r, 1, 2, n, diff_range(r, 1, n));
    a[1] = (int32_t)screen();
    a[2] = (int32_t)DIFF_SCRATCH;
    *(uint8_t *)far_host(DIFF_SCRATCH) = (uint8_t)diff_range(r, 0, 2);
    if (diff_chance(r, 20))  // a nil frame pointer: ok := false
        memset(rec_at((uint16_t)(REC_PTRS + (a[0] - 1) * 4)), 0, 4);
    if (diff_chance(r, 10))
        *rec_at(0) = 0;
}

static void setup_decode(DiffRng *r, int32_t *a)
{
    min_mem(r);
    int id = pick_main(r);
    int n = load_main(id);
    int f = diff_range(r, 1, n);
    while (f > 1 && frame_size(f) == 0)
        f--;
    a[0] = (int32_t)frame_ptr(f);
    a[1] = (int32_t)screen();
    a[2] = diff_pick(r, 0x40, 0x40, 0, 0x80, diff_range(r, 0, 255));
}

// A made-up frame in the scratch buffer with every opcode form, the long
// (16-bit) skips and runs included.
static void setup_decode_synth(DiffRng *r, int32_t *a)
{
    min_mem(r);
    uint8_t *p = far_host(DIFF_SCRATCH);
    uint16_t w = (uint16_t)diff_range(r, 1, 120), h = (uint16_t)diff_range(r, 1, 40);
    p[9] = (uint8_t)w;
    p[10] = (uint8_t)(w >> 8);
    p[11] = (uint8_t)h;
    p[12] = (uint8_t)(h >> 8);
    int i = 13;
    int left = w * h;
    while (left > 0 && i < (int)DIFF_SCRATCH_SIZE - 8) {
        int n;
        switch (diff_rand(r, 5)) {
        case 0:
            p[i++] = (uint8_t)diff_rand(r, 0x80);
            n = 1;
            break;
        case 1:  // short skip
            n = diff_range(r, 1, left < 0x3F ? left : 0x3F);
            p[i++] = (uint8_t)(0x80 + n);
            break;
        case 2:  // long skip
            n = diff_range(r, 1, left);
            p[i++] = 0x80;
            p[i++] = (uint8_t)n;
            p[i++] = (uint8_t)(n >> 8);
            break;
        case 3:  // short run
            n = diff_range(r, 1, left < 0x3F ? left : 0x3F);
            p[i++] = (uint8_t)(0xC0 + n);
            p[i++] = (uint8_t)diff_rand(r, 256);
            break;
        default:  // long run
            n = diff_range(r, 1, left);
            p[i++] = 0xC0;
            p[i++] = (uint8_t)n;
            p[i++] = (uint8_t)(n >> 8);
            p[i++] = (uint8_t)diff_rand(r, 256);
            break;
        }
        left -= n;
    }
    if (left > 0) {  // out of room: one long skip to finish
        p[i++] = 0x80;
        p[i++] = (uint8_t)left;
        p[i++] = (uint8_t)(left >> 8);
    }
    a[0] = (int32_t)DIFF_SCRATCH;
    a[1] = (int32_t)far_ptr(ds.Gfx_ScreenSeg, (uint16_t)diff_range(r, 0, 0x4000));
    a[2] = diff_pick(r, 0x40, 0, diff_range(r, 0, 255));
}

static void setup_wedge(DiffRng *r, int32_t *a)
{
    // Any source block: the screen itself, a heap block, low memory.
    a[0] = diff_pick(r, (int32_t)screen(), (int32_t)far_ptr(0x2000, 0), (int32_t)far_ptr(0x3000, 0x10),
                     (int32_t)*(FarPtr *)ds_at(0x7E08));
    // Something to copy: random bytes in the source rows.
    uint8_t *p = far_host((FarPtr)a[0]);
    for (int i = 0x0C28; i < 0x5E20; i += diff_range(r, 1, 64))
        p[i] = (uint8_t)diff_rand(r, 256);
}

// The original's file variable of the graphics data file (Gfx data file,
// TP FileRec): the port keeps its open file on the host side.
static const DiffRange file_rec[] = {DIFF_DS(0x91EE, 0x80), {0, 0}};

int main(int argc, char *argv[])
{
    diff_Anim_LoadWarAnim.ignore = file_rec;
    diff_Anim_LoadMainAnim.ignore = file_rec;
    diff_Anim_LoadGrAnim.ignore = file_rec;

    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    if (!diff_boot("1500:quit")) {
        printf("FAILED to reach the command centre\n");
        return 1;
    }
    DiffState *base = diff_save();

    const struct {
        DiffFn *fn;
        DiffSetup setup;
        int cases;
    } fns[] = {
        {&diff_Anim_LoadWarAnim, setup_load_war, 30},
        {&diff_Anim_LoadMainAnim, setup_load_main, 30},
        {&diff_Anim_LoadGrAnim, setup_load_gr, 30},
        {&diff_Anim_FreeWarAnim, setup_free_war, 25},
        {&diff_Anim_FreeMainAnim, setup_free_main, 25},
        {&diff_Anim_FreeGrAnim, setup_free_gr, 25},
        {&diff_Anim_DrawAnimKeyed, setup_keyed, 30},
        {&diff_Anim_DrawAnimFrame, setup_frame, 30},
        {&diff_Anim_DecodeAnimFrame, setup_decode, 30},
        {&diff_Anim_DecodeAnimFrame, setup_decode_synth, 30},
        {&diff_Anim_BlitWedgeToScreen, setup_wedge, 8},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, &base, 1, fns[i].setup, diff_cases(fns[i].cases));
    diff_free(base);
    return diff_finish();
}
