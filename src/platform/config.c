#include "platform/config.h"

#include <SDL3/SDL.h>

static constexpr Config DEFAULTS = {
    .music = CONFIG_MUSIC_SETUP,
    .effects = true,
    .music_volume = 100,
    .effects_volume = 100,
    .display = CONFIG_WINDOWED,
    .window_width = 1280,  // 320x200 shown 4:3, times 4
    .window_height = 960,
};

Config config = DEFAULTS;

static char *path;

static const char *const MUSIC_NAMES[] = {"setup", "none", "adlib", "gus", "amiga"};
static const char *const DISPLAY_NAMES[] = {"off", "on", "desktop"};

const char *config_music_name(ConfigMusic music)
{
    return MUSIC_NAMES[music];
}

void config_reset(void)
{
    config = DEFAULTS;
    SDL_free(path);
    path = nullptr;
}

char *config_default_path(void)
{
    char *pref = SDL_GetPrefPath("Reunion", "Reunion");
    if (!pref)
        return nullptr;
    char *out = nullptr;
    SDL_asprintf(&out, "%sreunion.ini", pref);
    SDL_free(pref);
    return out;
}

static int fix_index(const char *key)
{
    for (int i = 0; i < config.fix_count; i++)
        if (SDL_strcmp(config.fix_keys[i], key) == 0)
            return i;
    return -1;
}

bool config_fix(const char *key)
{
    int i = fix_index(key);
    return i < 0 || config.fix_on[i];
}

void config_set_fix(const char *key, bool on)
{
    int i = fix_index(key);
    if (i < 0) {
        if (config.fix_count == CONFIG_MAX_FIXES || SDL_strlen(key) >= CONFIG_KEY_SIZE)
            return;
        i = config.fix_count++;
        SDL_strlcpy(config.fix_keys[i], key, CONFIG_KEY_SIZE);
    }
    config.fix_on[i] = on;
}

// --- Reading ------------------------------------------------------------------

static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    char *end = s + SDL_strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
        *--end = '\0';
    return s;
}

static bool parse_bool(const char *v, bool *out)
{
    static const char *const yes[] = {"on", "true", "yes", "1"};
    static const char *const no[] = {"off", "false", "no", "0"};
    for (int i = 0; i < 4; i++) {
        if (SDL_strcasecmp(v, yes[i]) == 0) {
            *out = true;
            return true;
        }
        if (SDL_strcasecmp(v, no[i]) == 0) {
            *out = false;
            return true;
        }
    }
    return false;
}

static void parse_int(const char *v, int *out, int lo, int hi)
{
    char *end;
    long n = SDL_strtol(v, &end, 10);
    if (end != v && !*end && n >= lo && n <= hi)
        *out = (int)n;
}

static void parse_name(const char *v, int *out, const char *const *names, int count)
{
    for (int i = 0; i < count; i++)
        if (SDL_strcasecmp(v, names[i]) == 0)
            *out = i;
}

static void set_value(const char *section, const char *key, const char *v)
{
    bool b;
    if (SDL_strcasecmp(section, "fixes") == 0) {
        if (parse_bool(v, &b))
            config_set_fix(key, b);
        return;
    }
#define IS(s, k) (SDL_strcasecmp(section, s) == 0 && SDL_strcasecmp(key, k) == 0)
    if (IS("cheats", "freeze_money") && parse_bool(v, &b))
        config.freeze_money = b;
    else if (IS("cheats", "freeze_ore") && parse_bool(v, &b))
        config.freeze_ore = b;
    else if (IS("sound", "music"))
        parse_name(v, (int *)&config.music, MUSIC_NAMES, (int)SDL_arraysize(MUSIC_NAMES));
    else if (IS("sound", "effects") && parse_bool(v, &b))
        config.effects = b;
    else if (IS("sound", "music_volume"))
        parse_int(v, &config.music_volume, 0, 100);
    else if (IS("sound", "effects_volume"))
        parse_int(v, &config.effects_volume, 0, 100);
    else if (IS("video", "fullscreen"))
        parse_name(v, (int *)&config.display, DISPLAY_NAMES, (int)SDL_arraysize(DISPLAY_NAMES));
    else if (IS("video", "window_width"))
        parse_int(v, &config.window_width, 320, 16384);
    else if (IS("video", "window_height"))
        parse_int(v, &config.window_height, 240, 16384);
    else if (IS("video", "fullscreen_width"))
        parse_int(v, &config.fullscreen_width, 0, 16384);
    else if (IS("video", "fullscreen_height"))
        parse_int(v, &config.fullscreen_height, 0, 16384);
    else if (IS("video", "fullscreen_refresh"))
        config.fullscreen_refresh = SDL_clamp((float)SDL_atof(v), 0.0f, 1000.0f);
    else if (IS("game", "skip_intro") && parse_bool(v, &b))
        config.skip_intro = b;
#undef IS
}

