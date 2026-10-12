// import_data: imports the real copies, the floppy release (argv[1]) and,
// when vendor/ holds one, the CD image (*.iso next to the floppy folder);
// then the CD's font sheet must draw text exactly as the floppy's does.
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

// Imports `path` and checks the edition, the root and that every listed
// file loads from the import.
static void import(const char *path, Edition edition, const char *root)
{
    ImportReport r;
    CHECK(import_game(path, PLAYER, gamedata_files, gamedata_file_count, &r, nullptr));
    for (int i = 0; i < r.problem_count; i++)
        printf("  %s\n", r.problems[i]);
    if (r.error)
        printf("  %s\n", r.error);
    CHECK(r.edition == edition && r.root && SDL_strcmp(r.root, root) == 0);
    import_report_free(&r);
    Edition marked = EDITION_UNKNOWN;
    char *dir = import_data_dir(PLAYER, &marked);
    CHECK(dir && marked == edition && fs_init(dir) && gamedata_edition() == edition);
    SDL_free(dir);
    int loaded = 0;
    for (int i = 0; i < gamedata_file_count; i++) {
        size_t size = 0;
        uint8_t *data = fs_load(gamedata_files[i].path, &size);
        loaded += data && gamedata_crc32(0, data, size) == gamedata_files[i].crc;
        SDL_free(data);
    }
    // Every path but the other edition's GRAFIKA\CHARSET1.PIC.
    CHECK(loaded == gamedata_file_count - 1);
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

// Every character of the charset through both text routines, and one that
// isn't in it through DrawText6x8 (TypeText would read glyph 255, past the
// sheet), from the font sheet of the data in the fs root.
static void draw_text(bool cd)
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
    char chars[80];
    SDL_memcpy(chars, ds.Gfx_CharsetChars.s, 78);
    chars[78] = '\0';
    CHECK(!SDL_strchr(chars, '~'));
    for (int line = 0; line < 2; line++) {
        char s[42] = {0};
        SDL_strlcpy(s, chars + line * 39, 40);
        Gfx_TypeText(s, 4, (int16_t)(40 + 10 * line), 50, 0x20);
        if (line)
            s[39] = '~';
        Gfx_DrawText6x8(s, 4, (int16_t)(10 + 10 * line), 50, 0x10);
    }
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
    import(argv[1], EDITION_FLOPPY, "");
    static uint8_t floppy[200][320];
    draw_text(false);
    SDL_memcpy(floppy, vid_screen, sizeof floppy);
    CHECK(SDL_memcmp(floppy[12], floppy[200 - 1], 320) != 0);  // the text drew something

    if (iso) {
        import(iso, EDITION_CD, "REUNION.ENG");
        draw_text(true);
        CHECK(SDL_memcmp(floppy, vid_screen, sizeof floppy) == 0);
        // The CD's sheet in the floppy's layout draws other pixels.
        draw_text(false);
        CHECK(SDL_memcmp(floppy, vid_screen, sizeof floppy) != 0);
    } else {
        printf("no CD image next to %s, skipping it\n", argv[1]);
    }
    SDL_free(iso);

    fs_shutdown();
    remove_tree(TMP);
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
