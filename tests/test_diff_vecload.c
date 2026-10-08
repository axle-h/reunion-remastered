// Differential test of unit VecLoad (3d38): loading VECTORS\Vn.VEC models,
// the rotating solid view and the wireframe hologram, original (under
// Unicorn) against the port, from the shop screen and from states with
// other models loaded, with random angles. See docs/difftest.md. Needs the
// game data.
//
// Not tested directly: ReadObject and ReadVecObject (nested in LoadVec and
// LoadWireVec, they read the parent's open file); they run inside those.
#include <stdio.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/system.h"
#include "game/vec3d.h"
#include "game/vecload.h"

DIFF_PROC(VecLoad_Init, VecLoad_Init());
DIFF_PROC(VecLoad_AllocVecBuffers, VecLoad_AllocVecBuffers());
DIFF_PROC(VecLoad_FreeVecBuffers, VecLoad_FreeVecBuffers());
DIFF_PROC(VecLoad_AllocWireBuffers, VecLoad_AllocWireBuffers());
DIFF_PROC(VecLoad_FreeWireBuffers, VecLoad_FreeWireBuffers());
DIFF_PROC(VecLoad_LoadVec, VecLoad_LoadVec((int16_t)a[0]));
DIFF_PROC(VecLoad_LoadWireVec, VecLoad_LoadWireVec((int16_t)a[0]));
DIFF_PROC(VecLoad_DrawObject, VecLoad_DrawObject((int16_t)a[0]));
DIFF_PROC(VecLoad_DrawFrame, VecLoad_DrawFrame());
DIFF_PROC(VecLoad_DrawBlueFrame, VecLoad_DrawBlueFrame());
DIFF_PROC(VecLoad_DrawWireObject, VecLoad_DrawWireObject((int16_t)a[0]));
DIFF_PROC(VecLoad_DrawWireScene, VecLoad_DrawWireScene());
DIFF_PROC(VecLoad_SetWirePalette, VecLoad_SetWirePalette());
// Nested in DrawFrame: the argument is DrawFrame's BP, the count its local
// at [BP-0Eh] (in the stack segment, above the harness's stack).
DIFF_PROC(VecLoad_SortByDepth,
          VecLoad_SortByDepth(*(int16_t *)(mem + (uint32_t)MEM_SS_SEG * 16 + (uint16_t)(a[0] - 0x0E))));

enum : uint16_t {
    ANGLES = 0x7E48,
    OBJECT_COUNT = 0x7E50,
    ZOOM = 0x7E52,
    WIRE_SCALE = 0x7E54,
    SPINS = 0x8578,
    BUFFER = 0x8584,
    PROJ_CENTROIDS = 0x85EE,
    ORDER = 0x8652,
    PARENT_BP = 0x1390,  // [BP-0Eh] = SS:1382, between the harness's SP (1380h) and the heap
};

static int16_t *w(uint16_t off) { return (int16_t *)ds_at(off); }

static void random_angles(DiffRng *r)
{
    for (int i = 0; i < 3; i++) {
        w(ANGLES)[i] = (int16_t)diff_pick(r, 0, 300, 1079, diff_range(r, 0, 1079), diff_range(r, 0, 1079));
        if (diff_chance(r, 30))
            w(SPINS)[i] = (int16_t)diff_pick(r, 0, 1, 5, 20, 1079, 1080);
    }
}

// --- the solid view ------------------------------------------------------------------

static void setup_frame(DiffRng *r, int32_t *a)
{
    (void)a;
    random_angles(r);
    *w(0x899A) = (int16_t)diff_pick(r, 1, 50, 200, *w(ZOOM), *w(ZOOM) - 1,
                                            diff_range(r, 0, 255));  // DS:899A vertex scale
}

static void setup_object(DiffRng *r, int32_t *a)
{
    random_angles(r);
    // DrawFrame keeps the scale at most the zoom; larger ones make points
    // off the buffer, and polygons that take minutes to fill.
    *w(0x899A) = (int16_t)diff_pick(r, *w(ZOOM), diff_range(r, 1, *w(ZOOM)));
    int n = *w(OBJECT_COUNT);
    a[0] = n > 0 ? diff_range(r, 1, n) : 1;
}

static void setup_sort(DiffRng *r, int32_t *a)
{
    int n = diff_pick(r, 0, 1, 2, diff_range(r, 1, 17), diff_range(r, 1, 17), 17);
    *(int16_t *)(mem + (uint32_t)MEM_SS_SEG * 16 + PARENT_BP - 0x0E) = (int16_t)n;
    for (int i = 1; i <= 17; i++) {
        *w((uint16_t)(ORDER + i * 2)) = (int16_t)i;
        *w((uint16_t)(PROJ_CENTROIDS + i * 6 - 2)) =
            (int16_t)diff_pick(r, -999, 0, 5, diff_range(r, -100, 100), diff_range(r, -2000, 2000));
    }
    for (int i = n; i > 1; i--) {  // shuffle the first n
        int j = diff_range(r, 1, i);
        int16_t t = *w((uint16_t)(ORDER + i * 2));
        *w((uint16_t)(ORDER + i * 2)) = *w((uint16_t)(ORDER + j * 2));
        *w((uint16_t)(ORDER + j * 2)) = t;
    }
    a[0] = PARENT_BP;
}

