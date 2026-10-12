#include "platform/playerdir.h"

#include <SDL3/SDL.h>

const char *playerdir(void)
{
    static char *dir;
    static bool asked;
    if (!asked) {
        asked = true;
        dir = SDL_GetPrefPath("Reunion", "Reunion");
    }
    return dir;
}

char *playerdir_path(const char *name)
{
    const char *d = playerdir();
    char *out = nullptr;
    if (d)
        SDL_asprintf(&out, "%s%s", d, name);
    return out;
}
