#include "platform/video.h"

#include <SDL3/SDL.h>

#include "platform/config.h"
#include "platform/playerdir.h"

// Both modes were shown on a 4:3 monitor (13h pixels are 20% taller than wide).
enum : int { DISPLAY_HEIGHT = VID_WIDTH * 3 / 4 };

// The VGA's 64 KB at A000h; rows 0..199 are shown. Games draw past row 199
// (and wrap 16-bit offsets inside the 64 KB), as on the hardware.
static uint8_t video_memory[0x10000];
uint8_t (*vid_screen)[VID_WIDTH] = (uint8_t (*)[VID_WIDTH])video_memory;
uint8_t vid_palette[256][3];

void vid_clear(void)
{
    SDL_memset(video_memory, 0, sizeof video_memory);
}

uint8_t vid12_screen[VID12_HEIGHT][VID12_WIDTH];
uint8_t vid_attr[16] = { 0, 1, 2, 3, 4, 5, 0x14, 7, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f };
static VidMode mode = VID_MODE_13H;

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static void (*overlay)(SDL_Renderer *);
// What Alt+Enter goes back to from a window.
static ConfigDisplay last_fullscreen = CONFIG_DESKTOP;

static bool make_texture(void)
{
    SDL_DestroyTexture(texture);
    texture = nullptr;
    int w = mode == VID_MODE_12H ? VID12_WIDTH : VID_WIDTH;
    int h = mode == VID_MODE_12H ? VID12_HEIGHT : VID_HEIGHT;
    SDL_SetRenderLogicalPresentation(renderer, w, w * 3 / 4, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888,
                                SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!texture) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Texture: %s", SDL_GetError());
        return false;
    }
    // Keeps pixels sharp under the non-integer line stretch.
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_PIXELART);
    return true;
}

void vid_set_mode(VidMode m)
{
    if (m == VID_MODE_12H) {
        SDL_memset(vid12_screen, 0, sizeof vid12_screen);
        vid_reset_attr();
    }
    mode = m;
    if (renderer)
        make_texture();
}

VidMode vid_mode(void)
{
    return mode;
}

void vid_reset_attr(void)
{
    static const uint8_t def[16] = { 0, 1, 2, 3, 4, 5, 0x14, 7, 0x38, 0x39, 0x3a, 0x3b,
                                     0x3c, 0x3d, 0x3e, 0x3f };
    SDL_memcpy(vid_attr, def, sizeof def);
}

bool vid12_blit_strip(int y, int rows, const uint8_t *pixels)
{
    if (!pixels || y < 0 || rows < 0 || y + rows > VID12_HEIGHT)
        return false;
    uint8_t *dst = &vid12_screen[y][0];  // rows run on past the first line
    for (int i = 0; i < rows * VID12_WIDTH; i++)
        dst[i] = pixels[i] & 15;
    return true;
}

void vid12_planar_write(int offset, uint8_t color, uint8_t bitmask, uint8_t planemask)
{
    if (offset < 0 || offset >= VID12_HEIGHT * VID12_WIDTH / 8)
        return;
    uint8_t *px = &vid12_screen[0][0] + offset * 8;
    for (int b = 0; b < 8; b++) {
        if (!(bitmask & (0x80 >> b)))
            continue;
        uint8_t keep = (uint8_t)~planemask & 15;
        px[b] = (px[b] & keep) | (color & planemask & 15);
    }
}

#ifdef __SANITIZE_ADDRESS__
// Read by LeakSanitizer at exit. Mesa's Gallium driver allocates a block
// once (in a pthread_once) when SDL's offscreen driver initialises EGL, and
// keeps the only pointer to it in its static data. SDL unloads the driver
// at exit, so LSan reports the block. It is not the game's, and not a real
// leak: with dlclose a no-op the report goes away.
const char *__lsan_default_suppressions(void);
const char *__lsan_default_suppressions(void)
{
    return "leak:SDL_EGL_InitializeOffscreen\n";
}
#endif

