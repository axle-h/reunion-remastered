// Differential test of unit RawText (4049): the three raw text loaders, the
// empty procedure and SetPaletteRange (the DAC is compared). See
// docs/difftest.md. Needs the game data.
#include <stdio.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/rawtext.h"
#include "game/system.h"

DIFF_PROC(RawText_LoadSzFace, RawText_LoadSzFace());
DIFF_PROC(RawText_LoadSzTalal, RawText_LoadSzTalal());
DIFF_PROC(RawText_LoadSzFaj, RawText_LoadSzFaj());
DIFF_PROC(RawText_Nop, RawText_Nop());
DIFF_PROC(RawText_SetPaletteRange, RawText_SetPaletteRange((int16_t)a[0], (int16_t)a[1], (FarPtr)a[2]));

// A heap state a little different each time (the blocks land elsewhere).
static void setup_load(DiffRng *r, int32_t *a)
{
    (void)a;
    if (diff_chance(r, 50))
        System_GetMem((uint16_t)diff_range(r, 1, 0x4000));
}

// first..last of a palette anywhere in the scratch buffer, values of any
// byte (the DAC keeps 6 bits). The range stays within the buffer and the
// count positive: last < first sends ~64 KB from beyond it.
static void setup_palette_range(DiffRng *r, int32_t *a)
{
    int first = diff_pick(r, 0, 0, 0x40, 0xF1, diff_range(r, 0, 255));
    int last = diff_pick(r, 255, first, 0xFE, diff_range(r, first, 255));
    uint16_t ofs = (uint16_t)diff_range(r, 0, 0x100);
    uint8_t *p = far_host(DIFF_SCRATCH + ofs);
    for (int i = 0; i < 768; i++)
        p[i] = (uint8_t)(diff_chance(r, 80) ? diff_rand(r, 64) : diff_rand(r, 256));
    a[0] = first;
    a[1] = last;
    a[2] = (int32_t)(DIFF_SCRATCH + ofs);
    if (diff_chance(r, 15)) {  // past the end of the DAC: the index wraps to 0
        a[1] = diff_range(r, 256, 300);
        a[2] = (int32_t)DIFF_SCRATCH;
    }
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
    diff_run(&diff_RawText_LoadSzFace, &base, 1, setup_load, diff_cases(4));
    diff_run(&diff_RawText_LoadSzTalal, &base, 1, setup_load, diff_cases(4));
    diff_run(&diff_RawText_LoadSzFaj, &base, 1, setup_load, diff_cases(4));
    diff_run(&diff_RawText_Nop, &base, 1, nullptr, diff_cases(1));
    diff_run(&diff_RawText_SetPaletteRange, &base, 1, setup_palette_range, diff_cases(40));
    diff_free(base);
    return diff_finish();
}
