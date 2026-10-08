#include <SDL3/SDL.h>

#include "assets/animdef.h"
#include "platform/fs.h"

bool animdef_decode(const uint8_t *data, size_t size, AnimDef *out)
{
    *out = (AnimDef){};
    if (size == 0 || size % ANIMDEF_SCRIPT_SIZE != 0) {
        return false;
    }
    int count = (int)(size / ANIMDEF_SCRIPT_SIZE);
    AnimDefScript *scripts = SDL_calloc(count, sizeof *scripts);
    if (!scripts) {
        return false;
    }
    for (int s = 0; s < count; s++) {
        const uint8_t *p = data + (size_t)s * ANIMDEF_SCRIPT_SIZE;
        AnimDefScript *script = &scripts[s];
        for (int e = 0; e < ANIMDEF_MAX_STEPS; e++, p += 4) {
            script->steps[e] = (AnimDefStep){p[0], p[1], p[2], p[3]};
            script->num_steps = e + 1;
            if (p[3] == ANIMDEF_END) {
                break;
            }
        }
    }
    out->num_scripts = count;
    out->scripts = scripts;
    return true;
}

bool animdef_load(const char *dos_path, AnimDef *out)
{
    size_t size;
    uint8_t *data = fs_load(dos_path, &size);
    if (!data) {
        *out = (AnimDef){};
        return false;
    }
    bool ok = animdef_decode(data, size, out);
    SDL_free(data);
    return ok;
}

void animdef_free(AnimDef *def)
{
    SDL_free(def->scripts);
    *def = (AnimDef){};
}
