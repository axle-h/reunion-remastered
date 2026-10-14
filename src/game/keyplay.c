#include "game/keyplay.h"

#include <SDL3/SDL.h>

#include "game/dseg.h"
#include "game/improvements.h"
#include "game/main_program.h"
#include "game/mouse.h"
#include "platform/config.h"
#include "platform/input.h"
#include "platform/timing.h"
#include "platform/video.h"

const KeyActionInfo KeyPlay_Actions[KEY_ACTION_COUNT] = {
    [KEY_NEXT] = {"next", "Next", "Tab"},
    [KEY_PREVIOUS] = {"previous", "Previous", "Shift+Tab"},
    [KEY_UP] = {"up", "Up", "Up"},
    [KEY_DOWN] = {"down", "Down", "Down"},
    [KEY_LEFT] = {"left", "Left", "Left"},
    [KEY_RIGHT] = {"right", "Right", "Right"},
    [KEY_NUDGE_UP] = {"nudge_up", "Pointer up", "Shift+Up"},
    [KEY_NUDGE_DOWN] = {"nudge_down", "Pointer down", "Shift+Down"},
    [KEY_NUDGE_LEFT] = {"nudge_left", "Pointer left", "Shift+Left"},
    [KEY_NUDGE_RIGHT] = {"nudge_right", "Pointer right", "Shift+Right"},
    [KEY_CLICK] = {"click", "Click", "Return, Keypad Enter"},
    [KEY_RIGHT_CLICK] = {"right_click", "Right click", "Space"},
    [KEY_BACK] = {"back", "Back", "Escape"},
    [KEY_CLOCK] = {"clock", "Clock (next hour)", "H"},
    [KEY_TOOLBAR_1] = {"toolbar_1", "Toolbar 1", "1"},
    [KEY_TOOLBAR_1 + 1] = {"toolbar_2", "Toolbar 2", "2"},
    [KEY_TOOLBAR_1 + 2] = {"toolbar_3", "Toolbar 3", "3"},
    [KEY_TOOLBAR_1 + 3] = {"toolbar_4", "Toolbar 4", "4"},
    [KEY_TOOLBAR_1 + 4] = {"toolbar_5", "Toolbar 5", "5"},
    [KEY_TOOLBAR_1 + 5] = {"toolbar_6", "Toolbar 6", "6"},
    [KEY_TOOLBAR_PAGE] = {"toolbar_page", "Toolbar page", "7"},
    [KEY_COMMAND_CENTRE] = {"command_centre", "Command centre", "F2"},
    [KEY_STAR_MAP] = {"star_map", "Star map", "F3"},
    [KEY_PLANET] = {"planet", "Planet", "F4"},
    [KEY_SHIP_INFO] = {"ship_info", "Ship info", "F5"},
    [KEY_SPACE_LOCAL] = {"space_local", "Space local", "F6"},
    [KEY_RESEARCH] = {"research", "Research", "F7"},
    [KEY_COMMANDERS] = {"commanders", "Commanders", "F8"},
    [KEY_INFO_BUY] = {"info_buy", "Info-buy", "F9"},
    [KEY_MESSAGES] = {"messages", "Messages", "F10"},
};

enum : int {
    SCREEN_COMMAND = 1,
    CMD_BACK_TO_MAIN = 24,  // BACK TO M.SCREEN: the command centre
    MAX_BINDINGS = 2,       // keys per action
    MAX_TARGETS = 128,
    HOLD_FRAMES = 4,        // a click's press, at least (as the autopilot's)
    NUDGE = 4,              // pixels
    WAIT_LIMIT = 700,       // retraces a shortcut waits for a screen
    BUSY_HIDE = 10,         // retraces out of the main loop's wait that hide the frame
    // The toolbar (Mouse_HitTest): six buttons, the page arrow, the clock.
    BAR_W = 0x30,
    BAR_H = 0x21,
    PAGE_X = 0x120,
    CLOCK_X = 0x10C,
    CLOCK_Y = 0x28,
};

// The command centre's hotspot for each room shortcut.
static const int16_t ROOM_HOTSPOT[] = {
    [KEY_STAR_MAP - KEY_STAR_MAP] = 9,   [KEY_PLANET - KEY_STAR_MAP] = 12,
    [KEY_SHIP_INFO - KEY_STAR_MAP] = 11, [KEY_SPACE_LOCAL - KEY_STAR_MAP] = 10,
    [KEY_RESEARCH - KEY_STAR_MAP] = 5,   [KEY_COMMANDERS - KEY_STAR_MAP] = 7,
    [KEY_INFO_BUY - KEY_STAR_MAP] = 8,   [KEY_MESSAGES - KEY_STAR_MAP] = 6,
};

