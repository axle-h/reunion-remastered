// Differential test of unit Vec3D (3e17): projection, normals and centroids,
// polygon fill and wireframe edges, original (under Unicorn) against the
// port, on random models drawn into the screen segment. See docs/difftest.md.
// Needs the game data.
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/vec3d.h"

DIFF_PROC(Vec3D_DrawEdges, Vec3D_DrawEdges((uint16_t)a[0], (FarPtr)a[1], (FarPtr)a[2],
                                           (int16_t)a[3], (uint8_t)a[4]));
DIFF_PROC(Vec3D_DrawFaces, Vec3D_DrawFaces((uint16_t)a[0], (FarPtr)a[1], (FarPtr)a[2],
                                           (FarPtr)a[3], (uint8_t)a[4]));
DIFF_PROC(Vec3D_PrepareObject,
          Vec3D_PrepareObject((int16_t)a[0], (int16_t)a[1], (FarPtr)a[2], (FarPtr)a[3],
                              (FarPtr)a[4], (uint8_t)a[5], (uint8_t)a[6]));
DIFF_PROC(Vec3D_FillPoly, Vec3D_FillPoly((uint16_t)a[0], (int16_t)a[1], (FarPtr)a[2], (uint8_t)a[3]));
DIFF_PROC(Vec3D_FillSpans,
          Vec3D_FillSpans((uint16_t)a[0], (int16_t)a[1], (int16_t)a[2], (int16_t)a[3]));
DIFF_PROC(Vec3D_TraceEdge,
          Vec3D_TraceEdge((int16_t)a[0], (int16_t)a[1], (int16_t)a[2], (int16_t)a[3], (FarPtr)a[4]));
DIFF_PROC(Vec3D_Project, Vec3D_Project((int16_t)a[0], (int16_t)a[1], (int16_t)a[2], (FarPtr)a[3],
                                       (FarPtr)a[4], (int16_t)a[5], (int16_t)a[6], (int16_t)a[7]));
DIFF_PROC(Vec3D_InitCosTable, Vec3D_InitCosTable());

// Scratch layout (DIFF_SCRATCH, 4 KB): vertices, faces/edges, normals, points.
enum : uint16_t { VERTS = 0x000, FACES = 0x300, NORMALS = 0x800, POINTS = 0xA00, TABLE = 0xB00 };
static FarPtr scratch(uint16_t off) { return (FarPtr)DIFF_SCRATCH + off; }
static uint8_t *scratch_host(uint16_t off) { return far_host(scratch(off)); }

static void put16(uint8_t *p, int16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)((uint16_t)v >> 8);
}

static int16_t *ds16(uint16_t off) { return (int16_t *)ds_at(off); }

// n random points of three words in [-lim, lim], with `wild` percent of any
// value.
static void random_points(DiffRng *r, uint16_t off, int n, int lim, int wild)
{
    for (int i = 0; i < n * 3; i++)
        put16(scratch_host((uint16_t)(off + i * 2)),
              (int16_t)(diff_chance(r, wild) ? diff_range(r, -32768, 32767) : diff_range(r, -lim, lim)));
}

// --- DrawEdges ------------------------------------------------------------------------

static void setup_edges(DiffRng *r, int32_t *a)
{
    int nv = diff_range(r, 2, 60);
    random_points(r, VERTS, nv, diff_pick(r, 30, 60, 60, 200), diff_pick(r, 0, 0, 0, 0, 1));
    int ne = diff_pick(r, 1, diff_range(r, 1, 40), diff_range(r, 40, 150), 0);
    for (int i = 0; i < (ne > 0 ? ne * 2 : 0); i++)
        *scratch_host((uint16_t)(FACES + i)) = (uint8_t)diff_range(r, 1, nv);
    a[0] = 0xA000;
    a[1] = (int32_t)scratch(VERTS);
    a[2] = (int32_t)scratch(FACES);
    a[3] = ne;
    a[4] = (int32_t)diff_rand(r, 256);
}

// --- DrawFaces --------------------------------------------------------------------------

