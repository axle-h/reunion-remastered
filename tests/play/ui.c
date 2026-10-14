#include "play/ui.h"

#include <SDL3/SDL.h>

#include "game/dseg.h"
#include "game/main_program.h"
#include "game/mouse.h"
#include "platform/input.h"
#include "play/lang.h"
#include "play/play.h"
#include "play/ui_message.h"

enum : int {
    TOOLBAR_CELL_W = 0x30,  // Mouse_HitTest: six 48-pixel cells, y 0..32
    TOOLBAR_Y = 16,
    TOOLBAR_TOGGLE_X = 0x130,
    CLOCK_X = 260,          // the date: the clock button (hit-test id 0Eh)
    CLOCK_Y = 40,
    SETTLE = 30,            // retraces for fades after a screen is set up
    IDLE_RUN = 3,
    BUSY_TIMEOUT = 5000,
    HOLD_LIMIT = 100,       // retraces a press waits for the game's poll
    CLICK_TRIES = 5,
    SCREEN_TIMEOUT = 3000,
    HOUR_RETRACES = 101,    // running retraces per game hour (main_program.c)
    AIM_MARGIN = 3,         // pixels kept from lower hotspots (orbits move on)
    AIM_TRIES = 100,
    AIM_STEP = 10,
};

// --- Waits ---

int ui_screen(void)
{
    return ds.Game_CurrentScreen;
}

bool ui_wait_until(bool (*pred)(void *arg), void *arg, int timeout)
{
    uint64_t end = play_frame() + (uint64_t)timeout;
    for (;;) {
        ui_handle_interrupts();
        if (pred(arg))
            return true;
        if (play_frame() >= end)
            return false;
        play_wait(1);
    }
}

typedef struct {
    bool (*pred)(void *arg);
    void *arg;
    int64_t end;
} HourWait;

static bool pred_or_hours(void *arg)
{
    const HourWait *w = arg;
    return w->pred(w->arg) || ui_clock_hours() >= w->end;
}

bool ui_wait_hours(bool (*pred)(void *arg), void *arg, int hours)
{
    HourWait w = {pred, arg, ui_clock_hours() + hours};
    ui_wait_until(pred_or_hours, &w, hours * HOUR_RETRACES * 3 + SCREEN_TIMEOUT);
    return pred(arg);
}

// The main loop has been waiting for input for the last few retraces: the
// screen is not busy (fading, typing text, animating a door) and takes clicks.
bool ui_wait_idle(int timeout)
{
    uint64_t end = play_frame() + (uint64_t)timeout, last = Game_EventWaitFrames;
    int run = 0;
    for (;;) {
        ui_handle_interrupts();
        if (Game_EventWaitFrames != last) {
            run = Game_EventWaitFrames == last + 1 ? run + 1 : 1;
            last = Game_EventWaitFrames;
        } else {
            run = 0;
        }
        if (run >= IDLE_RUN)
            return true;
        if (play_frame() >= end)
            return false;
        play_wait(1);
    }
}

void ui_settle(void)
{
    if (!ui_wait_idle(BUSY_TIMEOUT))
        play_fail("the game stayed busy (screen %d)", ui_screen());
}

static bool screen_shown(void *arg)
{
    return ds.Game_CurrentScreen == *(int *)arg && !ds.Game_ScreenSwitch;
}

// The main loop has waited for input for SETTLE retraces since *arg.
static bool settled(void *arg)
{
    return Game_EventWaitFrames >= *(uint64_t *)arg + SETTLE;
}

bool ui_wait_screen(int screen, int timeout)
{
    if (!ui_wait_until(screen_shown, &screen, timeout))
        return false;
    uint64_t since = Game_EventWaitFrames;
    return ui_wait_until(settled, &since, timeout) && ds.Game_CurrentScreen == screen;
}

static bool hotspot_shown(void *arg)
{
    return ui_find_hotspot(arg, 1) != 0;
}

bool ui_wait_hotspot(const char *caption, int timeout)
{
    return ui_wait_until(hotspot_shown, (void *)caption, timeout);
}

// --- Clicks ---