// Back takes the first of these the toolbar has: the ways out that undo or
// decide nothing.
static const int16_t BACK_COMMANDS[] = {
    CMD_BACK_TO_MAIN,
    58,  // ABORT
    57,  // ABORT MOVE
    32,  // CANCEL BUY
    42,  // CANCEL ATTACK
    35,  // SELECT OFF
    54,  // END TALK
};

// --- Keys -----------------------------------------------------------------------------

typedef struct Binding {
    SDL_Keycode key;
    SDL_Keymod mod;  // SDL_KMOD_SHIFT, _CTRL, _ALT
} Binding;

static Binding bindings[KEY_ACTION_COUNT][MAX_BINDINGS];

static SDL_Keymod plain_mod(SDL_Keymod m)
{
    SDL_Keymod r = 0;
    if (m & SDL_KMOD_SHIFT)
        r |= SDL_KMOD_SHIFT;
    if (m & SDL_KMOD_CTRL)
        r |= SDL_KMOD_CTRL;
    if (m & SDL_KMOD_ALT)
        r |= SDL_KMOD_ALT;
    return r;
}

// "Shift+Tab": false if a key name is unknown.
static bool parse_key(const char *s, Binding *b)
{
    static const struct {
        const char *prefix;
        SDL_Keymod mod;
    } MODS[] = {{"shift+", SDL_KMOD_SHIFT}, {"ctrl+", SDL_KMOD_CTRL}, {"alt+", SDL_KMOD_ALT}};
    b->mod = 0;
    for (bool more = true; more;) {
        more = false;
        while (*s == ' ')
            s++;
        for (size_t i = 0; i < SDL_arraysize(MODS); i++) {
            size_t n = SDL_strlen(MODS[i].prefix);
            if (SDL_strncasecmp(s, MODS[i].prefix, n) == 0 && s[n] != '\0') {
                b->mod |= MODS[i].mod;
                s += n;
                more = true;
            }
        }
    }
    b->key = SDL_GetKeyFromName(s);
    return b->key != SDLK_UNKNOWN;
}

// Up to MAX_BINDINGS keys, apart by commas. False if one is unknown.
static bool parse_keys(const char *s, Binding out[MAX_BINDINGS])
{
    SDL_memset(out, 0, sizeof(Binding) * MAX_BINDINGS);
    char buf[CONFIG_BINDING_SIZE];
    SDL_strlcpy(buf, s, sizeof buf);
    int n = 0;
    char *save = nullptr;
    for (char *tok = SDL_strtok_r(buf, ",", &save); tok; tok = SDL_strtok_r(nullptr, ",", &save)) {
        char *end = tok + SDL_strlen(tok);
        while (end > tok && end[-1] == ' ')
            *--end = '\0';
        if (n == MAX_BINDINGS || !parse_key(tok, &out[n++]))
            return false;
    }
    return n > 0;
}

const char *KeyPlay_Keys(KeyAction a)
{
    const char *s = config_binding(KeyPlay_Actions[a].key);
    return s ? s : KeyPlay_Actions[a].defaults;
}

// The action key e is bound to, or -1. Shift+Click is a Shift+click.
static int action_of(const SDL_KeyboardEvent *e)
{
    SDL_Keymod mod = plain_mod(e->mod);
    for (int pass = 0; pass < 2; pass++) {
        for (int a = 0; a < KEY_ACTION_COUNT; a++) {
            if (pass == 1 && a != KEY_CLICK && a != KEY_RIGHT_CLICK)
                continue;
            for (int i = 0; i < MAX_BINDINGS; i++) {
                const Binding *b = &bindings[a][i];
                if (b->key != SDLK_UNKNOWN && b->key == e->key && b->mod == mod)
                    return a;
            }
        }
        if (!(mod & SDL_KMOD_SHIFT))
            break;
        mod &= ~SDL_KMOD_SHIFT;
    }
    return -1;
}

// --- Targets --------------------------------------------------------------------------

typedef struct Target {
    SDL_Rect r;
    int cw, ch;       // cell size of a grid, else 0
    int16_t hotspot;  // the hotspot it is, else 0
} Target;