// The two-sided objects and their skipped faces, or any model/slot.
static void random_model_slot(DiffRng *r)
{
    static const int16_t two_sided[][2] = {{7, 1}, {12, 7}, {18, 5}, {26, 7}, {26, 9}, {27, 4}};
    int k = diff_range(r, 0, 9);
    if (k < 6) {
        *ds16(VEC3D_MODEL) = two_sided[k][0];
        *ds16(VEC3D_SLOT) = two_sided[k][1];
    } else {
        *ds16(VEC3D_MODEL) = (int16_t)diff_range(r, 1, 35);
        *ds16(VEC3D_SLOT) = (int16_t)diff_range(r, 1, 17);
    }
}

// count faces of 3..19 vertex indices (1..nv), FFh-terminated, sometimes all
// 20 bytes without a terminator.
static void random_faces(DiffRng *r, int count, int nv)
{
    for (int f = 0; f < count; f++) {
        uint8_t *p = scratch_host((uint16_t)(FACES + f * 20));
        int n = diff_pick(r, 3, 3, 4, 4, diff_range(r, 3, 19), 20);
        for (int k = 0; k < 20; k++)
            p[k] = k < n ? (uint8_t)diff_range(r, 1, nv) : 0xFF;
    }
}

static void setup_faces(DiffRng *r, int32_t *a)
{
    random_model_slot(r);
    int nv = diff_range(r, 3, 40);
    // Projected points: in the buffer at (x + 86h, y + 43h). Wild ones would
    // trace edges into the stack segment (the edge tables are in DS).
    random_points(r, VERTS, nv, diff_pick(r, 20, 50, 60), 0);
    int count = diff_pick(r, 1, diff_range(r, 1, 10), diff_range(r, 1, 25), 0);
    random_faces(r, count, nv);
    for (int i = 0; i < count * 3; i++)
        put16(scratch_host((uint16_t)(NORMALS + i * 2)),
              (int16_t)diff_pick(r, 0, -1, diff_range(r, -60, 60), diff_range(r, -60, 60),
                                 diff_range(r, -400, 400)));
    a[0] = 0xA000;
    a[1] = (int32_t)scratch(VERTS);
    a[2] = (int32_t)scratch(FACES);
    a[3] = (int32_t)scratch(NORMALS);
    a[4] = count;
}

// --- PrepareObject ------------------------------------------------------------------------

static void setup_prepare(DiffRng *r, int32_t *a)
{
    int16_t model = (int16_t)diff_pick(r, 6, 9, 11, 12, 14, 15, 16, 19, 21, 23, 24, 25, 26, 32,
                                       diff_range(r, 1, 35));
    int16_t slot = (int16_t)diff_pick(r, diff_range(r, 1, 17), diff_range(r, 1, 14),
                                      diff_range(r, 1, 7));
    int nv = diff_pick(r, 0, 1, 3, diff_range(r, 3, 30), diff_range(r, 3, 30), diff_range(r, 3, 120));
    random_points(r, VERTS, nv, diff_pick(r, 5, 40, 120, 600), 3);
    if (model == 24 && diff_chance(r, 70)) {
        // Centroids at +-21h or +-43h, the hand-fixed ones.
        for (int j = 0; j < 2; j++) {
            int16_t c = (int16_t)(diff_pick(r, 0x21, 0x43, -0x21, -0x43, 5));
            for (int i = 0; i < nv; i++)
                put16(scratch_host((uint16_t)(VERTS + i * 6 + j * 2)), c);
        }
    }
    int nf = diff_pick(r, 0, 1, diff_range(r, 1, 30), diff_range(r, 30, 60));
    random_faces(r, nf, nv);
    a[0] = model;
    a[1] = slot;
    a[2] = (int32_t)scratch(VERTS);
    a[3] = (int32_t)scratch(FACES);
    a[4] = (int32_t)scratch(NORMALS);
    a[5] = nv;
    a[6] = nf;
}

// --- FillPoly / FillSpans / TraceEdge -------------------------------------------------------

static void setup_fillpoly(DiffRng *r, int32_t *a)
{
    int n = diff_pick(r, 2, 3, 3, 4, diff_range(r, 3, 19));
    int cx = diff_range(r, 0, 319), cy = diff_range(r, 0, 199), rad = diff_pick(r, 3, 30, 100);
    for (int i = 0; i < n; i++) {
        int x = cx + diff_range(r, -rad, rad), y = cy + diff_range(r, -rad, rad);
        x = x < 0 ? 0 : x > 319 ? 319 : x;
        y = y < 0 ? 0 : y > 199 ? 199 : y;
        if (i > 0 && diff_chance(r, 15))  // repeated y (horizontal edges, ties)
            y = scratch_host((uint16_t)(POINTS + i * 4 - 2))[0];
        put16(scratch_host((uint16_t)(POINTS + i * 4)), (int16_t)x);
        put16(scratch_host((uint16_t)(POINTS + i * 4 + 2)), (int16_t)y);
    }
    a[0] = 0xA000;
    a[1] = n;
    a[2] = (int32_t)scratch(POINTS);
    a[3] = (int32_t)diff_rand(r, 256);
}

