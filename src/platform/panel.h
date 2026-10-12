// Drawing for the port's own screens, the port menu and the import screen:
// a panel in a 320x240 space scaled to the window (by whole steps, which
// keep SDL's 8x8 debug font sharp), over the dimmed game frame.
#pragma once

#include <SDL3/SDL_render.h>

enum : int {
    PANEL_VW = 320,
    PANEL_VH = 240,
    PANEL_CHAR = 8,
    PANEL_X = 8,
    PANEL_Y = 8,
    PANEL_W = 304,
    PANEL_H = 224,
    PANEL_TOP = 32,  // below the title and its rule
};

// Dims the frame and draws the panel, `title` at its top left and `note`
// at its top right; leaves the renderer scaled to the 320x240 space.
void panel_begin(SDL_Renderer *r, const char *title, const char *note);
// Back to output pixels.
void panel_end(SDL_Renderer *r);

void panel_color(SDL_Renderer *r, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
void panel_fill(SDL_Renderer *r, float x, float y, float w, float h);
void panel_text(SDL_Renderer *r, float x, float y, const char *s);
// Word-wraps `s` into lines of `cols` characters, `step` apart, from y
// while a line fits above max_y. Returns the y below the last line.
float panel_wrap(SDL_Renderer *r, float x, float y, int cols, float step, float max_y,
                 const char *s);

// A window position (mouse events) in the 320x240 space.
void panel_mouse(float wx, float wy, float *x, float *y);
bool panel_inside(float x, float y, float rx, float ry, float rw, float rh);
