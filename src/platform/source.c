#include "platform/source.h"

#include <SDL3/SDL.h>
#include <miniz.h>

typedef enum : int { SOURCE_FOLDER, SOURCE_ZIP, SOURCE_ISO } SourceKind;

// Bounds on what a source may hold, so that a wrong pick (a home folder, a
// damaged image) ends quickly.
enum : int {
    MAX_FILES = 100000,
    MAX_DEPTH = 8,
    MAX_FOLDERS = 5000,
};

struct Source {
    SourceKind kind;
    SDL_IOStream *io;  // ZIP, ISO
    mz_zip_archive zip;
    uint32_t block;    // ISO: logical block size
    char **games;      // the games' folders, the preferred one first
    int game_count;
    SourceFile *files;
    int count, cap;
};

// --- Paths ----------------------------------------------------------------------

static int path_cmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        int x = *a == '/' ? '\\' : SDL_toupper((unsigned char)*a);
        int y = *b == '/' ? '\\' : SDL_toupper((unsigned char)*b);
        if (x != y || !x)
            return x - y;
    }
}

static SourceFile *add_file(Source *src, const char *path)
{
    if (src->count == MAX_FILES)
        return nullptr;
    if (src->count == src->cap) {
        int cap = src->cap ? src->cap * 2 : 256;
        SourceFile *f = SDL_realloc(src->files, (size_t)cap * sizeof *f);
        if (!f)
            return nullptr;
        src->files = f;
        src->cap = cap;
    }
    SourceFile *f = &src->files[src->count];
    *f = (SourceFile){.path = SDL_strdup(path)};
    if (!f->path)
        return nullptr;
    for (char *c = f->path; *c; c++)
        if (*c == '/')
            *c = '\\';
    src->count++;
    return f;
}

// Is the last part of a game's path REUNION.ENG (either slash, with or
// without one at the end)?
static bool is_english(const char *path)
{
    size_t n = SDL_strlen(path);
    while (n > 0 && (path[n - 1] == '\\' || path[n - 1] == '/'))
        n--;
    return n >= 11 && SDL_strncasecmp(path + n - 11, "REUNION.ENG", 11) == 0
           && (n == 11 || path[n - 12] == '\\' || path[n - 12] == '/');
}

// Orders game folders: REUNION.ENG first, then by path.
static int game_cmp(const void *a, const void *b)
{
    const char *x = *(char *const *)a, *y = *(char *const *)b;
    if (is_english(x) != is_english(y))
        return is_english(x) ? -1 : 1;
    return SDL_strcasecmp(x, y);
}

static bool add_game(Source *src, char *prefix)
{
    for (int g = 0; g < src->game_count; g++) {
        if (SDL_strcasecmp(src->games[g], prefix) == 0) {
            SDL_free(prefix);
            return true;
        }
    }
    char **grown = SDL_realloc(src->games, (size_t)(src->game_count + 1) * sizeof *grown);
    if (!grown) {
        SDL_free(prefix);
        return false;
    }
    src->games = grown;
    src->games[src->game_count++] = prefix;
    return true;
}

// The games among the files' paths (archives): every shallowest folder
// holding a GRAFIKA folder, as prefixes ("" or "A\\B\\").
static bool pick_roots(Source *src)
{
    int best_depth = MAX_DEPTH + 1;
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < src->count; i++) {
            const char *p = src->files[i].path;
            int depth = 0;
            for (const char *part = p; *part; depth++) {
                const char *end = SDL_strchr(part, '\\');
                if (!end || depth > MAX_DEPTH || (pass == 1 && depth > best_depth))
                    break;
                if (end - part == 7 && SDL_strncasecmp(part, "GRAFIKA", 7) == 0) {
                    if (pass == 0)
                        best_depth = SDL_min(best_depth, depth);
                    else if (depth == best_depth && !add_game(src, SDL_strndup(p, (size_t)(part - p))))
                        return false;
                    break;
                }
                part = end + 1;
            }
        }
    }
    SDL_qsort(src->games, (size_t)src->game_count, sizeof *src->games, game_cmp);
    return src->game_count > 0;
}