static KeyPlayContext context;

static Target targets[MAX_TARGETS];
static int target_count;

// A rectangle already listed only gets the cell size.
static void add_target(int x, int y, int w, int h, int cw, int ch, int16_t hotspot)
{
    SDL_Rect r = {x, y, w, h};
    SDL_Rect screen = {0, 0, 320, 200};
    if (w <= 0 || h <= 0 || !SDL_GetRectIntersection(&r, &screen, &r))
        return;
    for (int i = 0; i < target_count; i++) {
        if (SDL_RectsEqual(&targets[i].r, &r)) {
            if (cw > 0) {
                targets[i].cw = cw;
                targets[i].ch = ch;
            }
            return;
        }
    }
    if (target_count < MAX_TARGETS)
        targets[target_count++] = (Target){r, cw, ch, hotspot};
}

static void add_grid(int x, int y, int cols, int rows, int cw, int ch)
{
    add_target(x, y, cols * cw, rows * ch, cw, ch, 0);
}

// The toolbar's slot (MenuBar, 1..12) shown at button h (1..6).
static int shown_slot(int h)
{
    return ds.GfxPath_PageFlagB == 0 ? h + 6 : h;
}

static int16_t shown_command(int h)
{
    return ds.GfxPath_MenuBar[shown_slot(h) - 1].Command;
}

// A grid standing for a hotspot whose cells the screen tells apart by
// the click's position: the hotspot is not listed itself.
static void grid_for(bool skip[67], int16_t j, int x, int y, int cols, int rows, int cw, int ch)
{
    if (rows <= 0 || j > ds.Mouse_HotspotCount)
        return;
    skip[j] = true;
    add_grid(x, y, cols, rows, cw, ch);
}

// What a screen takes clicks in beyond its hotspots, its grids, and the
// hotspots that do nothing (stale from the screen before, or not read).
static void screen_targets(bool skip[67])
{
    switch (ds.Game_CurrentScreen) {
    case 5:  // the shop's product list: rows 1..13 (Shop_HandleListClick)
        if (ds.Shop_ListMode && !ds.Shop_QtyEdit)
            grid_for(skip, 2, 0xE, 0x39, 1, 13, 0x62, 9);
        break;
    case 12:  // the save slots
        grid_for(skip, 5, 0x0B, 0x40, 1, 12, 0x82, 9);
        break;
    case 16:  // icons past the groups shown
        for (int16_t j = (int16_t)(3 + ds.Game_ShownGroupCount); j <= 34; j++)
            skip[j] = true;
        break;
    case 20:
        if (ds.Mouse_HotspotCount == 0xC) {
            skip[12] = true;  // left by the screen before
            grid_for(skip, 1, 0x5D, 0x35, 14, 9, 16, 16);
        }
        break;
    case 24:  // the advisor's questions (Advisor_PollAdvisorMenu)
        grid_for(skip, 1, 0x70, 0x5D, 1, SDL_min(ds.Advisor_CurMenu.Count, 5), 0x60, 9);
        break;
    case 25:  // the questions, a line each (Pub_UpdateQuestionHover)
        if (!ds.Pub_Replied)
            grid_for(skip, 1, 6, 0x86, 1, SDL_min(ds.Pub_LineCount, 7), 0xD2, 9);
        break;
    case 34:
        if (!ds.AlienTalk_Locked)
            grid_for(skip, 1, 6, 0x86, 1, SDL_min(ds.AlienTalk_MenuLineCount, 7), 0xD2, 9);
        break;
    case 28:  // registered, never read
        skip[1] = skip[2] = skip[3] = true;
        break;
    case 32:
        if (ds.Mouse_HotspotCount == 4) {
            skip[4] = true;  // left by the unit assignment
            grid_for(skip, 1, 0x40, 0x34, 16, 9, 16, 16);
        }
        break;
    case 37:  // the scroll bar is dragged
        grid_for(skip, 1, 0x134, 0x37, 1, 14, 0xC, 10);
        break;
    default:
        break;
    }
}

static int by_position(const void *a, const void *b)
{
    const SDL_Rect *x = &((const Target *)a)->r, *y = &((const Target *)b)->r;
    if (x->y != y->y)
        return x->y - y->y;
    return x->x - y->x;
}

