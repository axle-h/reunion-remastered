// vec3d: the cosine table, projection of known points (hiword rounding,
// mode 2), edge tracing, FillPoly on a triangle, PrepareObject's centroid
// and normals, and (with game data) a whole model through VecLoad.
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "game/dseg.h"
#include "game/system.h"
#include "game/vec3d.h"
#include "game/vecload.h"
#include "platform/fs.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static int32_t cos500(int i)
{
    int32_t v;
    memcpy(&v, ds_at((uint16_t)(VEC3D_COS_TABLE + i * 4)), 4);
    return v;
}

static int16_t *words(FarPtr p)
{
    return far_host(p);
}

static void cos_table(void)
{
    mem_init();
    Vec3D_InitCosTable();
    CHECK(cos500(0) == 500);
    CHECK(cos500(30) == 433);   // 433.01
    CHECK(cos500(45) == 354);   // 353.55
    CHECK(cos500(60) == 250);
    CHECK(cos500(90) == 0);
    CHECK(cos500(120) == -250);
    CHECK(cos500(180) == -500);
    CHECK(cos500(270) == 0);
    CHECK(cos500(360) == 500);
    CHECK(cos500(1) == 500);    // 499.92
    CHECK(cos500(89) == 9);     // 8.73
    // Within half a unit of the true value everywhere, and symmetric.
    bool close = true, sym = true;
    for (int i = 0; i <= 360; i++) {
        double want = SDL_cos(i * SDL_PI_D / 180) * 500;
        close = close && SDL_fabs(cos500(i) - want) <= 0.5 + 1e-6;
        sym = sym && cos500(i) == cos500(360 - i);
    }
    CHECK(close);
    CHECK(sym);
    // The table ends right before the spin words at DS:8578.
    CHECK(*(int16_t *)ds_at(0x8578) == 0);
}

static void project(void)
{
    mem_init();
    Vec3D_InitCosTable();
    FarPtr src = far_ptr(0x5000, 0), dst = far_ptr(0x5100, 0);
    int16_t *s = words(src), *d = words(dst);
    // Angles 0: the matrix is diag(500, -500, 500). Coordinates times 40
    // times the matrix entry, then the high word (rounds toward -inf).
    s[0] = 10, s[1] = 20, s[2] = 30;
    s[3] = -10, s[4] = 0x0105, s[5] = 0;  // only the low byte (5) counts
    Vec3D_Project(0, 0, 0, src, dst, 40, 2, 1);
    CHECK(d[0] == 3 && d[1] == -7 && d[2] == 9);    // 200000, -400000, 600000 >> 16
    CHECK(d[3] == -4 && d[4] == -2 && d[5] == 0);   // -200000, -100000 >> 16
    // Mode 2 swaps the first two rows.
    Vec3D_Project(0, 0, 0, src, dst, 40, 1, 2);
    CHECK(d[0] == -7 && d[1] == 3 && d[2] == 9);
    // a = 90 (and 450 etc., which wrap): rows (0, 500, 0), (500, 0, 0),
    // (0, 0, 500).
    Vec3D_Project(90, 0, 0, src, dst, 40, 1, 1);
    CHECK(d[0] == 6 && d[1] == 3 && d[2] == 9);     // 400000, 200000, 600000
    Vec3D_Project(450, 360, 720, src, dst, 40, 1, 1);
    CHECK(d[0] == 6 && d[1] == 3 && d[2] == 9);
    // Scale is used as a signed byte (8-bit imul): 256 + 40 is 40.
    Vec3D_Project(0, 0, 0, src, dst, 296, 1, 1);
    CHECK(d[0] == 3 && d[1] == -7 && d[2] == 9);
    // b = 30: rows (433, 0, 250), (0, -500, 0), (-250, 0, 433).
    s[0] = 100, s[1] = 0, s[2] = 0;
    Vec3D_Project(0, 30, 0, src, dst, 100, 1, 1);
    // X = int8(100) * int8(100) = 10000: 10000 * 433 >> 16 = 66 and
    // -2500000 >> 16 = -39.
    CHECK(d[0] == 66 && d[1] == 0 && d[2] == -39);
}

static void trace_edge(void)
{
    mem_init();
    FarPtr t = far_ptr(MEM_DS_SEG, VEC3D_EDGES_A);
    int16_t *tab = words(t);
    // Vertical: the whole y range, whatever the direction.
    Vec3D_TraceEdge(7, 12, 7, 5, t);
    bool v = true;
    for (int y = 5; y <= 12; y++)
        v = v && tab[y] == 7;
    CHECK(v && tab[4] == 0 && tab[13] == 0);
    // Horizontal: nothing.
    SDL_memset(tab, 0, 400);
    Vec3D_TraceEdge(3, 20, 40, 20, t);
    CHECK(tab[20] == 0);
    // x-major (dx 10, dy 2): the last x of each row stays.
    Vec3D_TraceEdge(0, 0, 10, 2, t);
    CHECK(tab[0] == 2 && tab[1] == 7 && tab[2] == 10);
    // y-major, drawn from the left end (swapped), stepping y down.
    SDL_memset(tab, 0, 400);
    Vec3D_TraceEdge(4, 10, 0, 18, t);
    CHECK(tab[18] == 0 && tab[10] == 4);
    bool mono = true;
    for (int y = 11; y <= 18; y++)
        mono = mono && tab[y] <= tab[y - 1];
    CHECK(mono);
}

