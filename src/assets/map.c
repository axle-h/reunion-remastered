#include "assets/map.h"

#include <SDL3/SDL.h>

#include "platform/fs.h"

bool map_decode(const uint8_t *data, size_t size, struct map *out)
{
    *out = (struct map){};
    if (size < 2)
        return false;
    size_t count = (size_t)data[0] * data[1];
    if (size != 2 + count)
        return false;

    out->cells = SDL_malloc(count ? count : 1);
    if (!out->cells)
        return false;
    SDL_memcpy(out->cells, data + 2, count);
    out->width = data[0];
    out->height = data[1];
    return true;
}

bool map_load(const char *dos_path, struct map *out)
{
    size_t size;
    uint8_t *data = fs_load(dos_path, &size);
    if (!data) {
        *out = (struct map){};
        return false;
    }
    bool ok = map_decode(data, size, out);
    SDL_free(data);
    return ok;
}

void map_free(struct map *map)
{
    SDL_free(map->cells);
    *map = (struct map){};
}
