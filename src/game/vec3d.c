#include "game/vec3d.h"

#include "game/dseg.h"
#include "game/gfx.h"
#include "game/minmax.h"
#include "game/system.h"

// --- memory helpers ------------------------------------------------------------

// Byte at p + off. The original's 16-bit offsets wrap inside the segment;
// from p's start, so does off.
static inline uint8_t *at(uint8_t *p, uint16_t off)
{
    return p + off;
}

static inline int16_t rdw(uint8_t *p, uint16_t off)
{
    return (int16_t)(*at(p, off) | *at(p, (uint16_t)(off + 1)) << 8);
}

static inline void wrw(uint8_t *p, uint16_t off, int16_t v)
{
    *at(p, off) = (uint8_t)v;
    *at(p, (uint16_t)(off + 1)) = (uint8_t)((uint16_t)v >> 8);
}

// cos500[i] (the callers keep i in 0..359).
static int32_t cos500(int16_t i)
{
    return ds.Vec3D_CosTable[i];
}

// System LongMul (low 32 bits) and LongDiv by 500 (truncating, like div).
static inline int32_t lmul(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a * (uint32_t)b);
}

static inline int32_t d500(int32_t a)
{
    return a / 500;
}

static Real48 real_const(uint8_t e, uint8_t m4, uint8_t m3, uint8_t m2, uint8_t m1, uint8_t m0)
{
    return (Real48){{e, m0, m1, m2, m3, m4}};
}

// --- DrawEdges -------------------------------------------------------------------

// 3e17:0000
void Vec3D_DrawEdges(uint8_t *bufSeg, uint8_t *verts, uint8_t *edges, int16_t count,
                     uint8_t colour)
{
    uint16_t n = (uint16_t)count;
    if (n < 1)
        return;
    for (uint16_t i = 1;; i++) {
        uint8_t a = *at(edges, (uint16_t)(i * 2 - 2));
        uint8_t b = *at(edges, (uint16_t)(i * 2 - 1));
        Gfx_DrawLine(bufSeg, (int16_t)(rdw(verts, (uint16_t)(a * 6 - 6)) + 0x46),
                     (int16_t)(rdw(verts, (uint16_t)(a * 6 - 4)) + 0x32),
                     (int16_t)(rdw(verts, (uint16_t)(b * 6 - 6)) + 0x46),
                     (int16_t)(rdw(verts, (uint16_t)(b * 6 - 4)) + 0x32), colour);
        if (i == n)
            break;
    }
}

// --- DrawFaces -------------------------------------------------------------------

// DrawFaces' outline buffer (a local in the original): up to 20 points and
// the closing one FillPoly adds.
static uint8_t face_points[21 * 4];

// Faces of the two-sided objects that are never drawn. The back-face pass
// also skips face 28h of model 7 object 1; the front-face pass doesn't.
static bool face_skipped(int16_t model, int16_t slot, uint8_t face, bool back_pass)
{
    if (model == 7 && slot == 1 &&
        (face == 0x13 || face == 0x24 || face == 0x40 || face == 0x47 ||
         (back_pass && face == 0x28)))
        return true;
    if (model == 12 && slot == 7 && face == 0x1B)
        return true;
    if (model == 18 && slot == 5 &&
        (face == 0x0B || face == 0x0D || face == 0x0F || face == 0x11 || face == 0x12))
        return true;
    return false;
}

// One pass over the faces: the back faces (nz < 0) with the shading reversed,
// or the front faces (nz >= 0).
static void draw_face_pass(uint8_t *bufSeg, uint8_t *verts, uint8_t *faces, uint8_t *normals,
                           uint8_t count, bool back_pass)
{
    int16_t model = ds.Vec3D_Model, slot = ds.Vec3D_Slot;
    uint8_t *pts = face_points;
    for (uint8_t f = 1;; f++) {
        int16_t nx = rdw(normals, (uint16_t)(f * 6 - 6));
        int16_t nz = rdw(normals, (uint16_t)(f * 6 - 2));
        if ((nz < 0) == back_pass) {
            // Reads index k before testing k < 20, so a face without FFh
            // ends after 19 points (its 20th byte is read, not used).
            uint8_t k = 1;
            for (;; k++) {
                uint8_t v = *at(faces, (uint16_t)((uint16_t)(f * 0x14) + k - 0x15));
                if (v == 0xFF || k >= 0x14)
                    break;
                int16_t x = rdw(verts, (uint16_t)(v * 6 - 6));
                int16_t y = rdw(verts, (uint16_t)(v * 6 - 4));
                wrw(pts, (uint16_t)(k * 4 - 4), (int16_t)(x + 0x86));
                wrw(pts, (uint16_t)(k * 4 - 2), (int16_t)(y + 0x43));
            }
            int16_t shade = back_pass ? (int16_t)(nx - nz) : (int16_t)(nz - nx);
            shade = (int16_t)((uint16_t)(shade + 0x18) << 2);
            shade = (int16_t)(shade / 5);
            if (shade < 0)
                shade = 0;
            if (shade > 0x2E)
                shade = 0x2E;
            shade = (int16_t)(shade + 0xC0);
            if (!face_skipped(model, slot, f, back_pass))
                Vec3D_FillPoly(bufSeg, (int16_t)(k - 1), pts, (uint8_t)shade);
        }
        if (f == count)
            break;
    }
}