bool vid_init(const char *title)
{
    if (!SDL_CreateWindowAndRenderer(title, config.window_width, config.window_height,
                                     SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Window: %s", SDL_GetError());
        return false;
    }

    // No display vsync: the game is paced at the VGA's 70.086 Hz by
    // timing.c, and a 60 Hz display's vsync would slow it down by 14%.
    SDL_SetRenderVSync(renderer, 0);
    // The game draws its own mouse pointer into the frame.
    SDL_HideCursor();
    if (!make_texture())
        return false;
    if (config.display != CONFIG_WINDOWED)
        vid_apply_display();
    return true;
}

bool vid_visible(void)
{
    const char *driver = SDL_GetCurrentVideoDriver();
    return window && driver && SDL_strcmp(driver, "offscreen") != 0
           && SDL_strcmp(driver, "dummy") != 0;
}

void vid_shutdown(void)
{
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    texture = nullptr;
    renderer = nullptr;
    window = nullptr;
}

void vid_present(void)
{
    uint32_t lut[256];
    for (int i = 0; i < 256; i++) {
        // Expand 6-bit DAC values to 8 bits.
        uint32_t r = vid_palette[i][0] & 63;
        uint32_t g = vid_palette[i][1] & 63;
        uint32_t b = vid_palette[i][2] & 63;
        r = r << 2 | r >> 4;
        g = g << 2 | g >> 4;
        b = b << 2 | b >> 4;
        lut[i] = r << 16 | g << 8 | b;
    }

    void *pixels;
    int pitch;
    if (SDL_LockTexture(texture, nullptr, &pixels, &pitch)) {
        if (mode == VID_MODE_12H) {
            for (int y = 0; y < VID12_HEIGHT; y++) {
                uint32_t *row = (uint32_t *)((uint8_t *)pixels + y * pitch);
                for (int x = 0; x < VID12_WIDTH; x++)
                    row[x] = lut[vid_attr[vid12_screen[y][x] & 15]];
            }
        } else {
            for (int y = 0; y < VID_HEIGHT; y++) {
                uint32_t *row = (uint32_t *)((uint8_t *)pixels + y * pitch);
                for (int x = 0; x < VID_WIDTH; x++)
                    row[x] = lut[vid_screen[y][x]];
            }
        }
        SDL_UnlockTexture(texture);
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, nullptr, nullptr);
    if (overlay) {
        int w, h;
        SDL_RendererLogicalPresentation lp;
        SDL_GetRenderLogicalPresentation(renderer, &w, &h, &lp);
        SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
        overlay(renderer);
        SDL_SetRenderLogicalPresentation(renderer, w, h, lp);
    }
    SDL_RenderPresent(renderer);
}

void vid_set_overlay(void (*draw)(SDL_Renderer *))
{
    overlay = draw;
}

void vid_apply_display(void)
{
    if (!window)
        return;
    if (config.display == CONFIG_WINDOWED) {
        SDL_SetWindowFullscreen(window, false);
        SDL_SetWindowSize(window, config.window_width, config.window_height);
        return;
    }
    last_fullscreen = config.display;
    SDL_DisplayMode mode;
    SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    if (config.display == CONFIG_DESKTOP) {
        SDL_SetWindowFullscreenMode(window, nullptr);
    } else if (config.fullscreen_width > 0
               && SDL_GetClosestFullscreenDisplayMode(display, config.fullscreen_width,
                                                      config.fullscreen_height,
                                                      config.fullscreen_refresh, true, &mode)) {
        SDL_SetWindowFullscreenMode(window, &mode);
    } else {
        const SDL_DisplayMode *desktop = SDL_GetDesktopDisplayMode(display);
        SDL_SetWindowFullscreenMode(window, desktop);
    }
    SDL_SetWindowFullscreen(window, true);
}

void vid_toggle_fullscreen(void)
{
    config.display = config.display == CONFIG_WINDOWED ? last_fullscreen : CONFIG_WINDOWED;
    vid_apply_display();
    config_save();
}

void vid_window_event(const SDL_Event *e)
{
    // A window resized by hand keeps its size.
    if (e->type == SDL_EVENT_WINDOW_RESIZED && config.display == CONFIG_WINDOWED && window
        && !(SDL_GetWindowFlags(window) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED))) {
        config.window_width = e->window.data1;
        config.window_height = e->window.data2;
    }
}

