#include "platform/menu.h"

#include <SDL3/SDL.h>

#include "platform/audio.h"
#include "platform/config.h"
#include "platform/importscreen.h"
#include "platform/input.h"
#include "platform/panel.h"
#include "platform/video.h"

// Layout, in the panel's 320x240 space (platform/panel.h).
enum : int {
    CHAR = PANEL_CHAR,
    TAB_X = 12,
    TAB_W = 100,
    TAB_H = 14,
    TOP_Y = PANEL_TOP,
    SPLIT_X = 112,
    ROW_X = 116,
    ROW_W = 192,
    ROW_H = 13,
    VISIBLE_ROWS = 12,
    HELP_Y = 198,
    HELP_COLS = 36,
    MAX_ROWS = 48,
    MAX_SIZES = 32,
};

typedef enum : int {
    SEC_CHEATS,
    SEC_SOUND,
    SEC_VIDEO,
    SEC_GAME,
    SEC_IMPROVEMENTS,
    SEC_KEYS,
    SEC_FIXES,
    SEC_SYSTEM,
    SECTIONS,
} Section;

static const char *const SECTION_NAMES[SECTIONS] = {"Cheats", "Sound", "Video", "Game",
                                                    "Improvements", "Keys", "Fixes", "System"};

typedef enum : int { ROW_TOGGLE, ROW_CHOICE, ROW_ACTION, ROW_NOTE } RowKind;

typedef enum : int {
    R_FREEZE_MONEY,
    R_FREEZE_ORE,
    R_ADD_MONEY,
    R_ADD_ORE,
    R_MUSIC,
    R_MUSIC_VOLUME,
    R_EFFECTS,
    R_EFFECTS_VOLUME,
    R_DISPLAY,
    R_SIZE,
    R_SKIP_INTRO,
    R_COLONY_STATS,
    R_DETOXIN_RATE,
    R_LANGUAGE,
    R_IMPROVEMENTS_ON,
    R_IMPROVEMENTS_OFF,
    R_FIXES_ON,
    R_FIXES_OFF,
    R_IMPORT,
    R_RESTART,
    R_QUIT,
    R_NOTE,
    R_KEY,
    R_IMPROVEMENT,  // + the improvement's index
    R_FIX = R_IMPROVEMENT + CONFIG_MAX_SWITCHES,  // + the fix's index
} RowId;

typedef struct Row {
    RowKind kind;
    int id;
    bool on;       // toggle
    bool enabled;
    char label[32];
    char value[24];  // choice
    const char *help;
} Row;

static MenuHooks hooks;
static bool open;
static Section section;
static int focus = -1;  // row, or -1: the section list
static int scroll[SECTIONS];
static char status[48];  // replaces the help line until the focus moves
static Row rows[MAX_ROWS];
static int row_count;
static char row_text[64];

void menu_set_hooks(const MenuHooks *h)
{
    hooks = *h;
}

static void changed(MenuChange c)
{
    if (hooks.changed)
        hooks.changed(c);
}

// --- Rows ---------------------------------------------------------------------

// The languages to choose from (IN_*), at least one.
static unsigned languages(void)
{
    unsigned in = hooks.languages ? hooks.languages() : 0;
    return in ? in : IN_ENGLISH;
}

// config.language if it can be had, else the one the game falls back on.
static Language shown_language(void)
{
    unsigned in = languages();
    if (in & 1u << config.language)
        return config.language;
    return (Language)SDL_MostSignificantBitIndex32(in & -in);
}

static Row *add(RowKind kind, int id, const char *label, const char *help)
{
    Row *r = &rows[row_count++];
    *r = (Row){.kind = kind, .id = id, .enabled = true, .help = help};
    SDL_strlcpy(r->label, label, sizeof r->label);
    return r;
}

static const char *music_label(ConfigMusic m)
{
    static const char *const names[] = {"Setup file", "None", "Sound Blaster", "UltraSound"};
    return names[m];
}

