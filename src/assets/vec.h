// .VEC vector models (VECTORS\Vn.VEC); see docs/formats/vec.md.
// Parsing only: values are kept exactly as stored in the file.
#pragma once

#include <stddef.h>
#include <stdint.h>

enum : int {
    VEC_MAX_OBJECTS = 17,   // the game's per-model buffers hold this many
    VEC_MAX_FACE_VERTS = 19 // the renderer never reads a 20th index
};

typedef struct {
    int16_t x, y, z;
} VecVertex;

// Vertex indices count from 1.
typedef struct {
    uint8_t a, b;
} VecEdge;

typedef struct {
    uint8_t count; // 3..19
    uint8_t index[VEC_MAX_FACE_VERTS];
} VecFace;

typedef struct {
    uint16_t size; // as stored; the game only compares it with -1
    bool size_ok;  // size matches the actual byte length of the object
    int num_vertices, num_edges, num_faces;
    VecVertex *vertices;
    VecEdge *edges;
    VecFace *faces;
} VecObject;

typedef struct {
    int num_objects;
    VecObject *objects;
} VecModel;

// Fails if the data is truncated, has trailing bytes, too many objects, or
// an index outside 1..num_vertices.
bool vec_decode(const uint8_t *data, size_t size, VecModel *out);

// Loads VECTORS\V<number>.VEC (1..35).
bool vec_load(const char *dos_path, VecModel *out);
void vec_free(VecModel *model);

// A coordinate as the game's projection sees it: the file value divided by
// 10 (truncating), of which only the low byte is read, as a signed byte.
int8_t vec_game_coord(int16_t file_value);
