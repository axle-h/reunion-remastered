#include "game/vecload.h"

#include <SDL3/SDL.h>

#include "game/blit.h"
#include "game/dseg.h"
#include "game/minmax.h"
#include "game/rawtext.h"
#include "game/system.h"
#include "game/vec3d.h"
#include "platform/fs.h"

enum : uint16_t {
    ANGLE_A = 0x7E48,
    ANGLE_B = 0x7E4A,
    ANGLE_C = 0x7E4C,
    OBJECT_COUNT = 0x7E50,
    ZOOM = 0x7E52,
    WIRE_SCALE = 0x7E54,
    PROJECTED = 0x7E56,     // far pointer
    NORMALS = 0x7E5A,       // far pointers [0..17]
    VERTS = 0x7E9E,         // far pointers, + 4 * slot
    VERT_COUNT = 0x7EE4,    // words, + 2 * slot
    EDGES = 0x7F04,
    EDGE_COUNT = 0x7F4A,
    FACES = 0x7F6A,
    FACE_COUNT = 0x7FB0,
    SPIN_A = 0x8578,
    SPIN_B = 0x857A,
    SPIN_C = 0x857C,
    BUFFER = 0x8584,        // far pointer to the off-screen buffer
    PROJ_CENTROIDS = 0x85EE,
    ORDER = 0x8652,         // words, + 2 * index
    SCALE = 0x899A,

    ZOOM_TABLE = 0x56CD,    // byte per model
    MODE_TABLE = 0x56F1,    // byte per model: 2 = swap the first two matrix rows
    SPIN_TABLE = 0x5713,    // 3 bytes per model

    WINDOW = 0x0CCC,        // the solid view's window in the buffer (x 76, y 10)
    WINDOW_SCREEN = 0x44C6, // where it goes on screen (x 6, y 55)
};

static inline uint16_t *w(uint16_t off)
{
    return (uint16_t *)ds_at(off);
}

static inline FarPtr *fp(uint16_t off)
{
    return (FarPtr *)ds_at(off);
}

static inline uint16_t buffer_seg(void)
{
    return (uint16_t)(*fp(BUFFER) >> 16);
}

static inline uint8_t model_byte(uint16_t table, int16_t n)
{
    return *(uint8_t *)ds_at((uint16_t)(table + n));
}

// Game_MinMemAvail := min(Game_MinMemAvail, MemAvail).
static void track_min_mem(void)
{
    if (System_MemAvail() < ds.Game_MinMemAvail)
        ds.Game_MinMemAvail = System_MemAvail();
}

// Angle units (1/3 degree) div 3, unsigned like the original.
static inline int16_t deg(uint16_t off)
{
    return (int16_t)(*w(off) / 3);
}

// Adds the spin and wraps at 1080 (unsigned compare).
static void advance_angles(void)
{
    static const uint16_t angle[3] = {ANGLE_A, ANGLE_B, ANGLE_C};
    static const uint16_t spin[3] = {SPIN_A, SPIN_B, SPIN_C};
    for (int i = 0; i < 3; i++) {
        *w(angle[i]) += *w(spin[i]);
        if (*w(angle[i]) >= 0x438)
            *w(angle[i]) -= 0x438;
    }
}

// --- the open model file -------------------------------------------------------
// LoadVec's local file variable, as a whole file in host memory. BlockRead
// without a result variable fails with error 100 on a short read; the
// original's CheckIO after every read turns that into a run-time error.

static struct {
    uint8_t *data;
    size_t size, pos;
} vec_file;

// Assign + Reset(f, 1) + IOResult: false if the file can't be opened.
static bool file_open(int16_t n)
{
    PString num, path;
    System_StrLong(n, 0, &num, 255);
    System_StrLoad(&path, PSTR("vectors\\v"));
    System_StrConcat(&path, &num);
    System_StrConcat(&path, PSTR(".vec"));
    char name[256];
    SDL_memcpy(name, path.s, path.len);
    name[path.len] = '\0';
    SDL_free(vec_file.data);
    vec_file.pos = 0;
    vec_file.data = fs_load(name, &vec_file.size);
    return vec_file.data != nullptr;
}