// Window sizes or full-screen modes, and which one is current (-1: none).
static int window_sizes[MAX_SIZES][2];
static SDL_DisplayMode modes[MAX_SIZES];

static int size_choices(int *current)
{
    *current = -1;
    if (config.display == CONFIG_WINDOWED) {
        int n = vid_window_sizes(window_sizes, MAX_SIZES);
        for (int i = 0; i < n; i++)
            if (window_sizes[i][0] == config.window_width && window_sizes[i][1] == config.window_height)
                *current = i;
        return n;
    }
    // Entry 0 is the desktop's resolution (width 0).
    int n = 1 + vid_fullscreen_modes(modes + 1, MAX_SIZES - 1);
    modes[0] = (SDL_DisplayMode){};
    for (int i = 0; i < n; i++)
        if (modes[i].w == config.fullscreen_width && modes[i].h == config.fullscreen_height
            && (i == 0 || config.fullscreen_refresh == 0
                || SDL_lroundf(modes[i].refresh_rate) == SDL_lroundf(config.fullscreen_refresh)))
            *current = i;
    return n;
}

static void size_text(char *out, size_t size)
{
    int current;
    size_choices(&current);
    if (config.display == CONFIG_WINDOWED)
        SDL_snprintf(out, size, "%dx%d", config.window_width, config.window_height);
    else if (config.fullscreen_width == 0)
        SDL_snprintf(out, size, "Desktop");
    else if (config.fullscreen_refresh > 0)
        SDL_snprintf(out, size, "%dx%d %ldHz", config.fullscreen_width, config.fullscreen_height,
                     SDL_lroundf(config.fullscreen_refresh));
    else
        SDL_snprintf(out, size, "%dx%d", config.fullscreen_width, config.fullscreen_height);
}