// Holds the button just pressed until the game's input poll has taken it,
// and returns the retraces held, or 0 if it gave up. The main loop polls
// (Mouse_Poll: a snapshot of the buttons and the position) before each
// retrace and hit-tests that snapshot after it, so while it waits for input
// the press is polled before the next retrace and taken right after it. A
// game busy then (a handler's own frames, an hour's simulation) polls later,
// and a press moved off first would be lost. Busy for long, the game likely
// shows a box, which waits for the button to go up: then it gives up, and
// the caller releases the button before any poll can see it. So a press is
// either polled once (if the mouse then moves off, as UI_PRESS does) or not
// at all, never twice. This reads the game's input state, not anything shown:
// it times the fixture's own input, as ui_wait_idle's Game_EventWaitFrames
// does, and decides nothing.
static int hold_until_polled(void)
{
    InputMouse m = input_mouse();  // as clamped to the screen
    for (int held = 1; held <= HOLD_LIMIT; held++) {
        play_wait(1);
        // ui_wait_idle saw polls with the button up, so a snapshot of it
        // down is from after the press.
        if (ds.Mouse_Buttons != 0 && ds.Mouse_ClickX == m.x && ds.Mouse_ClickY == m.y) {
            if (held > 1)
                play_log("press at %d,%d polled after %d retraces", m.x, m.y, held);
            return held;
        }
    }
    return 0;
}

void ui_click_as(int x, int y, UiClick how)
{
    int screen = ui_screen();
    int button = how == UI_CLICK_RIGHT || how == UI_PRESS_RIGHT ? 2 : how == UI_CLICK_SHIFT ? 1 | 4 : 1;
    int scenes = ui_scenes_played();
    for (int tries = 1;; tries++) {
        if (!ui_wait_idle(BUSY_TIMEOUT))
            play_fail("the game stayed busy before a click at %d,%d (screen %d)", x, y, ui_screen());
        if (ui_screen() != screen && ui_interrupt_screen())
            play_fail("screen %d came up before a click at %d,%d on screen %d", ui_screen(), x, y,
                      screen);
        // A story scene played meanwhile returns to the command centre: the
        // click was meant for the screen before, so it is dropped (the
        // caller sees nothing happen and goes there again).
        if (ui_screen() != screen && ui_scenes_played() != scenes) {
            play_log("a story scene came before a click at %d,%d on screen %d: dropped", x, y,
                     screen);
            return;
        }
        play_mouse(x, y, button);
        int held = hold_until_polled();
        if (held == 0) {
            // Not polled: no handler saw it, so pressing again is safe once
            // the game takes input again (ui_wait_idle closes a box).
            play_mouse(x, y, 0);
            play_wait(2);
            play_log("press at %d,%d not polled in %d retraces (screen %d)", x, y, HOLD_LIMIT,
                     ui_screen());
            if (tries == CLICK_TRIES)
                play_fail("the game did not take a press at %d,%d (screen %d)", x, y, ui_screen());
            continue;
        }
        if (how == UI_PRESS || how == UI_PRESS_RIGHT) {
            // The hit test after this retrace takes the press; the next poll
            // finds the button down over no hotspot, and the release there
            // runs no handler.
            ui_inert_point(0, &x, &y);
            play_mouse(x, y, button);
            play_wait(4);
        } else if (held < 4) {
            play_wait(4 - held);
        }
        play_mouse(x, y, 0);
        play_wait(2);
        return;
    }
}

void ui_click(int x, int y)
{
    ui_click_as(x, y, UI_CLICK);
}

// --- Toolbar ---

// Toolbar slots 1..12 (GfxPath::MenuBar: label string[18], command).
static const uint8_t *slot_label(int slot)
{
    return (const uint8_t *)&ds.GfxPath_MenuBar[slot - 1].Caption;
}

static int16_t slot_command(int slot)
{
    return ds.GfxPath_MenuBar[slot - 1].Command;
}

static bool page1_shown(void)
{
    return ds.GfxPath_PageFlagB != 0;
}

// Pascal string p equals s, ignoring case and p's blanks at both ends
// (toolbar labels are padded).
static int slot_labelled(const char *label)
{
    for (int slot = 1; slot <= 12; slot++)
        if (slot_command(slot) != 0
            && lang_matches_constant((const char *)slot_label(slot) + 1, slot_label(slot)[0], label))
            return slot;
    return 0;
}

static int slot_with_command(int command)
{
    for (int slot = 1; slot <= 12; slot++)
        if (command != 0 && slot_command(slot) == command)
            return slot;
    return 0;
}

static bool page_is(void *arg)
{
    return page1_shown() == *(bool *)arg;
}

static void click_slot(int slot)
{
    bool want_page1 = slot <= 6;
    if (page1_shown() != want_page1) {
        ui_click(TOOLBAR_TOGGLE_X, TOOLBAR_Y);
        if (!ui_wait_until(page_is, &want_page1, 200))
            play_fail("toolbar page did not turn for slot %d", slot);
        play_wait(SETTLE);
    }
    int col = (slot - 1) % 6;
    ui_click(col * TOOLBAR_CELL_W + TOOLBAR_CELL_W / 2, TOOLBAR_Y);
}

