// .PIC images ("SpidyGfx").
#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int width;
    int height;
    uint8_t *pixels;           // width * height palette indices
    uint8_t palette[256][3];   // 6-bit VGA values (0..63)
} Pic;

// Decodes a picture with its own palette. A header width that makes rows
// overrun (INFO/INFO26.PIC) is corrected to width + 1.
bool pic_decode(const uint8_t *data, size_t size, Pic *out);
bool pic_load(const char *dos_path, Pic *out);
void pic_free(Pic *pic);

// Reads a palette from a .PIC image or a bare 768-byte file (CREDITS/PAL3.PIC).
bool pic_palette_decode(const uint8_t *data, size_t size, uint8_t palette[256][3]);
bool pic_palette_load(const char *dos_path, uint8_t palette[256][3]);

// Options of the game's loader (405f:0bef), minus the fade.
typedef struct {
    int rows;                 // rows to decode, clipped to the picture height
    size_t start;             // offset in dest of the first pixel (y * stride + x)
    uint8_t colour_offset;    // added to pixels and to palette positions
    uint8_t (*palette)[3];    // caller's 256-entry palette, or nullptr
    int first_colour;         // file palette range copied to palette[colour_offset + i]
    int last_colour;
} PicOptions;

// Decodes into dest as one linear stream of width * rows bytes, like the game
// (rows never skip to a next screen line). Writes past dest_size are dropped.
// width and height (may be null) receive the picture size.
bool pic_decode_into(const uint8_t *data, size_t size, uint8_t *dest, size_t dest_size,
                     const PicOptions *opt, int *width, int *height);
bool pic_load_into(const char *dos_path, uint8_t *dest, size_t dest_size,
                   const PicOptions *opt, int *width, int *height);