static void setup_loadvec(DiffRng *r, int32_t *a)
{
    if (diff_chance(r, 30))
        ds.Game_MinMemAvail = diff_range(r, 0, 0x7FFFFFFF);
    a[0] = diff_pick(r, diff_range(r, 1, 35), diff_range(r, 1, 35), diff_range(r, 1, 35), 14, 24, 0,
                     36);
}

// --- the wireframe ---------------------------------------------------------------------

static void setup_wire(DiffRng *r, int32_t *a)
{
    random_angles(r);
    *w(WIRE_SCALE) = (int16_t)diff_pick(r, 30, 30, diff_range(r, 0, 60), diff_range(r, 0, 255));
    int n = *w(OBJECT_COUNT);
    a[0] = n > 0 ? diff_range(r, 1, n) : 1;
}

static void setup_none(DiffRng *r, int32_t *a)
{
    (void)r;
    (void)a;
}

// Base states.
enum { SOLID_MODELS = 13, WIRE_MODELS = 4 };
// The ones with depth fixes in DrawFrame, two-sided objects or hand-fixed
// centroids.
static const int16_t solid_models[SOLID_MODELS] = {1, 7, 13, 14, 17, 21, 24, 29, 32, 12, 18, 27, 35};
static const int16_t wire_models[WIRE_MODELS] = {9, 1, 20, 33};

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    // Plain: nothing allocated.
    if (!diff_boot_new_game(1500)) {
        printf("FAILED to boot\n");
        return 1;
    }
    DiffState *plain = diff_save();

    // Solid: the shop screen (its own model), then other models in its buffers.
    if (!diff_boot("900:click:24,16;1300:quit")) {
        printf("FAILED to reach the shop\n");
        return 1;
    }
    DiffState *solid[1 + SOLID_MODELS];
    solid[0] = diff_save();
    for (int i = 0; i < SOLID_MODELS; i++) {
        diff_load(solid[0]);
        VecLoad_LoadVec(solid_models[i]);
        VecLoad_DrawFrame();
        solid[1 + i] = diff_save();
    }

    // Wireframe: like Command::ShowHologram.
    DiffState *wire[WIRE_MODELS];
    for (int i = 0; i < WIRE_MODELS; i++) {
        diff_load(plain);
        VecLoad_AllocWireBuffers();
        *(FarPtr *)ds_at(BUFFER) = System_GetMem(0x8980);
        VecLoad_LoadWireVec(wire_models[i]);
        VecLoad_SetWirePalette();
        wire[i] = diff_save();
    }

    diff_run(&diff_VecLoad_Init, &plain, 1, setup_none, diff_cases(1));
    diff_run(&diff_VecLoad_AllocVecBuffers, &plain, 1, setup_none, diff_cases(1));
    diff_run(&diff_VecLoad_AllocWireBuffers, &plain, 1, setup_none, diff_cases(1));
    diff_run(&diff_VecLoad_FreeVecBuffers, solid, 1 + SOLID_MODELS, setup_none, diff_cases(2));
    diff_run(&diff_VecLoad_FreeWireBuffers, wire, WIRE_MODELS, setup_none, diff_cases(2));
    diff_run(&diff_VecLoad_SetWirePalette, wire, WIRE_MODELS, setup_none, diff_cases(1));
    diff_run(&diff_VecLoad_LoadVec, solid, 1 + SOLID_MODELS, setup_loadvec, diff_cases(70));
    diff_run(&diff_VecLoad_LoadWireVec, wire, WIRE_MODELS, setup_loadvec, diff_cases(40));
    diff_VecLoad_DrawObject.near = true;
    diff_run(&diff_VecLoad_DrawObject, solid, 1 + SOLID_MODELS, setup_object, diff_cases(45));
    diff_VecLoad_SortByDepth.near = true;
    diff_run(&diff_VecLoad_SortByDepth, solid, 1 + SOLID_MODELS, setup_sort, diff_cases(30));
    diff_run(&diff_VecLoad_DrawFrame, solid, 1 + SOLID_MODELS, setup_frame, diff_cases(45));
    diff_run(&diff_VecLoad_DrawBlueFrame, solid, 1 + SOLID_MODELS, setup_frame, diff_cases(18));
    diff_VecLoad_DrawWireObject.near = true;
    diff_run(&diff_VecLoad_DrawWireObject, wire, WIRE_MODELS, setup_wire, diff_cases(24));
    diff_run(&diff_VecLoad_DrawWireScene, wire, WIRE_MODELS, setup_wire, diff_cases(24));

    diff_free(plain);
    for (int i = 0; i < 1 + SOLID_MODELS; i++)
        diff_free(solid[i]);
    for (int i = 0; i < WIRE_MODELS; i++)
        diff_free(wire[i]);
    return diff_finish();
}
