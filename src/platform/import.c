#include "platform/import.h"

#include <SDL3/SDL.h>

#include "platform/source.h"

// In data/: the import finished. Not a DOS file, so the game never asks
// for it.
static const char MARKER[] = "import.ini";

// `game` is the folder of a language other than data/'s ("REUNION.DEU\\"),
// or "".
static void problem(ImportReport *r, const char *what, const char *game, const char *path)
{
    char **p = SDL_realloc(r->problems, (size_t)(r->problem_count + 1) * sizeof *p);
    if (!p)
        return;
    r->problems = p;
    p[r->problem_count] = nullptr;
    SDL_asprintf(&p[r->problem_count], "%s: %s%s", what, game, path);
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

// The listed version of this size and CRC that `language` may have, or
// nullptr.
static const GameFile *match(const GameFile *files, int first, int end, Language language,
                             uint64_t size, uint32_t crc)
{
    for (int k = first; k < end; k++)
        if (gamedata_in_language(&files[k], language) && files[k].size == size
            && files[k].crc == crc)
            return &files[k];
    return nullptr;
}

static bool size_listed(const GameFile *files, int first, int end, Language language,
                        uint64_t size)
{
    for (int k = first; k < end; k++)
        if (gamedata_in_language(&files[k], language) && files[k].size == size)
            return true;
    return false;
}

// Does `language` have its own version of the path, not the one of `base`?
// Every path differs from LANGUAGE_COUNT, so a check against it takes all.
static bool differs(const GameFile *files, int first, int end, Language language, Language base)
{
    if (base == LANGUAGE_COUNT)
        return true;
    for (int k = first; k < end; k++)
        if (files[k].languages && gamedata_in_language(&files[k], language))
            return !gamedata_in_language(&files[k], base);
    return false;
}

// One game of the copy to check and copy: the main one has every file,
// another language only the files where it differs from the main one.
typedef struct Game {
    int index;          // in the source
    Language language;
    Language base;      // LANGUAGE_COUNT for the main game, else its language
    const char *label;  // before paths in problems: "" or "REUNION.DEU\\"
    char *dir;          // where it goes, under data.partial/
} Game;

static int game_groups(const Game *g, const GameFile *files, int count)
{
    int n = 0;
    for (int i = 0; i < count; i = group_end(files, count, i))
        n += differs(files, i, group_end(files, count, i), g->language, g->base);
    return n;
}

// Every listed file present and the same: else the report names them.
static bool check(Source *src, const Game *g, const GameFile *files, int count, ImportReport *r,
                  ImportProgress *progress)
{
    for (int i = 0; i < count; i = group_end(files, count, i)) {
        int end = group_end(files, count, i);
        if (!differs(files, i, end, g->language, g->base))
            continue;
        const SourceFile *sf = source_find_in(src, g->index, files[i].path);
        if (!sf) {
            problem(r, "Missing", g->label, files[i].path);
        } else if (!size_listed(files, i, end, g->language, sf->size)) {
            problem(r, "Different", g->label, files[i].path);
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
            const GameFile *m = match(files, i, end, g->language, sf->size, crc);
            if (!m)
                problem(r, "Different", g->label, files[i].path);
            else if (m->edition && g->base == LANGUAGE_COUNT)
                r->edition = m->edition;
        }
        step(progress);
    }
    return r->problem_count == 0;
}

static bool copy(Source *src, const Game *g, const GameFile *files, int count, ImportReport *r,
                 ImportProgress *progress)
{
    for (int i = 0; i < count; i = group_end(files, count, i)) {
        int end = group_end(files, count, i);
        if (!differs(files, i, end, g->language, g->base))
            continue;
        const SourceFile *sf = source_find_in(src, g->index, files[i].path);
        size_t size = 0;
        uint8_t *data = sf ? source_read(src, sf, &size) : nullptr;
        if (!data)
            return fail(r, "Reading %s: %s", files[i].path);
        bool same = match(files, i, end, g->language, size, gamedata_crc32(0, data, size)) != nullptr;
        bool written = same && write_file(g->dir, files[i].path, data, size);
        SDL_free(data);
        if (!same) {
            problem(r, "Changed while it was copied", g->label, files[i].path);
            return false;
        }
        if (!written)
            return fail(r, "Writing %s: %s", files[i].path);
        step(progress);
    }
    return true;
}

// A game's language: by the CD's folder name (REUNION.DEU), else by its
// TEXT\MESSAGE.TXT; LANGUAGE_COUNT if neither tells.
static Language game_language(Source *src, int game, const GameFile *files, int count)
{
    const char *name = source_game(src, game);
    const char *base = name;
    for (const char *c = name; *c; c++)
        if (*c == '\\')
            base = c + 1;
    if (SDL_strlen(base) == 11 && SDL_strncasecmp(base, "REUNION.", 8) == 0) {
        Language l = gamedata_language_from_name(base + 8);
        if (l < LANGUAGE_COUNT)
            return l;
    }
    static const char PROBE[] = "TEXT\\MESSAGE.TXT";
    const SourceFile *sf = source_find_in(src, game, PROBE);
    if (!sf)
        return LANGUAGE_COUNT;
    uint32_t crc = sf->crc;
    if (!sf->crc_known) {
        size_t size = 0;
        uint8_t *data = source_read(src, sf, &size);
        if (!data)
            return LANGUAGE_COUNT;
        crc = gamedata_crc32(0, data, size);
        SDL_free(data);
    }
    for (int i = 0; i < count; i++)
        if (files[i].languages && SDL_strcmp(files[i].path, PROBE) == 0
            && files[i].size == sf->size && files[i].crc == crc)
            return (Language)SDL_MostSignificantBitIndex32(files[i].languages);
    return LANGUAGE_COUNT;
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

// Each slot's save from the first game that has one, data/'s first.
static void copy_saves(Source *src, const Game *games, int game_count, const char *player_dir,
                       ImportReport *r)
{
    char *dir = nullptr;
    SDL_asprintf(&dir, "%ssave", player_dir);
    if (!dir)
        return;
    SDL_CreateDirectory(dir);
    for (int n = 1; n <= IMPORT_SAVE_SLOTS; n++) {
        char dos[32], *to = nullptr;
        SDL_snprintf(dos, sizeof dos, "SAVE\\SPIDYSAV.%d", n);
        const SourceFile *sf = nullptr;
        for (int g = 0; g < game_count && !sf; g++)
            sf = source_find_in(src, games[g].index, dos);
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

// The games to import: the English one if there is one, else the first,
// fills data/; each other language goes to data/<FOLDER> (one game per
// language; games of no known language are left out). Returns how many.
static int pick_games(Source *src, const GameFile *files, int count, const char *partial,
                      Game games[LANGUAGE_COUNT])
{
    int n = source_game_count(src);
    Language langs[64];
    n = SDL_min(n, (int)SDL_arraysize(langs));
    int main = 0;
    for (int g = n - 1; g >= 0; g--) {
        langs[g] = game_language(src, g, files, count);
        if (langs[g] == LANGUAGE_ENGLISH)
            main = g;
    }
    // Checked as English if nothing tells: the problems are then named.
    Language main_lang = langs[main] < LANGUAGE_COUNT ? langs[main] : LANGUAGE_ENGLISH;
    games[0] = (Game){.index = main, .language = main_lang, .base = LANGUAGE_COUNT, .label = ""};
    int picked = 1;
    for (int g = 0; g < n; g++) {
        bool taken = g == main || langs[g] == LANGUAGE_COUNT;
        for (int k = 0; k < picked && !taken; k++)
            taken = games[k].language == langs[g];
        if (taken)
            continue;
        Game *x = &games[picked++];
        *x = (Game){.index = g, .language = langs[g], .base = main_lang};
        SDL_asprintf(&x->dir, "%s/%s", partial, gamedata_language_folder(langs[g]));
        char *label = nullptr;
        SDL_asprintf(&label, "%s\\", source_game(src, g));
        x->label = label ? label : "";
    }
    games[0].dir = SDL_strdup(partial);
    return picked;
}

static void free_games(Game *games, int n)
{
    for (int g = 0; g < n; g++) {
        SDL_free(games[g].dir);
        if (g > 0 && games[g].label[0])
            SDL_free((char *)games[g].label);
    }
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
    char *partial = nullptr;
    SDL_asprintf(&partial, "%sdata.partial", player_dir);
    Game games[LANGUAGE_COUNT] = {};
    int game_count = partial ? pick_games(src, files, count, partial, games) : 0;
    bool ok = game_count > 0 && games[0].dir;
    if (ok) {
        r->root = SDL_strdup(source_game(src, games[0].index));
        r->language = games[0].language;
        int total = 0;
        for (int g = 0; g < game_count; g++)
            total += game_groups(&games[g], files, count);
        if (progress)
            SDL_SetAtomicInt(&progress->total, 2 * total);
    }
    for (int g = 0; ok && g < game_count; g++)
        ok = check(src, &games[g], files, count, r, progress) || r->error == nullptr;
    ok = ok && r->problem_count == 0;
    if (ok) {
        remove_tree(partial);  // left by an import that never finished
        ok = SDL_CreateDirectory(partial) || fail(r, "Creating %s: %s", partial);
        for (int g = 0; ok && g < game_count; g++) {
            ok = copy(src, &games[g], files, count, r, progress);
            r->languages |= (uint8_t)(1u << games[g].language);
        }
        if (ok) {
            char marker[256];
            int len = SDL_snprintf(marker, sizeof marker,
                                   "; The Reunion port imported the game here.\n"
                                   "list = %d\nedition = %s\nlanguages =",
                                   GAMEDATA_LIST_VERSION, gamedata_edition_name(r->edition));
            for (int g = 0; g < game_count; g++)
                len += SDL_snprintf(marker + len, sizeof marker - (size_t)len, " %s",
                                    gamedata_language_name(games[g].language));
            SDL_strlcat(marker, "\n", sizeof marker);
            ok = write_file(partial, MARKER, marker, SDL_strlen(marker))
                 || fail(r, "Writing %s: %s", MARKER);
        }
        ok = ok && swap_in(player_dir, partial, r);
        if (!ok)
            remove_tree(partial);
    }
    if (ok)
        copy_saves(src, games, game_count, player_dir, r);
    if (!ok)
        r->languages = 0;
    free_games(games, game_count);
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

char *import_data_dir(const char *player_dir, ImportedData *info)
{
    char *dir = nullptr, *marker = nullptr;
    SDL_asprintf(&dir, "%sdata", player_dir);
    SDL_asprintf(&marker, "%sdata/%s", player_dir, MARKER);
    char *text = marker ? SDL_LoadFile(marker, nullptr) : nullptr;
    SDL_free(marker);
    int list = 0;
    ImportedData in = {.language = LANGUAGE_ENGLISH};
    char *save = nullptr;
    for (char *line = text ? SDL_strtok_r(text, "\r\n", &save) : nullptr; line;
         line = SDL_strtok_r(nullptr, "\r\n", &save)) {
        char name[16];
        if (SDL_sscanf(line, "list = %d", &list) == 1)
            continue;
        if (SDL_sscanf(line, "edition = %15s", name) == 1)
            in.edition = gamedata_edition_from_name(name);
        if (SDL_strncmp(line, "languages =", 11) == 0) {
            char *words = line + 11, *at = nullptr;
            for (char *w = SDL_strtok_r(words, " ", &at); w; w = SDL_strtok_r(nullptr, " ", &at)) {
                Language l = gamedata_language_from_name(w);
                if (l == LANGUAGE_COUNT)
                    continue;
                if (!in.languages)
                    in.language = l;
                in.languages |= (uint8_t)(1u << l);
            }
        }
    }
    SDL_free(text);
    if (list != GAMEDATA_LIST_VERSION || !in.languages) {
        SDL_free(dir);
        return nullptr;
    }
    if (info)
        *info = in;
    return dir;
}
