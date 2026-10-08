// Differential test of unit Scale (3955): the 17 row scalers and ScaleSprite,
// original (under Unicorn) against the port, on random pixels at random
// offsets of the screen segment. See docs/difftest.md. Needs the game data.
#include <stdio.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/scale.h"

#define ROW(n) DIFF_PROC(Scale_ScaleRow##n, Scale_ScaleRow##n((FarPtr)a[0], (FarPtr)a[1]))
ROW(16);
ROW(17);
ROW(18);
ROW(19);
ROW(20);
ROW(21);
ROW(22);
ROW(23);
ROW(24);
ROW(25);
ROW(26);
ROW(27);
ROW(28);
ROW(29);
ROW(30);
ROW(31);
ROW(32);
#undef ROW
DIFF_PROC(Scale_ScaleSprite, Scale_ScaleSprite((FarPtr)a[0], (FarPtr)a[1], (int16_t)a[2]));

// Random bytes in [off, off + n) of segment seg (offsets wrap like si/di).
static void fill(DiffRng *r, uint16_t seg, uint16_t off, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        *(uint8_t *)far_host(far_ptr(seg, (uint16_t)(off + i))) = (uint8_t)diff_rand(r, 256);
}

// Source and destination offsets for a block of `span` bytes: they don't
// overlap (the original's movsw copies two bytes at a time, the port one, so
// overlapping copies differ, and no caller overlaps them) and the block
// doesn't cross the segment's end (a word at FFFFh wraps differently on an
// 8086, a 386 and Unicorn).
static void offsets(DiffRng *r, uint32_t span, uint16_t *s, uint16_t *d)
{
    for (;;) {
        *s = (uint16_t)diff_pick(r, 0, diff_range(r, 0, (int32_t)(0x10000 - span)),
                                 (int32_t)(0x10000 - span));
        *d = (uint16_t)diff_pick(r, 0, diff_range(r, 0, (int32_t)(0x10000 - span)),
                                 (int32_t)(0x10000 - span));
        if (*s + span <= *d || *d + span <= *s)
            return;
    }
}

static void setup_row(DiffRng *r, int32_t *a)
{
    uint16_t s, d;
    offsets(r, 32, &s, &d);
    fill(r, 0xA000, s, 32);
    a[0] = (int32_t)far_ptr(0xA000, s);
    a[1] = (int32_t)far_ptr(0xA000, d);
}

static void setup_sprite(DiffRng *r, int32_t *a)
{
    uint16_t s, d;
    offsets(r, 31 * 320 + 32, &s, &d);
    fill(r, 0xA000, s, 32 * 320);
    a[0] = (int32_t)far_ptr(0xA000, s);
    a[1] = (int32_t)far_ptr(0xA000, d);
    a[2] = diff_pick(r, diff_range(r, 16, 32), diff_range(r, 16, 32), diff_range(r, 16, 32), 16, 32,
                     15, 33, 0, -5, 100, diff_range(r, -32768, 32767));
}

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    if (!diff_boot_new_game(1500)) {
        printf("FAILED to boot\n");
        return 1;
    }
    DiffState *base = diff_save();
    DiffFn *rows[] = {&diff_Scale_ScaleRow16, &diff_Scale_ScaleRow17, &diff_Scale_ScaleRow18,
                      &diff_Scale_ScaleRow19, &diff_Scale_ScaleRow20, &diff_Scale_ScaleRow21,
                      &diff_Scale_ScaleRow22, &diff_Scale_ScaleRow23, &diff_Scale_ScaleRow24,
                      &diff_Scale_ScaleRow25, &diff_Scale_ScaleRow26, &diff_Scale_ScaleRow27,
                      &diff_Scale_ScaleRow28, &diff_Scale_ScaleRow29, &diff_Scale_ScaleRow30,
                      &diff_Scale_ScaleRow31, &diff_Scale_ScaleRow32};
    for (size_t i = 0; i < sizeof rows / sizeof rows[0]; i++)
        diff_run(rows[i], &base, 1, setup_row, diff_cases(8));
    diff_run(&diff_Scale_ScaleSprite, &base, 1, setup_sprite, diff_cases(40));
    diff_free(base);
    return diff_finish();
}
