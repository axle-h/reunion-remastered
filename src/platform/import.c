#include "platform/import.h"

#include <SDL3/SDL.h>

#include "platform/source.h"

// In data/: the import finished. Not a DOS file, so the game never asks
// for it.
static const char MARKER[] = "import.ini";

static void problem(ImportReport *r, const char *what, const char *path)
{
    char **p = SDL_realloc(r->problems, (size_t)(r->problem_count + 1) * sizeof *p);
    if (!p)
        return;
    r->problems = p;
    p[r->problem_count] = nullptr;
    SDL_asprintf(&p[r->problem_count], "%s: %s", what, path);
    if (p[r->problem_count])
        r->problem_count++;
}

static bool fail(ImportReport *r, const char *fmt, const char *arg)
{
    SDL_free(r->error);
    r->error = nullptr;
    SDL_asprintf(&r->error, fmt, arg, SDL_GetError());
    return false;
}

static void step(ImportProgress *progress)
{
    if (progress)
        SDL_AddAtomicInt(&progress->done, 1);
}

// --- Folders ------------------------------------------------------------------------

typedef struct Names {
    char **names;
    int count;
} Names;

static SDL_EnumerationResult collect(void *userdata, const char *dirname, const char *fname)
{
    (void)dirname;
    Names *n = userdata;
    char **grown = SDL_realloc(n->names, (size_t)(n->count + 1) * sizeof *grown);
    if (!grown)
        return SDL_ENUM_FAILURE;
    n->names = grown;
    n->names[n->count] = SDL_strdup(fname);
    if (n->names[n->count])
        n->count++;
    return SDL_ENUM_CONTINUE;
}

// Removes `path` and, if it is a folder, everything in it.
static void remove_tree(const char *path)
{
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(path, &info))
        return;
    if (info.type == SDL_PATHTYPE_DIRECTORY) {
        Names n = {0};
        SDL_EnumerateDirectory(path, collect, &n);
        for (int i = 0; i < n.count; i++) {
            char *child = nullptr;
            SDL_asprintf(&child, "%s/%s", path, n.names[i]);
            if (child)
                remove_tree(child);
            SDL_free(child);
            SDL_free(n.names[i]);
        }
        SDL_free(n.names);
    }
    SDL_RemovePath(path);
}

// Writes `data` to dir/DOS\PATH, creating the folders on the way.
static bool write_file(const char *dir, const char *dos_path, const void *data, size_t size)
{
    char *path = nullptr;
    SDL_asprintf(&path, "%s/%s", dir, dos_path);
    if (!path)
        return false;
    for (char *c = path + SDL_strlen(dir) + 1; *c; c++) {
        if (*c == '\\' || *c == '/') {
            *c = '\0';
            SDL_CreateDirectory(path);
            *c = '/';
        }
    }
    bool ok = SDL_SaveFile(path, data, size);
    SDL_free(path);
    return ok;
}

// --- The import -------------------------------------------------------------------

// The entries for one path: files[first .. end - 1].
static int group_end(const GameFile *files, int count, int first)
{
    int end = first + 1;
    while (end < count && SDL_strcmp(files[end].path, files[first].path) == 0)
        end++;
    return end;
}

// The listed version a file of this size and CRC is, or nullptr.
static const GameFile *match(const GameFile *files, int first, int end, uint64_t size, uint32_t crc)
{
    for (int k = first; k < end; k++)
        if (files[k].size == size && files[k].crc == crc)
            return &files[k];
    return nullptr;
}

static bool size_listed(const GameFile *files, int first, int end, uint64_t size)
{
    for (int k = first; k < end; k++)
        if (files[k].size == size)
            return true;
    return false;
}

// Every listed file present and the same: else the report names them.
static bool check(Source *src, const GameFile *files, int count, ImportReport *r,
                  ImportProgress *progress)
{
    for (int i = 0; i < count; i = group_end(files, count, i)) {
        int end = group_end(files, count, i);
        const SourceFile *sf = source_find(src, files[i].path);
        if (!sf) {
            problem(r, "Missing", files[i].path);
        } else if (!size_listed(files, i, end, sf->size)) {
            problem(r, "Different", files[i].path);
        } else {
            uint32_t crc = sf->crc;
            if (!sf->crc_known) {
                size_t size = 0;
                uint8_t *data = source_read(src, sf, &size);
                if (!data)
                    return fail(r, "Reading %s: %s", files[i].path);
                crc = gamedata_crc32(0, data, size);
                SDL_free(data);
            }
            const GameFile *m = match(files, i, end, sf->size, crc);
            if (!m)
                problem(r, "Different", files[i].path);
            else if (m->edition)
                r->edition = m->edition;
        }
        step(progress);
    }
    return r->problem_count == 0;
}

static bool copy(Source *src, const GameFile *files, int count, const char *partial,
                 ImportReport *r, ImportProgress *progress)
{
    for (int i = 0; i < count; i = group_end(files, count, i)) {
        int end = group_end(files, count, i);
        const SourceFile *sf = source_find(src, files[i].path);
        size_t size = 0;
        uint8_t *data = sf ? source_read(src, sf, &size) : nullptr;
        if (!data)
            return fail(r, "Reading %s: %s", files[i].path);
        bool same = match(files, i, end, size, gamedata_crc32(0, data, size)) != nullptr;
        bool written = same && write_file(partial, files[i].path, data, size);
        SDL_free(data);
        if (!same) {
            problem(r, "Changed while it was copied", files[i].path);
            return false;
        }
        if (!written)
            return fail(r, "Writing %s: %s", files[i].path);
        step(progress);
    }
    return true;
}