// Keeps the files under the games' prefixes, with paths relative to their
// game; the prefixes become the folders' names.
static void keep_games(Source *src)
{
    int kept = 0;
    for (int i = 0; i < src->count; i++) {
        SourceFile f = src->files[i];
        int game = -1;
        size_t n = 0;
        for (int g = 0; g < src->game_count && game < 0; g++) {
            n = SDL_strlen(src->games[g]);
            if (SDL_strncasecmp(f.path, src->games[g], n) == 0 && f.path[n])
                game = g;
        }
        if (game >= 0) {
            SDL_memmove(f.path, f.path + n, SDL_strlen(f.path + n) + 1);
            f.game = game;
            src->files[kept++] = f;
        } else {
            SDL_free(f.path);
            SDL_free(f.host);
        }
    }
    src->count = kept;
    for (int g = 0; g < src->game_count; g++) {
        size_t n = SDL_strlen(src->games[g]);
        if (n)
            src->games[g][n - 1] = '\0';
    }
}

// --- Folders ----------------------------------------------------------------------

typedef struct Walk {
    Source *src;
    int game;
    const char *rel;  // the folder's path relative to the game, "" or "A\\"
    char **subdirs;   // its subfolders' names
    int subdir_count;
    bool has_grafika;
    bool failed;
} Walk;

static SDL_EnumerationResult walk_entry(void *userdata, const char *dirname, const char *fname)
{
    Walk *w = userdata;
    char *host = nullptr;
    SDL_asprintf(&host, "%s%s", dirname, fname);
    SDL_PathInfo info;
    if (!host || !SDL_GetPathInfo(host, &info)) {
        SDL_free(host);
        return SDL_ENUM_CONTINUE;
    }
    if (info.type == SDL_PATHTYPE_DIRECTORY) {
        if (SDL_strcasecmp(fname, "GRAFIKA") == 0)
            w->has_grafika = true;
        char **s = SDL_realloc(w->subdirs, (size_t)(w->subdir_count + 1) * sizeof *s);
        if (s) {
            w->subdirs = s;
            s[w->subdir_count++] = SDL_strdup(fname);
        }
    } else if (info.type == SDL_PATHTYPE_FILE && w->src) {
        char *rel = nullptr;
        SDL_asprintf(&rel, "%s%s", w->rel, fname);
        SourceFile *f = rel ? add_file(w->src, rel) : nullptr;
        SDL_free(rel);
        if (!f) {
            SDL_free(host);
            w->failed = true;
            return SDL_ENUM_FAILURE;
        }
        f->size = (uint64_t)info.size;
        f->host = host;
        f->game = w->game;
        return SDL_ENUM_CONTINUE;
    }
    SDL_free(host);
    return SDL_ENUM_CONTINUE;
}

static void free_names(char **names, int n)
{
    for (int i = 0; i < n; i++)
        SDL_free(names[i]);
    SDL_free(names);
}

// With a trailing separator.
static char *dir_path(const char *dir)
{
    size_t n = SDL_strlen(dir);
    bool slash = n > 0 && (dir[n - 1] == '/' || dir[n - 1] == '\\');
    char *out = nullptr;
    SDL_asprintf(&out, "%s%s", dir, slash ? "" : "/");
    return out;
}

// Every file under `dir` (with a trailing separator), as `rel` + its path,
// in `game`.
static bool list_folder(Source *src, int game, const char *dir, const char *rel, int depth)
{
    Walk w = {.src = src, .game = game, .rel = rel};
    SDL_EnumerateDirectory(dir, walk_entry, &w);
    bool ok = !w.failed;
    for (int i = 0; ok && i < w.subdir_count && depth < MAX_DEPTH; i++) {
        char *sub = nullptr, *sub_rel = nullptr;
        SDL_asprintf(&sub, "%s%s/", dir, w.subdirs[i]);
        SDL_asprintf(&sub_rel, "%s%s\\", rel, w.subdirs[i]);
        ok = sub && sub_rel && list_folder(src, game, sub, sub_rel, depth + 1);
        SDL_free(sub);
        SDL_free(sub_rel);
    }
    free_names(w.subdirs, w.subdir_count);
    return ok;
}