static void file_close(void)
{
    SDL_free(vec_file.data);
    vec_file.data = nullptr;
}

static void block_read(FarPtr dst, uint16_t count)
{
    if (vec_file.pos + count > vec_file.size)
        System_RunError(100);
    for (uint16_t i = 0; i < count; i++)
        *(uint8_t *)far_host(far_ptr((uint16_t)(dst >> 16), (uint16_t)((uint16_t)dst + i))) =
            vec_file.data[vec_file.pos + i];
    vec_file.pos += count;
}

static void block_read_host(uint8_t *dst, uint16_t count)
{
    if (vec_file.pos + count > vec_file.size)
        System_RunError(100);
    SDL_memcpy(dst, vec_file.data + vec_file.pos, count);
    vec_file.pos += count;
}

static inline FarPtr ds_ptr(uint16_t off)
{
    return far_ptr(MEM_DS_SEG, off);
}

// Shared body of ReadObject and ReadVecObject: the faces go to faces_dst.
static bool read_object(int16_t slot, FarPtr faces_dst)
{
    uint8_t word[2];
    block_read_host(word, 2);
    uint16_t size = (uint16_t)(word[0] | word[1] << 8);
    if (size == 0xFFFF)
        return false;
    uint16_t s2 = (uint16_t)(slot * 2), s4 = (uint16_t)(slot * 4);
    block_read(ds_ptr((uint16_t)(VERT_COUNT + s2)), 2);
    block_read(*fp((uint16_t)(VERTS + s4)), (uint16_t)(*w((uint16_t)(VERT_COUNT + s2)) * 6));
    block_read(ds_ptr((uint16_t)(EDGE_COUNT + s2)), 2);
    block_read(*fp((uint16_t)(EDGES + s4)), (uint16_t)(*w((uint16_t)(EDGE_COUNT + s2)) * 2));
    block_read(ds_ptr((uint16_t)(FACE_COUNT + s2)), 2);
    block_read(faces_dst, (uint16_t)(*w((uint16_t)(FACE_COUNT + s2)) * 0x14));

    // Coordinates div 10.
    int16_t n = (int16_t)*w((uint16_t)(VERT_COUNT + s2));
    FarPtr verts = *fp((uint16_t)(VERTS + s4));
    for (int16_t i = 1; i <= n; i++) {
        for (int j = 0; j < 3; j++) {
            uint8_t *p = far_host(far_ptr((uint16_t)(verts >> 16),
                                          (uint16_t)((uint16_t)verts + i * 6 - 6 + j * 2)));
            int16_t v = (int16_t)(p[0] | p[1] << 8);
            v = (int16_t)(v / 10);
            p[0] = (uint8_t)v;
            p[1] = (uint8_t)((uint16_t)v >> 8);
        }
    }
    return true;
}

// --- solid view ----------------------------------------------------------------

// 3d38:0000
void VecLoad_DrawObject(int16_t slot)
{
    *w(VEC3D_SLOT) = (uint16_t)slot;
    uint16_t s2 = (uint16_t)(slot * 2), s4 = (uint16_t)(slot * 4);
    int16_t mode = model_byte(MODE_TABLE, (int16_t)*w(VEC3D_MODEL));
    Vec3D_Project(deg(ANGLE_A), deg(ANGLE_B), deg(ANGLE_C), *fp((uint16_t)(VERTS + s4)),
                  *fp(PROJECTED), (int16_t)*w(SCALE), (int16_t)*w((uint16_t)(VERT_COUNT + s2)),
                  mode);
    Vec3D_Project(deg(ANGLE_A), deg(ANGLE_B), deg(ANGLE_C), *fp((uint16_t)(NORMALS + s4)),
                  *fp(NORMALS), 0x40, (int16_t)*w((uint16_t)(FACE_COUNT + s2)), mode);
    Vec3D_DrawFaces(buffer_seg(), *fp(PROJECTED), *fp((uint16_t)(FACES + s4)), *fp(NORMALS),
                    (uint8_t)*w((uint16_t)(FACE_COUNT + s2)));
}

