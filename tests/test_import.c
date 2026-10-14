// import: copies of the game in a folder, a ZIP and an ISO image, each built
// here from a small fake list of game files. The edition, the root at any
// depth, the CD's languages (REUNION.ENG to data/, REUNION.DEU's own files
// to data/DEU; a lone German game), the DOS saves with a slot already in
// use, a missing file, a different one (in a folder and in a ZIP, in data/'s
// game and in another language's), a copy with no game, an import left
// half done, an old list version and a re-import.
#include <SDL3/SDL.h>
#include <miniz.h>
#include <stdio.h>

#include "platform/gamedata.h"
#include "platform/import.h"
#include "platform/source.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static const char TMP[] = "test_import.tmp";
static const char PLAYER[] = "test_import.tmp/player/";

// The fake list; sizes and CRCs are filled in from the contents.
static GameFile files[] = {
    {"GRAFIKA\\CHARSET1.PIC", 0, 0, EDITION_FLOPPY, 0},
    {"GRAFIKA\\CHARSET1.PIC", 0, 0, EDITION_CD, 0},
    {"GRAFIKA\\MAIN.PIC", 0, 0, EDITION_UNKNOWN, 0},
    {"SAVE\\INIT", 0, 0, EDITION_UNKNOWN, 0},
    {"TEXT\\MESSAGE.TXT", 0, 0, EDITION_UNKNOWN, IN_ENGLISH},
    {"TEXT\\MESSAGE.TXT", 0, 0, EDITION_UNKNOWN, IN_GERMAN},
    {"WAR\\GRLOST.PIC", 0, 0, EDITION_UNKNOWN, IN_ENGLISH | IN_GERMAN},
};
enum { FILE_COUNT = (int)SDL_arraysize(files) };

static uint8_t contents[FILE_COUNT][400];

typedef struct Entry {
    char path[64];
    const uint8_t *data;
    size_t size;
} Entry;

// A copy of the game: the listed files (CHARSET1 of `edition`, the
// versions of `language`), two DOS saves and two files that are not on the
// list, under `prefix`.
typedef struct Copy {
    Entry e[16];
    int count;
} Copy;

static uint8_t save1[] = "save one", save2[] = "save two", extra[] = "not a game file";

static void add(Copy *c, const char *prefix, const char *path, const uint8_t *data, size_t size)
{
    Entry *e = &c->e[c->count++];
    SDL_snprintf(e->path, sizeof e->path, "%s%s", prefix, path);
    e->data = data;
    e->size = size;
}

static Copy make_copy(const char *prefix, Edition edition, Language language)
{
    Copy c = {0};
    for (int i = 0; i < FILE_COUNT; i++) {
        if ((files[i].edition && files[i].edition != edition)
            || !gamedata_in_language(&files[i], language))
            continue;
        // One in lower case: names match in any case.
        const char *path = i == 2 ? "grafika\\main.pic" : files[i].path;
        add(&c, prefix, path, contents[i], files[i].size);
    }
    add(&c, prefix, "SAVE\\SPIDYSAV.1", save1, sizeof save1);
    add(&c, prefix, "SAVE\\SPIDYSAV.2", save2, sizeof save2);
    add(&c, prefix, "GRWAR\\REUNION.PRG", extra, sizeof extra);
    add(&c, prefix, "README.TXT", extra, sizeof extra);
    return c;
}

static Entry *find(Copy *c, const char *suffix)
{
    for (int i = 0; i < c->count; i++) {
        size_t n = SDL_strlen(c->e[i].path), m = SDL_strlen(suffix);
        if (n >= m && SDL_strcasecmp(c->e[i].path + n - m, suffix) == 0)
            return &c->e[i];
    }
    return nullptr;
}

// --- Writing copies ---------------------------------------------------------------

static char *path_of(const char *name)
{
    char *p = nullptr;
    SDL_asprintf(&p, "%s/%s", TMP, name);
    return p;
}

static void write_at(const char *dir, const char *dos, const void *data, size_t size)
{
    char *p = nullptr;
    SDL_asprintf(&p, "%s/%s", dir, dos);
    for (char *c = p + SDL_strlen(dir) + 1; *c; c++)
        if (*c == '\\') {
            *c = '\0';
            SDL_CreateDirectory(p);
            *c = '/';
        }
    CHECK(SDL_SaveFile(p, data, size));
    SDL_free(p);
}

