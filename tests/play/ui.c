#include "play/ui.h"

#include <SDL3/SDL.h>

#include "game/dseg.h"
#include "game/main_program.h"
#include "game/mouse.h"
#include "play/play.h"

enum : int {
    TOOLBAR_CELL_W = 0x30,  // Mouse_HitTest: six 48-pixel cells, y 0..32
    TOOLBAR_Y = 16,
    TOOLBAR_TOGGLE_X = 0x130,
    SETTLE = 30,            // retraces for fades after a screen is set up
};

// Toolbar slot records 1..12 (DS:7C51 + 21n: label string[18], command).
static const uint8_t *slot_label(int slot)
{
    return (const uint8_t *)ds_at((uint16_t)(0x7C51 + slot * 21));
}

static int16_t slot_command(int slot)
{
    return *(const int16_t *)ds_at((uint16_t)(0x7C64 + slot * 21));
}

static bool page1_shown(void)
{
    return *(const uint8_t *)ds_at(0x7C63) != 0;
}

// Pascal string p equals s, ignoring case and p's trailing blanks (toolbar
// labels are padded).
static bool pstr_equals(const uint8_t *p, const char *s)
{
    size_t len = p[0], n = SDL_strlen(s);
    while (len > 0 && p[len] == ' ')
        len--;
    return len == n && SDL_strncasecmp((const char *)p + 1, s, n) == 0;
}

int ui_screen(void)
{
    return ds.Game_CurrentScreen;
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
    if (!play_wait_until(screen_shown, &screen, timeout))
        return false;
    uint64_t since = Game_EventWaitFrames;
    return play_wait_until(settled, &since, timeout) && ds.Game_CurrentScreen == screen;
}

// The main loop has been waiting for input for the last few retraces: the
// screen is not busy (fading, typing text, animating a door) and takes clicks.
enum : int { IDLE_RUN = 3 };
static struct { uint64_t last; int run; } idle;

static bool is_idle(void *unused)
{
    (void)unused;
    if (Game_EventWaitFrames != idle.last) {
        idle.run = Game_EventWaitFrames == idle.last + 1 ? idle.run + 1 : 1;
        idle.last = Game_EventWaitFrames;
    } else {
        idle.run = 0;
    }
    return idle.run >= IDLE_RUN;
}

bool ui_wait_idle(int timeout)
{
    idle.last = Game_EventWaitFrames;
    idle.run = 0;
    return play_wait_until(is_idle, nullptr, timeout);
}

void ui_click(int x, int y)
{
    if (!ui_wait_idle(5000))
        play_fail("the game stayed busy before a click at %d,%d", x, y);
    play_click(x, y);
}

static bool page_is(void *arg)
{
    return page1_shown() == *(bool *)arg;
}

bool ui_toolbar(const char *label)
{
    for (int slot = 1; slot <= 12; slot++) {
        if (slot_command(slot) == 0 || !pstr_equals(slot_label(slot), label))
            continue;
        bool want_page1 = slot <= 6;
        if (page1_shown() != want_page1) {
            ui_click(TOOLBAR_TOGGLE_X, TOOLBAR_Y);
            if (!play_wait_until(page_is, &want_page1, 200))
                play_fail("toolbar page did not turn for '%s'", label);
            play_wait(SETTLE);
        }
        int col = (slot - 1) % 6;
        ui_click(col * TOOLBAR_CELL_W + TOOLBAR_CELL_W / 2, TOOLBAR_Y);
        return true;
    }
    return false;
}

static int find_hotspot(const char *caption)
{
    for (int j = 1; j <= ds.Mouse_HotspotCount; j++)
        if (pstr_equals((const uint8_t *)&Mouse_Hotspot((int16_t)j)->Caption, caption))
            return j;
    return 0;
}

static bool hotspot_shown(void *arg)
{
    return find_hotspot(arg) != 0;
}

bool ui_wait_hotspot(const char *caption, int timeout)
{
    return play_wait_until(hotspot_shown, (void *)caption, timeout);
}

bool ui_hotspot(const char *caption)
{
    int j = find_hotspot(caption);
    if (j == 0)
        return false;
    const THotspot *h = Mouse_Hotspot((int16_t)j);
    ui_click(h->X + h->W / 2, h->Y + h->H / 2);
    return true;
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
