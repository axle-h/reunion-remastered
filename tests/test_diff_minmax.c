// Differential test of unit MinMax (484a): Max, Min and the two swaps, with
// random and boundary values. See docs/difftest.md. Needs the game data.
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/minmax.h"

DIFF_FUNC(MinMax_Max, MinMax_Max(a[0], a[1]));
DIFF_FUNC(MinMax_Min, MinMax_Min(a[0], a[1]));
DIFF_PROC(MinMax_SwapInt, MinMax_SwapInt(far_host((FarPtr)a[0]), far_host((FarPtr)a[1])));
DIFF_PROC(MinMax_SwapWord, MinMax_SwapWord(far_host((FarPtr)a[0]), far_host((FarPtr)a[1])));

static int32_t value(DiffRng *r)
{
    return diff_pick(r, 0, 1, -1, 0x7FFFFFFF, (int32_t)0x80000000, 0xFFFF, 0x10000, -0x10000,
                     (int32_t)diff_rand(r, 0xFFFFFFFFu), diff_range(r, -100, 100));
}

static void setup_minmax(DiffRng *r, int32_t *a)
{
    a[0] = value(r);
    a[1] = diff_chance(r, 15) ? a[0] : value(r);
}

// Two words somewhere in the scratch buffer or DS (possibly the same one).
static void setup_swap(DiffRng *r, int32_t *a)
{
    uint8_t *s = far_host(DIFF_SCRATCH);
    for (int i = 0; i < 64; i++)
        s[i] = (uint8_t)diff_rand(r, 256);
    a[0] = (int32_t)(DIFF_SCRATCH + diff_range(r, 0, 30) * 2);
    a[1] = diff_chance(r, 10) ? a[0] : (int32_t)(DIFF_SCRATCH + diff_range(r, 0, 61));
    if (diff_chance(r, 20))
        a[1] = (int32_t)far_ptr(MEM_DS_SEG, offsetof(DSeg, Game_Year));
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
    diff_run(&diff_MinMax_Max, &base, 1, setup_minmax, diff_cases(40));
    diff_run(&diff_MinMax_Min, &base, 1, setup_minmax, diff_cases(40));
    diff_run(&diff_MinMax_SwapInt, &base, 1, setup_swap, diff_cases(20));
    diff_run(&diff_MinMax_SwapWord, &base, 1, setup_swap, diff_cases(20));
    diff_free(base);
    return diff_finish();
}