static void collect_targets(void)
{
    target_count = 0;
    switch (context) {
    case KEYPLAY_START_MENU:  // Title_StartMenu
        for (int i = 0; i < 3; i++)
            add_target(0x4B, (int[]){0x30, 0x4C, 0x69}[i], 0xB0, (int[]){0x1C, 0x1D, 0x1C}[i], 0, 0, 0);
        return;
    case KEYPLAY_HERO:  // Title_HeroSelect: the half the hero stands in
        add_target(0, 0, 0xA0, 200, 0, 0, 0);
        add_target(0xA0, 0, 0xA0, 200, 0, 0, 0);
        return;
    case KEYPLAY_SCREENS:
        break;
    }
    for (int h = 1; h <= 6; h++)
        if (shown_command(h) != 0)
            add_target((h - 1) * BAR_W, 0, BAR_W, BAR_H, 0, 0, 0);
    if (ds.GfxPath_PageFlagA)
        add_target(PAGE_X, 0, 320 - PAGE_X, BAR_H, 0, 0, 0);
    bool skip[67] = {};
    screen_targets(skip);
    for (int16_t j = 1; j <= ds.Mouse_HotspotCount && j <= 66; j++) {
        const THotspot *h = Mouse_Hotspot(j);
        if (!skip[j])
            add_target(h->X, h->Y, h->W, h->H, 0, 0, j);
    }
    SDL_qsort(targets, (size_t)target_count, sizeof *targets, by_position);
}

// The smallest target holding (x, y), or -1.
static int target_at(int x, int y)
{
    int best = -1;
    for (int i = 0; i < target_count; i++) {
        SDL_Point p = {x, y};
        if (SDL_PointInRect(&p, &targets[i].r)
            && (best < 0 || targets[i].r.w * targets[i].r.h < targets[best].r.w * targets[best].r.h))
            best = i;
    }
    return best;
}

static SDL_Point centre(const SDL_Rect *r)
{
    return (SDL_Point){r->x + r->w / 2, r->y + r->h / 2};
}

// The cell of grid t holding (x, y).
static SDL_Rect cell_at(const Target *t, int x, int y)
{
    int c = (x - t->r.x) / t->cw, r = (y - t->r.y) / t->ch;
    return (SDL_Rect){t->r.x + c * t->cw, t->r.y + r * t->ch, t->cw, t->ch};
}

// The hotspot the game's hit test takes at (x, y) (Mouse_HitTest): the
// lowest index; none on the toolbar and the clock, which come first.
static int16_t hit_at(int x, int y)
{
    if (y <= 0x20 || (x >= 0xDA && y >= 0x22 && y <= 0x2D))
        return 0;
    for (int16_t j = 1; j <= ds.Mouse_HotspotCount && j <= 66; j++) {
        const THotspot *h = Mouse_Hotspot(j);
        if (x >= h->X && x < h->X + h->W && y >= h->Y && y < h->Y + h->H)
            return j;
    }
    return 0;
}

// Where the pointer goes on target t: the middle cell of a grid; on a
// hotspot, the point nearest its middle that a click there would hit (a
// lower hotspot or the clock can cover the middle, as orbiting planets do).
static SDL_Point landing(const Target *t)
{
    SDL_Point p = centre(&t->r);
    if (t->cw > 0) {
        SDL_Rect c = cell_at(t, p.x, p.y);
        return centre(&c);
    }
    if (!t->hotspot || hit_at(p.x, p.y) == t->hotspot)
        return p;
    SDL_Point best = p;
    int best_d = -1;
    for (int y = t->r.y; y < t->r.y + t->r.h; y += 2) {
        for (int x = t->r.x; x < t->r.x + t->r.w; x += 2) {
            int d = (x - p.x) * (x - p.x) + (y - p.y) * (y - p.y);
            if ((best_d < 0 || d < best_d) && hit_at(x, y) == t->hotspot) {
                best = (SDL_Point){x, y};
                best_d = d;
            }
        }
    }
    return best;
}

// --- The pointer and the buttons ------------------------------------------------------

static bool focus_shown;
static int busy_frames;
static uint64_t last_wait_frames;
static int idle_frames;  // retraces the main loop has waited for a click in a row

typedef struct Button {
    bool held;          // by a key
    bool release;       // the key is up: release once HOLD_FRAMES have passed
    uint64_t since;
} Button;

static Button buttons[2];  // left, right

// The hotspot the pointer was put on, followed while it moves (the star
// map's planets orbit): 0 if none.
static struct {
    int16_t hotspot, screen;
    SDL_Point at;  // its centre
    int away;      // frames it has been elsewhere
} follow;