// data.partial/ becomes data/; an earlier data/ goes only once the new one
// is in place.
static bool swap_in(const char *player_dir, const char *partial, ImportReport *r)
{
    char *data = nullptr, *old = nullptr;
    SDL_asprintf(&data, "%sdata", player_dir);
    SDL_asprintf(&old, "%sdata.old", player_dir);
    bool ok = data && old;
    if (ok) {
        remove_tree(old);
        bool had = SDL_GetPathInfo(data, nullptr);
        if (had && !SDL_RenamePath(data, old)) {
            ok = fail(r, "Replacing %s: %s", data);
        } else if (!SDL_RenamePath(partial, data)) {
            ok = fail(r, "Renaming the import to %s: %s", data);
            if (had)
                SDL_RenamePath(old, data);
        } else {
            remove_tree(old);
        }
    }
    SDL_free(data);
    SDL_free(old);
    return ok;
}

static void copy_saves(Source *src, const char *player_dir, ImportReport *r)
{
    char *dir = nullptr;
    SDL_asprintf(&dir, "%ssave", player_dir);
    if (!dir)
        return;
    SDL_CreateDirectory(dir);
    for (int n = 1; n <= IMPORT_SAVE_SLOTS; n++) {
        char dos[32], *to = nullptr;
        SDL_snprintf(dos, sizeof dos, "SAVE\\SPIDYSAV.%d", n);
        const SourceFile *sf = source_find(src, dos);
        if (!sf)
            continue;
        SDL_asprintf(&to, "%s/spidysav.%d", dir, n);
        if (to && SDL_GetPathInfo(to, nullptr)) {
            r->save_skipped[n] = true;
        } else if (to) {
            size_t size = 0;
            uint8_t *data = source_read(src, sf, &size);
            if (data && SDL_SaveFile(to, data, size))
                r->saves_copied++;
            else
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Importing %s: %s", dos, SDL_GetError());
            SDL_free(data);
        }
        SDL_free(to);
    }
    SDL_free(dir);
}

bool import_game(const char *path, const char *player_dir, const GameFile *files, int count,
                 ImportReport *r, ImportProgress *progress)
{
    *r = (ImportReport){0};
    int groups = 0;
    for (int i = 0; i < count; i = group_end(files, count, i))
        groups++;
    if (progress) {
        SDL_SetAtomicInt(&progress->done, 0);
        SDL_SetAtomicInt(&progress->total, 2 * groups);
    }
    Source *src = source_open(path);
    if (!src) {
        r->error = SDL_strdup(SDL_GetError());
        return false;
    }
    r->root = SDL_strdup(source_root(src));
    char *partial = nullptr;
    SDL_asprintf(&partial, "%sdata.partial", player_dir);
    bool ok = partial && check(src, files, count, r, progress);
    if (ok) {
        remove_tree(partial);  // left by an import that never finished
        ok = SDL_CreateDirectory(partial) || fail(r, "Creating %s: %s", partial);
        ok = ok && copy(src, files, count, partial, r, progress);
        if (ok) {
            char marker[128];
            SDL_snprintf(marker, sizeof marker,
                         "; The Reunion port imported the game here.\n"
                         "list = %d\nedition = %s\n",
                         GAMEDATA_LIST_VERSION, gamedata_edition_name(r->edition));
            ok = write_file(partial, MARKER, marker, SDL_strlen(marker))
                 || fail(r, "Writing %s: %s", MARKER);
        }
        ok = ok && swap_in(player_dir, partial, r);
        if (!ok)
            remove_tree(partial);
    }
    if (ok)
        copy_saves(src, player_dir, r);
    SDL_free(partial);
    source_close(src);
    r->ok = ok;
    return ok;
}

void import_report_free(ImportReport *r)
{
    for (int i = 0; i < r->problem_count; i++)
        SDL_free(r->problems[i]);
    SDL_free(r->problems);
    SDL_free(r->root);
    SDL_free(r->error);
    *r = (ImportReport){0};
}

char *import_data_dir(const char *player_dir, Edition *edition)
{
    char *dir = nullptr, *marker = nullptr;
    SDL_asprintf(&dir, "%sdata", player_dir);
    SDL_asprintf(&marker, "%sdata/%s", player_dir, MARKER);
    char *text = marker ? SDL_LoadFile(marker, nullptr) : nullptr;
    SDL_free(marker);
    int list = 0;
    Edition ed = EDITION_UNKNOWN;
    char *save = nullptr;
    for (char *line = text ? SDL_strtok_r(text, "\r\n", &save) : nullptr; line;
         line = SDL_strtok_r(nullptr, "\r\n", &save)) {
        char name[16];
        if (SDL_sscanf(line, "list = %d", &list) == 1)
            continue;
        if (SDL_sscanf(line, "edition = %15s", name) == 1)
            ed = gamedata_edition_from_name(name);
    }
    SDL_free(text);
    if (list != GAMEDATA_LIST_VERSION) {
        SDL_free(dir);
        return nullptr;
    }
    if (edition)
        *edition = ed;
    return dir;
}
