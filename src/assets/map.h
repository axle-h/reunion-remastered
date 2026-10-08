// .MAP planet-surface terrain grids (tile indices; see docs/formats/map.md).
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