static void setup_fillspans(DiffRng *r, int32_t *a)
{
    int16_t first = (int16_t)diff_range(r, 0, 199);
    int16_t last = (int16_t)diff_pick(r, first, diff_range(r, first, 199), 199);
    for (int y = 0; y < 200; y++) {
        *ds16((uint16_t)(VEC3D_EDGES_A + y * 2)) = (int16_t)diff_range(r, 0, 319);
        *ds16((uint16_t)(VEC3D_EDGES_B + y * 2)) =
            (int16_t)diff_pick(r, diff_range(r, 0, 319), *ds16((uint16_t)(VEC3D_EDGES_A + y * 2)));
    }
    a[0] = 0xA000;
    a[1] = first;
    a[2] = last;
    a[3] = diff_pick(r, (int32_t)diff_rand(r, 256), 0xFF, (int32_t)diff_rand(r, 0x10000));
}

static void setup_trace(DiffRng *r, int32_t *a)
{
    a[0] = diff_range(r, 0, 319);
    a[1] = diff_range(r, 0, 199);
    a[2] = diff_pick(r, a[0], diff_range(r, 0, 319), a[0] + diff_range(r, -3, 3));
    a[3] = diff_pick(r, a[1], diff_range(r, 0, 199), a[1] + diff_range(r, -3, 3));
    if (a[3] < 0 || a[3] > 199)
        a[3] = a[1];
    a[4] = (int32_t)diff_pick(r, (int32_t)scratch(TABLE), (int32_t)far_ptr(MEM_DS_SEG, VEC3D_EDGES_A),
                              (int32_t)far_ptr(MEM_DS_SEG, VEC3D_EDGES_B));
}

// --- Project / InitCosTable -----------------------------------------------------------------

static int32_t angle(DiffRng *r)
{
    return diff_pick(r, 0, 89, 90, 359, diff_range(r, 0, 359), diff_range(r, 0, 359),
                     diff_range(r, 360, 1079));
}

static void setup_project(DiffRng *r, int32_t *a)
{
    a[0] = angle(r);
    a[1] = angle(r);
    a[2] = angle(r);
    int count = diff_pick(r, 1, diff_range(r, 1, 30), diff_range(r, 1, 120));
    random_points(r, VERTS, count, 32767, 0);
    a[3] = (int32_t)scratch(VERTS);
    a[4] = (int32_t)diff_pick(r, (int32_t)scratch(NORMALS - 0x300), (int32_t)scratch(VERTS));
    a[5] = diff_pick(r, 0x40, 200, diff_range(r, 0, 255), diff_range(r, -32768, 32767));
    a[6] = count;
    a[7] = diff_pick(r, 1, 2, 0, 3);
}

static void setup_cos(DiffRng *r, int32_t *a)
{
    (void)a;
    for (int i = 0; i < 361 * 4; i++)
        *(uint8_t *)ds_at((uint16_t)(VEC3D_COS_TABLE + i)) = (uint8_t)diff_rand(r, 256);
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
    const struct {
        DiffFn *fn;
        DiffSetup setup;
        int cases;
    } fns[] = {
        {&diff_Vec3D_Project, setup_project, 60},
        {&diff_Vec3D_TraceEdge, setup_trace, 80},
        {&diff_Vec3D_FillSpans, setup_fillspans, 40},
        {&diff_Vec3D_FillPoly, setup_fillpoly, 80},
        {&diff_Vec3D_DrawEdges, setup_edges, 40},
        {&diff_Vec3D_DrawFaces, setup_faces, 150},
        {&diff_Vec3D_PrepareObject, setup_prepare, 600},
        {&diff_Vec3D_InitCosTable, setup_cos, 2},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, &base, 1, fns[i].setup, diff_cases(fns[i].cases));
    diff_free(base);
    return diff_finish();
}
