#include "platform/panel.h"

#include <SDL3/SDL.h>

#include "platform/video.h"

// Scale and offset of the 320x240 space in render output pixels.
static float scale = 1, origin_x, origin_y;

static void layout(SDL_Renderer *r)
{
    int w = PANEL_VW, h = PANEL_VH;
    if (r)
        SDL_GetCurrentRenderOutputSize(r, &w, &h);
    float s = SDL_min((float)w / PANEL_VW, (float)h / PANEL_VH);
    scale = s >= 1 ? SDL_floorf(s) : s;  // whole pixels keep the font sharp
    origin_x = SDL_floorf((w / scale - PANEL_VW) / 2);
    origin_y = SDL_floorf((h / scale - PANEL_VH) / 2);
}

void panel_mouse(float wx, float wy, float *x, float *y)
{
    SDL_Window *win = vid_window();
    int ww = 1, wh = 1, pw = 1, ph = 1;
    if (win) {
        SDL_GetWindowSize(win, &ww, &wh);
        SDL_GetWindowSizeInPixels(win, &pw, &ph);
    }
    layout(vid_renderer());
    *x = wx * pw / SDL_max(ww, 1) / scale - origin_x;
    *y = wy * ph / SDL_max(wh, 1) / scale - origin_y;
}

bool panel_inside(float x, float y, float rx, float ry, float rw, float rh)
{
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

void panel_color(SDL_Renderer *r, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    SDL_SetRenderDrawColor(r, red, green, blue, alpha);
}

void panel_fill(SDL_Renderer *r, float x, float y, float w, float h)
{
    SDL_FRect rect = {origin_x + x, origin_y + y, w, h};
    SDL_RenderFillRect(r, &rect);
}

void panel_text(SDL_Renderer *r, float x, float y, const char *s)
{
    SDL_RenderDebugText(r, origin_x + x, origin_y + y, s);
}

float panel_wrap(SDL_Renderer *r, float x, float y, int cols, float step, float max_y,
                 const char *s)
{
    while (*s && y + PANEL_CHAR <= max_y) {
        const char *nl = SDL_strchr(s, '\n');
        int n = nl ? (int)(nl - s) : (int)SDL_strlen(s);
        if (n > cols) {
            n = cols;
            while (n > 0 && s[n] != ' ')
                n--;
            if (n == 0)
                n = cols;
        }
        char line[128];
        SDL_strlcpy(line, s, (size_t)SDL_min(n + 1, (int)sizeof line));
        panel_text(r, x, y, line);
        s += n;
        if (*s == '\n')
            s++;
        else
            while (*s == ' ')
                s++;
        y += step;
    }
    return y;
}

void panel_begin(SDL_Renderer *r, const char *title, const char *note)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    panel_color(r, 0, 0, 0, 150);
    SDL_RenderFillRect(r, nullptr);

    layout(r);
    SDL_SetRenderScale(r, scale, scale);
    panel_color(r, 100, 120, 180, 255);
    panel_fill(r, PANEL_X - 1, PANEL_Y - 1, PANEL_W + 2, PANEL_H + 2);
    panel_color(r, 14, 18, 36, 225);
    panel_fill(r, PANEL_X, PANEL_Y, PANEL_W, PANEL_H);

    panel_color(r, 255, 215, 120, 255);
    panel_text(r, 16, PANEL_Y + 7, title);
    panel_color(r, 140, 150, 180, 255);
    panel_text(r, PANEL_X + PANEL_W - 8 - (float)(SDL_strlen(note) + 1) * PANEL_CHAR, PANEL_Y + 7,
               note);
    panel_color(r, 60, 72, 110, 255);
    panel_fill(r, PANEL_X + 4, PANEL_TOP - 8, PANEL_W - 8, 1);
}

void panel_end(SDL_Renderer *r)
{
    SDL_SetRenderScale(r, 1, 1);
}