static void move_pointer(int x, int y)
{
    InputMouse m = input_mouse();
    input_set_mouse(x, y, m.left, m.right);
    focus_shown = true;
    follow.hotspot = 0;
}

static void land(const Target *t)
{
    SDL_Point p = landing(t);
    move_pointer(p.x, p.y);
    if (t->cw == 0 && t->hotspot) {
        follow.hotspot = t->hotspot;
        follow.screen = ds.Game_CurrentScreen;
        follow.at = centre(&t->r);
        follow.away = 0;
    }
}

// Keeps the pointer on a moving hotspot while its centre moves a little
// each frame. The star map puts its planets back where they started for a
// frame now and then (PlanetInfo_SetupSystemViewHotspots, before the next
// PlanetInfo_UpdateOrbitPositions): a jump is waited out, unless it lasts.
static void follow_hotspot(void)
{
    enum : int { STEP = 16, JUMP_FRAMES = 8 };
    if (!follow.hotspot || buttons[0].held || buttons[1].held)
        return;
    const THotspot *h = Mouse_Hotspot(follow.hotspot);
    SDL_Point at = {h->X + h->W / 2, h->Y + h->H / 2};
    if (follow.hotspot > ds.Mouse_HotspotCount || h->W <= 0 || ds.Game_CurrentScreen != follow.screen
        || SDL_abs(at.x - follow.at.x) > STEP || SDL_abs(at.y - follow.at.y) > STEP) {
        if (++follow.away >= JUMP_FRAMES)
            follow.hotspot = 0;
        return;
    }
    follow.away = 0;
    if (at.x != follow.at.x || at.y != follow.at.y) {
        InputMouse m = input_mouse();
        input_set_mouse(m.x + at.x - follow.at.x, m.y + at.y - follow.at.y, m.left, m.right);
        follow.at = at;
    }
}

static void set_button(int b, bool down)
{
    InputMouse m = input_mouse();
    input_set_mouse(m.x, m.y, b == 0 ? down : m.left, b == 1 ? down : m.right);
}

static void press(int b)
{
    set_button(b, true);
    buttons[b] = (Button){.held = true, .since = timing_frames()};
}

static void release(int b)
{
    if (buttons[b].held)
        buttons[b].release = true;
}

static void update_buttons(void)
{
    InputMouse m = input_mouse();
    for (int b = 0; b < 2; b++) {
        Button *k = &buttons[b];
        if (k->held && !(b == 0 ? m.left : m.right))
            *k = (Button){};  // let go elsewhere (input_release)
        else if (k->held && k->release && timing_frames() - k->since >= HOLD_FRAMES) {
            set_button(b, false);
            *k = (Button){};
        }
    }
}

// --- Navigation -----------------------------------------------------------------------

static void step_target(int dir)
{
    collect_targets();
    if (target_count == 0)
        return;
    InputMouse m = input_mouse();
    int i = target_at(m.x, m.y);
    if (i < 0) {
        // The first one after the pointer in reading order.
        i = dir > 0 ? target_count - 1 : 0;
        for (int j = 0; j < target_count; j++) {
            const SDL_Rect *r = &targets[j].r;
            bool after = r->y > m.y || (r->y == m.y && r->x > m.x);
            if (dir > 0 && after) {
                i = j - 1;
                break;
            }
            if (dir < 0 && !after)
                i = j + 1;
        }
    }
    i = (i + dir + target_count) % target_count;
    land(&targets[i]);
}

static bool scroll_surface(int dx, int dy);

