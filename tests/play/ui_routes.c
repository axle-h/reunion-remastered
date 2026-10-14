#include "play/ui_routes.h"

#include <SDL3/SDL.h>

#include "play/play.h"
#include "play/ui.h"
#include "play/ui_message.h"
#include "play/ui_ships.h"

enum : int {
    SCR_GROUP = 17,
    SCR_DESTINATION = 18,
    SCR_ROUTE = 39,
    SCREEN_TIMEOUT = 3000,
    HIDDEN_HOURS = 48,
    // Where the editor writes (routes.c).
    LIST_Y = 0x62,
    ROW_PITCH = 10,
    NR_X = 0x14,
    STOP_X = 0x2E,
    STOP_CHARS = 40,
    STATE_X = 0xC,
    STATE_Y = 0xBA,
    STATE_CHARS = 0x31,
    COLOUR_SELECTED = 0xFC,
    COLOUR_HEADING = 0xF6,
};

static int row_y(int k)
{
    return LIST_Y + (k - 1) * ROW_PITCH + 1;
}

// The mouse cursor off the list and the footer: text under it doesn't read.
static void park_mouse(void)
{
    int x, y;
    ui_inert_point(0, &x, &y);
    play_mouse(x, y, 0);
    play_wait(2);
}

int ui_route_stops(char names[][48], int max)
{
    park_mouse();
    int n = 0;
    for (int k = 1; k <= UI_ROUTE_STOPS && n < max; k++) {
        char t[64];
        ui_read_text(STOP_X, row_y(k), STOP_CHARS, t, sizeof t);
        if (t[0] == '\0')
            break;
        SDL_strlcpy(names[n++], t, 48);
    }
    return n;
}

int ui_route_selected(void)
{
    park_mouse();
    for (int k = 1; k <= UI_ROUTE_STOPS; k++)
        if (ui_text_colour(STOP_X, row_y(k)) == COLOUR_SELECTED)
            return k;
    return 0;
}

int ui_route_heading(void)
{
    park_mouse();
    // The number is right-aligned in three cells.
    for (int k = 1; k <= UI_ROUTE_STOPS; k++)
        if (ui_text_colour(NR_X + 12, row_y(k)) == COLOUR_HEADING)
            return k;
    return 0;
}

void ui_route_state(char *out, size_t size)
{
    park_mouse();
    ui_read_text(STATE_X, STATE_Y, STATE_CHARS, out, size);
}

bool ui_route_open(const char *group)
{
    ui_open_group(group, SCR_GROUP);
    if (!ui_toolbar("ROUTE"))
        return false;
    if (!ui_wait_screen(SCR_ROUTE, SCREEN_TIMEOUT))
        play_fail("ui_route_open: no route editor");
    return true;
}

static bool on_editor(void *unused)
{
    (void)unused;
    return ui_screen() == SCR_ROUTE;
}

static bool not_yet(void *unused)
{
    (void)unused;
    return false;
}

bool ui_route_add(const char *system, const char *planet, const char *moon, char *refusal,
                  size_t size)
{
    refusal[0] = '\0';
    for (int hidden = 0; hidden < HIDDEN_HOURS; hidden++) {
        char before[UI_ROUTE_STOPS][48];
        int n = ui_route_stops(before, UI_ROUTE_STOPS);
        uint64_t since = play_frame();
        if (!ui_toolbar("ADD STOP"))
            play_fail("ui_route_add: no ADD STOP");
        ui_settle();
        if (ui_screen() == SCR_ROUTE) {  // refused before the map
            const char *box = ui_box_seen("", since);
            SDL_strlcpy(refusal, box ? box : "", size);
            return false;
        }
        if (!ui_wait_screen(SCR_DESTINATION, SCREEN_TIMEOUT))
            play_fail("ui_route_add: no destination map");
        UiPick pick = ui_pick_destination(system, planet, moon);
        if (!ui_wait_until(on_editor, nullptr, SCREEN_TIMEOUT))
            play_fail("ui_route_add: not back on the editor");
        ui_settle();
        const char *box = ui_box_seen("", since);
        if (box) {
            SDL_strlcpy(refusal, box, size);
            return false;
        }
        if (pick == UI_PICK_DONE) {
            char after[UI_ROUTE_STOPS][48];
            int m = ui_route_stops(after, UI_ROUTE_STOPS);
            return m == n + 1;
        }
        if (pick == UI_PICK_REFUSED)
            return false;
        play_log("ui_route_add: %s behind the sun, again in an hour", planet);
        ui_wait_hours(not_yet, nullptr, 1);
    }
    return false;
}

void ui_route_select(int k)
{
    if (!ui_hotspot_as("Select stop", k, UI_CLICK))
        play_fail("ui_route_select: no stop %d", k);
    ui_settle();
}

bool ui_route_move(int dir)
{
    bool ok = ui_hotspot(dir < 0 ? "Move stop up" : "Move stop down");
    ui_settle();
    return ok;
}
