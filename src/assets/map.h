// .MAP planet-surface terrain grids, MAP\MAP<surface type>_<variant>.MAP:
// byte width, byte height, then width * height tile indices, row-major (no
// header beyond that). Shipped maps run from 20x20 to 86x60.
#pragma once

#include <stddef.h>
#include <stdint.h>

struct map {
    int width;
    int height;
    uint8_t *cells; // row-major, width * height tile indices
};

bool map_decode(const uint8_t *data, size_t size, struct map *out);
bool map_load(const char *dos_path, struct map *out);
void map_free(struct map *map);