static void build_rows(void)
{
    row_count = 0;
    Row *r;
    bool running = hooks.game_running && hooks.game_running();
    char label[32];
    switch (section) {
    case SEC_CHEATS:
        r = add(ROW_TOGGLE, R_FREEZE_MONEY, "Freeze money",
                "Credits never fall: purchases cost nothing, income still counts.");
        r->on = config.freeze_money;
        r = add(ROW_TOGGLE, R_FREEZE_ORE, "Freeze ore",
                "New Earth's ore never falls: production uses none, deliveries count.");
        r->on = config.freeze_ore;
        SDL_snprintf(label, sizeof label, "Add %d credits", hooks.add_money);
        r = add(ROW_ACTION, R_ADD_MONEY, label, "Only during a game.");
        r->enabled = running;
        SDL_snprintf(label, sizeof label, "Add %d ore each", hooks.add_ore);
        r = add(ROW_ACTION, R_ADD_ORE, label, "To each of New Earth's six ores. Only during a game.");
        r->enabled = running;
        break;
    case SEC_SOUND:
        r = add(ROW_CHOICE, R_MUSIC, "Music",
                "Sound Blaster: FM music in the game. Cut-scenes sound alike on both.");
        SDL_strlcpy(r->value, music_label(config.music), sizeof r->value);
        r = add(ROW_CHOICE, R_MUSIC_VOLUME, "Music volume", nullptr);
        SDL_snprintf(r->value, sizeof r->value, "%d%%", config.music_volume);
        r = add(ROW_TOGGLE, R_EFFECTS, "Sound effects", nullptr);
        r->on = config.effects;
        r = add(ROW_CHOICE, R_EFFECTS_VOLUME, "Effects volume", nullptr);
        SDL_snprintf(r->value, sizeof r->value, "%d%%", config.effects_volume);
        r->enabled = config.effects;
        break;
    case SEC_VIDEO: {
        static const char *const names[] = {"Window", "Full screen", "Desktop"};
        r = add(ROW_CHOICE, R_DISPLAY, "Display",
                "Full screen changes the display's mode; Desktop keeps it. Alt+Enter toggles.");
        SDL_strlcpy(r->value, names[config.display], sizeof r->value);
        if (config.display != CONFIG_DESKTOP) {
            r = add(ROW_CHOICE, R_SIZE, "Size", nullptr);
            size_text(r->value, sizeof r->value);
        }
        break;
    }
    case SEC_GAME:
        r = add(ROW_TOGGLE, R_SKIP_INTRO, "Skip intro",
                "Start at the title screen, without the credits and intro.");
        r->on = config.skip_intro;
        r = add(ROW_TOGGLE, R_COLONY_STATS, "Colony stats",
                "Planet info: click POP for food, housing, health and demands.");
        r->on = config.colony_stats;
        r = add(ROW_TOGGLE, R_DETOXIN_RATE, "Detoxin rate",
                "Droids screen and Derrick info: all the Derricks' Detoxin T/PTP.");
        r->on = config.detoxin_rate;
        r = add(ROW_CHOICE, R_LANGUAGE, "Language", "The CD's games. From the next restart (System).");
        SDL_strlcpy(r->value, gamedata_language_label(shown_language()), sizeof r->value);
        r->enabled = (languages() & (languages() - 1)) != 0;  // more than one
        break;
    case SEC_IMPROVEMENTS:
        add(ROW_ACTION, R_IMPROVEMENTS_ON, "All on", "Changes to the original game's design.");
        add(ROW_ACTION, R_IMPROVEMENTS_OFF, "All off", "The original's design. The fixes stay as they are.");
        bool port_saves = false;
        for (int i = 0; i < hooks.improvement_count && row_count < MAX_ROWS; i++) {
            const MenuSwitch *f = &hooks.improvements[i];
            r = add(ROW_TOGGLE, R_IMPROVEMENT + i, f->label, f->help);
            r->on = config_improvement(f->key);
            port_saves |= r->on && f->port_saves;
        }
        if (port_saves && row_count + 2 <= MAX_ROWS) {
            static const char warning[] = "Saves made now can't be loaded by the DOS game.";
            add(ROW_NOTE, R_NOTE, "Saves made now can't be", warning)->enabled = false;
            add(ROW_NOTE, R_NOTE, "loaded by the DOS game.", warning)->enabled = false;
        }
        break;
    case SEC_KEYS: {
        static const char help_keys[] =
            "Keyboard play (Improvements). Change the keys in reunion.ini, [keys].";
        for (int i = 0; i < hooks.key_count && row_count < MAX_ROWS; i++) {
            r = add(ROW_NOTE, R_KEY, hooks.keys[i].label,
                    hooks.keys[i].help ? hooks.keys[i].help : help_keys);
            SDL_strlcpy(r->value, hooks.keys[i].keys, sizeof r->value);
        }
        break;
    }
    case SEC_FIXES:
        add(ROW_ACTION, R_FIXES_ON, "All on", "Fixes to bugs of the original game.");
        add(ROW_ACTION, R_FIXES_OFF, "All off (original)",
            "The original game, bugs and all: every fix and improvement off.");
        for (int i = 0; i < hooks.fix_count && row_count < MAX_ROWS; i++) {
            const MenuSwitch *f = &hooks.fixes[i];
            r = add(ROW_TOGGLE, R_FIX + i, f->label, f->help);
            r->on = config_fix(f->key);
        }
        break;
    case SEC_SYSTEM:
        add(ROW_ACTION, R_IMPORT, "Import game data...",
            "Replace the game data with another copy: a folder, a ZIP or the CD's ISO.");
        add(ROW_ACTION, R_RESTART, "Restart now", "Back to the title screen. Unsaved progress is lost.");
        add(ROW_ACTION, R_QUIT, "Quit now", "Unsaved progress is lost.");
        break;
    default:
        break;
    }
}

// --- Changes ------------------------------------------------------------------

static void apply_volumes(void)
{
    audio_set_user_volumes(config.music_volume / 100.0f,
                           config.effects ? config.effects_volume / 100.0f : 0.0f);
}