// 3e17:00cc
void Vec3D_DrawFaces(uint8_t *bufSeg, uint8_t *verts, uint8_t *faces, uint8_t *normals,
                     uint8_t count)
{
    int16_t model = ds.Vec3D_Model, slot = ds.Vec3D_Slot;
    bool two_sided = (model == 7 && slot == 1) || (model == 12 && slot == 7) ||
                     (model == 18 && slot == 5) || (model == 26 && slot == 7) ||
                     (model == 26 && slot == 9) || (model == 27 && slot == 4);
    if (two_sided && count != 0)
        draw_face_pass(bufSeg, verts, faces, normals, count, true);
    if (count != 0)
        draw_face_pass(bufSeg, verts, faces, normals, count, false);
}

// --- PrepareObject ---------------------------------------------------------------

static inline int16_t abs16(int16_t v)
{
    return (int16_t)(v < 0 ? (uint16_t)-(uint16_t)v : (uint16_t)v);  // abs(-8000h) = -8000h
}

// cwd; idiv: v / d, run-time error 200 for d = 0 like the CPU's divide trap.
static int16_t idiv16(int16_t v, int16_t d)
{
    if (d == 0)
        System_RunError(200);
    return (int16_t)(v / d);
}

// v / |v|: the sign of v (division by zero, run-time error 200, for v = 0).
static inline int16_t sign_div(int16_t v)
{
    return idiv16(v, abs16(v));
}

