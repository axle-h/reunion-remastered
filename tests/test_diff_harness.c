// The differential test harness itself (tests/diff, docs/difftest.md): the
// calling conventions it supports, on small routines of known behaviour.
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/groups.h"
#include "game/minmax.h"
#include "game/planetinfo.h"
#include "game/pub.h"

// longint arguments and result.
DIFF_FUNC(MinMax_Min, MinMax_Min(a[0], a[1]));
DIFF_FUNC(MinMax_Max, MinMax_Max(a[0], a[1]));
// var parameters (far pointers into mem[]).
DIFF_PROC(MinMax_SwapInt, MinMax_SwapInt(far_host((FarPtr)a[0]), far_host((FarPtr)a[1])));
// integer result.
DIFF_FUNC(Groups_FindPlanetForce,
          Groups_FindPlanetForce((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
// A near routine ("near;" in its symbols.txt comment).
DIFF_FUNC(Pub_DateToHours, Pub_DateToHours(a[0], a[1], a[2], a[3]));
// A string result: the hidden first argument points to the caller's buffer.
DIFF_PROC(PlanetInfo_GetBodyName, PlanetInfo_GetBodyName(far_host((FarPtr)a[0]), (int16_t)a[1],
                                                         (int16_t)a[2], (int16_t)a[3]));

static void setup_longs(DiffRng *r, int32_t *a)
{
    a[0] = (int32_t)diff_rand(r, 0xFFFFFFFFu);
    a[1] = diff_chance(r, 20) ? a[0] : (int32_t)diff_rand(r, 0xFFFFFFFFu);
}

static void setup_swap(DiffRng *r, int32_t *a)
{
    int16_t *w = far_host(DIFF_SCRATCH);
    w[0] = (int16_t)diff_rand(r, 0x10000);
    w[1] = (int16_t)diff_rand(r, 0x10000);
    a[0] = (int32_t)DIFF_SCRATCH;
    a[1] = (int32_t)(DIFF_SCRATCH + 2);
}

static void setup_location(DiffRng *r, int32_t *a)
{
    a[0] = diff_range(r, 1, 8);
    a[1] = diff_range(r, 0, ds.Galaxy_PlanetCount[a[0] - 1]);
    a[2] = diff_pick(r, 0, 0, 1, 2);
}

static void setup_date(DiffRng *r, int32_t *a)
{
    a[0] = diff_range(r, 2927, 2990);
    a[1] = diff_range(r, 1, 12);
    a[2] = diff_range(r, 1, 30);
    a[3] = diff_range(r, 0, 23);
}

static void setup_name(DiffRng *r, int32_t *a)
{
    a[0] = (int32_t)DIFF_SCRATCH;
    a[1] = 1;  // the Sun's planets and moons
    a[2] = diff_range(r, 1, ds.Galaxy_PlanetCount[0]);
    a[3] = diff_pick(r, 0, 0, 1);
    memset(far_host(DIFF_SCRATCH), 0xCC, 256);
}

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    if (!diff_boot_new_game(1500))
        return 1;
    DiffState *base = diff_save();
    diff_run(&diff_MinMax_Min, &base, 1, setup_longs, diff_cases(20));
    diff_run(&diff_MinMax_Max, &base, 1, setup_longs, diff_cases(20));
    diff_run(&diff_MinMax_SwapInt, &base, 1, setup_swap, diff_cases(5));
    diff_run(&diff_Groups_FindPlanetForce, &base, 1, setup_location, diff_cases(20));
    diff_run(&diff_Pub_DateToHours, &base, 1, setup_date, diff_cases(20));
    diff_run(&diff_PlanetInfo_GetBodyName, &base, 1, setup_name, diff_cases(10));
    diff_free(base);
    return diff_finish();
}