// An arrow: one cell in a grid, else (or off its edge) the nearest target
// that way, measured from the pointer.
static void arrow(int dx, int dy)
{
    collect_targets();
    InputMouse m = input_mouse();
    int cur = target_at(m.x, m.y);
    if (cur >= 0 && targets[cur].cw > 0) {
        const Target *t = &targets[cur];
        SDL_Rect c = cell_at(t, m.x, m.y);
        SDL_Point p = {c.x + dx * t->cw + t->cw / 2, c.y + dy * t->ch + t->ch / 2};
        if (SDL_PointInRect(&p, &t->r)) {
            move_pointer(p.x, p.y);
            return;
        }
        if (ds.Game_CurrentScreen == 20 && t->r.x == 0x5D && scroll_surface(dx, dy))
            return;
    }
    SDL_Point from = {m.x, m.y};
    if (cur >= 0 && targets[cur].cw == 0)
        from = centre(&targets[cur].r);
    int best = -1, best_score = 0;
    for (int i = 0; i < target_count; i++) {
        if (i == cur)
            continue;
        // The nearest point of the target to the pointer.
        const SDL_Rect *r = &targets[i].r;
        int nx = SDL_clamp(from.x, r->x, r->x + r->w - 1);
        int ny = SDL_clamp(from.y, r->y, r->y + r->h - 1);
        int along = dx ? (nx - from.x) * dx : (ny - from.y) * dy;
        int across = dx ? SDL_abs(ny - from.y) : SDL_abs(nx - from.x);
        if (along <= 0)
            continue;
        int score = along + 2 * across;
        if (best < 0 || score < best_score) {
            best = i;
            best_score = score;
        }
    }
    if (best >= 0)
        land(&targets[best]);
}

static void nudge(int dx, int dy)
{
    InputMouse m = input_mouse();
    move_pointer(m.x + dx * NUDGE, m.y + dy * NUDGE);
}

// --- Shortcuts ------------------------------------------------------------------------

// A shortcut is a few steps, each a click while the main loop waits for
// one, or a wait; then the pointer goes back.
typedef enum : int {
    STEP_BUTTON,   // click toolbar button `arg` (1..6, 7 the page arrow)
    STEP_COMMAND,  // click the button with command `arg`, turning the page first
    STEP_HOTSPOT,  // click hotspot `arg`
    STEP_SCREEN,   // wait until screen `arg` waits for a click
    STEP_POINT,    // click at (arg % 1024, arg / 1024)
} StepKind;

typedef struct Step {
    StepKind kind;
    int arg;
} Step;

enum : int { MAX_STEPS = 4 };

static struct {
    Step steps[MAX_STEPS];
    int count, next;
    uint64_t started;  // frame the current step began
    bool clicking;     // the step's click is pressed
    bool last;         // and ends the step (not the page turned first)
    bool turned;       // STEP_COMMAND turned the page
    bool restore;      // put the pointer back at (x, y) once the button is up
    int x, y;
} shortcut;

static bool shortcut_running(void)
{
    return shortcut.next < shortcut.count;
}

static void remember_pointer(void)
{
    InputMouse m = input_mouse();
    shortcut.x = m.x;
    shortcut.y = m.y;
    shortcut.restore = true;
}

static void start_shortcut(const Step *steps, int n)
{
    SDL_memcpy(shortcut.steps, steps, sizeof *steps * (size_t)n);
    shortcut.count = n;
    shortcut.next = 0;
    shortcut.started = timing_frames();
    shortcut.clicking = shortcut.turned = false;
    remember_pointer();
}

static void end_step(void)
{
    shortcut.next++;
    shortcut.started = timing_frames();
    shortcut.clicking = shortcut.turned = false;
}

// The toolbar button (1..6) showing command c, 7 if the other page has it,
// 0 if neither.
static int command_button(int16_t c)
{
    for (int h = 1; h <= 6; h++)
        if (shown_command(h) == c)
            return h;
    if (ds.GfxPath_PageFlagA)
        for (int h = 1; h <= 6; h++)
            if (ds.GfxPath_MenuBar[(shown_slot(h) + 5) % 12].Command == c)
                return 7;
    return 0;
}

static SDL_Point button_point(int h)
{
    return (SDL_Point){h == 7 ? PAGE_X + (320 - PAGE_X) / 2 : (h - 1) * BAR_W + BAR_W / 2, BAR_H / 2};
}

static void click_at(SDL_Point p, bool last)
{
    input_set_mouse(p.x, p.y, 1, 0);
    buttons[0] = (Button){.held = true, .release = true, .since = timing_frames()};
    shortcut.clicking = true;
    shortcut.last = last;
}

// Starts the current step's click; false if it can't be had.
static bool start_click(const Step *s)
{
    switch (s->kind) {
    case STEP_BUTTON:
        click_at(button_point(s->arg), true);
        return true;
    case STEP_COMMAND: {
        int h = command_button((int16_t)s->arg);
        if (h == 0 || (h == 7 && shortcut.turned))
            return false;
        shortcut.turned |= h == 7;
        click_at(button_point(h), h != 7);
        return true;
    }
    case STEP_HOTSPOT: {
        const THotspot *h = Mouse_Hotspot((int16_t)s->arg);
        if (s->arg > ds.Mouse_HotspotCount || h->W <= 0 || h->H == 0)
            return false;
        click_at(landing(&(Target){{h->X, h->Y, h->W, h->H}, 0, 0, (int16_t)s->arg}), true);
        return true;
    }
    case STEP_POINT:
        click_at((SDL_Point){s->arg % 1024, s->arg / 1024}, true);
        return true;
    case STEP_SCREEN:
        break;
    }
    return false;
}