void config_load(const char *file)
{
    SDL_free(path);
    path = file ? SDL_strdup(file) : nullptr;
    // With a config file the port picks the music; AdLib is what the
    // shipped SETUP.SPD gives.
    if (config.music == CONFIG_MUSIC_SETUP)
        config.music = CONFIG_MUSIC_ADLIB;
    size_t size;
    char *text = path ? SDL_LoadFile(path, &size) : nullptr;
    if (!text)
        return;
    char section[32] = "";
    char *save = nullptr;
    for (char *line = SDL_strtok_r(text, "\n", &save); line; line = SDL_strtok_r(nullptr, "\n", &save)) {
        char *semi = SDL_strchr(line, ';');
        if (semi)
            *semi = '\0';
        char *s = trim(line);
        if (*s == '#' || !*s)
            continue;
        if (*s == '[') {
            char *end = SDL_strchr(s, ']');
            if (end) {
                *end = '\0';
                SDL_strlcpy(section, trim(s + 1), sizeof section);
            }
            continue;
        }
        char *eq = SDL_strchr(s, '=');
        if (!eq)
            continue;
        *eq = '\0';
        set_value(section, trim(s), trim(eq + 1));
    }
    SDL_free(text);
    // The menu doesn't offer "as the setup file says".
    if (config.music == CONFIG_MUSIC_SETUP)
        config.music = CONFIG_MUSIC_ADLIB;
}

// --- Writing ------------------------------------------------------------------

static const char *on_off(bool b)
{
    return b ? "on" : "off";
}

bool config_save(void)
{
    if (!path)
        return true;
    char *tmp = nullptr;
    SDL_asprintf(&tmp, "%s.tmp", path);
    SDL_IOStream *io = tmp ? SDL_IOFromFile(tmp, "w") : nullptr;
    if (!io) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Config %s: %s", path, SDL_GetError());
        SDL_free(tmp);
        return false;
    }
    bool ok = SDL_IOprintf(io,
                           "; Reunion port settings (F1 in the game). Rewritten by the game.\n"
                           "\n[cheats]\n"
                           "freeze_money = %s\n"
                           "freeze_ore = %s\n"
                           "\n[sound]\n"
                           "music = %s ; none | adlib | gus | amiga\n"
                           "effects = %s\n"
                           "music_volume = %d\n"
                           "effects_volume = %d\n"
                           "\n[video]\n"
                           "fullscreen = %s ; off | on | desktop\n"
                           "window_width = %d\n"
                           "window_height = %d\n"
                           "fullscreen_width = %d ; 0: the desktop's resolution\n"
                           "fullscreen_height = %d\n"
                           "fullscreen_refresh = %g\n"
                           "\n[game]\n"
                           "skip_intro = %s\n"
                           "\n[fixes]\n",
                           on_off(config.freeze_money), on_off(config.freeze_ore),
                           MUSIC_NAMES[config.music], on_off(config.effects), config.music_volume,
                           config.effects_volume, DISPLAY_NAMES[config.display],
                           config.window_width, config.window_height, config.fullscreen_width,
                           config.fullscreen_height, (double)config.fullscreen_refresh,
                           on_off(config.skip_intro)) > 0;
    for (int i = 0; ok && i < config.fix_count; i++)
        ok = SDL_IOprintf(io, "%s = %s\n", config.fix_keys[i], on_off(config.fix_on[i])) > 0;
    ok = SDL_CloseIO(io) && ok;
    ok = ok && SDL_RenamePath(tmp, path);
    if (!ok) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Config %s: %s", path, SDL_GetError());
        SDL_RemovePath(tmp);
    }
    SDL_free(tmp);
    return ok;
}
