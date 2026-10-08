#include "assets/text.h"

#include <SDL3/SDL.h>

#include "platform/fs.h"

static const struct {
    int slot_size, slots;
} raw_layout[] = {
    [TEXT_RAW_FACE] = {41, 4},
    [TEXT_RAW_TALAL] = {31, 7},
    [TEXT_RAW_FAJ] = {36, 12},
};

void text_decode_line(const uint8_t *data, size_t size, int line, char out[TEXT_LINE_MAX])
{
    out[0] = '\0';
    if (line < 1)
        return;

    // Lines end with CR LF; the CR LF itself is not obfuscated.
    size_t pos = 0;
    for (int n = 1; n < line; n++) {
        while (pos + 1 < size && !(data[pos] == '\r' && data[pos + 1] == '\n'))
            pos++;
        if (pos + 1 >= size)
            return;
        pos += 2;
    }

    size_t len = 0;
    while (pos + len < size && data[pos + len] != '\r' && len < TEXT_LINE_MAX - 1) {
        int i = (int)len + 1;
        out[len] = (char)((data[pos + len] + 0x5780 - i * 0x4F) % 0xE0 - 0x20);
        len++;
    }
    out[len] = '\0';
}

bool text_read_line(const char *dos_path, int line, char out[TEXT_LINE_MAX])
{
    size_t size;
    uint8_t *data = fs_load(dos_path, &size);
    if (!data) {
        out[0] = '\0';
        return false;
    }
    text_decode_line(data, size, line, out);
    SDL_free(data);
    return true;
}

bool text_raw_decode(const uint8_t *data, size_t size, enum text_raw_kind kind, struct text_raw *out)
{
    *out = (struct text_raw){};
    if (kind > TEXT_RAW_FAJ)
        return false;
    int slot_size = raw_layout[kind].slot_size;
    int slots = raw_layout[kind].slots;
    size_t record_size = (size_t)slot_size * slots;
    if (size == 0 || size % record_size)
        return false;
    for (size_t off = 0; off < size; off += slot_size) {
        if (data[off] >= slot_size)
            return false;
    }

    out->data = SDL_malloc(size);
    if (!out->data)
        return false;
    SDL_memcpy(out->data, data, size);
    out->size = size;
    out->slot_size = slot_size;
    out->slots = slots;
    out->records = (int)(size / record_size);
    return true;
}

bool text_raw_load(const char *dos_path, enum text_raw_kind kind, struct text_raw *out)
{
    size_t size;
    uint8_t *data = fs_load(dos_path, &size);
    if (!data) {
        *out = (struct text_raw){};
        return false;
    }
    bool ok = text_raw_decode(data, size, kind, out);
    SDL_free(data);
    return ok;
}

void text_raw_free(struct text_raw *raw)
{
    SDL_free(raw->data);
    *raw = (struct text_raw){};
}

bool text_raw_get(const struct text_raw *raw, int record, int slot, char out[TEXT_RAW_MAX])
{
    out[0] = '\0';
    if (record < 0 || record >= raw->records || slot < 0 || slot >= raw->slots)
        return false;
    const uint8_t *s = raw->data + ((size_t)record * raw->slots + slot) * raw->slot_size;
    SDL_memcpy(out, s + 1, s[0]);
    out[s[0]] = '\0';
    return true;
}