static inline int16_t *depth(int16_t obj)
{
    return (int16_t *)ds_at((uint16_t)(PROJ_CENTROIDS + obj * 6 - 6 + 4));
}

static inline int16_t *order(int16_t i)
{
    return (int16_t *)ds_at((uint16_t)(ORDER + i * 2));
}

// 3d38:00e5
void VecLoad_SortByDepth(int16_t count)
{
    int16_t last = (int16_t)(count - 1);
    if (last < 1)
        return;
    for (int16_t i = 1;; i++) {
        int16_t best = i;
        int16_t key = *depth(*order(i));
        if (i + 1 <= count)
            for (int16_t j = (int16_t)(i + 1);; j++) {
                if (key < *depth(*order(j))) {
                    best = j;
                    key = *depth(*order(j));
                }
                if (j == count)
                    break;
            }
        MinMax_SwapInt(order(i), order(best));
        if (i == last)
            break;
    }
}

// 3d38:01aa
void VecLoad_DrawFrame(void)
{
    *w(SCALE) += 1;
    if ((int16_t)*w(SCALE) > (int16_t)*w(ZOOM))
        *w(SCALE) = *w(ZOOM);
    Blit_CopyRect(far_ptr(buffer_seg(), WINDOW), far_ptr(ds.Gfx_ScreenSeg, WINDOW_SCREEN), 0x72,
                  0x73);
    Blit_FillRectAt(far_ptr(buffer_seg(), WINDOW), 0x72, 0x73, 0x40);
    for (int16_t i = 1; i <= 17; i++)
        for (int j = 0; j < 3; j++)
            *(int16_t *)ds_at((uint16_t)(0x85E8 + i * 6 + j * 2)) = -999;

    int16_t model = (int16_t)*w(VEC3D_MODEL);
    Vec3D_Project(deg(ANGLE_A), deg(ANGLE_B), deg(ANGLE_C), ds_ptr(VEC3D_CENTROIDS + 6),
                  ds_ptr(PROJ_CENTROIDS), 0x40, (int16_t)*w(OBJECT_COUNT),
                  model_byte(MODE_TABLE, model));
    // Depth fixes: push objects to the back (+) or front (-).
    if (model == 1)
        *depth(1) += 1000;
    if (model == 13) {
        *depth(2) += 100;
        *depth(4) += 100;
        *depth(9) += 100;
        *depth(10) += 100;
        *depth(11) += 100;
        *depth(12) += 100;
    }
    if (model == 17) {
        *depth(1) += 1000;
        *depth(3) -= 1000;
    }
    if (model == 21)
        *depth(3) += 1;
    if (model == 27)
        *depth(2) -= 2;
    if (model == 29)
        *depth(1) += 1000;
    if (model == 32) {
        *depth(3) += 6;
        *depth(5) -= 4;
        *depth(7) -= 3;
        *depth(8) += 1000;
        *depth(9) += 1000;
        *depth(10) += 1000;
        *depth(11) += 1000;
        MinMax_SwapInt(depth(6), depth(7));
    }

    int16_t count = (int16_t)*w(OBJECT_COUNT);
    if (count >= 1)
        for (int16_t i = 1;; i++) {
            *order(i) = i;
            if (i == count)
                break;
        }
    if (model == 35) {
        VecLoad_DrawObject(1);
    } else {
        VecLoad_SortByDepth(count);
        int16_t n = (int16_t)*w(OBJECT_COUNT);
        if (n >= 1)
            for (int16_t i = 1;; i++) {
                VecLoad_DrawObject(*order(i));
                if (i == n)
                    break;
            }
    }
    advance_angles();
}

