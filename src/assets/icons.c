#include <SDL3/SDL.h>

#include "assets/icons.h"
#include "platform/fs.h"

enum : int {
    ICON_PIXELS = ICON_WIDTH * ICON_HEIGHT,
    BIAS = 2,
};

static bool unrle(const uint8_t *chunk, size_t len, uint8_t *dst)
{
    int n = 0;
    size_t q = 0;
    while (n < ICON_PIXELS) {
        if (q >= len) {
            return false;
        }
        uint8_t b = chunk[q++];
        int run = 1;
        if (b >= 0xC0) {
            if (q >= len) {
                return false;
            }
            run = b & 0x3F;
            b = chunk[q++];
        }
        // The last run overshoots the icon in every chunk; the excess is dropped.
        for (; run > 0 && n < ICON_PIXELS; run--) {
            dst[n++] = (uint8_t)(b + BIAS);
        }
    }
    return true;
}

bool icons_decode(const uint8_t *data, size_t size, Icons *out)
{
    out->pixels = SDL_malloc((size_t)ICON_COUNT * ICON_PIXELS);
    if (!out->pixels) {
        return false;
    }
    size_t p = 0;
    for (int i = 0; i < ICON_COUNT; i++) {
        if (size - p < 2) {
            goto fail;
        }
        size_t len = data[p] | data[p + 1] << 8;
        p += 2;
        if (size - p < len || !unrle(data + p, len, out->pixels + (size_t)i * ICON_PIXELS)) {
            goto fail;
        }
        p += len;
    }
    if (p != size) {
        goto fail;
    }
    return true;

fail:
    icons_free(out);
    return false;
}

bool icons_load(const char *dos_path, Icons *out)
{
    size_t size;
    uint8_t *data = fs_load(dos_path, &size);
    if (!data) {
        *out = (Icons){};
        return false;
    }
    bool ok = icons_decode(data, size, out);
    SDL_free(data);
    return ok;
}

void icons_free(Icons *icons)
{
    SDL_free(icons->pixels);
    icons->pixels = nullptr;
}
