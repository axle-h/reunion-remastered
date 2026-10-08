#include "game/dos.h"

#include <SDL3/SDL.h>

// 48b9:0000
void Dos_GetEnv(uint8_t *result, const uint8_t *name)
{
    char key[32];
    uint8_t n = name[0] > 31 ? 31 : name[0];
    for (int i = 0; i < n; i++) {
        char c = (char)name[1 + i];
        key[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
    }
    key[n] = 0;

    const char *v = SDL_getenv(key);
    size_t len = v ? SDL_strlen(v) : 0;
    if (len > 255)
        len = 255;
    result[0] = (uint8_t)len;
    if (len)
        SDL_memcpy(result + 1, v, len);
}
