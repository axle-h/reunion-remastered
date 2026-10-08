#include "assets/pic.h"

#include <SDL3/SDL.h>

#include "platform/fs.h"

enum : size_t {
    HEADER_SIZE = 12,
    PALETTE_SIZE = 768,
};

static const uint8_t MAGIC[8] = {'S','p','i','d','y','G','f','x'};

static bool valid(const uint8_t *data, size_t size)
{
    return data && size >= HEADER_SIZE + PALETTE_SIZE + 1 && SDL_memcmp(data, MAGIC, 8) == 0;
}

// Runs the game's RLE loop. Output past `cap` is dropped. Returns false if the
// data ends early; *overruns counts rows whose last run crossed the width.
static bool decode_rows(const uint8_t *data, size_t size, int width, int rows,
                        uint8_t *out, size_t cap, uint8_t offset, int *overruns)
{
    size_t pos = HEADER_SIZE;
    size_t dst = 0;
    *overruns = 0;
    for (int y = 0; y < rows; y++) {
        int col = 0;
        while (col < width) {
            if (pos >= size) {
                return false;
            }
            uint8_t b = data[pos++];
            int count = 1;
            if (b >= 0xC0) {
                count = b & 0x3F;
                if (pos >= size) {
                    return false;
                }
                b = data[pos++];
            }
            if (out) {
                for (int i = 0; i < count && dst + i < cap; i++) {
                    out[dst + i] = (uint8_t)(b + offset);
                }
            }
            dst += count;
            col += count;
        }
        *overruns += col != width;
    }
    return true;
}

// INFO/INFO26.PIC says 191 wide but stores 192.
static int effective_width(const uint8_t *data, size_t size, int width, int height)
{
    int overruns;
    if (decode_rows(data, size, width, height, nullptr, 0, 0, &overruns) && overruns == 0) {
        return width;
    }
    int fixed;
    if (decode_rows(data, size, width + 1, height, nullptr, 0, 0, &fixed) && fixed == 0) {
        return width + 1;
    }
    return width;
}

static void read_palette(const uint8_t *raw, uint8_t palette[256][3])
{
    for (int i = 0; i < 256; i++) {
        for (int c = 0; c < 3; c++) {
            palette[i][c] = raw[i * 3 + c] >> 2;
        }
    }
}

bool pic_decode(const uint8_t *data, size_t size, Pic *out)
{
    SDL_zerop(out);
    if (!valid(data, size)) {
        return false;
    }
    int width = data[8] | data[9] << 8;
    int height = data[10] | data[11] << 8;
    width = effective_width(data, size, width, height);

    size_t count = (size_t)width * height;
    uint8_t *pixels = SDL_calloc(count ? count : 1, 1);
    if (!pixels) {
        return false;
    }
    int overruns;
    if (!decode_rows(data, size, width, height, pixels, count, 0, &overruns)) {
        SDL_free(pixels);
        return false;
    }
    out->width = width;
    out->height = height;
    out->pixels = pixels;
    read_palette(data + size - PALETTE_SIZE, out->palette);
    return true;
}

bool pic_load(const char *dos_path, Pic *out)
{
    size_t size;
    uint8_t *data = fs_load(dos_path, &size);
    if (!data) {
        SDL_zerop(out);
        return false;
    }
    bool ok = pic_decode(data, size, out);
    SDL_free(data);
    return ok;
}

void pic_free(Pic *pic)
{
    SDL_free(pic->pixels);
    SDL_zerop(pic);
}

bool pic_palette_decode(const uint8_t *data, size_t size, uint8_t palette[256][3])
{
    if (valid(data, size)) {
        read_palette(data + size - PALETTE_SIZE, palette);
        return true;
    }
    if (data && size == PALETTE_SIZE) {
        read_palette(data, palette);
        return true;
    }
    return false;
}

bool pic_palette_load(const char *dos_path, uint8_t palette[256][3])
{
    size_t size;
    uint8_t *data = fs_load(dos_path, &size);
    if (!data) {
        return false;
    }
    bool ok = pic_palette_decode(data, size, palette);
    SDL_free(data);
    return ok;
}

bool pic_decode_into(const uint8_t *data, size_t size, uint8_t *dest, size_t dest_size,
                     const PicOptions *opt, int *width, int *height)
{
    if (!valid(data, size) || opt->start > dest_size) {
        return false;
    }
    int w = data[8] | data[9] << 8;
    int h = data[10] | data[11] << 8;
    w = effective_width(data, size, w, h);
    int rows = SDL_min(opt->rows, h);

    int overruns;
    if (!decode_rows(data, size, w, rows, dest + opt->start, dest_size - opt->start,
                     opt->colour_offset, &overruns)) {
        return false;
    }
    if (opt->palette) {
        uint8_t raw[256][3];
        read_palette(data + size - PALETTE_SIZE, raw);
        for (int i = opt->first_colour; i <= opt->last_colour && i < 256; i++) {
            int to = opt->colour_offset + i;
            if (i >= 0 && to < 256) {
                SDL_memcpy(opt->palette[to], raw[i], 3);
            }
        }
    }
    if (width) {
        *width = w;
    }
    if (height) {
        *height = h;
    }
    return true;
}

bool pic_load_into(const char *dos_path, uint8_t *dest, size_t dest_size,
                   const PicOptions *opt, int *width, int *height)
{
    size_t size;
    uint8_t *data = fs_load(dos_path, &size);
    if (!data) {
        return false;
    }
    bool ok = pic_decode_into(data, size, dest, dest_size, opt, width, height);
    SDL_free(data);
    return ok;
}