static char *write_folder(const char *name, const Copy *c)
{
    char *dir = path_of(name);
    SDL_CreateDirectory(dir);
    for (int i = 0; i < c->count; i++)
        write_at(dir, c->e[i].path, c->e[i].data, c->e[i].size);
    return dir;
}

static char *write_zip(const char *name, const Copy *c)
{
    char *path = path_of(name);
    mz_zip_archive zip = {0};
    CHECK(mz_zip_writer_init_file(&zip, path, 0));
    for (int i = 0; i < c->count; i++) {
        char entry[64];
        SDL_strlcpy(entry, c->e[i].path, sizeof entry);
        for (char *ch = entry; *ch; ch++)
            if (*ch == '\\')
                *ch = '/';
        CHECK(mz_zip_writer_add_mem(&zip, entry, c->e[i].data, c->e[i].size, MZ_BEST_COMPRESSION));
    }
    CHECK(mz_zip_writer_finalize_archive(&zip));
    mz_zip_writer_end(&zip);
    return path;
}

// A plain ISO 9660 image: 8.3 names with ";1", one block per folder.
typedef struct Node {
    char name[16];
    bool dir;
    int kids[16], kid_count;
    const uint8_t *data;
    uint32_t size, lba;
} Node;

static Node nodes[128];
static int node_count;

static int child(int parent, const char *name, bool dir)
{
    Node *p = &nodes[parent];
    for (int i = 0; i < p->kid_count; i++)
        if (SDL_strcmp(nodes[p->kids[i]].name, name) == 0)
            return p->kids[i];
    int n = node_count++;
    nodes[n] = (Node){.dir = dir};
    SDL_strlcpy(nodes[n].name, name, sizeof nodes[n].name);
    p->kids[p->kid_count++] = n;
    return n;
}

static uint32_t place(int n, uint32_t lba)
{
    Node *node = &nodes[n];
    node->lba = lba;
    if (!node->dir) {
        lba += (node->size + 2047) / 2048;
        return lba == node->lba ? lba + 1 : lba;
    }
    node->size = 2048;
    lba++;
    for (int i = 0; i < node->kid_count; i++)
        lba = place(node->kids[i], lba);
    return lba;
}

static void put32(uint8_t *p, uint32_t v)  // both-endian
{
    for (int i = 0; i < 4; i++) {
        p[i] = (uint8_t)(v >> (8 * i));
        p[7 - i] = (uint8_t)(v >> (8 * i));
    }
}

static int record(uint8_t *p, const char *name, size_t len, uint32_t lba, uint32_t size, bool dir)
{
    int n = 33 + (int)len + (len % 2 == 0);
    SDL_memset(p, 0, (size_t)n);
    p[0] = (uint8_t)n;
    put32(p + 2, lba);
    put32(p + 10, size);
    p[25] = dir ? 2 : 0;
    p[28] = 1;
    p[32] = (uint8_t)len;
    SDL_memcpy(p + 33, name, len);
    return n;
}

static void write_node(uint8_t *img, int n, int parent)
{
    Node *node = &nodes[n];
    if (!node->dir) {
        SDL_memcpy(img + (size_t)node->lba * 2048, node->data, node->size);
        return;
    }
    uint8_t *p = img + (size_t)node->lba * 2048;
    p += record(p, "\0", 1, node->lba, 2048, true);
    p += record(p, "\1", 1, nodes[parent].lba, 2048, true);
    for (int i = 0; i < node->kid_count; i++) {
        Node *k = &nodes[node->kids[i]];
        char name[24];
        if (k->dir)
            SDL_strlcpy(name, k->name, sizeof name);
        else
            SDL_snprintf(name, sizeof name, "%s%s;1", k->name, SDL_strchr(k->name, '.') ? "" : ".");
        p += record(p, name, SDL_strlen(name), k->lba, k->size, k->dir);
        write_node(img, node->kids[i], n);
    }
}