// Hand fixes to the drawing order: the integer centroid of particular
// objects (only used for depth sorting) is overridden per model.
static void fix_centroid(int16_t model, int16_t slot, int16_t *c)
{
    int16_t *x = &c[0], *y = &c[1], *z = &c[2];
    if (model == 6) {
        if (slot == 2) *z = 14;
        if (slot == 3) *x = -4;
        if (slot == 4) *x = 4;
        *y = 0;
        *x = (int16_t)(*x * 3);
        *z = (int16_t)(*z * 3);
    }
    if (model == 9) {
        if (slot == 2) *y = 1;
        if (slot == 3) *y = -120;
        if (slot == 4) *z = 0;
        if (slot == 5) *z = 0;
        if (slot == 4) *y = 120;
        if (slot == 5) *y = 120;
    }
    if (model == 11 && slot == 2)
        *y = (int16_t)(*y + 11);
    if (model == 12 && slot == 7)
        *z = (int16_t)(*z + 10);
    if (model == 14) {
        *z = 0;
        if (slot == 1) *x = 0;
        if (slot == 1) *y = 0;
        if (slot == 2) *x = -4;
        if (slot == 2) *y = -100;
        if (slot == 3) *x = 4;
        if (slot == 3) *y = -100;
        if (slot >= 4 && slot <= 14) *y = 0;
        if (slot >= 4 && slot <= 6) *x = -4;
        if (slot >= 7 && slot <= 11) *x = 0;
        if (slot >= 12 && slot <= 14) *x = 4;
        if (slot == 4 || slot == 8 || slot == 12 || slot == 7) *z = 20;
        if (slot == 5 || slot == 9 || slot == 13) *z = 0;
        if (slot == 6 || slot == 10 || slot == 14 || slot == 11) *z = -20;
        if (slot == 4 || slot == 8 || slot == 12) *y = 0;
        if (slot == 5 || slot == 9 || slot == 13) *y = 40;
        if (slot == 6 || slot == 10 || slot == 14) *y = 0;
        if (slot == 7) *y = -5;
        if (slot == 11) *y = -5;
    }
    if (model == 15) {
        *z = 0;
        if (slot == 1) *y = 100;
        if (slot > 1) *y = -100;
        if (slot == 3) *x = -8;
        if (slot == 4) *x = 8;
    }
    if (model == 16) {
        *y = 0;
        *z = 0;
    }
    if (model == 19) {
        if (slot <= 5) *x = 0;
        *z = 0;
        if (slot >= 2 && slot <= 5) *z = -50;
        if (slot == 1) *y = 0;
        if (slot == 6 || slot == 7) *y = 3;
        if (slot == 2) *y = 2;
        if (slot == 3) *y = -10;
        if (slot == 4) *y = -20;
        if (slot == 5) *y = -30;
    }
    if (model == 21) {
        if (slot > 2) *x = 0;
        if (slot != 6) *y = 0;
        *z = 0;
        if (slot == 5) *y = -5;
        if (slot == 6) *y = -10;
    }
    if (model == 23) {
        *z = 0;
        if (slot == 2) *z = -7;
        if (slot == 7) *z = 120;
        *x = 0;
        if (slot == 2) *x = -5;
        if (slot == 3) *x = -110;
        if (slot == 4) *x = -108;
        if (slot == 5) *x = 108;
        if (slot == 6) *x = 110;
        *y = 0;
        if (slot == 2) *y = -9;
        if (slot == 4) *y = 5;
        if (slot == 5) *y = 5;
    }
    if (model == 24) {
        if (slot <= 8 && abs16(*x) == 0x21) *x = (int16_t)(sign_div(*x) * 5);
        if (slot <= 8 && abs16(*y) == 0x21) *y = (int16_t)(sign_div(*y) * 5);
        if (slot <= 8 && abs16(*x) == 0x43) *x = (int16_t)(sign_div(*x) * 20);
        if (slot <= 8 && abs16(*y) == 0x43) *y = (int16_t)(sign_div(*y) * 20);
        if (slot == 10) *x = -120;
        if (slot == 11) *x = 120;
        // BUG? Run-time error 200 if one of these centroids has x = 0.
        if (slot <= 8) *x = (int16_t)(sign_div(*x) * 100 + *x);
    }
    if (model == 25)
        *z = 0;
    if (model == 26) {
        *y = 0;
        *z = 0;
    }
    if (model == 32) {
        if (slot <= 4) *y = 0;
        if (slot > 4) *x = 0;
        *z = 0;
        if (slot == 1) *x = -5;
        if (slot == 2) *x = 5;
        if (slot == 3) *z = -25;
        if (slot == 3) *x = 0;
        if (slot == 4) *x = 0;
        if (slot == 5) *z = -25;
        if (slot == 6) *z = -25;
        if (slot == 7) *z = -25;
    }
}

