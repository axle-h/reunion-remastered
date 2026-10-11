#include "game/vecload.h"

#include <SDL3/SDL.h>

#include "game/blit.h"
#include "game/dseg.h"
#include "game/gfx.h"
#include "game/rawtext.h"
#include "game/system.h"
#include "game/vec3d.h"
#include "platform/fs.h"

enum : uint16_t {
    WINDOW = 0x0CCC,        // the solid view's window in the buffer (x 76, y 10)
    WINDOW_SCREEN = 0x44C6, // where it goes on screen (x 6, y 55)
};

// The off-screen buffer, addressed by its segment (a whole segment in the
// port: System_GetSegment).
static inline uint8_t *buffer_base(void)
{
    return ds.VecLoad_Buffer;
}

// Game_MinMemAvail := min(Game_MinMemAvail, MemAvail).
static void track_min_mem(void)
{
    if (System_MemAvail() < ds.Game_MinMemAvail)
        ds.Game_MinMemAvail = System_MemAvail();
}

// Angle i (1/3 degrees) div 3, unsigned like the original.
static inline int16_t deg(int i)
{
    return (int16_t)(ds.VecLoad_Angles[i] / 3);
}

// Adds the spin and wraps at 1080 (unsigned compare).
static void advance_angles(void)
{
    for (int i = 0; i < 3; i++) {
        ds.VecLoad_Angles[i] += ds.VecLoad_Spin[i];
        if (ds.VecLoad_Angles[i] >= 0x438)
            ds.VecLoad_Angles[i] -= 0x438;
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

static void block_read(uint8_t *dst, uint16_t count)
{
    if (vec_file.pos + count > vec_file.size)
        System_RunError(100);
    SDL_memcpy(dst, vec_file.data + vec_file.pos, count);
    vec_file.pos += count;
}

// Shared body of ReadObject and ReadVecObject: the faces go to faces_dst.
static bool read_object(int16_t slot, uint8_t *faces_dst)
{
    uint8_t word[2];
    block_read(word, 2);
    uint16_t size = (uint16_t)(word[0] | word[1] << 8);
    if (size == 0xFFFF)
        return false;
    int16_t s = (int16_t)(slot - 1);
    block_read((uint8_t *)&ds.VecLoad_VertCount[s], 2);
    block_read(ds.VecLoad_Verts[s], (uint16_t)(ds.VecLoad_VertCount[s] * 6));
    block_read((uint8_t *)&ds.VecLoad_EdgeCount[s], 2);
    block_read(ds.VecLoad_Edges[s], (uint16_t)(ds.VecLoad_EdgeCount[s] * 2));
    block_read((uint8_t *)&ds.VecLoad_FaceCount[s], 2);
    block_read(faces_dst, (uint16_t)(ds.VecLoad_FaceCount[s] * 0x14));

    // Coordinates div 10.
    int16_t n = ds.VecLoad_VertCount[s];
    uint8_t *verts = ds.VecLoad_Verts[s];
    for (int16_t i = 1; i <= n; i++) {
        for (int j = 0; j < 3; j++) {
            uint8_t *p = verts + i * 6 - 6 + j * 2;
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
    ds.Vec3D_Slot = slot;
    int16_t s = (int16_t)(slot - 1);
    int16_t mode = ds.VecLoad_ModelMode[ds.Vec3D_Model - 1];
    Vec3D_Project(deg(0), deg(1), deg(2), ds.VecLoad_Verts[s], ds.VecLoad_Projected,
                  ds.VecLoad_Scale, ds.VecLoad_VertCount[s], mode);
    Vec3D_Project(deg(0), deg(1), deg(2), ds.VecLoad_Normals[slot], ds.VecLoad_Normals[0], 0x40,
                  ds.VecLoad_FaceCount[s], mode);
    Vec3D_DrawFaces(buffer_base(), ds.VecLoad_Projected, ds.VecLoad_Faces[s],
                    ds.VecLoad_Normals[0], (uint8_t)ds.VecLoad_FaceCount[s]);
}

// The depth (projected z) of object obj.
static inline int16_t depth(int16_t obj)
{
    return ds.VecLoad_ProjCentroids[obj - 1].Z;
}

static inline int16_t order(int16_t i)
{
    return ds.VecLoad_Order[i - 1];
}

// Depth fix: adds d to object obj's depth (16-bit).
static inline void push(int16_t obj, int16_t d)
{
    TVecPoint *p = &ds.VecLoad_ProjCentroids[obj - 1];
    p->Z = (int16_t)(p->Z + d);
}

// 3d38:00e5
void VecLoad_SortByDepth(int16_t count)
{
    int16_t last = (int16_t)(count - 1);
    if (last < 1)
        return;
    for (int16_t i = 1;; i++) {
        int16_t best = i;
        int16_t key = depth(order(i));
        if (i + 1 <= count)
            for (int16_t j = (int16_t)(i + 1);; j++) {
                if (key < depth(order(j))) {
                    best = j;
                    key = depth(order(j));
                }
                if (j == count)
                    break;
            }
        // MinMax_SwapInt(Order[i], Order[best])
        int16_t t = ds.VecLoad_Order[i - 1];
        ds.VecLoad_Order[i - 1] = ds.VecLoad_Order[best - 1];
        ds.VecLoad_Order[best - 1] = t;
        if (i == last)
            break;
    }
}

// 3d38:01aa
void VecLoad_DrawFrame(void)
{
    ds.VecLoad_Scale = (int16_t)(ds.VecLoad_Scale + 1);
    if (ds.VecLoad_Scale > ds.VecLoad_Zoom)
        ds.VecLoad_Scale = ds.VecLoad_Zoom;
    Blit_CopyRect(buffer_base() + WINDOW, gfx_screen() + WINDOW_SCREEN, 0x72, 0x73);
    Blit_FillRectAt(buffer_base() + WINDOW, 0x72, 0x73, 0x40);
    for (int16_t i = 1; i <= 17; i++) {
        TVecPoint *p = &ds.VecLoad_ProjCentroids[i - 1];
        p->X = -999;
        p->Y = -999;
        p->Z = -999;
    }

    int16_t model = ds.Vec3D_Model;
    Vec3D_Project(deg(0), deg(1), deg(2), (uint8_t *)&ds.Vec3D_Centroids,
                  (uint8_t *)&ds.VecLoad_ProjCentroids, 0x40, ds.VecLoad_ObjectCount,
                  ds.VecLoad_ModelMode[model - 1]);
    // Depth fixes: push objects to the back (+) or front (-).
    if (model == 1)
        push(1, 1000);
    if (model == 13) {
        push(2, 100);
        push(4, 100);
        push(9, 100);
        push(10, 100);
        push(11, 100);
        push(12, 100);
    }
    if (model == 17) {
        push(1, 1000);
        push(3, -1000);
    }
    if (model == 21)
        push(3, 1);
    if (model == 27)
        push(2, -2);
    if (model == 29)
        push(1, 1000);
    if (model == 32) {
        push(3, 6);
        push(5, -4);
        push(7, -3);
        push(8, 1000);
        push(9, 1000);
        push(10, 1000);
        push(11, 1000);
        // MinMax_SwapInt(depth 6, depth 7)
        int16_t t = depth(6);
        ds.VecLoad_ProjCentroids[5].Z = depth(7);
        ds.VecLoad_ProjCentroids[6].Z = t;
    }

    int16_t count = ds.VecLoad_ObjectCount;
    if (count >= 1)
        for (int16_t i = 1;; i++) {
            ds.VecLoad_Order[i - 1] = i;
            if (i == count)
                break;
        }
    if (model == 35) {
        VecLoad_DrawObject(1);
    } else {
        VecLoad_SortByDepth(count);
        int16_t n = ds.VecLoad_ObjectCount;
        if (n >= 1)
            for (int16_t i = 1;; i++) {
                VecLoad_DrawObject(order(i));
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
    Blit_CopyRect(buffer_base() + WINDOW, gfx_screen() + WINDOW_SCREEN, 0x72, 0x73);
}

// --- wireframe -------------------------------------------------------------------

// 3d38:0413
void VecLoad_DrawWireObject(int16_t idx)
{
    int16_t s = (int16_t)(idx - 1);
    Vec3D_Project(deg(0), deg(1), deg(2), ds.VecLoad_Verts[s], ds.VecLoad_Projected,
                  ds.VecLoad_WireScale, ds.VecLoad_VertCount[s], 1);
    Vec3D_DrawEdges(buffer_base(), ds.VecLoad_Projected, ds.VecLoad_Edges[s],
                    ds.VecLoad_EdgeCount[s], 0x41);
}

// 3d38:0490
void VecLoad_DrawWireScene(void)
{
    int16_t n = ds.VecLoad_ObjectCount;
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
    RawText_SetPaletteRange(0x40, 0x41, Gfx_PalettePtr());
}

// --- buffers ---------------------------------------------------------------------

// Projected points, projected normals, normals[1..17], vertices[1..17] and
// edges[1..17], in this order (the allocation order decides where V34/V35's
// oversized object overflows to).
static void alloc_common(void)
{
    ds.VecLoad_Projected = System_GetMem(0x4B0);
    ds.VecLoad_Normals[0] = System_GetMem(0x4B0);
    for (int i = 1; i <= 17; i++)
        ds.VecLoad_Normals[i] = System_GetMem(0x4B0);
    for (int i = 1; i <= 17; i++)
        ds.VecLoad_Verts[i - 1] = System_GetMem(0x4B0);
    for (int i = 1; i <= 17; i++)
        ds.VecLoad_Edges[i - 1] = System_GetMem(600);
}

static void free_common(void)
{
    System_FreeMem(ds.VecLoad_Projected, 0x4B0);
    System_FreeMem(ds.VecLoad_Normals[0], 0x4B0);
    for (int i = 1; i <= 17; i++)
        System_FreeMem(ds.VecLoad_Normals[i], 0x4B0);
    for (int i = 1; i <= 17; i++)
        System_FreeMem(ds.VecLoad_Verts[i - 1], 0x4B0);
    for (int i = 1; i <= 17; i++)
        System_FreeMem(ds.VecLoad_Edges[i - 1], 600);
}

// 3d38:051c
void VecLoad_AllocVecBuffers(void)
{
    alloc_common();
    for (int i = 1; i <= 17; i++)
        ds.VecLoad_Faces[i - 1] = System_GetMem(0x960);
}

// 3d38:05d8
void VecLoad_FreeVecBuffers(void)
{
    free_common();
    for (int i = 1; i <= 17; i++)
        System_FreeMem(ds.VecLoad_Faces[i - 1], 0x960);
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
    ds.Vec3D_Model = model;
    // LoadModel asks for the slot after the last object to read the end
    // marker; with 17 objects that slot has no buffer (and none is used).
    return read_object(slot, slot <= 17 ? ds.VecLoad_Faces[slot - 1] : nullptr);
}

static void set_angles(uint16_t a, uint16_t b, uint16_t c)
{
    ds.VecLoad_Angles[0] = a;
    ds.VecLoad_Angles[1] = b;
    ds.VecLoad_Angles[2] = c;
}

// 3d38:084a
void VecLoad_LoadVec(int16_t n)
{
    Blit_FillRectAt(buffer_base() + WINDOW, 0x72, 0x73, 0x40);
    ds.VecLoad_Zoom = ds.VecLoad_ModelZoom[n - 1];
    ds.VecLoad_ObjectCount = 0;
    if (!file_open(n))
        return;
    while (VecLoad_ReadObject(n, (int16_t)(ds.VecLoad_ObjectCount + 1)))
        ds.VecLoad_ObjectCount++;
    file_close();

    int16_t count = ds.VecLoad_ObjectCount;
    if (count > 0)
        for (int16_t i = 1;; i++) {
            Vec3D_PrepareObject(n, i, ds.VecLoad_Verts[i - 1], ds.VecLoad_Faces[i - 1],
                                ds.VecLoad_Normals[i],
                                (uint8_t)ds.VecLoad_VertCount[i - 1],
                                (uint8_t)ds.VecLoad_FaceCount[i - 1]);
            if (i == count)
                break;
        }
    for (int k = 0; k < 3; k++)
        ds.VecLoad_Spin[k] = ds.VecLoad_ModelSpin[n - 1][k];
    set_angles(0, 0, 300);
    if (n == 14)
        set_angles(215, 45, 54);
    if (n == 24) {
        ds.VecLoad_Angles[1] = 21;
        ds.VecLoad_Angles[2] = 32;
    }
    ds.VecLoad_Scale = 200;
    track_min_mem();
}

// 3d38:0b2e
bool VecLoad_ReadVecObject(int16_t n, int16_t slot)
{
    (void)n;
    return read_object(slot, ds.VecLoad_Buffer);
}

// 3d38:0cd6
void VecLoad_LoadWireVec(int16_t n)
{
    ds.VecLoad_WireScale = 30;
    ds.VecLoad_ObjectCount = 0;
    if (!file_open(n))
        return;
    while (VecLoad_ReadVecObject(n, (int16_t)(ds.VecLoad_ObjectCount + 1)))
        ds.VecLoad_ObjectCount++;
    file_close();
    ds.VecLoad_Spin[0] = 5;
    ds.VecLoad_Spin[1] = 0;
    ds.VecLoad_Spin[2] = 0;
    set_angles(0, 0, 300);
    track_min_mem();
    Blit_FillRectAt(buffer_base() + 0x0010, 0x6C, 0x6A, 0x40);
}

// 3d38:0dd7
void VecLoad_Init(void)
{
    Vec3D_InitCosTable();
}