int vid_window_sizes(int sizes[][2], int max)
{
    SDL_Rect bounds = {.w = 1920, .h = 1080};
    if (window)
        SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(window), &bounds);
    int n = 0;
    for (int k = 2; n < max; k++) {
        int w = VID_WIDTH * k, h = DISPLAY_HEIGHT * k;
        if ((w > bounds.w || h > bounds.h) && n > 0)
            break;
        sizes[n][0] = w;
        sizes[n][1] = h;
        n++;
    }
    return n;
}

int vid_fullscreen_modes(SDL_DisplayMode *modes, int max)
{
    int count = 0, n = 0;
    SDL_DisplayMode **all = window ? SDL_GetFullscreenDisplayModes(SDL_GetDisplayForWindow(window), &count)
                                   : nullptr;
    for (int i = 0; all && i < count && n < max; i++) {
        // Largest first; one entry per size and (whole) refresh rate.
        bool dup = false;
        for (int j = 0; j < n && !dup; j++)
            dup = modes[j].w == all[i]->w && modes[j].h == all[i]->h
                  && SDL_lroundf(modes[j].refresh_rate) == SDL_lroundf(all[i]->refresh_rate);
        if (!dup)
            modes[n++] = *all[i];
    }
    SDL_free(all);
    return n;
}

SDL_Window *vid_window(void)
{
    return window;
}

bool vid_save_image(const char *path)
{
    bool m12 = mode == VID_MODE_12H;
    int w = m12 ? VID12_WIDTH : VID_WIDTH, h = m12 ? VID12_HEIGHT : VID_HEIGHT;
    SDL_Surface *s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_INDEX8);
    if (!s)
        return false;
    SDL_Palette *pal = SDL_CreateSurfacePalette(s);
    SDL_Color colors[256];
    for (int i = 0; i < 256; i++) {
        uint8_t r = vid_palette[i][0] & 63, g = vid_palette[i][1] & 63, b = vid_palette[i][2] & 63;
        colors[i] = (SDL_Color){(uint8_t)(r << 2 | r >> 4), (uint8_t)(g << 2 | g >> 4),
                                (uint8_t)(b << 2 | b >> 4), 255};
    }
    if (pal)
        SDL_SetPaletteColors(pal, colors, 0, 256);
    for (int y = 0; y < h; y++) {
        uint8_t *row = (uint8_t *)s->pixels + y * s->pitch;
        for (int x = 0; x < w; x++)
            row[x] = m12 ? vid_attr[vid12_screen[y][x] & 15] : vid_screen[y][x];
    }
    size_t n = SDL_strlen(path);
    bool ok = n >= 4 && SDL_strcasecmp(path + n - 4, ".png") == 0 ? SDL_SavePNG(s, path)
                                                                   : SDL_SaveBMP(s, path);
    if (!ok)
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Saving %s: %s", path, SDL_GetError());
    SDL_DestroySurface(s);
    return ok;
}

void vid_screenshot(void)
{
    char *dir = playerdir_path("screenshots");
    if (!dir) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Screenshot: %s", SDL_GetError());
        return;
    }
    char path[1100];
    SDL_CreateDirectory(dir);
    SDL_Time now = 0;
    SDL_GetCurrentTime(&now);
    SDL_DateTime dt = {0};
    SDL_TimeToDateTime(now, &dt, true);
    SDL_snprintf(path, sizeof path, "%s/reunion-%04d%02d%02d-%02d%02d%02d-%03d.bmp", dir, dt.year,
                 dt.month, dt.day, dt.hour, dt.minute, dt.second, dt.nanosecond / 1000000);
    SDL_free(dir);
    if (vid_save_image(path))
        SDL_Log("Screenshot saved to %s", path);
}

SDL_Renderer *vid_renderer(void)
{
    return renderer;
}
