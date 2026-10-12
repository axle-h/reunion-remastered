// icons: ICON\ICON.ALL against a reference decoder.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "assets/icons.h"
#include "platform/fs.h"

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAILED line %d: %s\n", __LINE__, #cond);               \
            failures++;                                                    \
        }                                                                  \
    } while (0)

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }

    int failures = 0;
    Icons icons;
    if (!icons_load("ICON\\ICON.ALL", &icons)) {
        printf("FAILED to load\n");
        return 1;
    }
    CHECK(SDL_crc32(0, icons.pixels, ICON_COUNT * ICON_WIDTH * ICON_HEIGHT) == 3997745984u);
    static const struct {
        int n;
        uint32_t crc;
    } each[] = {{0, 0xded9a948u}, {1, 0x49bae9adu}, {33, 0x256eb2e8u}, {67, 0x6028e03eu}};
    for (size_t i = 0; i < SDL_arraysize(each); i++) {
        CHECK(SDL_crc32(0, icons_pixels(&icons, each[i].n), ICON_WIDTH * ICON_HEIGHT) == each[i].crc);
    }
    icons_free(&icons);

    // Synthetic chunk: 15 runs of 63 zeros, then a run of 20 that overshoots
    // the 960 pixels; the last run is FFh, which is palette index 1.
    uint8_t chunk[2 + 2 * 16] = {32, 0};
    for (int i = 0; i < 15; i++) {
        chunk[2 + 2 * i] = 0xC0 | 63;
        chunk[3 + 2 * i] = 0;
    }
    chunk[32] = 0xC0 | 20;
    chunk[33] = 0xFF;
    uint8_t *data = SDL_malloc(ICON_COUNT * sizeof chunk);
    for (int i = 0; i < ICON_COUNT; i++) {
        SDL_memcpy(data + i * sizeof chunk, chunk, sizeof chunk);
    }
    CHECK(icons_decode(data, ICON_COUNT * sizeof chunk, &icons));
    CHECK(icons_pixels(&icons, 67)[0] == 2 && icons_pixels(&icons, 67)[944] == 2 &&
          icons_pixels(&icons, 67)[945] == 1 && icons_pixels(&icons, 67)[959] == 1);
    icons_free(&icons);
    CHECK(!icons_decode(data, ICON_COUNT * sizeof chunk - 1, &icons));
    CHECK(!icons_decode(data, ICON_COUNT * sizeof chunk + 1, &icons));
    SDL_free(data);

    fs_shutdown();
    printf("%s\n", failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