// The toolbar is looked up once the screen takes clicks: a box closed or a
// screen changed meanwhile may change it.
bool ui_toolbar(const char *label)
{
    ui_settle();
    int slot = slot_labelled(label);
    if (slot)
        click_slot(slot);
    return slot != 0;
}

bool ui_toolbar_command(int command)
{
    ui_settle();
    int slot = slot_with_command(command);
    if (slot)
        click_slot(slot);
    return slot != 0;
}

bool ui_toolbar_has(const char *label)
{
    return slot_labelled(label) != 0;
}

bool ui_toolbar_has_command(int command)
{
    return slot_with_command(command) != 0;
}

// --- Hotspots ---

static bool caption_is(const THotspot *h, const char *s)
{
    size_t start = 0, len = h->Caption.len, n = SDL_min(SDL_strlen(s), (size_t)18);
    while (len > 0 && h->Caption.s[len - 1] == ' ')
        len--;
    while (start < len && h->Caption.s[start] == ' ')
        start++;
    return (len - start == n && SDL_strncasecmp(h->Caption.s + start, s, n) == 0)
           || lang_matches(h->Caption.s, h->Caption.len, s);
}

bool ui_hotspot_captioned(int j, const char *caption)
{
    if (j < 1 || j > ds.Mouse_HotspotCount)
        return false;
    const THotspot *h = Mouse_Hotspot((int16_t)j);
    return h->W > 0 && h->H > 0 && caption_is(h, caption);
}

const char *ui_hotspot_caption(int j)
{
    static char s[19];
    s[0] = 0;
    if (j < 1 || j > ds.Mouse_HotspotCount)
        return s;
    const THotspot *h = Mouse_Hotspot((int16_t)j);
    if (h->W <= 0 || h->H <= 0)
        return s;
    size_t start = 0, len = SDL_min((size_t)h->Caption.len, sizeof s - 1);
    while (len > 0 && h->Caption.s[len - 1] == ' ')
        len--;
    while (start < len && h->Caption.s[start] == ' ')
        start++;
    SDL_memcpy(s, h->Caption.s + start, len - start);
    s[len - start] = 0;
    char english[64];
    SDL_strlcpy(s, lang_untranslate_line(s, english, sizeof english), sizeof s);
    return s;
}

int ui_find_hotspot(const char *caption, int k)
{
    for (int j = 1; j <= ds.Mouse_HotspotCount; j++)
        if (ui_hotspot_captioned(j, caption) && --k == 0)
            return j;
    return 0;
}

static bool in_rect(const THotspot *h, int x, int y, int margin)
{
    return h->W > 0 && h->H > 0 && x >= h->X - margin && x < h->X + h->W + margin
           && y >= h->Y - margin && y < h->Y + h->H + margin;
}

// A lower hotspot than j covers (x, y), or comes within `margin` of it.
static bool covered(int j, int x, int y, int margin)
{
    for (int k = 1; k < j; k++)
        if (in_rect(Mouse_Hotspot((int16_t)k), x, y, margin))
            return true;
    return false;
}

// The point of hotspot j nearest its middle, inside its 2-pixel rim, that
// no lower hotspot comes within `margin` of.
static bool clear_point(int j, int margin, int *x, int *y)
{
    const THotspot *h = Mouse_Hotspot((int16_t)j);
    int cx = h->X + h->W / 2, cy = h->Y + h->H / 2, best = -1;
    int rim = h->W > 4 && h->H > 4 ? 2 : 0;
    for (int py = h->Y + rim; py < h->Y + h->H - rim; py++) {
        for (int px = h->X + rim; px < h->X + h->W - rim; px++) {
            int d = (px - cx) * (px - cx) + (py - cy) * (py - cy);
            if ((best < 0 || d < best) && !covered(j, px, py, margin)) {
                best = d;
                *x = px;
                *y = py;
            }
        }
    }
    return best >= 0;
}

// Some lower hotspot overlaps hotspot j.
static bool overlapped(int j)
{
    const THotspot *h = Mouse_Hotspot((int16_t)j);
    for (int k = 1; k < j; k++) {
        const THotspot *o = Mouse_Hotspot((int16_t)k);
        if (o->W > 0 && o->H > 0 && o->X < h->X + h->W && h->X < o->X + o->W && o->Y < h->Y + h->H
            && h->Y < o->Y + o->H)
            return true;
    }
    return false;
}

static bool never(void *unused)
{
    (void)unused;
    return false;
}