static int name_cmp(const void *a, const void *b)
{
    return SDL_strcasecmp(*(char *const *)a, *(char *const *)b);
}

typedef struct FolderGame {
    char *name;  // the folder's own name, "" for `top` itself
    char *host;  // with a separator
} FolderGame;

static int folder_game_cmp(const void *a, const void *b)
{
    return game_cmp(&((const FolderGame *)a)->name, &((const FolderGame *)b)->name);
}

// Breadth first: the shallowest folders holding GRAFIKA, the preferred one
// first. Returns how many, 0 if none.
static int find_folder_games(const char *top, FolderGame **games)
{
    char **level = SDL_malloc(sizeof *level);
    int count = 1, visited = 0, found = 0;
    *games = nullptr;
    if (!level)
        return 0;
    level[0] = dir_path(top);
    for (int depth = 0; !found && count > 0 && depth <= MAX_DEPTH; depth++) {
        char **next = nullptr;
        int next_count = 0;
        SDL_qsort(level, (size_t)count, sizeof *level, name_cmp);
        for (int i = 0; i < count && visited < MAX_FOLDERS; i++, visited++) {
            Walk w = {0};
            SDL_EnumerateDirectory(level[i], walk_entry, &w);
            const char *base = level[i];
            size_t n = SDL_strlen(base);
            for (size_t k = n - 1; k > 0; k--)
                if (base[k - 1] == '/' || base[k - 1] == '\\') {
                    base += k;
                    break;
                }
            FolderGame *grown = w.has_grafika
                                    ? SDL_realloc(*games, (size_t)(found + 1) * sizeof *grown)
                                    : nullptr;
            if (grown) {
                *games = grown;
                grown[found].host = SDL_strdup(level[i]);
                grown[found].name = depth ? SDL_strndup(base, SDL_strlen(base) - 1) : SDL_strdup("");
                found++;
            }
            char **more = SDL_realloc(next, (size_t)(next_count + w.subdir_count) * sizeof *next);
            if (more) {
                next = more;
                for (int s = 0; s < w.subdir_count; s++)
                    SDL_asprintf(&next[next_count++], "%s%s/", level[i], w.subdirs[s]);
            }
            free_names(w.subdirs, w.subdir_count);
        }
        free_names(level, count);
        level = next;
        count = next_count;
    }
    free_names(level, count);
    if (found)
        SDL_qsort(*games, (size_t)found, sizeof **games, folder_game_cmp);
    return found;
}

// --- ZIP --------------------------------------------------------------------------

static size_t zip_read(void *opaque, mz_uint64 ofs, void *buf, size_t n)
{
    SDL_IOStream *io = opaque;
    if (SDL_SeekIO(io, (Sint64)ofs, SDL_IO_SEEK_SET) < 0)
        return 0;
    return SDL_ReadIO(io, buf, n);
}

// miniz allocates with SDL's allocator, so extracted files are freed with
// SDL_free like the others.
static void *zip_alloc(void *opaque, size_t items, size_t size)
{
    (void)opaque;
    return SDL_malloc(items * size);
}

static void zip_free(void *opaque, void *p)
{
    (void)opaque;
    SDL_free(p);
}

static void *zip_realloc(void *opaque, void *p, size_t items, size_t size)
{
    (void)opaque;
    return SDL_realloc(p, items * size);
}

static bool open_zip(Source *src)
{
    Sint64 size = SDL_GetIOSize(src->io);
    src->zip.m_pAlloc = zip_alloc;
    src->zip.m_pFree = zip_free;
    src->zip.m_pRealloc = zip_realloc;
    src->zip.m_pRead = zip_read;
    src->zip.m_pIO_opaque = src->io;
    if (size <= 0 || !mz_zip_reader_init(&src->zip, (mz_uint64)size, 0))
        return false;
    src->kind = SOURCE_ZIP;
    mz_uint n = mz_zip_reader_get_num_files(&src->zip);
    for (mz_uint i = 0; i < n; i++) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&src->zip, i, &st) || st.m_is_directory)
            continue;
        SourceFile *f = add_file(src, st.m_filename);
        if (!f)
            return SDL_SetError("Too many files in the archive");
        f->size = st.m_uncomp_size;
        f->crc = st.m_crc32;
        f->crc_known = true;
        f->where = i;
    }
    return true;
}

