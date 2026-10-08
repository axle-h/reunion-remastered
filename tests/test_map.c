// map: dimensions and cells of planet maps, checked against tools/map.py.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "assets/map.h"
#include "platform/fs.h"

static const struct {
    const char *path;
    int width, height;
    uint32_t crc;
} cases[] = {
    {"MAP\\MAP1_0.MAP", 48, 48, 231954271},
    {"MAP\\MAP8_0.MAP", 86, 60, 3761725940},
    {"MAP\\MAP10_0.MAP", 40, 30, 3533280138},
};

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }

    bool ok = true;
    for (size_t i = 0; i < SDL_arraysize(cases); i++) {
        struct map m;
        bool good = map_load(cases[i].path, &m) && m.width == cases[i].width &&
                    m.height == cases[i].height &&
                    SDL_crc32(0, m.cells, (size_t)m.width * m.height) == cases[i].crc;
        if (!good)
            printf("FAILED %s\n", cases[i].path);
        ok = ok && good;
        map_free(&m);
    }

    struct map m;
    ok = ok && !map_load("MAP\\NOSUCH.MAP", &m);
    const uint8_t short_body[] = {2, 2, 1, 2, 3};
    ok = ok && !map_decode(short_body, sizeof short_body, &m);

    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
