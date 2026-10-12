#include "platform/fs.h"

#include <SDL3/SDL.h>

#include "platform/gamedata.h"

static char *root;

bool fs_init(const char *data_dir)
{
    SDL_free(root);
    root = SDL_strdup(data_dir);
    SDL_PathInfo info;
    return root && SDL_GetPathInfo(root, &info) && info.type == SDL_PATHTYPE_DIRECTORY;
}

const char *fs_root(void)
{
    return root;
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
        // The upper-case name first (the DOS names, as the import writes
        // them), then the name as given; otherwise scan the directory
        // ignoring case.
        char *next;
        SDL_asprintf(&next, "%s/%s", path, part);
        for (char *c = next ? next + SDL_strlen(path) + 1 : nullptr; c && *c; c++)
            *c = (char)SDL_toupper((unsigned char)*c);
        if (next && !SDL_GetPathInfo(next, nullptr)) {
            SDL_free(next);
            SDL_asprintf(&next, "%s/%s", path, part);
        }
        if (!next || !SDL_GetPathInfo(next, nullptr)) {
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
#ifndef NDEBUG
    if (!gamedata_listed(dos_path))
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s is not on the list of game files "
                    "(src/platform/gamefiles.c): an import won't have it", dos_path);
#endif
    size_t len = 0;
    uint8_t *data = SDL_LoadFile(path, &len);
    SDL_free(path);
    if (data && size)
        *size = len;
    return data;
}