static void run_shortcut(void)
{
    if (!shortcut_running()) {
        if (shortcut.restore && !buttons[0].held && !buttons[1].held) {
            InputMouse m = input_mouse();
            input_set_mouse(shortcut.x, shortcut.y, m.left, m.right);
            shortcut.restore = false;
        }
        return;
    }
    const Step *s = &shortcut.steps[shortcut.next];
    if (timing_frames() - shortcut.started > WAIT_LIMIT) {
        shortcut.next = shortcut.count;  // gave up
        return;
    }
    if (s->kind == STEP_SCREEN) {
        if (ds.Game_CurrentScreen == s->arg && idle_frames >= 2)
            end_step();
        return;
    }
    if (shortcut.clicking) {
        if (buttons[0].held)
            return;
        if (shortcut.last)
            end_step();
        else
            shortcut.clicking = false;
        return;
    }
    if (idle_frames >= 2 && !start_click(s))
        shortcut.next = shortcut.count;
}

// Opens room `hotspot` of the command centre (0: just the centre).
static void room_shortcut(int16_t hotspot)
{
    Step steps[3];
    int n = 0;
    if (ds.Game_CurrentScreen != SCREEN_COMMAND) {
        if (!command_button(CMD_BACK_TO_MAIN))
            return;
        steps[n++] = (Step){STEP_COMMAND, CMD_BACK_TO_MAIN};
        steps[n++] = (Step){STEP_SCREEN, SCREEN_COMMAND};
    }
    if (hotspot)
        steps[n++] = (Step){STEP_HOTSPOT, hotspot};
    if (n)
        start_shortcut(steps, n);
}

static void back_shortcut(void)
{
    for (size_t i = 0; i < SDL_arraysize(BACK_COMMANDS); i++) {
        if (command_button(BACK_COMMANDS[i])) {
            start_shortcut(&(Step){STEP_COMMAND, BACK_COMMANDS[i]}, 1);
            return;
        }
    }
}

static void toolbar_shortcut(int h)
{
    if (h == 7 ? ds.GfxPath_PageFlagA != 0 : shown_command(h) != 0)
        start_shortcut(&(Step){STEP_BUTTON, h}, 1);
}

// An arrow off the edge of the surface map scrolls it a cell, by a click on
// the radar where the view would then be (screen_surface): the view jumps
// there on the press and stays on the release. False at the map's edge.
static bool scroll_surface(int dx, int dy)
{
    int mode = ds.Surface_Mode;
    if (mode != 1 && mode != 2 && mode != 6)
        return false;
    int sx = SDL_clamp(ds.Surface_ScrollX + dx, 0, SDL_max((int)ds.Surface_MapWidth - 14, 0));
    int sy = SDL_clamp(ds.Surface_ScrollY + dy, 0, SDL_max((int)ds.Surface_MapHeight - 9, 0));
    if (sx == ds.Surface_ScrollX && sy == ds.Surface_ScrollY)
        return false;
    int k = ds.Surface_RadarDouble ? 2 : 1;
    int x = ds.Surface_RadarX + k * (sx + 6), y = ds.Surface_RadarY + k * (sy + 3);
    if (!shortcut_running() && idle_frames > 0)
        start_shortcut(&(Step){STEP_POINT, x + y * 1024}, 1);
    return true;
}

// Held, the clock's button is held: the hours run as with the mouse.
static void clock_shortcut(void)
{
    remember_pointer();
    input_set_mouse(CLOCK_X, CLOCK_Y, 1, 0);
    buttons[0] = (Button){.held = true, .since = timing_frames()};
}

// --- The hook -------------------------------------------------------------------------

static bool started;

