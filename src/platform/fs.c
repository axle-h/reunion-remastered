#include "platform/fs.h"

#include <SDL3/SDL.h>

static char *root;

bool fs_init(const char *data_dir)
{
    SDL_free(root);
    root = SDL_strdup(data_dir);
    SDL_PathInfo info;
    return root && SDL_GetPathInfo(root, &info) && info.type == SDL_PATHTYPE_DIRECTORY;
}

void fs_shutdown(void)
{
    SDL_free(root);
    root = nullptr;
}

struct match {
    const char *want;
    char *found;
};

static SDL_EnumerationResult find_entry(void *userdata, const char *dirname, const char *fname)
{
    (void)dirname;
    struct match *m = userdata;
    if (SDL_strcasecmp(fname, m->want) == 0) {
        m->found = SDL_strdup(fname);
        return SDL_ENUM_SUCCESS;
    }
    return SDL_ENUM_CONTINUE;
}

char *fs_resolve(const char *dos_path)
{
    if (!root)
        return nullptr;

    char *path = SDL_strdup(root);
    char *rest = SDL_strdup(dos_path);
    char *save = nullptr;
    for (char *part = SDL_strtok_r(rest, "\\/", &save); part && path;
         part = SDL_strtok_r(nullptr, "\\/", &save)) {
        // Exact name first (cheap); otherwise scan the directory ignoring case.
        char *next;
        SDL_asprintf(&next, "%s/%s", path, part);
        SDL_PathInfo info;
        if (!next || !SDL_GetPathInfo(next, &info)) {
            SDL_free(next);
            struct match m = { part, nullptr };
            SDL_EnumerateDirectory(path, find_entry, &m);
            next = nullptr;
            if (m.found)
                SDL_asprintf(&next, "%s/%s", path, m.found);
            SDL_free(m.found);
        }
        SDL_free(path);
        path = next;
    }
    SDL_free(rest);
    return path;
}

uint8_t *fs_load(const char *dos_path, size_t *size)
{
    char *path = fs_resolve(dos_path);
    if (!path)
        return nullptr;
    size_t len = 0;
    uint8_t *data = SDL_LoadFile(path, &len);
    SDL_free(path);
    if (data && size)
        *size = len;
    return data;
}