// --- ISO 9660 ---------------------------------------------------------------------

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static bool read_at(SDL_IOStream *io, uint64_t pos, void *buf, size_t n)
{
    return SDL_SeekIO(io, (Sint64)pos, SDL_IO_SEEK_SET) >= 0 && SDL_ReadIO(io, buf, n) == n;
}

// The files of the directory at `extent`, `size` bytes, as `rel` + name.
static bool list_iso_dir(Source *src, uint32_t extent, uint32_t size, const char *rel, int depth)
{
    if (depth > MAX_DEPTH || size > 16u << 20)
        return SDL_SetError("Damaged ISO image");
    uint8_t *dir = SDL_malloc(size ? size : 1);
    if (!dir || !read_at(src->io, (uint64_t)extent * src->block, dir, size)) {
        SDL_free(dir);
        return SDL_SetError("Damaged ISO image");
    }
    bool ok = true;
    for (uint32_t pos = 0; ok && pos < size;) {
        uint8_t len = dir[pos];
        if (len == 0) {  // records don't cross a block: on to the next one
            pos = (pos / src->block + 1) * src->block;
            continue;
        }
        if (len < 34 || pos + len > size || 33u + dir[pos + 32] > len) {
            ok = SDL_SetError("Damaged ISO image");
            break;
        }
        const uint8_t *r = dir + pos;
        pos += len;
        uint8_t name_len = r[32];
        if (name_len == 1 && r[33] <= 1)  // "." and ".."
            continue;
        char name[256];
        SDL_memcpy(name, r + 33, name_len);
        name[name_len] = '\0';
        char *semi = SDL_strchr(name, ';');  // the version, ";1"
        if (semi)
            *semi = '\0';
        size_t n = SDL_strlen(name);
        if (n > 0 && name[n - 1] == '.')  // "INIT." has no extension
            name[--n] = '\0';
        char *path = nullptr;
        SDL_asprintf(&path, "%s%s", rel, name);
        if (!path)
            ok = false;
        else if (r[25] & 2) {
            char *sub = nullptr;
            SDL_asprintf(&sub, "%s\\", path);
            ok = sub && list_iso_dir(src, le32(r + 2), le32(r + 10), sub, depth + 1);
            SDL_free(sub);
        } else {
            SourceFile *f = add_file(src, path);
            if (f) {
                f->size = le32(r + 10);
                f->where = (uint64_t)le32(r + 2) * src->block;
            } else {
                ok = SDL_SetError("Too many files in the image");
            }
        }
        SDL_free(path);
    }
    SDL_free(dir);
    return ok;
}

// False, without an error, if this is no ISO 9660 image.
static bool open_iso(Source *src, bool *is_iso)
{
    uint8_t vd[2048];
    *is_iso = false;
    for (uint64_t sector = 16; sector < 32; sector++) {
        if (!read_at(src->io, sector * 2048, vd, sizeof vd) || SDL_memcmp(vd + 1, "CD001", 5) != 0)
            return false;
        if (vd[0] == 255)  // the set terminator
            return false;
        if (vd[0] != 1)    // not the primary volume descriptor
            continue;
        *is_iso = true;
        src->kind = SOURCE_ISO;
        src->block = (uint32_t)vd[128] | (uint32_t)vd[129] << 8;
        if (src->block < 512 || src->block > 2048)
            return SDL_SetError("Damaged ISO image");
        const uint8_t *root = vd + 156;
        return list_iso_dir(src, le32(root + 2), le32(root + 10), "", 0);
    }
    return false;
}

// --- Opening ----------------------------------------------------------------------