static bool on_key(const SDL_KeyboardEvent *e)
{
    if (!improvements.keyboard_play || !started || ds.TextFile_InputActive)
        return false;
    int a = action_of(e);
    if (a < 0)
        return false;
    if (!e->down) {
        if (a == KEY_CLICK || a == KEY_CLOCK)
            release(0);
        else if (a == KEY_RIGHT_CLICK)
            release(1);
        return true;
    }
    switch (a) {
    case KEY_NEXT: step_target(1); return true;
    case KEY_PREVIOUS: step_target(-1); return true;
    case KEY_UP: arrow(0, -1); return true;
    case KEY_DOWN: arrow(0, 1); return true;
    case KEY_LEFT: arrow(-1, 0); return true;
    case KEY_RIGHT: arrow(1, 0); return true;
    case KEY_NUDGE_UP: nudge(0, -1); return true;
    case KEY_NUDGE_DOWN: nudge(0, 1); return true;
    case KEY_NUDGE_LEFT: nudge(-1, 0); return true;
    case KEY_NUDGE_RIGHT: nudge(1, 0); return true;
    case KEY_CLICK:
    case KEY_RIGHT_CLICK:
        if (!e->repeat && !shortcut_running())
            press(a == KEY_CLICK ? 0 : 1);
        return true;
    default: break;
    }
    // Shortcuts: only while the main loop waits for a click. In a box or a
    // wait the key is taken all the same: a key there would end every story
    // picture to come (Gfx_KeyAbort stays set), and Click goes on.
    if (idle_frames == 0 || context != KEYPLAY_SCREENS)
        return true;
    if (e->repeat || shortcut_running() || buttons[0].held || buttons[1].held)
        return true;
    if (a == KEY_CLOCK) {
        clock_shortcut();
        return true;
    }
    if (a >= KEY_TOOLBAR_1 && a <= KEY_TOOLBAR_PAGE) {
        toolbar_shortcut(a - KEY_TOOLBAR_1 + 1);
        return true;
    }
    if (a == KEY_BACK) {
        back_shortcut();
        return true;
    }
    if (a == KEY_COMMAND_CENTRE) {
        room_shortcut(0);
        return true;
    }
    if (a >= KEY_STAR_MAP && a <= KEY_MESSAGES) {
        room_shortcut(ROOM_HOTSPOT[a - KEY_STAR_MAP]);
        return true;
    }
    return true;
}

// --- Every retrace --------------------------------------------------------------------

void KeyPlay_Init(void)
{
    for (int a = 0; a < KEY_ACTION_COUNT; a++) {
        const char *s = KeyPlay_Keys((KeyAction)a);
        if (!parse_keys(s, bindings[a])) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[keys] %s = %s: unknown key, using %s",
                        KeyPlay_Actions[a].key, s, KeyPlay_Actions[a].defaults);
            parse_keys(KeyPlay_Actions[a].defaults, bindings[a]);
        }
    }
    input_set_key_hook(on_key);
}

void KeyPlay_SetContext(KeyPlayContext c)
{
    context = c;
}

void KeyPlay_Start(void)
{
    KeyPlay_Reset();
    started = true;
}

void KeyPlay_Stop(void)
{
    KeyPlay_Reset();
    started = false;
}

void KeyPlay_Reset(void)
{
    SDL_zero(buttons);
    SDL_zero(shortcut);
    SDL_zero(follow);
    focus_shown = false;
    context = KEYPLAY_SCREENS;
    idle_frames = busy_frames = 0;
    last_wait_frames = Game_EventWaitFrames;
    vid_set_focus(nullptr);
}

void KeyPlay_Frame(void)
{
    if (Game_EventWaitFrames != last_wait_frames) {
        idle_frames++;
        busy_frames = 0;
    } else {
        idle_frames = 0;
        busy_frames++;
    }
    last_wait_frames = Game_EventWaitFrames;
    if (!improvements.keyboard_play || !started) {
        if (focus_shown)
            vid_set_focus(nullptr);
        focus_shown = false;
        return;
    }
    update_buttons();
    run_shortcut();
    if (input_take_mouse_moved()) {
        focus_shown = false;
        follow.hotspot = 0;
    }
    follow_hotspot();
    const SDL_Rect *frame = nullptr;
    SDL_Rect r;
    if (focus_shown && busy_frames < BUSY_HIDE) {
        collect_targets();
        InputMouse m = input_mouse();
        int i = target_at(m.x, m.y);
        if (i >= 0) {
            r = targets[i].cw > 0 ? cell_at(&targets[i], m.x, m.y) : targets[i].r;
            frame = &r;
        }
    }
    vid_set_focus(frame);
}