static char *write_iso(const char *name, const Copy *a, const Copy *b)
{
    node_count = 1;
    nodes[0] = (Node){.dir = true};
    const Copy *copies[] = {a, b};
    for (int c = 0; c < 2; c++)
        for (int i = 0; i < copies[c]->count; i++) {
            const Entry *e = &copies[c]->e[i];
            char path[64];
            SDL_strlcpy(path, e->path, sizeof path);
            int at = 0;
            char *save = nullptr;
            for (char *part = SDL_strtok_r(path, "\\", &save); part;
                 part = SDL_strtok_r(nullptr, "\\", &save)) {
                bool last = !save || !*save;
                for (char *ch = part; *ch; ch++)
                    *ch = (char)SDL_toupper((unsigned char)*ch);
                at = child(at, part, !last);
            }
            nodes[at].data = e->data;
            nodes[at].size = (uint32_t)e->size;
        }
    uint32_t blocks = place(0, 18);
    uint8_t *img = SDL_calloc(blocks, 2048);
    uint8_t *pvd = img + 16 * 2048, *end = img + 17 * 2048;
    pvd[0] = 1;
    SDL_memcpy(pvd + 1, "CD001\1", 6);
    pvd[128] = 0x00, pvd[129] = 0x08, pvd[130] = 0x08, pvd[131] = 0x00;  // 2048, both-endian
    record(pvd + 156, "\0", 1, nodes[0].lba, 2048, true);
    end[0] = 255;
    SDL_memcpy(end + 1, "CD001\1", 6);
    write_node(img, 0, 0);
    char *path = path_of(name);
    CHECK(SDL_SaveFile(path, img, (size_t)blocks * 2048));
    SDL_free(img);
    return path;
}

// --- Checking ---------------------------------------------------------------------

typedef struct Names {
    char *names[64];
    int count;
} Names;

static SDL_EnumerationResult collect(void *userdata, const char *dirname, const char *fname)
{
    (void)dirname;
    Names *n = userdata;
    if (n->count < 64)
        n->names[n->count++] = SDL_strdup(fname);
    return SDL_ENUM_CONTINUE;
}

static void remove_tree(const char *path)
{
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(path, &info))
        return;
    if (info.type == SDL_PATHTYPE_DIRECTORY) {
        Names n = {0};
        SDL_EnumerateDirectory(path, collect, &n);
        for (int i = 0; i < n.count; i++) {
            char *p = nullptr;
            SDL_asprintf(&p, "%s/%s", path, n.names[i]);
            remove_tree(p);
            SDL_free(p);
            SDL_free(n.names[i]);
        }
    }
    SDL_RemovePath(path);
}

static bool exists(const char *rel)
{
    char *p = nullptr;
    SDL_asprintf(&p, "%s%s", PLAYER, rel);
    bool found = SDL_GetPathInfo(p, nullptr);
    SDL_free(p);
    return found;
}

static bool file_is(const char *rel, const void *data, size_t size)
{
    char *p = nullptr;
    SDL_asprintf(&p, "%s%s", PLAYER, rel);
    size_t n = 0;
    void *got = SDL_LoadFile(p, &n);
    bool same = got && n == size && SDL_memcmp(got, data, size) == 0;
    SDL_free(got);
    SDL_free(p);
    return same;
}

// The edition of the import in use, or EDITION_UNKNOWN if none is.
static Edition installed(void)
{
    ImportedData in;
    char *dir = import_data_dir(PLAYER, &in);
    bool found = dir != nullptr;
    SDL_free(dir);
    return found ? in.edition : EDITION_UNKNOWN;
}

// The languages of the import in use (IN_*), data/'s in *main.
static unsigned installed_languages(Language *main)
{
    ImportedData in = {};
    char *dir = import_data_dir(PLAYER, &in);
    SDL_free(dir);
    *main = in.language;
    return in.languages;
}

static bool run(const char *path, ImportReport *r)
{
    ImportProgress progress;
    bool done = import_game(path, PLAYER, files, FILE_COUNT, r, &progress);
    CHECK(done == r->ok);
    if (done)  // checked and copied, every path counted twice
        CHECK(SDL_GetAtomicInt(&progress.done) == SDL_GetAtomicInt(&progress.total)
              && SDL_GetAtomicInt(&progress.total) >= 10);
    if (!done && r->error)
        printf("  (%s)\n", r->error);
    return done;
}

static bool only_problem(const ImportReport *r, const char *text)
{
    bool same = r->problem_count == 1 && SDL_strcmp(r->problems[0], text) == 0;
    if (!same)
        for (int i = 0; i < r->problem_count; i++)
            printf("  problem: %s\n", r->problems[i]);
    return same;
}

