// playerdir: the player's folder and the move of the saves from the folder
// earlier builds used (SDL_GetPrefPath("Amnesty Design", "Reunion")/save/),
// which keeps a save whose slot is taken and removes the old folders once
// they are empty.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "platform/playerdir.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static const char TMP[] = "test_playerdir.tmp";

static bool file_is(const char *path, const char *text)
{
    size_t n = 0;
    char *got = SDL_LoadFile(path, &n);
    bool same = got && n == SDL_strlen(text) && SDL_memcmp(got, text, n) == 0;
    SDL_free(got);
    return same;
}

static void put(const char *path, const char *text)
{
    CHECK(SDL_SaveFile(path, text, SDL_strlen(text)));
}

static bool exists(const char *path)
{
    return SDL_GetPathInfo(path, nullptr);
}

int main(void)
{
    // Laid out as pref paths are: <base>/<org>/<app>/.
    SDL_CreateDirectory("test_playerdir.tmp/Reunion/Reunion/save");
    SDL_CreateDirectory("test_playerdir.tmp/Amnesty Design/Reunion/save");
    playerdir_set("test_playerdir.tmp/Reunion/Reunion");
    CHECK(SDL_strcmp(playerdir(), "test_playerdir.tmp/Reunion/Reunion/") == 0);
    char *ini = playerdir_path("reunion.ini");
    CHECK(ini && SDL_strcmp(ini, "test_playerdir.tmp/Reunion/Reunion/reunion.ini") == 0);
    SDL_free(ini);

    put("test_playerdir.tmp/Amnesty Design/Reunion/save/spidysav.1", "old 1");
    put("test_playerdir.tmp/Amnesty Design/Reunion/save/spidysav.3", "old 3");
    put("test_playerdir.tmp/Reunion/Reunion/save/spidysav.3", "new 3");
    playerdir_migrate();
    CHECK(file_is("test_playerdir.tmp/Reunion/Reunion/save/spidysav.1", "old 1"));
    CHECK(file_is("test_playerdir.tmp/Reunion/Reunion/save/spidysav.3", "new 3"));
    CHECK(!exists("test_playerdir.tmp/Amnesty Design/Reunion/save/spidysav.1"));
    CHECK(file_is("test_playerdir.tmp/Amnesty Design/Reunion/save/spidysav.3", "old 3"));

    // Once the player has moved the last one, the old folders go.
    SDL_RemovePath("test_playerdir.tmp/Amnesty Design/Reunion/save/spidysav.3");
    playerdir_migrate();
    CHECK(!exists("test_playerdir.tmp/Amnesty Design"));
    CHECK(file_is("test_playerdir.tmp/Reunion/Reunion/save/spidysav.1", "old 1"));
    // And without them there is nothing to do.
    playerdir_migrate();
    CHECK(!exists("test_playerdir.tmp/Amnesty Design"));

    SDL_RemovePath("test_playerdir.tmp/Reunion/Reunion/save/spidysav.1");
    SDL_RemovePath("test_playerdir.tmp/Reunion/Reunion/save/spidysav.3");
    SDL_RemovePath("test_playerdir.tmp/Reunion/Reunion/save");
    SDL_RemovePath("test_playerdir.tmp/Reunion/Reunion");
    SDL_RemovePath("test_playerdir.tmp/Reunion");
    SDL_RemovePath(TMP);
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
