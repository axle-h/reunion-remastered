// Unit Vec3D (3e17): the vector-model renderer (docs/formats/vec.md):
// orthographic projection with a cosine table, per-object normals and
// centroids, flat-shaded polygon fill and wireframe edges.
//
// Everything is integer code ported for bit-exact results: 16-bit words
// wrap, 16-bit offsets from a pointer wrap (bufSeg is a segment's start),
// and Real48 arithmetic goes through the System unit's ports. Points are 6-byte records of three words;
// faces are 20-byte records of 1-based vertex-index bytes ended by FFh;
// edges are two index bytes.
//
// Data segment state: Vec3D_CosTable, Vec3D_Centroids (per object slot),
// the two x-per-scanline edge tables Vec3D_EdgesA/B, and the current model
// and object slot (Vec3D_Model, Vec3D_Slot).
#pragma once

#include <stdint.h>


// 3e17:0000: one line per edge, from (x + 46h, y + 32h) of the projected
// points, in the segment starting at bufSeg (stride Gfx_LineStride).
void Vec3D_DrawEdges(uint8_t *bufSeg, uint8_t *verts, uint8_t *edges, int16_t count,
                     uint8_t colour);
// 3e17:00cc: fills every face whose projected normal has z >= 0, at
// (x + 86h, y + 43h), shaded from the normal; six objects (by Vec3D_Model/Slot)
// are two-sided and a few of their faces are skipped.
void Vec3D_DrawFaces(uint8_t *bufSeg, uint8_t *verts, uint8_t *faces, uint8_t *normals,
                     uint8_t count);
// 3e17:04d2: stores the rounded centroid in Vec3D_Centroids[slot] (with the
// hard-coded per-model fixes) and one normal per face (length 50, pointing
// toward the Real centroid) in normals.
void Vec3D_PrepareObject(int16_t model, int16_t slot, uint8_t *verts, uint8_t *faces,
                         uint8_t *normals, uint8_t nVerts, uint8_t nFaces);
// 3e17:11f9: fills the polygon of n (x, y) word pairs at pts; writes pts[n]
// (closes the outline). Nothing is clipped.
void Vec3D_FillPoly(uint8_t *bufSeg, int16_t n, uint8_t *pts, uint8_t colour);
// 3e17:13d6: rows firstY..lastY, |B[y] - A[y]| pixels from the smaller x,
// as words (colour + 1, colour): even columns get colour + 1.
void Vec3D_FillSpans(uint8_t *bufSeg, int16_t firstY, int16_t lastY, int16_t colour);
// 3e17:1438: table[y] = x along the edge (Bresenham). A horizontal edge
// writes nothing; a vertical one fills its whole y range.
void Vec3D_TraceEdge(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t *table);
// 3e17:1500: rotates count points from src into dst (whole degrees a, b, c;
// see docs/formats/vec.md), scaling the signed low bytes by the low byte of
// scale. mode 2 swaps the first two matrix rows.
void Vec3D_Project(int16_t a, int16_t b, int16_t c, uint8_t *src, uint8_t *dst, int16_t scale,
                   int16_t count, int16_t mode);
// 3e17:18db: cos500[i] = Round(Cos(i * pi / 180) * 500) in Real48.
void Vec3D_InitCosTable(void);