static int step(int v, int dir, int lo, int hi, bool wrap)
{
    v += dir;
    if (v < lo)
        return wrap ? hi : lo;
    if (v > hi)
        return wrap ? lo : hi;
    return v;
}

static void change_size(int dir)
{
    int current;
    int n = size_choices(&current);
    if (n == 0)
        return;
    int i;
    if (current >= 0) {
        i = step(current, dir, 0, n - 1, true);
    } else if (config.display == CONFIG_WINDOWED) {
        // A size set by hand: the next listed one up or down.
        int area = config.window_width * config.window_height;
        i = dir > 0 ? 0 : n - 1;
        for (int k = 0; k < n; k++) {
            int a = window_sizes[k][0] * window_sizes[k][1];
            if (dir > 0 && a <= area)
                i = SDL_min(k + 1, n - 1);
            if (dir < 0 && a < area)
                i = k;
        }
    } else {
        i = 0;
    }
    if (config.display == CONFIG_WINDOWED) {
        config.window_width = window_sizes[i][0];
        config.window_height = window_sizes[i][1];
    } else {
        config.fullscreen_width = modes[i].w;
        config.fullscreen_height = modes[i].h;
        config.fullscreen_refresh = modes[i].refresh_rate;
    }
    vid_apply_display();
}

static void close_menu(void)
{
    open = false;
}

// dir: -1 / +1 for a choice's previous / next value, 0 to activate.
static void act(const Row *r, int dir)
{
    if (!r->enabled)
        return;
    switch (r->id) {
    case R_FREEZE_MONEY:
        config.freeze_money = !config.freeze_money;
        changed(MENU_CHANGED_CHEATS);
        break;
    case R_FREEZE_ORE:
        config.freeze_ore = !config.freeze_ore;
        changed(MENU_CHANGED_CHEATS);
        break;
    case R_ADD_MONEY:
        changed(MENU_ADD_MONEY);
        SDL_snprintf(status, sizeof status, "Added %d credits.", hooks.add_money);
        return;
    case R_ADD_ORE:
        changed(MENU_ADD_ORE);
        SDL_snprintf(status, sizeof status, "Added %d of each ore.", hooks.add_ore);
        return;
    case R_MUSIC:
        config.music = step(config.music, dir ? dir : 1, CONFIG_MUSIC_NONE, CONFIG_MUSIC_GUS, true);
        changed(MENU_CHANGED_MUSIC);
        break;
    case R_MUSIC_VOLUME:
        config.music_volume = step(config.music_volume, (dir ? dir : 1) * 10, 0, 100, !dir);
        apply_volumes();
        break;
    case R_EFFECTS:
        config.effects = !config.effects;
        apply_volumes();
        break;
    case R_EFFECTS_VOLUME:
        config.effects_volume = step(config.effects_volume, (dir ? dir : 1) * 10, 0, 100, !dir);
        apply_volumes();
        break;
    case R_DISPLAY:
        config.display = step(config.display, dir ? dir : 1, CONFIG_WINDOWED, CONFIG_DESKTOP, true);
        vid_apply_display();
        break;
    case R_SIZE:
        change_size(dir ? dir : 1);
        break;
    case R_SKIP_INTRO:
        config.skip_intro = !config.skip_intro;
        break;
    case R_IMPROVEMENTS_ON:
    case R_IMPROVEMENTS_OFF:
        for (int i = 0; i < hooks.improvement_count; i++)
            config_set_improvement(hooks.improvements[i].key, r->id == R_IMPROVEMENTS_ON);
        changed(MENU_CHANGED_SWITCHES);
        break;
    case R_FIXES_ON:
    case R_FIXES_OFF:
        for (int i = 0; i < hooks.fix_count; i++)
            config_set_fix(hooks.fixes[i].key, r->id == R_FIXES_ON);
        // The original game has no improvements either.
        for (int i = 0; r->id == R_FIXES_OFF && i < hooks.improvement_count; i++)
            config_set_improvement(hooks.improvements[i].key, false);
        changed(MENU_CHANGED_SWITCHES);
        break;
    case R_COLONY_STATS:
        config.colony_stats = !config.colony_stats;
        break;
    case R_DETOXIN_RATE:
        config.detoxin_rate = !config.detoxin_rate;
        break;
    case R_LANGUAGE: {
        Language l = shown_language();
        do
            l = (Language)step(l, dir ? dir : 1, 0, LANGUAGE_COUNT - 1, true);
        while (!(languages() & 1u << l));
        config.language = l;
        break;
    }
    case R_IMPORT:
        importscreen_run(true);
        vid_set_overlay(menu_draw);
        if (input_quit_requested())
            close_menu();
        return;
    case R_RESTART:
        input_request_restart();
        close_menu();
        return;
    case R_QUIT:
        input_request_quit();
        close_menu();
        return;
    default:
        if (r->id >= R_FIX && r->id - R_FIX < hooks.fix_count) {
            const char *key = hooks.fixes[r->id - R_FIX].key;
            config_set_fix(key, !config_fix(key));
            changed(MENU_CHANGED_SWITCHES);
        } else if (r->id >= R_IMPROVEMENT && r->id - R_IMPROVEMENT < hooks.improvement_count) {
            const char *key = hooks.improvements[r->id - R_IMPROVEMENT].key;
            config_set_improvement(key, !config_improvement(key));
            changed(MENU_CHANGED_SWITCHES);
        }
        break;
    }
    config_save();
}

