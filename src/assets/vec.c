#include <SDL3/SDL.h>

#include "assets/vec.h"
#include "platform/fs.h"

enum : int {
    FACE_SIZE = 20,
    END_MARK = 0xFFFF,
};

typedef struct {
    const uint8_t *data;
    size_t size, pos;
} Reader;

static bool read_u16(Reader *r, uint16_t *v)
{
    if (r->size - r->pos < 2) {
        return false;
    }
    *v = (uint16_t)(r->data[r->pos] | r->data[r->pos + 1] << 8);
    r->pos += 2;
    return true;
}

static const uint8_t *take(Reader *r, size_t n)
{
    if (r->size - r->pos < n) {
        return nullptr;
    }
    const uint8_t *p = r->data + r->pos;
    r->pos += n;
    return p;
}

static bool read_object(Reader *r, VecObject *obj)
{
    uint16_t n;

    if (!read_u16(r, &n)) {
        return false;
    }
    obj->num_vertices = n;
    const uint8_t *p = take(r, 6u * n);
    if (!p) {
        return false;
    }
    obj->vertices = SDL_calloc(n ? n : 1, sizeof(VecVertex));
    if (!obj->vertices) {
        return false;
    }
    for (int i = 0; i < obj->num_vertices; i++, p += 6) {
        obj->vertices[i] = (VecVertex){
            (int16_t)(p[0] | p[1] << 8), (int16_t)(p[2] | p[3] << 8), (int16_t)(p[4] | p[5] << 8)};
    }

    if (!read_u16(r, &n) || !(p = take(r, 2u * n))) {
        return false;
    }
    obj->num_edges = n;
    obj->edges = SDL_calloc(n ? n : 1, sizeof(VecEdge));
    if (!obj->edges) {
        return false;
    }
    for (int i = 0; i < obj->num_edges; i++, p += 2) {
        obj->edges[i] = (VecEdge){p[0], p[1]};
        if (p[0] < 1 || p[0] > obj->num_vertices || p[1] < 1 || p[1] > obj->num_vertices) {
            return false;
        }
    }

    if (!read_u16(r, &n) || !(p = take(r, (size_t)FACE_SIZE * n))) {
        return false;
    }
    obj->num_faces = n;
    obj->faces = SDL_calloc(n ? n : 1, sizeof(VecFace));
    if (!obj->faces) {
        return false;
    }
    for (int i = 0; i < obj->num_faces; i++, p += FACE_SIZE) {
        VecFace *f = &obj->faces[i];
        // Bytes after the FFh terminator are leftovers the game never reads.
        while (f->count < VEC_MAX_FACE_VERTS && p[f->count] != 0xFF) {
            f->index[f->count] = p[f->count];
            f->count++;
        }
        if (f->count < 3) {
            return false;
        }
        for (int k = 0; k < f->count; k++) {
            if (f->index[k] < 1 || f->index[k] > obj->num_vertices) {
                return false;
            }
        }
    }

    return true;
}

bool vec_decode(const uint8_t *data, size_t size, VecModel *out)
{
    Reader r = {data, size, 0};
    VecModel m = {};
    int capacity = 0;

    for (;;) {
        uint16_t objsize;
        if (!read_u16(&r, &objsize)) {
            goto fail;
        }
        if (objsize == END_MARK) {
            break;
        }
        if (m.num_objects == VEC_MAX_OBJECTS) {
            goto fail;
        }
        if (m.num_objects == capacity) {
            capacity = capacity ? capacity * 2 : 8;
            VecObject *grown = SDL_realloc(m.objects, capacity * sizeof *grown);
            if (!grown) {
                goto fail;
            }
            m.objects = grown;
        }
        // Counted before parsing so a failed object is still freed.
        VecObject *obj = &m.objects[m.num_objects++];
        *obj = (VecObject){.size = objsize};
        size_t body_start = r.pos;
        if (!read_object(&r, obj)) {
            goto fail;
        }
        obj->size_ok = r.pos - body_start == objsize;
    }
    if (r.pos != size) {
        goto fail;
    }
    *out = m;
    return true;

fail:
    vec_free(&m);
    *out = (VecModel){};
    return false;
}

bool vec_load(const char *dos_path, VecModel *out)
{
    size_t size;
    uint8_t *data = fs_load(dos_path, &size);
    if (!data) {
        *out = (VecModel){};
        return false;
    }
    bool ok = vec_decode(data, size, out);
    SDL_free(data);
    return ok;
}

void vec_free(VecModel *model)
{
    for (int i = 0; i < model->num_objects; i++) {
        SDL_free(model->objects[i].vertices);
        SDL_free(model->objects[i].edges);
        SDL_free(model->objects[i].faces);
    }
    SDL_free(model->objects);
    *model = (VecModel){};
}

int8_t vec_game_coord(int16_t file_value)
{
    return (int8_t)(file_value / 10);
}