// 3e17:04d2. Ghidra fails on this one; ported from the disassembly. The
// original also computes det(P1, P2, P3) per face and never uses it.
void Vec3D_PrepareObject(int16_t model, int16_t slot, uint8_t *verts, uint8_t *faces,
                         uint8_t *normals, uint8_t nVerts, uint8_t nFaces)
{
    // Centroid: Real sums of the vertices divided by their count.
    Real48 cx = {0}, cy = {0}, cz = {0};
    for (uint16_t v = 1; v <= nVerts; v++) {
        cx = System_RealAdd(System_LongToReal(rdw(verts, (uint16_t)(v * 6 - 6))), cx);
        cy = System_RealAdd(System_LongToReal(rdw(verts, (uint16_t)(v * 6 - 4))), cy);
        cz = System_RealAdd(System_LongToReal(rdw(verts, (uint16_t)(v * 6 - 2))), cz);
    }
    cx = System_RealDiv(cx, System_LongToReal(nVerts));
    cy = System_RealDiv(cy, System_LongToReal(nVerts));
    cz = System_RealDiv(cz, System_LongToReal(nVerts));

    int16_t c[3] = {(int16_t)System_Round(cx), (int16_t)System_Round(cy),
                    (int16_t)System_Round(cz)};
    fix_centroid(model, slot, c);
    TVecPoint *cp = &ds.Vec3D_Centroids[slot - 1];
    cp->X = c[0];
    cp->Y = c[1];
    cp->Z = c[2];

    // Normals from the first three vertices of each face, in 32 bits.
    for (uint16_t f = 1; f <= nFaces; f++) {
        int32_t p[3][3];
        for (int k = 0; k < 3; k++) {
            uint8_t v = *at(faces, (uint16_t)(f * 0x14 - 0x14 + k));
            for (int j = 0; j < 3; j++)
                p[k][j] = rdw(verts, (uint16_t)(v * 6 - 6 + j * 2));
        }
        int32_t ax = p[1][0] - p[0][0], ay = p[1][1] - p[0][1], az = p[1][2] - p[0][2];
        int32_t bx = p[2][0] - p[0][0], by = p[2][1] - p[0][1], bz = p[2][2] - p[0][2];
        int32_t nx = (int32_t)((uint32_t)lmul(ay, bz) - (uint32_t)lmul(az, by));
        int32_t ny = (int32_t)((uint32_t)lmul(az, bx) - (uint32_t)lmul(ax, bz));
        int32_t nz = (int32_t)((uint32_t)lmul(ax, by) - (uint32_t)lmul(ay, bx));
        int32_t d = (int32_t)(0u - ((uint32_t)lmul(nx, p[0][0]) + (uint32_t)lmul(ny, p[0][1]) +
                                    (uint32_t)lmul(nz, p[0][2])));

        // n . C + d < 0 (n points away from the centroid): flip it.
        Real48 s = System_RealMul(System_LongToReal(nx), cx);
        s = System_RealAdd(s, System_RealMul(System_LongToReal(ny), cy));
        s = System_RealAdd(s, System_RealMul(System_LongToReal(nz), cz));
        s = System_RealAdd(s, System_LongToReal(d));
        if (System_RealCmp(s, (Real48){0}) < 0) {
            nx = (int32_t)(0u - (uint32_t)nx);
            ny = (int32_t)(0u - (uint32_t)ny);
            nz = (int32_t)(0u - (uint32_t)nz);
        }

        int32_t len2 = (int32_t)((uint32_t)lmul(nx, nx) + (uint32_t)lmul(ny, ny) +
                                 (uint32_t)lmul(nz, nz));
        Real48 k = System_RealDiv(System_Sqrt(System_LongToReal(len2)),
                                  real_const(0x86, 0x48, 0, 0, 0, 0));  // 50.0
        if (System_RealCmp(k, (Real48){0}) == 0)
            k = real_const(0x81, 0, 0, 0, 0, 0);  // 1.0
        uint16_t o = (uint16_t)(f * 6 - 6);
        wrw(normals, o, (int16_t)System_Round(System_RealDiv(System_LongToReal(nx), k)));
        wrw(normals, (uint16_t)(o + 2),
            (int16_t)System_Round(System_RealDiv(System_LongToReal(ny), k)));
        wrw(normals, (uint16_t)(o + 4),
            (int16_t)System_Round(System_RealDiv(System_LongToReal(nz), k)));
    }
}

// --- polygon fill ----------------------------------------------------------------

// The edge tables as bytes, indexed by 2 * y (signed: see TraceEdge on the
// original's wrap inside DS).
static inline uint8_t *edges_a(void)
{
    return (uint8_t *)&ds.Vec3D_EdgesA;
}

static inline uint8_t *edges_b(void)
{
    return (uint8_t *)&ds.Vec3D_EdgesB;
}

// 3e17:11f9
void Vec3D_FillPoly(uint8_t *bufSeg, int16_t n, uint8_t *pts, uint8_t colour)
{
    // Close the outline: point n + 1 = point 1.
    wrw(pts, (uint16_t)((n + 1) * 4 - 4), rdw(pts, 0));
    wrw(pts, (uint16_t)((n + 1) * 4 - 2), rdw(pts, 2));

    // Lowest and highest y (unsigned compares), scanning from point n down
    // to point 2; ties keep the point found first.
    uint16_t maxY = (uint16_t)rdw(pts, 2), minY = maxY;
    int16_t maxI = 1, minI = 1;
    uint16_t off = (uint16_t)((n - 1) * 4 + 2);
    uint16_t i = (uint16_t)n;
    do {
        uint16_t y = (uint16_t)rdw(pts, off);
        if (y > maxY) {
            maxY = y;
            maxI = (int16_t)i;
        }
        if (y < minY) {
            minY = y;
            minI = (int16_t)i;
        }
        off -= 4;
        i--;
    } while (i != 1);
    int16_t hi = maxI, lo = minI;
    if (hi < lo) {
        hi = minI;
        lo = maxI;
    }

    // Edges lo..hi into table A, hi..n+1 and 1..lo into table B.
    uint8_t *a = edges_a(), *b = edges_b();
#define EDGE(k, table)                                                                      \
    Vec3D_TraceEdge(rdw(pts, (uint16_t)((k) * 4 - 4)), rdw(pts, (uint16_t)((k) * 4 - 2)),   \
                    rdw(pts, (uint16_t)((k) * 4)), rdw(pts, (uint16_t)((k) * 4 + 2)), table)
    if (lo <= hi - 1)
        for (int16_t k = lo;; k++) {
            EDGE(k, a);
            if (k == hi - 1)
                break;
        }
    if (hi <= n)
        for (int16_t k = hi;; k++) {
            EDGE(k, b);
            if (k == n)
                break;
        }
    if (1 <= lo - 1)
        for (int16_t k = 1;; k++) {
            EDGE(k, b);
            if (k == lo - 1)
                break;
        }
#undef EDGE
    Vec3D_FillSpans(bufSeg, (int16_t)minY, (int16_t)maxY, colour);
}

