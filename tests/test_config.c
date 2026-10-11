// config: the defaults (no file), loading with bad and unknown entries,
// saving and loading back, and the fixes' switches.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "platform/config.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static void write_file(const char *path, const char *text)
{
    CHECK(SDL_SaveFile(path, text, SDL_strlen(text)));
}

int main(void)
{
    char dir[128], path[256];
    SDL_snprintf(dir, sizeof dir, "/tmp/reunion_test_config_%d", (int)SDL_GetCurrentTime(nullptr) & 0xFFFF);
    SDL_snprintf(path, sizeof path, "%s/reunion.ini", dir);
    CHECK(SDL_CreateDirectory(dir));

    // Without a file: the game as before (music from SETUP.SPD, all on).
    config_reset();
    CHECK(config.music == CONFIG_MUSIC_SETUP && config.effects && config.music_volume == 100);
    CHECK(config.display == CONFIG_WINDOWED && config.window_width == 1280 && config.window_height == 960);
    CHECK(!config.freeze_money && !config.skip_intro);
    CHECK(config_fix("anything"));
    CHECK(config_save());  // no path: nothing written
    CHECK(!SDL_GetPathInfo(path, nullptr));

    // A missing file: the app's defaults, AdLib music.
    config_load(path);
    CHECK(config.music == CONFIG_MUSIC_ADLIB);

    // Bad values keep the defaults; unknown keys and sections are ignored.
    write_file(path, "; comment\n"
                     "[cheats]\n"
                     "freeze_money = on\n"
                     "freeze_ore = maybe\n"
                     "[sound]\r\n"
                     "  music = Amiga ; inline comment\n"
                     "effects=off\n"
                     "music_volume = 140\n"
                     "effects_volume = 30\n"
                     "[video]\n"
                     "fullscreen = desktop\n"
                     "window_width = 960\n"
                     "window_height = x\n"
                     "fullscreen_refresh = 59.94\n"
                     "[game]\n"
                     "skip_intro = yes\n"
                     "colour = blue\n"
                     "[other]\n"
                     "freeze_money = off\n"
                     "[fixes]\n"
                     "find_jade = off\n"
                     "mine_all_systems = on\n"
                     "no equals sign\n");
    config_reset();
    config_load(path);
    CHECK(config.freeze_money && !config.freeze_ore);
    CHECK(config.music == CONFIG_MUSIC_AMIGA && !config.effects);
    CHECK(config.music_volume == 100 && config.effects_volume == 30);
    CHECK(config.display == CONFIG_DESKTOP && config.window_width == 960 && config.window_height == 960);
    CHECK(SDL_fabsf(config.fullscreen_refresh - 59.94f) < 0.001f);
    CHECK(config.skip_intro);
    CHECK(!config_fix("find_jade") && config_fix("mine_all_systems") && config_fix("unlisted"));

    // Saved and loaded back unchanged.
    config_set_fix("drifting_ship", false);
    config.fullscreen_width = 1920;
    config.fullscreen_height = 1080;
    Config before = config;
    CHECK(config_save());
    CHECK(SDL_GetPathInfo(path, nullptr));
    config_reset();
    config_load(path);
    CHECK(config.freeze_money == before.freeze_money && config.music == before.music);
    CHECK(config.effects_volume == before.effects_volume && config.display == before.display);
    CHECK(config.fullscreen_width == 1920 && config.fullscreen_height == 1080);
    CHECK(SDL_fabsf(config.fullscreen_refresh - before.fullscreen_refresh) < 0.001f);
    CHECK(config.fix_count == 3 && !config_fix("drifting_ship") && !config_fix("find_jade"));

    // "setup" is not something the menu offers: it reads as AdLib.
    write_file(path, "[sound]\nmusic = setup\n");
    config_reset();
    config_load(path);
    CHECK(config.music == CONFIG_MUSIC_ADLIB);

    config_reset();
    char tmp[300];
    SDL_snprintf(tmp, sizeof tmp, "%s.tmp", path);
    CHECK(!SDL_GetPathInfo(tmp, nullptr));
    SDL_RemovePath(path);
    SDL_RemovePath(dir);
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
