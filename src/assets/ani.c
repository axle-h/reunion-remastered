#include <SDL3/SDL.h>

#include "assets/ani.h"
#include "platform/fs.h"

constexpr size_t HEADER_SIZE = 13; // magic, width, height

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(p[0] | p[1] << 8);
}

// Walks one pixel stream; with dst == nullptr it only validates. Mirrors the
// game's 16-bit countdown, which stops only at exactly 0.
static bool run_stream(const AniFrame *f, int width, int height, uint8_t *dst,
                       size_t stride, uint8_t off)
{
    const uint8_t *p = f->stream, *end = p + f->size;
    uint16_t remaining = (uint16_t)(width * height);
    size_t pos = 0, total = (size_t)width * height;

    while (true) {
        if (p >= end)
            return false;
        uint8_t b = *p++;
        size_t n;
        int value = -1;
        if (b < 0x80) {
            n = 1;
            value = b;
        } else if (b == 0x80 || b == 0xC0) {
            if (end - p < (b == 0xC0 ? 3 : 2))
                return false;
            n = rd16(p);
            p += 2;
            if (b == 0xC0)
                value = *p++;
        } else if (b < 0xC0) {
            n = b & 0x3F;
        } else {
            if (p >= end)
                return false;
            n = b & 0x3F;
            value = *p++;
        }
        if (pos + n > total)
            return false;
        if (dst && value >= 0) {
            uint8_t v = (uint8_t)(value + off);
            for (size_t i = 0; i < n; i++) {
                size_t q = pos + i;
                dst[(q / (size_t)width) * stride + q % (size_t)width] = v;
            }
        }
        pos += n;
        remaining = (uint16_t)(remaining - n);
        if (remaining == 0)
            break;
    }
    return pos == total && p == end;
}

bool ani_decode(const uint8_t *data, size_t size, Ani *out)
{
    *out = (Ani){};
    int cap = 0;
    size_t pos = 0;

    uint8_t *copy = SDL_malloc(size ? size : 1);
    if (!copy)
        return false;
    SDL_memcpy(copy, data, size);
    out->data = copy;

    while (pos < size) {
        if (size - pos < 2 + HEADER_SIZE)
            goto fail;
        size_t n = rd16(copy + pos);
        const uint8_t *blk = copy + pos + 2;
        if (size - pos - 2 < n + HEADER_SIZE || SDL_memcmp(blk, "SpidyAnim", 9) != 0)
            goto fail;
        int w = rd16(blk + 9), h = rd16(blk + 11);
        if (w == 0 || h == 0)
            goto fail;
        if (out->count == 0) {
            out->width = w;
            out->height = h;
        } else if (w != out->width || h != out->height) {
            goto fail;
        }
        AniFrame f = {blk + HEADER_SIZE, n};
        if (!run_stream(&f, w, h, nullptr, 0, 0))
            goto fail;
        if (out->count == cap) {
            cap = cap ? cap * 2 : 16;
            AniFrame *nf = SDL_realloc(out->frames, cap * sizeof *nf);
            if (!nf)
                goto fail;
            out->frames = nf;
        }
        out->frames[out->count++] = f;
        pos += 2 + n + HEADER_SIZE;
    }
    if (out->count == 0)
        goto fail;
    return true;

fail:
    ani_free(out);
    return false;
}

bool ani_load(const char *dos_path, Ani *out)
{
    size_t size;
    uint8_t *data = fs_load(dos_path, &size);
    if (!data) {
        *out = (Ani){};
        return false;
    }
    bool ok = ani_decode(data, size, out);
    SDL_free(data);
    return ok;
}

void ani_free(Ani *ani)
{
    SDL_free(ani->frames);
    SDL_free(ani->data);
    *ani = (Ani){};
}

void ani_draw_frame(const Ani *ani, int index, uint8_t *dst, size_t stride,
                    uint8_t color_offset)
{
    if (index < 0 || index >= ani->count)
        return;
    run_stream(&ani->frames[index], ani->width, ani->height, dst, stride,
               color_offset);
}