Source *source_open(const char *path)
{
    Source *src = SDL_calloc(1, sizeof *src);
    if (!src)
        return nullptr;
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(path, &info)) {
        SDL_SetError("%s can't be read", path);
        SDL_free(src);
        return nullptr;
    }
    bool ok;
    if (info.type == SDL_PATHTYPE_DIRECTORY) {
        src->kind = SOURCE_FOLDER;
        FolderGame *games = nullptr;
        int n = find_folder_games(path, &games);
        ok = n > 0 || SDL_SetError("No game in %s: no folder there holds GRAFIKA", path);
        for (int g = 0; g < n; g++) {
            // Two folders of the same name at that depth: the first.
            bool take = ok && games[g].name && games[g].host;
            for (int k = 0; take && k < src->game_count; k++)
                take = SDL_strcasecmp(src->games[k], games[g].name) != 0;
            if (take)
                ok = add_game(src, games[g].name)  // takes the name
                     && list_folder(src, src->game_count - 1, games[g].host, "", 0);
            else
                SDL_free(games[g].name);
            SDL_free(games[g].host);
        }
        SDL_free(games);
    } else {
        src->io = SDL_IOFromFile(path, "rb");
        bool is_iso = false;
        ok = src->io && open_iso(src, &is_iso);
        if (src->io && !is_iso) {
            ok = open_zip(src);
            if (!ok && src->kind != SOURCE_ZIP)
                SDL_SetError("%s is not a folder, a ZIP archive or an ISO 9660 image", path);
        }
        if (ok && pick_roots(src))
            keep_games(src);
        else if (ok)
            ok = SDL_SetError("No game in %s: no folder in it holds GRAFIKA", path);
    }
    if (!ok) {
        source_close(src);
        return nullptr;
    }
    return src;
}

void source_close(Source *src)
{
    if (!src)
        return;
    if (src->kind == SOURCE_ZIP)
        mz_zip_reader_end(&src->zip);
    if (src->io)
        SDL_CloseIO(src->io);
    for (int i = 0; i < src->count; i++) {
        SDL_free(src->files[i].path);
        SDL_free(src->files[i].host);
    }
    SDL_free(src->files);
    for (int g = 0; g < src->game_count; g++)
        SDL_free(src->games[g]);
    SDL_free(src->games);
    SDL_free(src);
}

const char *source_root(const Source *src)
{
    return source_game(src, 0);
}

int source_game_count(const Source *src)
{
    return src->game_count;
}

const char *source_game(const Source *src, int game)
{
    return game >= 0 && game < src->game_count ? src->games[game] : "";
}

int source_file_count(const Source *src)
{
    return src->count;
}

const SourceFile *source_file(const Source *src, int i)
{
    return i >= 0 && i < src->count ? &src->files[i] : nullptr;
}

const SourceFile *source_find(const Source *src, const char *dos_path)
{
    return source_find_in(src, 0, dos_path);
}

const SourceFile *source_find_in(const Source *src, int game, const char *dos_path)
{
    for (int i = 0; i < src->count; i++)
        if (src->files[i].game == game && path_cmp(src->files[i].path, dos_path) == 0)
            return &src->files[i];
    return nullptr;
}

uint8_t *source_read(Source *src, const SourceFile *f, size_t *size)
{
    uint8_t *data = nullptr;
    size_t n = 0;
    switch (src->kind) {
    case SOURCE_FOLDER:
        data = SDL_LoadFile(f->host, &n);
        break;
    case SOURCE_ZIP:
        data = mz_zip_reader_extract_to_heap(&src->zip, (mz_uint)f->where, &n, 0);
        if (!data)
            SDL_SetError("%s: %s", f->path,
                         mz_zip_get_error_string(mz_zip_get_last_error(&src->zip)));
        break;
    case SOURCE_ISO:
        if (f->size > SIZE_MAX - 1)
            break;
        n = (size_t)f->size;
        data = SDL_malloc(n ? n : 1);
        if (data && !read_at(src->io, f->where, data, n)) {
            SDL_SetError("%s: read error", f->path);
            SDL_free(data);
            data = nullptr;
        }
        break;
    }
    if (data && size)
        *size = n;
    return data;
}