int main(void)
{
    for (int i = 0; i < FILE_COUNT; i++) {
        files[i].size = 100 + 37 * (uint32_t)i;
        for (uint32_t k = 0; k < files[i].size; k++)
            contents[i][k] = (uint8_t)(k * 31 + i * 7);
        files[i].crc = gamedata_crc32(0, contents[i], files[i].size);
    }
    remove_tree(TMP);
    SDL_CreateDirectory(TMP);
    SDL_CreateDirectory(PLAYER);
    ImportReport r;

    // A folder, the game one level down. Slot 2 already holds a save.
    write_at(PLAYER, "save\\spidysav.2", "mine", 4);
    Copy floppy = make_copy("Reunion\\", EDITION_FLOPPY, LANGUAGE_ENGLISH);
    char *folder = write_folder("folder", &floppy);
    CHECK(run(folder, &r));
    CHECK(r.edition == EDITION_FLOPPY && SDL_strcmp(r.root, "Reunion") == 0);
    CHECK(r.problem_count == 0 && !r.error);
    CHECK(r.saves_copied == 1 && !r.save_skipped[1] && r.save_skipped[2]);
    CHECK(file_is("save/spidysav.1", save1, sizeof save1) && file_is("save/spidysav.2", "mine", 4));
    CHECK(file_is("data/GRAFIKA/MAIN.PIC", contents[2], files[2].size));
    CHECK(file_is("data/GRAFIKA/CHARSET1.PIC", contents[0], files[0].size));
    CHECK(file_is("data/SAVE/INIT", contents[3], files[3].size));
    CHECK(file_is("data/TEXT/MESSAGE.TXT", contents[4], files[4].size));
    CHECK(!exists("data/SAVE/SPIDYSAV.1") && !exists("data/README.TXT") && !exists("data/GRWAR"));
    CHECK(!exists("data.partial") && !exists("data.old"));
    CHECK(installed() == EDITION_FLOPPY);
    Language main_lang;
    CHECK(r.languages == IN_ENGLISH && r.language == LANGUAGE_ENGLISH);
    CHECK(installed_languages(&main_lang) == IN_ENGLISH && main_lang == LANGUAGE_ENGLISH);
    char *dir = import_data_dir(PLAYER, nullptr);
    CHECK(dir && SDL_strcmp(dir, "test_import.tmp/player/data") == 0);
    SDL_free(dir);
    import_report_free(&r);

    // A ZIP of the CD's files, the game at the top, replaces it.
    write_at(PLAYER, "data\\OLD", "x", 1);
    Copy cd = make_copy("", EDITION_CD, LANGUAGE_ENGLISH);
    char *zip = write_zip("cd.zip", &cd);
    CHECK(run(zip, &r));
    CHECK(r.edition == EDITION_CD && SDL_strcmp(r.root, "") == 0);
    CHECK(r.saves_copied == 0 && r.save_skipped[1] && r.save_skipped[2]);
    CHECK(file_is("data/GRAFIKA/CHARSET1.PIC", contents[1], files[1].size));
    CHECK(!exists("data/OLD") && !exists("data.old"));
    CHECK(installed() == EDITION_CD);
    import_report_free(&r);

    // An ISO with two languages: REUNION.ENG goes to data/, of REUNION.DEU
    // only what differs from it, to data/DEU. The German game's MAIN.PIC
    // differs, but the two share it: only the English one is checked.
    Copy deu = make_copy("REUNION.DEU\\", EDITION_CD, LANGUAGE_GERMAN);
    Copy eng = make_copy("REUNION.ENG\\", EDITION_CD, LANGUAGE_ENGLISH);
    uint8_t other[sizeof contents[2]];
    SDL_memcpy(other, contents[2], files[2].size);
    other[5] ^= 1;
    find(&deu, "MAIN.PIC")->data = other;
    char *iso = write_iso("cd.iso", &deu, &eng);
    CHECK(run(iso, &r));
    CHECK(r.edition == EDITION_CD && SDL_strcmp(r.root, "REUNION.ENG") == 0);
    CHECK(r.languages == (IN_ENGLISH | IN_GERMAN) && r.language == LANGUAGE_ENGLISH);
    CHECK(file_is("data/SAVE/INIT", contents[3], files[3].size));  // "INIT.;1" on the image
    CHECK(file_is("data/TEXT/MESSAGE.TXT", contents[4], files[4].size));
    CHECK(file_is("data/DEU/TEXT/MESSAGE.TXT", contents[5], files[5].size));
    CHECK(!exists("data/DEU/GRAFIKA") && !exists("data/DEU/WAR") && !exists("data/DEU/SAVE"));
    CHECK(installed() == EDITION_CD);
    CHECK(installed_languages(&main_lang) == (IN_ENGLISH | IN_GERMAN)
          && main_lang == LANGUAGE_ENGLISH);
    import_report_free(&r);

    // A German file that differs: named with its folder, nothing copied.
    find(&deu, "MESSAGE.TXT")->data = contents[4];
    find(&deu, "MESSAGE.TXT")->size = files[5].size;
    char *bad_iso = write_iso("bad.iso", &deu, &eng);
    CHECK(!run(bad_iso, &r));
    CHECK(only_problem(&r, "Different: REUNION.DEU\\TEXT\\MESSAGE.TXT"));
    CHECK(installed_languages(&main_lang) == (IN_ENGLISH | IN_GERMAN));
    import_report_free(&r);

    // A German game alone, in a folder of another name: its MESSAGE.TXT
    // tells the language, and it fills data/.
    Copy lone = make_copy("Reunion\\", EDITION_CD, LANGUAGE_GERMAN);
    char *lone_dir = write_folder("lone", &lone);
    CHECK(run(lone_dir, &r));
    CHECK(r.languages == IN_GERMAN && r.language == LANGUAGE_GERMAN);
    CHECK(file_is("data/TEXT/MESSAGE.TXT", contents[5], files[5].size) && !exists("data/DEU"));
    CHECK(installed_languages(&main_lang) == IN_GERMAN && main_lang == LANGUAGE_GERMAN);
    import_report_free(&r);

    // A missing file: named, and nothing is copied.
    Copy missing = make_copy("", EDITION_CD, LANGUAGE_ENGLISH);
    *find(&missing, "MESSAGE.TXT") = missing.e[--missing.count];
    char *missing_dir = write_folder("missing", &missing);
    CHECK(!run(missing_dir, &r));
    CHECK(only_problem(&r, "Missing: TEXT\\MESSAGE.TXT") && !r.error);
    CHECK(installed() == EDITION_CD && !exists("data.partial"));
    import_report_free(&r);

    // A different file: by its CRC in a folder, by the archive's CRC in a
    // ZIP, and by its size.
    Copy changed = make_copy("", EDITION_CD, LANGUAGE_ENGLISH);
    find(&changed, "MAIN.PIC")->data = other;
    char *changed_dir = write_folder("changed", &changed);
    CHECK(!run(changed_dir, &r));
    CHECK(only_problem(&r, "Different: GRAFIKA\\MAIN.PIC"));
    import_report_free(&r);
    char *changed_zip = write_zip("changed.zip", &changed);
    CHECK(!run(changed_zip, &r));
    CHECK(only_problem(&r, "Different: GRAFIKA\\MAIN.PIC"));
    import_report_free(&r);
    find(&changed, "MAIN.PIC")->data = contents[2];
    find(&changed, "MESSAGE.TXT")->size--;
    char *short_dir = write_folder("short", &changed);
    CHECK(!run(short_dir, &r));
    CHECK(only_problem(&r, "Different: TEXT\\MESSAGE.TXT"));
    CHECK(installed() == EDITION_CD);
    import_report_free(&r);

    // No game: a folder without GRAFIKA, a file that is no archive.
    char *empty = path_of("empty");
    SDL_CreateDirectory(empty);
    CHECK(!run(empty, &r) && r.error && r.problem_count == 0);
    import_report_free(&r);
    char *junk = path_of("junk.bin");
    CHECK(SDL_SaveFile(junk, extra, sizeof extra));
    CHECK(!run(junk, &r) && r.error);
    import_report_free(&r);

    // An import that never finished: data.partial is never used, and the
    // next import clears it. data/ without its marker isn't used either.
    remove_tree("test_import.tmp/player/data");
    write_at(PLAYER, "data.partial\\GRAFIKA\\MAIN.PIC", "half", 4);
    write_at(PLAYER, "data\\GRAFIKA\\MAIN.PIC", "half", 4);
    CHECK(installed() == EDITION_UNKNOWN);
    CHECK(run(folder, &r));
    CHECK(file_is("data/GRAFIKA/MAIN.PIC", contents[2], files[2].size) && !exists("data.partial"));
    CHECK(installed() == EDITION_FLOPPY);
    import_report_free(&r);

    // An import made with another list version is not used.
    write_at(PLAYER, "data\\import.ini", "list = 0\nedition = floppy\n", 26);
    CHECK(installed() == EDITION_UNKNOWN);

    SDL_free(folder);
    SDL_free(zip);
    SDL_free(iso);
    SDL_free(bad_iso);
    SDL_free(lone_dir);
    SDL_free(missing_dir);
    SDL_free(changed_dir);
    SDL_free(changed_zip);
    SDL_free(short_dir);
    SDL_free(empty);
    SDL_free(junk);
    remove_tree(TMP);
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
