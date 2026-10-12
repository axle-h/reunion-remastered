#include "platform/playerdir.h"

#include <SDL3/SDL.h>

static char *dir;
static bool dir_set;

void playerdir_set(const char *d)
{
    SDL_free(dir);
    dir = nullptr;
    dir_set = d != nullptr;
    if (!d)
        return;
    size_t n = SDL_strlen(d);
    bool slash = n > 0 && (d[n - 1] == '/' || d[n - 1] == '\\');
    SDL_asprintf(&dir, "%s%s", d, slash ? "" : "/");
    SDL_CreateDirectory(dir);
}

const char *playerdir(void)
{
    if (!dir_set) {
        dir_set = true;
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

// The pref path of another org and app: SDL_GetPrefPath would create it.
// Every platform's pref path is <base>/<org>/<app>/.
static char *sibling_pref(const char *org, const char *app)
{
    const char *d = playerdir();
    if (!d)
        return nullptr;
    char *base = SDL_strdup(d);
    for (int up = 0; up < 3; up++) {  // the separator, the app, the org
        size_t n = SDL_strlen(base);
        while (n > 0 && base[n - 1] != '/' && base[n - 1] != '\\')
            n--;
        if (up < 2 && n > 0)
            n--;
        base[n] = '\0';
    }
    char sep = d[SDL_strlen(d) - 1];
    char *out = nullptr;
    if (base[0])
        SDL_asprintf(&out, "%s%s%c%s%c", base, org, sep, app, sep);
    SDL_free(base);
    return out;
}

typedef struct Move {
    const char *to;
    int moved, kept;
} Move;

static SDL_EnumerationResult move_save(void *userdata, const char *dirname, const char *fname)
{
    Move *m = userdata;
    char *from = nullptr, *to = nullptr;
    SDL_asprintf(&from, "%s%s", dirname, fname);
    SDL_asprintf(&to, "%s%s", m->to, fname);
    if (from && to) {
        if (SDL_GetPathInfo(to, nullptr)) {
            m->kept++;
            SDL_Log("Old save %s kept: %s exists", from, to);
        } else if (SDL_RenamePath(from, to) || (SDL_CopyFile(from, to) && SDL_RemovePath(from))) {
            m->moved++;
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Moving %s: %s", from, SDL_GetError());
        }
    }
    SDL_free(from);
    SDL_free(to);
    return SDL_ENUM_CONTINUE;
}

void playerdir_migrate(void)
{
    char *old = sibling_pref("Amnesty Design", "Reunion");
    char *old_save = nullptr;
    if (old)
        SDL_asprintf(&old_save, "%ssave/", old);
    SDL_PathInfo info;
    if (old_save && SDL_GetPathInfo(old_save, &info) && info.type == SDL_PATHTYPE_DIRECTORY) {
        char *save = playerdir_path("save/");
        if (save && SDL_CreateDirectory(save)) {
            Move m = {.to = save};
            SDL_EnumerateDirectory(old_save, move_save, &m);
            if (m.moved)
                SDL_Log("Moved %d save%s from %s to %s", m.moved, m.moved == 1 ? "" : "s",
                        old_save, save);
        }
        SDL_free(save);
        // Fails, as it should, while anything is left in them.
        if (SDL_RemovePath(old_save) && SDL_RemovePath(old)) {
            char *org = SDL_strdup(old);
            size_t n = SDL_strlen(org) - 1;  // the separator
            while (n > 0 && org[n - 1] != '/' && org[n - 1] != '\\')
                n--;
            org[n] = '\0';
            SDL_RemovePath(org);
            SDL_free(org);
        }
    }
    SDL_free(old_save);
    SDL_free(old);
}