// --- Navigation ---------------------------------------------------------------

static void set_focus(int row)
{
    if (row != focus)
        status[0] = '\0';
    focus = row;
    if (focus >= 0) {
        if (focus < scroll[section])
            scroll[section] = focus;
        if (focus >= scroll[section] + VISIBLE_ROWS)
            scroll[section] = focus - VISIBLE_ROWS + 1;
    }
}

static void set_section(int s, bool keep_focus_on_rows)
{
    section = (Section)((s + SECTIONS) % SECTIONS);
    status[0] = '\0';
    build_rows();
    focus = keep_focus_on_rows && row_count > 0 ? 0 : -1;
    scroll[section] = 0;
}

void menu_open(void)
{
    open = true;
    status[0] = '\0';
    focus = -1;
    build_rows();
}

bool menu_is_open(void)
{
    return open;
}

static void key(SDL_Keycode k, SDL_Keymod mod)
{
    build_rows();
    const Row *r = focus >= 0 && focus < row_count ? &rows[focus] : nullptr;
    switch (k) {
    case SDLK_ESCAPE:
    case SDLK_F1:
        close_menu();
        break;
    case SDLK_TAB:
        set_section(section + ((mod & SDL_KMOD_SHIFT) ? -1 : 1), focus >= 0);
        break;
    case SDLK_PAGEUP:
        set_section(section - 1, focus >= 0);
        break;
    case SDLK_PAGEDOWN:
        set_section(section + 1, focus >= 0);
        break;
    case SDLK_UP:
        if (focus < 0)
            set_section(section - 1, false);
        else if (focus > 0)
            set_focus(focus - 1);
        break;
    case SDLK_DOWN:
        if (focus < 0)
            set_section(section + 1, false);
        else if (focus + 1 < row_count)
            set_focus(focus + 1);
        break;
    case SDLK_LEFT:
        if (r && r->kind == ROW_CHOICE)
            act(r, -1);
        else
            set_focus(-1);
        break;
    case SDLK_RIGHT:
        if (!r && row_count > 0)
            set_focus(0);
        else if (r && r->kind == ROW_CHOICE)
            act(r, 1);
        break;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
    case SDLK_SPACE:
        if (k == SDLK_RETURN && (mod & SDL_KMOD_ALT))
            vid_toggle_fullscreen();
        else if (r)
            act(r, 0);
        else if (row_count > 0)
            set_focus(0);
        break;
    default:
        break;
    }
}

// --- Drawing and the mouse ----------------------------------------------------

