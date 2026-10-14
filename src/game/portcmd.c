#include "game/portcmd.h"

#include <SDL3/SDL.h>

#include "game/blit.h"
#include "game/gfxpath.h"
#include "game/lang.h"
#include "game/system.h"

// The route editor draws in the equipment screen's frame (BEOSZTAS) and sets
// its own toolbar after BACK TO M.SCREEN and CONTROL PANEL.
static const TScreenInfo route_screen = {
    .Name = {8, {'b', 'e', 'o', 's', 'z', 't', 'a', 's'}},
    .Command = -1,
    .IconCount = 2,
    .Icons = {24, 16},
};

typedef struct PortCommand {
    const char *label;  // as the original's, 17 characters; translated in lang.c
    const char *sound;
} PortCommand;

static const PortCommand commands[CMD_COUNT - CMD_ORIGINAL_COUNT] = {
    [CMD_ROUTE - CMD_ORIGINAL_COUNT] = {"ROUTE            ", "controll"},
    [CMD_ADD_STOP - CMD_ORIGINAL_COUNT] = {"ADD STOP         ", "unitadd"},
    [CMD_REMOVE_STOP - CMD_ORIGINAL_COUNT] = {"REMOVE STOP      ", "cancel"},
    [CMD_START_ROUTE - CMD_ORIGINAL_COUNT] = {"START ROUTE      ", "okay"},
    [CMD_END_ROUTE - CMD_ORIGINAL_COUNT] = {"END ROUTE        ", "abort"},
};

static const PortCommand *command(int16_t id)
{
    return id >= CMD_ORIGINAL_COUNT && id < CMD_COUNT ? &commands[id - CMD_ORIGINAL_COUNT] : nullptr;
}

const TScreenInfo *PortCmd_Screen(int16_t screen)
{
    if (screen == SCREEN_ROUTE)
        return &route_screen;
    return &ds.Ui_Screens[screen - 1];
}

void PortCmd_Label(int16_t id, char *out, size_t size)
{
    const PortCommand *c = command(id);
    if (c)
        SDL_strlcpy(out, Lang_Text(c->label), size);
    else
        pstr_get(out, size, &ds.Ui_ButtonLabels[id]);
}

void PortCmd_Sound(int16_t id, char *out, size_t size)
{
    const PortCommand *c = command(id);
    if (c)
        SDL_strlcpy(out, c->sound, size);
    else
        pstr_get(out, size, &ds.Ui_ButtonSounds[id - 1]);
}

// --- Icons ----------------------------------------------------------------------

enum : int {
    ICON_W = 40,
    ICON_H = 24,
    ROW = 320,
    AT = 0x504,  // where GfxPath_DrawIcon puts an icon in a cell
};

// The original commands whose icons the port's are made from.
enum : int16_t {
    CMD_PLANET_INFO = 9,
    CMD_ADD_STATION = 10,
    CMD_END_TALK = 54,
    CMD_ABORT_MOVE = 57,
    CMD_OK_BUILD = 59,
};

// Colours of the icons, after GfxPath_DrawIcon's +2: the background, the
// "?" of PLANET INFO (white and three greens) and END TALK's cross.
enum : uint8_t {
    SPACE = 0x02,
    WHITE = 0x19,
    GREEN_LIGHT = 0x1E,
    GREEN = 0x1F,
    GREEN_DARK = 0x21,
    RED_LIGHT = 0x13,
    RED = 0x14,
    RED_MID = 0x16,
    RED_DARK = 0x18,
};

// The "?" of PLANET INFO lies in this box, the planet in the circle.
enum : int {
    Q_X0 = 23, Q_Y0 = 6, Q_X1 = 34, Q_Y1 = 16,
    PLANET_X = 13, PLANET_Y = 11, PLANET_R2 = 76,  // radius about 8.7
    MOON_X = 34, MOON_Y = 4, MOON_R2 = 21,         // ADD MINER STATION's small planet
};

static uint8_t *pixel(uint8_t *icon, int x, int y)
{
    return icon + AT + y * ROW + x;
}

// The original icon of command `id`, decoded into a cell-sized buffer.
static void decode(int16_t id, uint8_t *buf)
{
    GfxPath_DrawIcon(ds.GfxPath_CommandIcon[id], buf);
}

static bool in_question(uint8_t c)
{
    return c == WHITE || c == GREEN_LIGHT || c == GREEN || c == GREEN_DARK;
}

static void erase_question(uint8_t *icon)
{
    for (int y = Q_Y0; y <= Q_Y1; y++)
        for (int x = Q_X0; x <= Q_X1; x++)
            if (in_question(*pixel(icon, x, y)))
                *pixel(icon, x, y) = SPACE;
}

