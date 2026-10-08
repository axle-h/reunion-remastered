#include "platform/video.h"

#include <SDL3/SDL.h>

// Both modes were shown on a 4:3 monitor (13h pixels are 20% taller than wide).
enum : int { DISPLAY_HEIGHT = VID_WIDTH * 3 / 4 };

static uint8_t screen_buffer[VID_HEIGHT][VID_WIDTH];
uint8_t (*vid_screen)[VID_WIDTH] = screen_buffer;
uint8_t vid_palette[256][3];

uint8_t vid12_screen[VID12_HEIGHT][VID12_WIDTH];
uint8_t vid_attr[16] = { 0, 1, 2, 3, 4, 5, 0x14, 7, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f };
static VidMode mode = VID_MODE_13H;

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;

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

bool vid_init(const char *title)
{
    if (!SDL_CreateWindowAndRenderer(title, VID_WIDTH * 4, DISPLAY_HEIGHT * 4,
                                     SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Window: %s", SDL_GetError());
        return false;
    }

    // No display vsync: the game is paced at the VGA's 70.086 Hz by
    // timing.c, and a 60 Hz display's vsync would slow it down by 14%.
    SDL_SetRenderVSync(renderer, 0);
    // The game draws its own mouse pointer into the frame.
    SDL_HideCursor();
    return make_texture();
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
    SDL_RenderPresent(renderer);
}

void vid_toggle_fullscreen(void)
{
    bool fullscreen = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN;
    SDL_SetWindowFullscreen(window, !fullscreen);
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
    char *pref = SDL_GetPrefPath("Reunion", "Reunion");
    if (!pref) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Screenshot: %s", SDL_GetError());
        return;
    }
    char dir[1024], path[1100];
    SDL_snprintf(dir, sizeof dir, "%sscreenshots", pref);
    SDL_free(pref);
    SDL_CreateDirectory(dir);
    SDL_Time now = 0;
    SDL_GetCurrentTime(&now);
    SDL_DateTime dt = {0};
    SDL_TimeToDateTime(now, &dt, true);
    SDL_snprintf(path, sizeof path, "%s/reunion-%04d%02d%02d-%02d%02d%02d-%03d.bmp", dir, dt.year,
                 dt.month, dt.day, dt.hour, dt.minute, dt.second, dt.nanosecond / 1000000);
    if (vid_save_image(path))
        SDL_Log("Screenshot saved to %s", path);
}

SDL_Renderer *vid_renderer(void)
{
    return renderer;
}