// 3d38:03b3
void VecLoad_DrawBlueFrame(void)
{
    for (int16_t i = 0; i <= 0x2F; i++) {
        ds.Gfx_Palette[0xC0 + i][0] = 0;
        ds.Gfx_Palette[0xC0 + i][1] = 0;
        ds.Gfx_Palette[0xC0 + i][2] = (uint8_t)((i * 40) / 48 + 10);
    }
    VecLoad_DrawFrame();
    Blit_CopyRect(far_ptr(buffer_seg(), WINDOW), far_ptr(ds.Gfx_ScreenSeg, WINDOW_SCREEN), 0x72,
                  0x73);
}

// --- wireframe -------------------------------------------------------------------

// 3d38:0413
void VecLoad_DrawWireObject(int16_t idx)
{
    uint16_t s2 = (uint16_t)(idx * 2), s4 = (uint16_t)(idx * 4);
    Vec3D_Project(deg(ANGLE_A), deg(ANGLE_B), deg(ANGLE_C), *fp((uint16_t)(VERTS + s4)),
                  *fp(PROJECTED), (int16_t)*w(WIRE_SCALE), (int16_t)*w((uint16_t)(VERT_COUNT + s2)),
                  1);
    Vec3D_DrawEdges(buffer_seg(), *fp(PROJECTED), *fp((uint16_t)(EDGES + s4)),
                    (int16_t)*w((uint16_t)(EDGE_COUNT + s2)), 0x41);
}

// 3d38:0490
void VecLoad_DrawWireScene(void)
{
    int16_t n = (int16_t)*w(OBJECT_COUNT);
    if (n >= 1)
        for (int16_t i = 1;; i++) {
            VecLoad_DrawWireObject(i);
            if (i == n)
                break;
        }
    advance_angles();
}

// 3d38:04f9
void VecLoad_SetWirePalette(void)
{
    ds.Gfx_Palette[0x41][0] = 0;
    ds.Gfx_Palette[0x41][1] = 0x38;
    ds.Gfx_Palette[0x41][2] = 0;
    RawText_SetPaletteRange(0x40, 0x41, ds_ptr(offsetof(DSeg, Gfx_Palette)));
}

// --- buffers ---------------------------------------------------------------------

// Projected points, projected normals, normals[1..17], vertices[1..17] and
// edges[1..17], in this order (the allocation order decides where V34/V35's
// oversized object overflows to).
static void alloc_common(void)
{
    *fp(PROJECTED) = System_GetMem(0x4B0);
    *fp(NORMALS) = System_GetMem(0x4B0);
    for (int i = 1; i <= 17; i++)
        *fp((uint16_t)(NORMALS + i * 4)) = System_GetMem(0x4B0);
    for (int i = 1; i <= 17; i++)
        *fp((uint16_t)(VERTS + i * 4)) = System_GetMem(0x4B0);
    for (int i = 1; i <= 17; i++)
        *fp((uint16_t)(EDGES + i * 4)) = System_GetMem(600);
}

static void free_common(void)
{
    System_FreeMem(*fp(PROJECTED), 0x4B0);
    System_FreeMem(*fp(NORMALS), 0x4B0);
    for (int i = 1; i <= 17; i++)
        System_FreeMem(*fp((uint16_t)(NORMALS + i * 4)), 0x4B0);
    for (int i = 1; i <= 17; i++)
        System_FreeMem(*fp((uint16_t)(VERTS + i * 4)), 0x4B0);
    for (int i = 1; i <= 17; i++)
        System_FreeMem(*fp((uint16_t)(EDGES + i * 4)), 600);
}

// 3d38:051c
void VecLoad_AllocVecBuffers(void)
{
    alloc_common();
    for (int i = 1; i <= 17; i++)
        *fp((uint16_t)(FACES + i * 4)) = System_GetMem(0x960);
}

