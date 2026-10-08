// smp: sample rate and PCM of a few sound effects, checked against tools/smp.py.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "assets/smp.h"
#include "platform/fs.h"

static const struct {
    const char *path;
    uint32_t rate;
    size_t length;
    uint32_t crc;
} cases[] = {
    {"SOUND\\SELECT1.SMP", 15384, 24997, 1736757955},
    {"SOUND2\\WARSND1.SMP", 13157, 7798, 1939778966},
    {"SOUND3\\GRSND1.SMP", 11111, 13459, 3639474080},
};

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }

    bool ok = true;
    for (size_t i = 0; i < SDL_arraysize(cases); i++) {
        struct smp s;
        bool good = smp_load(cases[i].path, &s) && s.rate == cases[i].rate &&
                    s.length == cases[i].length &&
                    SDL_crc32(0, s.samples, s.length) == cases[i].crc;
        if (!good)
            printf("FAILED %s\n", cases[i].path);
        ok = ok && good;
        smp_free(&s);
    }

    struct smp s;
    ok = ok && !smp_load("SOUND\\NOSUCH.SMP", &s);
    const uint8_t bad_term[] = {1, 3, 0, 0, 174, 0, 0x80, 1};
    ok = ok && !smp_decode(bad_term, sizeof bad_term, &s);
    const uint8_t tiny[] = {1, 3, 0, 0, 174, 0, 0x80, 0};
    ok = ok && smp_decode(tiny, sizeof tiny, &s) && s.length == 1 && s.rate == 12195;
    smp_free(&s);

    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