static int row_at(float x, float y)
{
    if (!panel_inside(x, y, ROW_X - 2, TOP_Y, ROW_W + 4, VISIBLE_ROWS * ROW_H))
        return -1;
    int i = (int)((y - TOP_Y) / ROW_H) + scroll[section];
    return i < row_count ? i : -1;
}

static int tab_at(float x, float y)
{
    if (!panel_inside(x, y, TAB_X - 2, TOP_Y, TAB_W, SECTIONS * TAB_H))
        return -1;
    return (int)((y - TOP_Y) / TAB_H);
}

static void mouse(const SDL_Event *e)
{
    float x, y;
    if (e->type == SDL_EVENT_MOUSE_WHEEL) {
        int max = SDL_max(row_count - VISIBLE_ROWS, 0);
        scroll[section] = SDL_clamp(scroll[section] - (int)e->wheel.y, 0, max);
        return;
    }
    if (e->type == SDL_EVENT_MOUSE_MOTION)
        panel_mouse(e->motion.x, e->motion.y, &x, &y);
    else
        panel_mouse(e->button.x, e->button.y, &x, &y);
    build_rows();
    int row = row_at(x, y), tab = tab_at(x, y);
    if (e->type == SDL_EVENT_MOUSE_MOTION) {
        if (row >= 0)
            set_focus(row);
        return;
    }
    if (e->type != SDL_EVENT_MOUSE_BUTTON_DOWN)
        return;
    if (e->button.button == SDL_BUTTON_RIGHT) {
        close_menu();
        return;
    }
    if (e->button.button != SDL_BUTTON_LEFT)
        return;
    if (tab >= 0) {
        set_section(tab, false);
    } else if (row >= 0) {
        set_focus(row);
        const Row *r = &rows[row];
        // A choice goes back on its "<", on anywhere else.
        int dir = 0;
        if (r->kind == ROW_CHOICE) {
            float value_x = ROW_X + ROW_W - (float)(SDL_strlen(r->value) + 2) * CHAR;
            dir = x >= value_x - CHAR / 2 && x < value_x + CHAR * 3 / 2 ? -1 : 1;
        }
        act(r, dir);
    } else if (!panel_inside(x, y, PANEL_X, PANEL_Y, PANEL_W, PANEL_H)) {
        close_menu();
    }
}

void menu_handle_event(const SDL_Event *e)
{
    switch (e->type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        input_request_quit();
        close_menu();
        break;
    case SDL_EVENT_KEY_DOWN:
        key(e->key.key, e->key.mod);
        break;
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_WHEEL:
        mouse(e);
        break;
    case SDL_EVENT_WINDOW_RESIZED:
        vid_window_event(e);
        break;
    default:
        break;
    }
    build_rows();
    if (focus >= row_count)
        focus = row_count - 1;
}

static void help(SDL_Renderer *r, const char *s)
{
    panel_wrap(r, TAB_X, HELP_Y, HELP_COLS, 10, PANEL_Y + PANEL_H - 1, s);
}

static void draw_row(SDL_Renderer *r, const Row *row, float y, bool focused)
{
    if (focused) {
        panel_color(r, 50, 70, 130, 255);
        panel_fill(r, ROW_X - 2, y - 3, ROW_W + 4, ROW_H);
    }
    if (!row->enabled)
        panel_color(r, 110, 110, 125, 255);
    else if (focused)
        panel_color(r, 255, 255, 255, 255);
    else
        panel_color(r, 215, 215, 228, 255);
    char buf[64];
    switch (row->kind) {
    case ROW_TOGGLE:
        SDL_snprintf(buf, sizeof buf, "[%c] %s", row->on ? 'x' : ' ', row->label);
        panel_text(r, ROW_X, y, buf);
        break;
    case ROW_ACTION:
        SDL_snprintf(buf, sizeof buf, "> %s", row->label);
        panel_text(r, ROW_X, y, buf);
        break;
    case ROW_NOTE:
        panel_text(r, ROW_X, y, row->label);
        panel_text(r, ROW_X + ROW_W - (float)SDL_strlen(row->value) * CHAR, y, row->value);
        break;
    case ROW_CHOICE:
        panel_text(r, ROW_X, y, row->label);
        SDL_snprintf(buf, sizeof buf, "<%s>", row->value);
        panel_text(r, ROW_X + ROW_W - (float)SDL_strlen(buf) * CHAR, y, buf);
        break;
    }
}

