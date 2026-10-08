// ani: frame counts, sizes and decoded pixels (delta and keyed drawing)
// checked against tools/ani.py.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "assets/ani.h"
#include "platform/fs.h"

constexpr uint8_t OFFSET = 0x40;

typedef struct {
    const char *path;
    int frames, w, h;
    uint32_t first;      // CRC of frame 1
    uint32_t last_delta; // last frame drawn over the previous one
    uint32_t last_keyed; // last frame drawn over frame 1
} Case;

static const Case cases[] = {
    {"ANIM\\MAIN3.ANI", 13, 44, 66, 1910900127u, 2260531314u, 1910900127u},
    {"ANIM\\MAIN17.ANI", 23, 100, 49, 1174378917u, 3803156219u, 2350833570u},
    {"ANIM\\MAIN13.ANI", 68, 320, 200, 2736053986u, 970673626u, 4134836992u},
    {"WAR\\WAR\\SA10.ANI", 13, 158, 149, 1824554735u, 394802645u, 3336632857u},
    {"WAR\\GRWAR\\GRANIM1.ANI", 88, 58, 80, 1635749981u, 338426420u, 1635749981u},
    {"INTRO\\ANIM3.ANI", 26, 320, 200, 987153005u, 3231186794u, 1778370127u},
    {"CREDITS\\ANIM3.ANI", 300, 320, 200, 3083141438u, 3083141438u, 3083141438u},
    {"VICTORY\\ANIM1.ANI", 19, 320, 200, 3262645695u, 1071426572u, 705479461u},
};

// Draws with a padded stride, then packs the rows for the CRC.
static uint32_t crc_of(const uint8_t *buf, size_t stride, int w, int h)
{
    uint8_t *packed = SDL_malloc((size_t)w * h);
    for (int y = 0; y < h; y++)
        SDL_memcpy(packed + (size_t)y * w, buf + y * stride, w);
    uint32_t crc = SDL_crc32(0, packed, (size_t)w * h);
    SDL_free(packed);
    return crc;
}

static bool check(const Case *c)
{
    Ani ani;
    if (!ani_load(c->path, &ani)) {
        printf("%s: load failed\n", c->path);
        return false;
    }
    bool ok = ani.count == c->frames && ani.width == c->w && ani.height == c->h;
    size_t stride = (size_t)c->w + 5;
    uint8_t *buf = SDL_calloc(stride * c->h, 1);
    uint8_t *key = SDL_calloc(stride * c->h, 1);

    ani_draw_frame(&ani, 0, buf, stride, OFFSET);
    ok = ok && crc_of(buf, stride, c->w, c->h) == c->first;
    SDL_memcpy(key, buf, stride * c->h);
    for (int i = 1; ok && i < ani.count; i++)
        ani_draw_frame(&ani, i, buf, stride, OFFSET);
    ok = ok && crc_of(buf, stride, c->w, c->h) == c->last_delta;
    ani_draw_frame(&ani, ani.count - 1, key, stride, OFFSET);
    ok = ok && crc_of(key, stride, c->w, c->h) == c->last_keyed;

    if (!ok)
        printf("%s: mismatch (%d frames %dx%d)\n", c->path, ani.count, ani.width, ani.height);
    SDL_free(buf);
    SDL_free(key);
    ani_free(&ani);
    return ok;
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }

    bool ok = true;
    for (size_t i = 0; i < SDL_arraysize(cases); i++)
        ok = check(&cases[i]) && ok;

    // Malformed input: bad magic, truncation, stray trailing byte.
    Ani ani;
    size_t size;
    uint8_t *data = fs_load("ANIM\\MAIN3.ANI", &size);
    ok = ok && data && ani_decode(data, size, &ani);
    ani_free(&ani);
    ok = ok && !ani_decode(data, size - 1, &ani);
    ok = ok && !ani_decode(data, 0, &ani);
    uint8_t *bad = SDL_malloc(size);
    SDL_memcpy(bad, data, size);
    bad[2] = 'X';
    ok = ok && !ani_decode(bad, size, &ani);
    SDL_free(bad);
    SDL_free(data);
    ok = ok && !ani_load("ANIM\\NOSUCH.ANI", &ani);

    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