void ui_click_hotspot(int j, UiClick how)
{
    const THotspot *h = Mouse_Hotspot((int16_t)j);
    int x = h->X + h->W / 2, y = h->Y + h->H / 2;
    if (overlapped(j)) {
        // Wait for the planets to part, then take what is clear.
        int i = 0;
        for (; i < AIM_TRIES && !clear_point(j, AIM_MARGIN, &x, &y); i++)
            ui_wait_until(never, nullptr, AIM_STEP);
        if (i == AIM_TRIES && !clear_point(j, 0, &x, &y)) {
            x = h->X + h->W / 2;
            y = h->Y + h->H / 2;
        }
    }
    ui_click_as(x, y, how);
}

bool ui_hotspot_as(const char *caption, int k, UiClick how)
{
    ui_settle();
    int j = ui_find_hotspot(caption, k);
    if (j)
        ui_click_hotspot(j, how);
    return j != 0;
}

bool ui_hotspot(const char *caption)
{
    return ui_hotspot_as(caption, 1, UI_CLICK);
}

// --- Clock ---

int ui_fast_forward(bool (*pred)(void *arg), void *arg, int hours)
{
    for (int h = 0; h < hours; h++) {
        if (pred(arg))
            return h;
        int16_t hour = ds.Game_Hour;  // the date on the status bar
        ui_click(CLOCK_X, CLOCK_Y);
        for (int i = 0; i < 20 && ds.Game_Hour == hour; i++)
            play_wait(1);
    }
    return pred(arg) ? hours : -1;
}

// --- Navigation ---

void ui_open_screen(const char *label, int screen)
{
    // A story scene that comes up as the button is pressed replaces the
    // screen switch (the scene returns to the command centre): the way is
    // taken again from where it left off.
    for (int tries = 1;; tries++) {
        if (ui_screen() == screen) {
            ui_settle();
            return;
        }
        int scenes = ui_scenes_played();
        if (!ui_toolbar_has(label) && screen != 1) {
            if (!ui_toolbar("BACK TO M.SCREEN") || !ui_wait_screen(1, SCREEN_TIMEOUT)) {
                if (tries < 3 && ui_scenes_played() != scenes)
                    continue;
                play_fail("no way back to the command centre from screen %d", ui_screen());
            }
        }
        if (ui_toolbar(label) && ui_wait_screen(screen, SCREEN_TIMEOUT))
            return;
        if (tries == 3 || ui_scenes_played() == scenes)
            play_fail("'%s' did not open screen %d (on %d)", label, screen, ui_screen());
        play_log("'%s': a story scene came between, again", label);
    }
}

void ui_hire(const char *kind, const char *name)
{
    if (ui_screen() != 1)
        play_fail("ui_hire: not on the command centre");
    if (!ui_hotspot("COMMANDERS") || !ui_wait_screen(2, 2000))
        play_fail("ui_hire: no hire screen");
    if (!ui_toolbar(kind) || !ui_wait_hotspot(name, 500))
        play_fail("ui_hire: no %s among the %s", name, kind);
    ui_hotspot(name);
    ui_toolbar("HIRE MAN");
    ui_wait_idle(2000);
    play_dump("hired");
    play_log("hired %s", name);
    if (!ui_toolbar("BACK TO M.SCREEN") || !ui_wait_screen(1, 2000))
        play_fail("ui_hire: no way back to the command centre");
}

static bool command_centre_running(void *unused)
{
    (void)unused;
    return ds.Game_CurrentScreen == 1 && ds.Game_ClockRunning && ds.Game_Credits > 0;
}

void ui_new_game(void)
{
    // The start menu and the hero choice draw no hotspots; their buttons are
    // fixed (tests/test_boot.c).
    play_wait(40);
    play_click(160, 60);  // NEW GAME
    play_wait(80);
    play_click(80, 100);  // left hero
    if (!play_wait_until(command_centre_running, nullptr, 3000) || !ui_wait_screen(1, 3000))
        play_fail("no command centre after new game");
    play_log("new game started");
}

void ui_log_screen(void)
{
    char line[512] = "";
    for (int slot = 1; slot <= 12; slot++) {
        if (slot_command(slot) == 0)
            continue;
        const uint8_t *l = slot_label(slot);
        SDL_snprintf(line + SDL_strlen(line), sizeof line - SDL_strlen(line), "%d:'%.*s'(%d) ", slot,
                     l[0], (const char *)l + 1, slot_command(slot));
    }
    play_log("screen %d toolbar(page %d): %s", ds.Game_CurrentScreen, page1_shown() ? 1 : 2, line);
    line[0] = 0;
    for (int j = 1; j <= ds.Mouse_HotspotCount; j++) {
        const THotspot *h = Mouse_Hotspot((int16_t)j);
        SDL_snprintf(line + SDL_strlen(line), sizeof line - SDL_strlen(line), "'%.*s' ",
                     h->Caption.len, h->Caption.s);
    }
    play_log("  hotspots: %s", line);
}
