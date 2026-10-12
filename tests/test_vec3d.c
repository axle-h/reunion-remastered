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
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static int32_t cos500(int i)
{
    return ds.Vec3D_CosTable[i];
}

static int16_t *words(uint8_t *p)
{
    return (int16_t *)p;
}

// Scratch memory, addressed by the segment numbers the tests were written
// with, 5000h..6FFFh (cleared by reset()).
static uint8_t scratch[0x20000];

static uint8_t *seg_host(uint16_t seg)
{
    return scratch + (seg - 0x5000) * 16;
}

static void reset(void)
{
    dseg_clear();
    vid_clear();
    SDL_memset(scratch, 0, sizeof scratch);
}

static void cos_table(void)
{
    reset();
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
    // The table ends right before the spin words.
    CHECK(ds.VecLoad_Spin[0] == 0);
}

static void project(void)
{
    reset();
    Vec3D_InitCosTable();
    uint8_t *src = seg_host(0x5000), *dst = seg_host(0x5100);
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
    reset();
    uint8_t *t = (uint8_t *)&ds.Vec3D_EdgesA;
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
    reset();
    uint8_t *buf = seg_host(0x6000);
    uint8_t *pts = seg_host(0x5000);
    int16_t *p = words(pts);
    // (10, 10), (30, 10), (10, 30); the slot after the last point gets a
    // copy of the first.
    p[0] = 10, p[1] = 10, p[2] = 30, p[3] = 10, p[4] = 10, p[5] = 30;
    Vec3D_FillPoly(buf, 3, pts, 0xC0);
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
    CHECK(ds.Vec3D_EdgesA[15] == 25);
    CHECK(ds.Vec3D_EdgesB[15] == 10);

    // A span starting on an odd column gets colour + 1 there too.
    SDL_memset(buf, 0, 320 * 40);
    p[0] = 11, p[1] = 5, p[2] = 15, p[3] = 5, p[4] = 15, p[5] = 6, p[6] = 11, p[7] = 6;
    Vec3D_FillPoly(buf, 4, pts, 0xD0);
    CHECK(buf[5 * 320 + 11] == 0xD1 && buf[5 * 320 + 12] == 0xD1 && buf[5 * 320 + 13] == 0xD0 &&
          buf[5 * 320 + 14] == 0xD1 && buf[5 * 320 + 15] == 0);
}

static void prepare(void)
{
    reset();
    system_init();
    uint8_t *verts = seg_host(0x5000), *faces = seg_host(0x5100), *normals = seg_host(0x5200);
    int16_t *v = words(verts);
    static const int16_t tetra[4][3] = {{0, 0, 0}, {10, 0, 0}, {0, 10, 0}, {0, 0, 10}};
    memcpy(v, tetra, sizeof tetra);
    uint8_t *f = faces;
    memset(f, 0xFF, 40);
    f[0] = 1, f[1] = 2, f[2] = 3;        // n = (0, 0, 100): toward the centroid
    f[20] = 1, f[21] = 3, f[22] = 2;     // n = (0, 0, -100): flipped
    Vec3D_PrepareObject(1, 2, verts, faces, normals, 4, 2);
    const TVecPoint *c = &ds.Vec3D_Centroids[2 - 1];
    CHECK(c->X == 3 && c->Y == 3 && c->Z == 3);  // Round(2.5) = 3
    int16_t *n = words(normals);
    CHECK(n[0] == 0 && n[1] == 0 && n[2] == 50);
    CHECK(n[3] == 0 && n[4] == 0 && n[5] == 50);

    // Model 6 fixes: y = 0, x and z times 3.
    Vec3D_PrepareObject(6, 1, verts, faces, normals, 4, 0);
    c = &ds.Vec3D_Centroids[1 - 1];
    CHECK(c->X == 9 && c->Y == 0 && c->Z == 9);
}

// V3.VEC loaded and drawn like the 3D research screen.
static void model(void)
{
    reset();
    dseg_init();
    system_init();
    VecLoad_Init();
    ds.VecLoad_Buffer = System_GetSegment();  // GetMem(0xAF00)  // as Shop::ShopScreen
    VecLoad_AllocVecBuffers();
    VecLoad_LoadVec(3);
    int16_t objects = ds.VecLoad_ObjectCount;
    CHECK(objects > 0 && objects <= 17);
    // Every normal has length about 50.
    bool len = true;
    for (int s = 1; s <= objects; s++) {
        int16_t nf = ds.VecLoad_FaceCount[s - 1];
        int16_t *n = (int16_t *)ds.VecLoad_Normals[s];
        for (int i = 0; i < nf; i++) {
            const int16_t *v = n + 3 * i;
            int l2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
            len = len && l2 >= 48 * 48 && l2 <= 52 * 52;
        }
    }
    CHECK(len);
    VecLoad_DrawFrame();
    // Only the window changed, and only to background and blue-ramp colours.
    uint8_t *buf = ds.VecLoad_Buffer;
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
    CHECK(ds.VecLoad_Angles[0] == ds.VecLoad_Spin[0] % 1080);
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