// A bar two pixels thick from (x0, y0), `len` long, across (horizontal) or
// down: lit on top or left, dark tips.
static void bar(uint8_t *icon, int x0, int y0, int len, bool across, uint8_t lit, uint8_t body,
                uint8_t tip)
{
    for (int i = 0; i < len; i++) {
        bool end = i == 0 || i == len - 1;
        for (int t = 0; t < 2; t++) {
            int x = across ? x0 + i : x0 + t, y = across ? y0 + t : y0 + i;
            *pixel(icon, x, y) = end ? tip : t == 0 ? lit : body;
        }
    }
}

// PLANET INFO with a + or a - where the "?" was.
static void sign_icon(uint8_t *icon, bool plus)
{
    decode(CMD_PLANET_INFO, icon);
    erase_question(icon);
    if (plus) {
        bar(icon, 24, 10, 10, true, GREEN_LIGHT, GREEN, GREEN_DARK);
        bar(icon, 28, 6, 10, false, GREEN_LIGHT, GREEN, GREEN_DARK);
        *pixel(icon, 28, 10) = WHITE;
    } else {
        bar(icon, 24, 10, 10, true, RED, RED_MID, RED_DARK);
        *pixel(icon, 25, 10) = RED_LIGHT;
    }
}

static int dist2(int x, int y, int cx, int cy)
{
    return (x - cx) * (x - cx) + (y - cy) * (y - cy);
}

static int isqrt(int n)
{
    int r = 0;
    while ((r + 1) * (r + 1) <= n)
        r++;
    return r;
}

// The loop: an ellipse round (LOOP_X, LOOP_Y), lit above, dark below.
enum : int { LOOP_X = 20, LOOP_Y = 12, LOOP_RX = 14, LOOP_RY = 9 };

static void loop_pixel(uint8_t *icon, int x, int y, uint8_t c)
{
    if (x < 0 || x >= ICON_W - 1 || y < 0 || y >= ICON_H
        || dist2(x, y, PLANET_X, PLANET_Y) <= PLANET_R2)
        return;  // behind the planet
    *pixel(icon, x, y) = c;
}

// An ellipse with radii rx and ry.
static void ellipse(uint8_t *icon, int rx, int ry, uint8_t c)
{
    for (int dx = -rx; dx <= rx; dx++) {
        int dy = isqrt((rx * rx - dx * dx) * ry * ry / (rx * rx));
        loop_pixel(icon, LOOP_X + dx, LOOP_Y - dy, c);
        loop_pixel(icon, LOOP_X + dx, LOOP_Y + dy, c);
    }
    for (int dy = -ry; dy <= ry; dy++) {
        int dx = isqrt((ry * ry - dy * dy) * rx * rx / (ry * ry));
        loop_pixel(icon, LOOP_X - dx, LOOP_Y + dy, c);
        loop_pixel(icon, LOOP_X + dx, LOOP_Y + dy, c);
    }
}

// Two pixels thick, the outer one lit.
static void draw_loop(uint8_t *icon)
{
    ellipse(icon, LOOP_RX, LOOP_RY, GREEN_LIGHT);
    ellipse(icon, LOOP_RX - 1, LOOP_RY - 1, GREEN);
}

// PLANET INFO's planet and ADD MINER STATION's small planet on a loop: an
// ellipse round both, behind the planet, with the small one on it.
static void route_icon(uint8_t *icon)
{
    uint8_t moon[ICON_H * ROW + AT] = {};
    decode(CMD_ADD_STATION, moon);
    decode(CMD_PLANET_INFO, icon);
    erase_question(icon);
    draw_loop(icon);
    // The small planet, moved onto the loop's right side.
    int dx = LOOP_X + LOOP_RX - MOON_X, dy = LOOP_Y - MOON_Y;
    for (int y = 0; y < 10; y++)
        for (int x = MOON_X - 5; x <= MOON_X + 5 && x < ICON_W - 1; x++) {
            uint8_t c = *pixel(moon, x, y);
            if (c != SPACE && dist2(x, y, MOON_X, MOON_Y) <= MOON_R2 && x + dx < ICON_W - 1)
                *pixel(icon, x + dx, y + dy) = c;
        }
}

void PortCmd_DrawIcon(int16_t id, uint8_t *cell)
{
    uint8_t icon[ICON_H * ROW + AT] = {};
    switch (id) {
    case CMD_ROUTE:
        route_icon(icon);
        break;
    case CMD_ADD_STOP:
    case CMD_REMOVE_STOP:
        sign_icon(icon, id == CMD_ADD_STOP);
        break;
    case CMD_START_ROUTE:
        decode(CMD_OK_BUILD, icon);
        break;
    case CMD_END_ROUTE:
        decode(CMD_ABORT_MOVE, icon);
        break;
    default:
        GfxPath_DrawIcon(ds.GfxPath_CommandIcon[id], cell);
        return;
    }
    // As GfxPath_DrawIcon copies its 39 columns.
    Blit_CopyRectStrided(icon + AT, cell + AT, ICON_W - 1, ICON_H, ROW, ROW);
}
