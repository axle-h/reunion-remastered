// vec: all 35 models against tools/vec.py (counts and CRC32 of the parsed data).
#include <SDL3/SDL.h>
#include <stdio.h>

#include "assets/vec.h"
#include "platform/fs.h"

static const struct {
    int number, objects, vertices, edges, faces;
    uint32_t crc;
} expected[] = {
    {1, 5, 80, 120, 50, 0xbcd91f11u},
    {2, 14, 136, 204, 96, 0x74b218ffu},
    {3, 1, 25, 48, 25, 0x8184e9c0u},
    {4, 7, 73, 116, 57, 0x72e930f3u},
    {5, 5, 56, 88, 42, 0xbe5cdcabu},
    {6, 4, 30, 47, 25, 0xd0e9285bu},
    {7, 2, 85, 168, 87, 0x8ede61a5u},
    {8, 11, 92, 140, 70, 0xc4ccc66cu},
    {9, 5, 78, 130, 62, 0x35c12b29u},
    {10, 13, 98, 148, 76, 0xf720915eu},
    {11, 13, 186, 283, 123, 0xc93a1b20u},
    {12, 7, 109, 180, 85, 0xb43564c0u},
    {13, 12, 108, 168, 84, 0xd3ede182u},
    {14, 14, 140, 216, 104, 0xd712d708u},
    {15, 4, 32, 48, 24, 0x6f3e6aebu},
    {16, 11, 88, 134, 68, 0x46cefb9bu},
    {17, 3, 74, 124, 56, 0x5e62f6f1u},
    {18, 5, 80, 116, 50, 0xabdc77ccu},
    {19, 7, 106, 171, 79, 0x2db5801cu},
    {20, 2, 61, 108, 51, 0xcd473c9au},
    {21, 6, 44, 66, 34, 0x95d68343u},
    {22, 4, 28, 42, 22, 0xc32bcc2cu},
    {23, 7, 56, 89, 47, 0xdd5b3813u},
    {24, 11, 136, 204, 90, 0x177a0f23u},
    {25, 17, 169, 248, 113, 0x0dddeee6u},
    {26, 15, 110, 150, 70, 0x2e084cbdu},
    {27, 5, 146, 222, 85, 0xc266426bu},
    {28, 6, 129, 228, 111, 0x5eee656eu},
    {29, 5, 76, 123, 57, 0xeb5b4125u},
    {30, 1, 5, 8, 5, 0x05d0d550u},
    {31, 5, 109, 180, 81, 0xc3b8b8a4u},
    {32, 11, 106, 159, 75, 0xb093c94au},
    {33, 10, 90, 141, 69, 0x2a38ef65u},
    {34, 5, 203, 412, 220, 0xc2fe7868u},
    {35, 1, 183, 380, 200, 0x01298411u},
};

// Vertices as 3 LE words, edges as bytes, faces as count + indices.
static uint32_t model_crc(const VecModel *m)
{
    uint32_t crc = 0;
    for (int i = 0; i < m->num_objects; i++) {
        const VecObject *o = &m->objects[i];
        for (int v = 0; v < o->num_vertices; v++) {
            uint8_t b[6] = {
                (uint8_t)o->vertices[v].x, (uint8_t)(o->vertices[v].x >> 8),
                (uint8_t)o->vertices[v].y, (uint8_t)(o->vertices[v].y >> 8),
                (uint8_t)o->vertices[v].z, (uint8_t)(o->vertices[v].z >> 8)};
            crc = SDL_crc32(crc, b, 6);
        }
        crc = SDL_crc32(crc, o->edges, 2 * (size_t)o->num_edges);
        for (int f = 0; f < o->num_faces; f++) {
            crc = SDL_crc32(crc, &o->faces[f], 1u + o->faces[f].count);
        }
    }
    return crc;
}

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAILED line %d: %s\n", __LINE__, #cond);               \
            failures++;                                                    \
        }                                                                  \
    } while (0)

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }

    int failures = 0;
    int totals[4] = {};
    VecModel models[36] = {};
    for (size_t i = 0; i < SDL_arraysize(expected); i++) {
        char path[32];
        SDL_snprintf(path, sizeof path, "VECTORS\\V%d.VEC", expected[i].number);
        VecModel *m = &models[expected[i].number];
        if (!vec_load(path, m)) {
            printf("FAILED to load %s\n", path);
            failures++;
            continue;
        }
        int v = 0, e = 0, f = 0;
        bool sizes = true;
        for (int k = 0; k < m->num_objects; k++) {
            v += m->objects[k].num_vertices;
            e += m->objects[k].num_edges;
            f += m->objects[k].num_faces;
            sizes = sizes && m->objects[k].size_ok;
        }
        if (m->num_objects != expected[i].objects || v != expected[i].vertices ||
            e != expected[i].edges || f != expected[i].faces ||
            model_crc(m) != expected[i].crc || !sizes) {
            printf("FAILED %s: %d/%d/%d/%d crc %08x\n", path, m->num_objects, v, e, f, model_crc(m));
            failures++;
        }
        totals[0] += m->num_objects;
        totals[1] += v;
        totals[2] += e;
        totals[3] += f;
    }
    CHECK(totals[0] == 254 && totals[1] == 3327 && totals[2] == 5409 && totals[3] == 2593);

    // The overflowing sphere: object 5 of V34 and the only object of V35.
    CHECK(models[34].num_objects == 5 && models[34].objects[4].num_vertices == 183);
    CHECK(models[34].objects[4].num_edges == 380 && models[34].objects[4].num_faces == 200);
    CHECK(models[35].num_objects == 1 && models[35].objects[0].num_faces == 200);
    // V27: a face with all 19 indices and no terminator.
    CHECK(models[27].objects[0].faces[9].count == VEC_MAX_FACE_VERTS);

    CHECK(vec_game_coord(1024) == 102 && vec_game_coord(-1024) == -102 && vec_game_coord(-9) == 0);

    // Malformed input is rejected.
    static const uint8_t empty[] = {0xFF, 0xFF};
    static const uint8_t cut[] = {0x10, 0x00, 0x01, 0x00};
    static const uint8_t trailing[] = {0xFF, 0xFF, 0x00};
    VecModel m;
    CHECK(vec_decode(empty, sizeof empty, &m) && m.num_objects == 0);
    vec_free(&m);
    CHECK(!vec_decode(cut, sizeof cut, &m));
    CHECK(!vec_decode(trailing, sizeof trailing, &m));
    CHECK(!vec_decode(empty, 1, &m));

    for (int i = 1; i <= 35; i++) {
        vec_free(&models[i]);
    }
    fs_shutdown();
    printf("%s\n", failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
