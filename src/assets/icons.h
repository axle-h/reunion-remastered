// ICON\ICON.ALL: 68 icons of 40x24 pixels.
#pragma once

#include <stddef.h>
#include <stdint.h>

enum : int {
    ICON_WIDTH = 40,
    ICON_HEIGHT = 24,
    ICON_COUNT = 68,
};

typedef struct {
    // Palette indices as the game's blitter draws them (the stored bytes
    // plus 2), ICON_COUNT icons of ICON_WIDTH * ICON_HEIGHT bytes, row-major.
    uint8_t *pixels;
} Icons;

bool icons_decode(const uint8_t *data, size_t size, Icons *out);
bool icons_load(const char *dos_path, Icons *out);
void icons_free(Icons *icons);

static inline const uint8_t *icons_pixels(const Icons *icons, int n)
{
    return icons->pixels + (size_t)n * ICON_WIDTH * ICON_HEIGHT;
}
