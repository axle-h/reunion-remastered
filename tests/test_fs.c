// fs: case-insensitive lookup of DOS paths in the game data.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "platform/fs.h"

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }

    size_t size = 0;
    uint8_t *a = fs_load("grafika\\main.pic", &size);
    uint8_t *b = fs_load("GRAFIKA/MAIN.PIC", nullptr);
    bool ok = a && b && size > 12 && SDL_memcmp(a, "SpidyGfx", 8) == 0;
    ok = ok && fs_load("GRAFIKA\\NOSUCH.PIC", nullptr) == nullptr;
    SDL_free(a);
    SDL_free(b);
    fs_shutdown();

    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