// 3e17:13d6
void Vec3D_FillSpans(uint8_t *bufSeg, int16_t firstY, int16_t lastY, int16_t colour)
{
    uint16_t rows = (uint16_t)(lastY - firstY + 1);
    // xchg ah, al / shr 2 / add: y * 320 for y < 256.
    uint16_t swapped = (uint16_t)((uint16_t)firstY << 8 | (uint16_t)firstY >> 8);
    uint16_t row = (uint16_t)(swapped + (swapped >> 2));
    uint16_t si = (uint16_t)(firstY * 2);
    uint8_t odd = (uint8_t)colour, even = (uint8_t)(colour + 1);
    uint8_t *buf = bufSeg;
    do {
        uint16_t xa = (uint16_t)rdw(edges_a() + (int16_t)si, 0);
        uint16_t xb = (uint16_t)rdw(edges_b() + (int16_t)si, 0);
        uint16_t cx = (uint16_t)(xb - xa);
        uint16_t di = row;
        if (xb < xa) {
            cx = (uint16_t)-cx;
            di += xb;
        } else {
            di += xa;
        }
        bool draw = true;
        if (di & 1) {
            // An odd first pixel gets colour + 1 too (stosb of al).
            cx--;
            if ((int16_t)cx < 0)
                draw = false;
            else
                *at(buf, di++) = even;
        }
        if (draw) {
            for (uint16_t w = cx >> 1; w != 0; w--) {
                *at(buf, di++) = even;
                *at(buf, di++) = odd;
            }
            if (cx & 1)
                *at(buf, di++) = even;
        }
        row += 320;
        si += 2;
    } while (--rows != 0);
}

// 3e17:1438. Two hand-written Bresenham loops (picked through a jump
// address in the original): x-major steps x every pixel and y when the
// error is non-negative before the step; y-major steps y every pixel and x
// when the error is non-negative after it.
void Vec3D_TraceEdge(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t *table)
{
    int16_t step = 2;
    uint16_t dx = (uint16_t)(x1 - x0);
    if (dx == 0) {
        // Vertical: table[min(y0, y1) .. max] = x0.
        int16_t from = y0;
        uint16_t cnt = (uint16_t)(y1 - y0);
        if ((int16_t)cnt < 0) {
            cnt = (uint16_t)-cnt;
            from = y1;
        }
        cnt++;
        // The original's offsets wrap inside DS; the edge tables are far
        // enough from its ends that y in -16384..16383 doesn't wrap.
        uint8_t *di = table + from * 2;
        for (; cnt != 0; cnt--, di += 2)
            wrw(di, 0, x0);
        return;
    }
    if ((int16_t)dx < 0) {
        dx = (uint16_t)-dx;
        int16_t t = x0;
        x0 = x1;
        x1 = t;
        t = y0;
        y0 = y1;
        y1 = t;
    }
    uint16_t dy = (uint16_t)(y1 - y0);
    if (dy == 0)
        return;  // horizontal: nothing
    if ((int16_t)dy < 0) {
        dy = (uint16_t)-dy;
        step = -2;
    }
    bool y_major = (int16_t)dy > (int16_t)dx;
    uint16_t major = y_major ? dy : dx, minor = y_major ? dx : dy;
    int16_t d1 = (int16_t)(minor * 2);
    int16_t err = (int16_t)(minor * 2 - major);
    int16_t d2 = (int16_t)(minor * 2 - major * 2);
    uint8_t *di = table + y0 * 2;
    int16_t x = x0;
    uint16_t cnt = (uint16_t)(major + 1);
    bool s = err >= 0;
    if (!y_major) {
        do {
            wrw(di, 0, x);
            x++;
            if (s) {
                di += step;
                err = (int16_t)(err + d2);
            } else {
                err = (int16_t)(err + d1);
            }
            s = err >= 0;
        } while (--cnt != 0);
    } else {
        do {
            wrw(di, 0, x);
            di += step;
            err = (int16_t)(err + (s ? d2 : d1));
            s = err >= 0;
            if (s)
                x++;
        } while (--cnt != 0);
    }
}