// 3d38:05d8
void VecLoad_FreeVecBuffers(void)
{
    free_common();
    for (int i = 1; i <= 17; i++)
        System_FreeMem(*fp((uint16_t)(FACES + i * 4)), 0x960);
}

// 3d38:0a00
void VecLoad_AllocWireBuffers(void)
{
    alloc_common();
}

// 3d38:0a96
void VecLoad_FreeWireBuffers(void)
{
    free_common();
}

// --- loading ---------------------------------------------------------------------

// 3d38:0696
bool VecLoad_ReadObject(int16_t model, int16_t slot)
{
    *w(VEC3D_MODEL) = (uint16_t)model;
    return read_object(slot, *fp((uint16_t)(FACES + slot * 4)));
}

// 3d38:084a
void VecLoad_LoadVec(int16_t n)
{
    Blit_FillRectAt(far_ptr(buffer_seg(), WINDOW), 0x72, 0x73, 0x40);
    *w(ZOOM) = model_byte(ZOOM_TABLE, n);
    *w(OBJECT_COUNT) = 0;
    if (!file_open(n))
        return;
    while (VecLoad_ReadObject(n, (int16_t)(*w(OBJECT_COUNT) + 1)))
        *w(OBJECT_COUNT) += 1;
    file_close();

    int16_t count = (int16_t)*w(OBJECT_COUNT);
    if (count > 0)
        for (int16_t i = 1;; i++) {
            uint16_t s2 = (uint16_t)(i * 2), s4 = (uint16_t)(i * 4);
            Vec3D_PrepareObject(n, i, *fp((uint16_t)(VERTS + s4)), *fp((uint16_t)(FACES + s4)),
                                *fp((uint16_t)(NORMALS + s4)),
                                (uint8_t)*w((uint16_t)(VERT_COUNT + s2)),
                                (uint8_t)*w((uint16_t)(FACE_COUNT + s2)));
            if (i == count)
                break;
        }
    *w(SPIN_A) = model_byte(SPIN_TABLE, (int16_t)(n * 3));
    *w(SPIN_B) = model_byte(SPIN_TABLE + 1, (int16_t)(n * 3));
    *w(SPIN_C) = model_byte(SPIN_TABLE + 2, (int16_t)(n * 3));
    *w(ANGLE_A) = 0;
    *w(ANGLE_B) = 0;
    *w(ANGLE_C) = 300;
    if (n == 14) {
        *w(ANGLE_A) = 215;
        *w(ANGLE_B) = 45;
        *w(ANGLE_C) = 54;
    }
    if (n == 24) {
        *w(ANGLE_B) = 21;
        *w(ANGLE_C) = 32;
    }
    *w(SCALE) = 200;
    track_min_mem();
}

// 3d38:0b2e
bool VecLoad_ReadVecObject(int16_t n, int16_t slot)
{
    (void)n;
    return read_object(slot, *fp(BUFFER));
}

// 3d38:0cd6
void VecLoad_LoadWireVec(int16_t n)
{
    *w(WIRE_SCALE) = 30;
    *w(OBJECT_COUNT) = 0;
    if (!file_open(n))
        return;
    while (VecLoad_ReadVecObject(n, (int16_t)(*w(OBJECT_COUNT) + 1)))
        *w(OBJECT_COUNT) += 1;
    file_close();
    *w(SPIN_A) = 5;
    *w(SPIN_B) = 0;
    *w(SPIN_C) = 0;
    *w(ANGLE_A) = 0;
    *w(ANGLE_B) = 0;
    *w(ANGLE_C) = 300;
    track_min_mem();
    Blit_FillRectAt(far_ptr(buffer_seg(), 0x0010), 0x6C, 0x6A, 0x40);
}

// 3d38:0dd7
void VecLoad_Init(void)
{
    Vec3D_InitCosTable();
}
