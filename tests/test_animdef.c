// animdef: WAR\ANIM\ANIM.DEF against a reference decoder.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "assets/animdef.h"
#include "platform/fs.h"

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAILED line %d: %s\n", __LINE__, #cond);               \
            failures++;                                                    \
        }                                                                  \
    } while (0)

static const int step_counts[25] = {26, 13, 13, 31, 28, 32, 35, 32, 29, 15, 12, 20, 13,
                                    13, 6,  137, 44, 51, 78, 28, 46, 25, 68, 140, 30};

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }

    int failures = 0;
    AnimDef def;
    if (!animdef_load("WAR\\ANIM\\ANIM.DEF", &def)) {
        printf("FAILED to load\n");
        return 1;
    }
    CHECK(def.num_scripts == 25);
    uint32_t crc = 0;
    int total = 0;
    for (int s = 0; s < def.num_scripts; s++) {
        const AnimDefScript *script = &def.scripts[s];
        CHECK(script->num_steps == step_counts[s]);
        uint8_t n = (uint8_t)script->num_steps;
        crc = SDL_crc32(crc, &n, 1);
        crc = SDL_crc32(crc, script->steps, 4u * script->num_steps);
        total += script->num_steps;
    }
    CHECK(total == 965);
    CHECK(crc == 0x3f852a71u);
    // Script 24 only ends at the very last of its 140 steps.
    CHECK(def.scripts[23].num_steps == ANIMDEF_MAX_STEPS);
    CHECK(def.scripts[23].steps[139].delay == ANIMDEF_END);
    CHECK(def.scripts[0].steps[1].anim == 1 && def.scripts[0].steps[1].sound == 16);
    animdef_free(&def);

    AnimDef bad;
    uint8_t buf[ANIMDEF_SCRIPT_SIZE] = {};
    CHECK(!animdef_decode(buf, sizeof buf - 1, &bad));
    CHECK(!animdef_decode(buf, 0, &bad));
    buf[3] = ANIMDEF_END;
    CHECK(animdef_decode(buf, sizeof buf, &bad) && bad.scripts[0].num_steps == 1);
    animdef_free(&bad);

    fs_shutdown();
    printf("%s\n", failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
