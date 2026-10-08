// Unit Vec3D (3e17): the vector-model renderer (docs/formats/vec.md):
// orthographic projection with a cosine table, per-object normals and
// centroids, flat-shaded polygon fill and wireframe edges.
//
// Everything is integer code ported for bit-exact results: 16-bit words
// wrap, 16-bit offsets wrap inside their segment, and Real48 arithmetic goes
// through the System unit's ports. Points are 6-byte records of three words;
// faces are 20-byte records of 1-based vertex-index bytes ended by FFh;
// edges are two index bytes.
//
// Data segment state (no names in dseg.h yet):
//   DS:7FD4  cos500[0..360]: longint, Round(cos(i deg) * 500)
//   DS:8582  centroids: 3 words per object slot (slot 1 at 8588h)
//   DS:8676 / DS:8806  the two x-per-scanline edge tables (200 words each)
//   DS:8996  current model number, DS:8998 current object slot
#pragma once

#include <stdint.h>

#include "game/mem.h"

enum : uint16_t {
    VEC3D_COS_TABLE = 0x7FD4,
    VEC3D_CENTROIDS = 0x8582,
    VEC3D_EDGES_A = 0x8676,
    VEC3D_EDGES_B = 0x8806,
    VEC3D_MODEL = 0x8996,
    VEC3D_SLOT = 0x8998,
};

// 3e17:0000: one line per edge, from (x + 46h, y + 32h) of the projected
// points, in segment bufSeg (stride Gfx_LineStride).
void Vec3D_DrawEdges(uint16_t bufSeg, FarPtr verts, FarPtr edges, int16_t count, uint8_t colour);
// 3e17:00cc: fills every face whose projected normal has z >= 0, at
// (x + 86h, y + 43h), shaded from the normal; six objects (by DS:8996/8998)
// are two-sided and a few of their faces are skipped.
void Vec3D_DrawFaces(uint16_t bufSeg, FarPtr verts, FarPtr faces, FarPtr normals, uint8_t count);
// 3e17:04d2: stores the rounded centroid at DS:8582 + 6 * slot (with the
// hard-coded per-model fixes) and one normal per face (length 50, pointing
// toward the Real centroid) in normals.
void Vec3D_PrepareObject(int16_t model, int16_t slot, FarPtr verts, FarPtr faces, FarPtr normals,
                         uint8_t nVerts, uint8_t nFaces);
// 3e17:11f9: fills the polygon of n (x, y) word pairs at pts; writes pts[n]
// (closes the outline). Nothing is clipped.
void Vec3D_FillPoly(uint16_t bufSeg, int16_t n, FarPtr pts, uint8_t colour);
// 3e17:13d6: rows firstY..lastY, |B[y] - A[y]| pixels from the smaller x,
// as words (colour + 1, colour): even columns get colour + 1.
void Vec3D_FillSpans(uint16_t bufSeg, int16_t firstY, int16_t lastY, int16_t colour);
// 3e17:1438: table[y] = x along the edge (Bresenham). A horizontal edge
// writes nothing; a vertical one fills its whole y range.
void Vec3D_TraceEdge(int16_t x0, int16_t y0, int16_t x1, int16_t y1, FarPtr table);
// 3e17:1500: rotates count points from src into dst (whole degrees a, b, c;
// see docs/formats/vec.md), scaling the signed low bytes by the low byte of
// scale. mode 2 swaps the first two matrix rows.
void Vec3D_Project(int16_t a, int16_t b, int16_t c, FarPtr src, FarPtr dst, int16_t scale,
                   int16_t count, int16_t mode);
// 3e17:18db: cos500[i] = Round(Cos(i * pi / 180) * 500) in Real48.
void Vec3D_InitCosTable(void);
