// import_data: imports the real copies, the floppy release (argv[1]) and,
// when vendor/ holds one, the CD image (*.iso next to the floppy folder)
// with its four languages, each of which must load its own files; then the
// CD's font must draw the floppy's characters exactly as the floppy's does,
// and the CD's accents.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/dseg.h"
#include "game/gfx.h"
#include "game/system.h"
#include "platform/fs.h"
#include "platform/gamedata.h"
#include "platform/import.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static const char TMP[] = "test_import_data.tmp";
static const char PLAYER[] = "test_import_data.tmp/";

static bool no_retrace(void)
{
    return true;
}

static SDL_EnumerationResult remove_entry(void *userdata, const char *dirname, const char *fname);

static void remove_tree(const char *path)
{
    SDL_PathInfo info;
    if (SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY)
        SDL_EnumerateDirectory(path, remove_entry, nullptr);
    SDL_RemovePath(path);
}

static SDL_EnumerationResult remove_entry(void *userdata, const char *dirname, const char *fname)
{
    (void)userdata;
    char *p = nullptr;
    SDL_asprintf(&p, "%s%s", dirname, fname);
    remove_tree(p);
    SDL_free(p);
    return SDL_ENUM_CONTINUE;
}

// Every listed path loads, in `language`'s version.
static void check_loads(Language language)
{
    int paths = 0, loaded = 0;
    for (int i = 0; i < gamedata_file_count; i++) {
        if (i > 0 && SDL_strcmp(gamedata_files[i].path, gamedata_files[i - 1].path) == 0)
            continue;
        paths++;
        size_t size = 0;
        uint8_t *data = fs_load(gamedata_files[i].path, &size);
        uint32_t crc = data ? gamedata_crc32(0, data, size) : 0;
        bool found = false;
        for (int k = i; k < gamedata_file_count && !found
                        && SDL_strcmp(gamedata_files[k].path, gamedata_files[i].path) == 0; k++)
            found = data && gamedata_in_language(&gamedata_files[k], language)
                    && gamedata_files[k].size == size && gamedata_files[k].crc == crc;
        if (!found)
            printf("  %s: not %s's\n", gamedata_files[i].path, gamedata_language_name(language));
        loaded += found;
        SDL_free(data);
    }
    CHECK(loaded == paths);
    CHECK(gamedata_language() == language);
}

// Imports `path` and checks the edition, the root, the languages and that
// every listed file loads from the import in each language.
static void import(const char *path, Edition edition, const char *root, unsigned languages)
{
    ImportReport r;
    CHECK(import_game(path, PLAYER, gamedata_files, gamedata_file_count, &r, nullptr));
    for (int i = 0; i < r.problem_count; i++)
        printf("  %s\n", r.problems[i]);
    if (r.error)
        printf("  %s\n", r.error);
    CHECK(r.edition == edition && r.root && SDL_strcmp(r.root, root) == 0);
    CHECK(r.languages == languages && r.language == LANGUAGE_ENGLISH);
    import_report_free(&r);
    ImportedData marked = {};
    char *dir = import_data_dir(PLAYER, &marked);
    CHECK(dir && marked.edition == edition && marked.languages == languages);
    CHECK(dir && fs_init(dir) && gamedata_edition() == edition);
    SDL_free(dir);
    for (Language l = 0; l < LANGUAGE_COUNT; l++) {
        if (!(languages & 1u << l))
            continue;
        char *lang_dir = nullptr;
        if (l != LANGUAGE_ENGLISH)
            SDL_asprintf(&lang_dir, "%sdata/%s", PLAYER, gamedata_language_folder(l));
        CHECK(fs_set_language_dir(lang_dir));
        SDL_free(lang_dir);
        check_loads(l);
    }
    fs_set_language_dir(nullptr);
}

// The CD image in `dir`, if there is one.
static SDL_EnumerationResult find_iso(void *userdata, const char *dirname, const char *fname)
{
    size_t n = SDL_strlen(fname);
    if (n > 4 && SDL_strcasecmp(fname + n - 4, ".iso") == 0) {
        SDL_asprintf((char **)userdata, "%s%s", dirname, fname);
        return SDL_ENUM_SUCCESS;
    }
    return SDL_ENUM_CONTINUE;
}

static void setup_text(bool cd)
{
    dseg_clear();
    vid_clear();
    dseg_init();
    system_init();
    ds.Gfx_LineStride = 320;
    ds.Gfx_LineStrideCopy = 320;
    ds.Mouse_CursorState = 1;
    gfx_wait_retrace = no_retrace;
    ds.Gfx_Charset = game_alloc(10368);
    static uint8_t pal[0x300];
    Gfx_LoadPic("grafika\\charset1", ds.Gfx_Charset + 0x80, 1, 0x20, 0, 0, 320, false, pal, 0, 2,
                0, 0, 2);
    gfx_set_cd_charset(cd);
}