void menu_draw(SDL_Renderer *r)
{
    panel_begin(r, "REUNION", "F1: return");
    panel_color(r, 60, 72, 110, 255);
    panel_fill(r, SPLIT_X, TOP_Y - 4, 1, VISIBLE_ROWS * ROW_H + 2);
    panel_fill(r, PANEL_X + 4, HELP_Y - 7, PANEL_W - 8, 1);

    for (int i = 0; i < SECTIONS; i++) {
        float y = TOP_Y + i * TAB_H;
        if (i == section) {
            panel_color(r, focus < 0 ? 50 : 34, focus < 0 ? 70 : 46, focus < 0 ? 130 : 86, 255);
            panel_fill(r, TAB_X - 4, y - 3, TAB_W, TAB_H - 1);
            panel_color(r, 255, 255, 255, 255);
        } else {
            panel_color(r, 175, 185, 210, 255);
        }
        panel_text(r, TAB_X, y, SECTION_NAMES[i]);
    }

    build_rows();
    int first = scroll[section];
    for (int i = first; i < row_count && i < first + VISIBLE_ROWS; i++)
        draw_row(r, &rows[i], TOP_Y + (i - first) * ROW_H, i == focus);
    panel_color(r, 140, 150, 180, 255);
    if (first > 0)
        panel_text(r, ROW_X + ROW_W - CHAR, TOP_Y - 12, "^");
    if (first + VISIBLE_ROWS < row_count)
        panel_text(r, ROW_X + ROW_W - CHAR, TOP_Y + VISIBLE_ROWS * ROW_H - 4, "v");

    panel_color(r, 170, 190, 225, 255);
    if (status[0])
        help(r, status);
    else if (focus >= 0 && focus < row_count && rows[focus].help)
        help(r, rows[focus].help);
    else if (focus < 0)
        help(r, "Up/Down: section. Right/Enter: settings. Tab: next section.");
    panel_end(r);
}

void menu_run(void)
{
    menu_open();
    SDL_ShowCursor();
    vid_set_overlay(menu_draw);
    while (open) {
        SDL_Event e;
        while (open && SDL_PollEvent(&e))
            menu_handle_event(&e);
        vid_present();
        SDL_Delay(15);
    }
    vid_set_overlay(nullptr);
    SDL_HideCursor();
    // Leaves no click or key for the game, and a frame without the menu.
    input_release();
    vid_present();
}

// --- Tests --------------------------------------------------------------------

int menu_row_count(void)
{
    build_rows();
    return row_count;
}

const char *menu_row_text(int row)
{
    build_rows();
    if (row < 0 || row >= row_count)
        return "";
    const Row *r = &rows[row];
    if (r->kind == ROW_TOGGLE)
        SDL_snprintf(row_text, sizeof row_text, "[%c] %s", r->on ? 'x' : ' ', r->label);
    else if (r->kind == ROW_CHOICE)
        SDL_snprintf(row_text, sizeof row_text, "%s <%s>", r->label, r->value);
    else if (r->kind == ROW_NOTE && r->value[0])
        SDL_snprintf(row_text, sizeof row_text, "%s %s", r->label, r->value);
    else if (r->kind == ROW_NOTE)
        SDL_strlcpy(row_text, r->label, sizeof row_text);
    else
        SDL_snprintf(row_text, sizeof row_text, "> %s", r->label);
    return row_text;
}

int menu_section(void)
{
    return section;
}

int menu_focus(void)
{
    return focus;
}