static void fill_poly(void)
{
    mem_init();
    uint16_t seg = 0x6000;
    uint8_t *buf = far_host(far_ptr(seg, 0));
    FarPtr pts = far_ptr(0x5000, 0);
    int16_t *p = words(pts);
    // (10, 10), (30, 10), (10, 30); the slot after the last point gets a
    // copy of the first.
    p[0] = 10, p[1] = 10, p[2] = 30, p[3] = 10, p[4] = 10, p[5] = 30;
    Vec3D_FillPoly(seg, 3, pts, 0xC0);
    CHECK(p[6] == 10 && p[7] == 10);
    // Row y covers x = 10 .. 39 - y (30 - y pixels): even x colour + 1.
    bool same = true;
    int count = 0;
    for (int y = 0; y < 40; y++)
        for (int x = 0; x < 64; x++) {
            uint8_t c = buf[y * 320 + x];
            bool in = y >= 10 && y < 30 && x >= 10 && x < 40 - y;
            uint8_t want = in ? (x & 1 ? 0xC0 : 0xC1) : 0;
            same = same && c == want;
            count += c != 0;
        }
    CHECK(same);
    CHECK(count == 210);  // 20 + 19 + ... + 1
    // The tables hold the two sides.
    CHECK(*(int16_t *)ds_at(VEC3D_EDGES_A + 2 * 15) == 25);
    CHECK(*(int16_t *)ds_at(VEC3D_EDGES_B + 2 * 15) == 10);

    // A span starting on an odd column gets colour + 1 there too.
    SDL_memset(buf, 0, 320 * 40);
    p[0] = 11, p[1] = 5, p[2] = 15, p[3] = 5, p[4] = 15, p[5] = 6, p[6] = 11, p[7] = 6;
    Vec3D_FillPoly(seg, 4, pts, 0xD0);
    CHECK(buf[5 * 320 + 11] == 0xD1 && buf[5 * 320 + 12] == 0xD1 && buf[5 * 320 + 13] == 0xD0 &&
          buf[5 * 320 + 14] == 0xD1 && buf[5 * 320 + 15] == 0);
}

static void prepare(void)
{
    mem_init();
    system_init();
    FarPtr verts = far_ptr(0x5000, 0), faces = far_ptr(0x5100, 0), normals = far_ptr(0x5200, 0);
    int16_t *v = words(verts);
    static const int16_t tetra[4][3] = {{0, 0, 0}, {10, 0, 0}, {0, 10, 0}, {0, 0, 10}};
    memcpy(v, tetra, sizeof tetra);
    uint8_t *f = far_host(faces);
    memset(f, 0xFF, 40);
    f[0] = 1, f[1] = 2, f[2] = 3;        // n = (0, 0, 100): toward the centroid
    f[20] = 1, f[21] = 3, f[22] = 2;     // n = (0, 0, -100): flipped
    Vec3D_PrepareObject(1, 2, verts, faces, normals, 4, 2);
    int16_t *c = ds_at(VEC3D_CENTROIDS + 2 * 6);
    CHECK(c[0] == 3 && c[1] == 3 && c[2] == 3);  // Round(2.5) = 3
    int16_t *n = words(normals);
    CHECK(n[0] == 0 && n[1] == 0 && n[2] == 50);
    CHECK(n[3] == 0 && n[4] == 0 && n[5] == 50);

    // Model 6 fixes: y = 0, x and z times 3.
    Vec3D_PrepareObject(6, 1, verts, faces, normals, 4, 0);
    c = ds_at(VEC3D_CENTROIDS + 1 * 6);
    CHECK(c[0] == 9 && c[1] == 0 && c[2] == 9);
}

// V3.VEC loaded and drawn like the 3D research screen.
static void model(void)
{
    mem_init();
    CHECK(dseg_init());
    system_init();
    ds.System_SegA000 = 0xA000;
    ds.Gfx_ScreenSeg = 0xA000;
    VecLoad_Init();
    *(FarPtr *)ds_at(0x8584) = System_GetMem(0xAF00);  // as Shop::ShopScreen
    VecLoad_AllocVecBuffers();
    VecLoad_LoadVec(3);
    int16_t objects = *(int16_t *)ds_at(0x7E50);
    CHECK(objects > 0 && objects <= 17);
    // Every normal has length about 50.
    bool len = true;
    for (int s = 1; s <= objects; s++) {
        int16_t nf = *(int16_t *)ds_at((uint16_t)(0x7FB0 + 2 * s));
        int16_t *n = far_host(*(FarPtr *)ds_at((uint16_t)(0x7E5A + 4 * s)));
        for (int i = 0; i < nf; i++) {
            const int16_t *v = n + 3 * i;
            int l2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
            len = len && l2 >= 48 * 48 && l2 <= 52 * 52;
        }
    }
    CHECK(len);
    VecLoad_DrawFrame();
    // Only the window changed, and only to background and blue-ramp colours.
    uint8_t *buf = far_host(far_ptr((uint16_t)(*(FarPtr *)ds_at(0x8584) >> 16), 0));
    int shaded = 0;
    bool colours = true;
    for (int y = 10; y < 10 + 0x73; y++)
        for (int x = 76; x < 76 + 0x72; x++) {
            uint8_t c = buf[y * 320 + x];
            colours = colours && (c == 0x40 || (c >= 0xC0 && c <= 0xEF));
            shaded += c != 0x40;
        }
    CHECK(colours);
    CHECK(shaded > 500);
    CHECK(*(uint16_t *)ds_at(0x7E48) == *(uint16_t *)ds_at(0x8578) % 1080);
    VecLoad_FreeVecBuffers();
}

int main(int argc, char *argv[])
{
    cos_table();
    project();
    trace_edge();
    fill_poly();
    prepare();
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping the model test\n");
        return ok ? 77 : 1;
    }
    model();
    return ok ? 0 : 1;
}