// Every character of the floppy's charset through both text routines, and
// on the floppy one that isn't in it through DrawText6x8 (blank; the CD
// draws é, cd_accents; TypeText would read glyph 255, past the sheet), from
// the font sheet of the data in the fs root.
static void draw_text(bool cd)
{
    setup_text(cd);
    char chars[80];
    SDL_memcpy(chars, ds.Gfx_CharsetChars.s, 78);
    chars[78] = '\0';
    CHECK(!SDL_strchr(chars, '~'));
    for (int line = 0; line < 2; line++) {
        char s[42] = {0};
        SDL_strlcpy(s, chars + line * 39, 40);
        Gfx_TypeText(s, 4, (int16_t)(40 + 10 * line), 50, 0x20);
        if (line && !cd)
            s[39] = '~';
        Gfx_DrawText6x8(s, 4, (int16_t)(10 + 10 * line), 50, 0x10);
    }
    gfx_set_cd_charset(false);
}

// The 6x8 cell drawn at (x, y).
static void cell(int x, int y, uint8_t out[48])
{
    for (int r = 0; r < 8; r++)
        SDL_memcpy(out + r * 6, &vid_screen[y + r][x], 6);
}

// Do a and b draw the same cell? With `typed`, a through TypeText.
static bool same_cells(const char *a, const char *b, bool typed)
{
    uint8_t ca[48], cb[48];
    vid_clear();
    if (typed)
        Gfx_TypeText(a, 4, 10, 1, 0x10);
    else
        Gfx_DrawText6x8(a, 4, 10, 1, 0x10);
    Gfx_DrawText6x8(b, 4, 20, 1, 0x10);
    cell(4, 10, ca);
    cell(4, 20, cb);
    return SDL_memcmp(ca, cb, sizeof ca) == 0;
}

// The CD's font: character 1 is ß, a character it lacks draws as é, and
// the French and Italian accents (code page 437) draw with their own
// glyphs, alike in both routines, all different but the three drawn with
// the nearest one.
static void cd_accents(void)
{
    setup_text(true);
    CHECK(same_cells("\x01", "\xE1", false));
    CHECK(same_cells("~", "\x82", false));
    CHECK(!same_cells("~", " ", false));
    static const char ACCENTS[] = "\x80\x82\x83\x85\x87\x88\x8A\x8B\x8C\x8D\x90\x93\x95\x96\x97"
                                  "\x8E\x99\x9A\x84\x94\x81\xE1";
    for (int i = 0; ACCENTS[i]; i++) {
        char a[2] = {ACCENTS[i], 0};
        CHECK(same_cells(a, a, true));
        for (int k = 0; k < i; k++) {
            char b[2] = {ACCENTS[k], 0};
            bool alike = (a[0] == '\x85' && b[0] == '\x83') || (a[0] == '\x8D' && b[0] == '\x8C');
            if (same_cells(a, b, false) != alike)
                printf("  %02x and %02x\n", (uint8_t)a[0], (uint8_t)b[0]);
            CHECK(same_cells(a, b, false) == alike);
        }
    }
    CHECK(same_cells("\x8B", "i", false));
    gfx_set_cd_charset(false);
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    char *parent = nullptr, *iso = nullptr;
    SDL_asprintf(&parent, "%s/..", argv[1]);
    SDL_EnumerateDirectory(parent, find_iso, &iso);
    SDL_free(parent);

    remove_tree(TMP);
    SDL_CreateDirectory(TMP);
    import(argv[1], EDITION_FLOPPY, "", IN_ENGLISH);
    static uint8_t floppy[200][320];
    draw_text(false);
    SDL_memcpy(floppy, vid_screen, sizeof floppy);
    CHECK(SDL_memcmp(floppy[12], floppy[200 - 1], 320) != 0);  // the text drew something

    if (iso) {
        import(iso, EDITION_CD, "REUNION.ENG", IN_ENGLISH | IN_GERMAN | IN_FRENCH | IN_ITALIAN);
        draw_text(true);
        CHECK(SDL_memcmp(floppy, vid_screen, sizeof floppy) == 0);
        // The CD's sheet in the floppy's layout draws other pixels.
        draw_text(false);
        CHECK(SDL_memcmp(floppy, vid_screen, sizeof floppy) != 0);
        cd_accents();
    } else {
        printf("no CD image next to %s, skipping it\n", argv[1]);
    }
    SDL_free(iso);

    fs_shutdown();
    remove_tree(TMP);
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
