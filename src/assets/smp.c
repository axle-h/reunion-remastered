#include "assets/smp.h"

#include <SDL3/SDL.h>

#include "platform/fs.h"

bool smp_decode(const uint8_t *data, size_t size, struct smp *out)
{
    *out = (struct smp){};
    if (size < 7 || data[0] != 1)
        return false;
    size_t block = data[1] | data[2] << 8 | (size_t)data[3] << 16;
    uint8_t tc = data[4];
    // Only codec 0 (8-bit PCM) exists; tc 255 would divide by one.
    if (data[5] != 0 || block < 2 || 4 + block + 1 != size || tc == 255 || data[4 + block] != 0)
        return false;

    out->length = block - 2;
    out->samples = SDL_malloc(out->length ? out->length : 1);
    if (!out->samples)
        return false;
    SDL_memcpy(out->samples, data + 6, out->length);
    out->rate = 1000000 / (256 - tc);
    return true;
}

bool smp_load(const char *dos_path, struct smp *out)
{
    size_t size;
    uint8_t *data = fs_load(dos_path, &size);
    if (!data) {
        *out = (struct smp){};
        return false;
    }
    bool ok = smp_decode(data, size, out);
    SDL_free(data);
    return ok;
}

void smp_free(struct smp *smp)
{
    SDL_free(smp->samples);
    *smp = (struct smp){};
}