// --- projection ------------------------------------------------------------------

// 3e17:1500
void Vec3D_Project(int16_t a, int16_t b, int16_t c, uint8_t *src, uint8_t *dst, int16_t scale,
                   int16_t count, int16_t mode)
{
    while (a >= 360)
        a -= 360;
    while (b >= 360)
        b -= 360;
    while (c >= 360)
        c -= 360;
    // Sines as cos(i - 90) (cos(i + 270) below 90).
    int32_t ca = cos500(a), sa = a < 90 ? cos500((int16_t)(a + 270)) : cos500((int16_t)(a - 90));
    int32_t cb = cos500(b), sb = b < 90 ? cos500((int16_t)(b + 270)) : cos500((int16_t)(b - 90));
    int32_t cc = cos500(c), sc = c < 90 ? cos500((int16_t)(c + 270)) : cos500((int16_t)(c - 90));

    // The matrix keeps the low words of the longint results.
    int16_t m[3][3];
    m[0][0] = (int16_t)d500(lmul(cb, ca));
    m[0][1] = (int16_t)d500(lmul(cb, sa));
    m[0][2] = (int16_t)sb;
    m[1][0] = (int16_t)(d500(lmul(d500(lmul(sc, sb)), ca)) + d500(lmul(cc, sa)));
    m[1][1] = (int16_t)(d500(lmul(d500(lmul(sc, sb)), sa)) + d500(lmul(-cc, ca)));
    m[1][2] = (int16_t)d500(lmul(-sc, cb));
    m[2][0] = (int16_t)(d500(lmul(sc, sa)) - d500(lmul(d500(lmul(cc, sb)), ca)));
    m[2][1] = (int16_t)(d500(lmul(-sc, ca)) - d500(lmul(d500(lmul(cc, sb)), sa)));
    m[2][2] = (int16_t)d500(lmul(cc, cb));
    if (mode == 2) {
        MinMax_SwapInt(&m[0][0], &m[1][0]);
        MinMax_SwapInt(&m[0][1], &m[1][1]);
        MinMax_SwapInt(&m[0][2], &m[1][2]);
    }

    // Last point first; si runs down to 0 (count 0 wraps through 64 KB, as
    // in the original).
    int8_t s8 = (int8_t)scale;
    uint16_t si = (uint16_t)(count * 6);
    do {
        si -= 6;
        int16_t p[3];
        for (int j = 0; j < 3; j++)
            p[j] = (int16_t)((int8_t)*at(src, (uint16_t)(si + j * 2)) * s8);
        for (int r = 0; r < 3; r++) {
            uint32_t sum = (uint32_t)((int32_t)p[0] * m[r][0]) +
                           (uint32_t)((int32_t)p[1] * m[r][1]) +
                           (uint32_t)((int32_t)p[2] * m[r][2]);
            wrw(dst, (uint16_t)(si + r * 2), (int16_t)(uint16_t)(sum >> 16));
        }
    } while (si != 0);
}

// 3e17:18db
void Vec3D_InitCosTable(void)
{
    const Real48 pi = real_const(0x82, 0x49, 0x0F, 0xDA, 0xA2, 0x21);
    const Real48 r180 = real_const(0x88, 0x34, 0, 0, 0, 0);
    const Real48 r500 = real_const(0x89, 0x7A, 0, 0, 0, 0);
    for (int16_t i = 0; i <= 360; i++) {
        Real48 x = System_RealDiv(System_RealMul(System_LongToReal(i), pi), r180);
        int32_t v = System_Round(System_RealMul(System_Cos(x), r500));
        ds.Vec3D_CosTable[i] = v;
    }
}
