// .ANI animations ("SpidyAnim"): delta-RLE frames back to back to the end of
// the file, each a word n, "SpidyAnim", word width, word height and n bytes
// of pixel stream. The file holds no frame count, palette or timing: the
// game's tables give those. Every shipped literal is in 00h..7Fh.
#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t *stream; // encoded pixel stream, points into Ani.data
    size_t size;
} AniFrame;

typedef struct {
    int width, height; // same for every frame
    int count;
    AniFrame *frames;
    uint8_t *data;     // backing copy of the file
} Ani;

// Parses and validates every frame. Returns false on malformed data.
bool ani_decode(const uint8_t *data, size_t size, Ani *out);
bool ani_load(const char *dos_path, Ani *out);
void ani_free(Ani *ani);

// Draws frame `index` (0-based) into dst like 3c64:057c: skipped pixels are
// left untouched and `color_offset` is added to every written value. Frame 0
// covers every pixel; delta files draw each frame over the previous one,
// keyed ones (SA*, MAIN17) draw frame 0 and then frame N.
void ani_draw_frame(const Ani *ani, int index, uint8_t *dst, size_t stride,
                    uint8_t color_offset);
