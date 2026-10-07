#include "platform/video.h"

#include <SDL3/SDL.h>

// Mode 13h was shown on a 4:3 monitor, so its pixels are 20% taller than wide.
enum : int { DISPLAY_HEIGHT = VID_WIDTH * 3 / 4 };

uint8_t vid_screen[VID_HEIGHT][VID_WIDTH];
uint8_t vid_palette[256][3];

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;

bool vid_init(const char *title)
{
    if (!SDL_CreateWindowAndRenderer(title, VID_WIDTH * 4, DISPLAY_HEIGHT * 4,
                                     SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Window: %s", SDL_GetError());
        return false;
    }

    SDL_SetRenderVSync(renderer, 1);
    SDL_SetRenderLogicalPresentation(renderer, VID_WIDTH, DISPLAY_HEIGHT,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888,
                                SDL_TEXTUREACCESS_STREAMING, VID_WIDTH, VID_HEIGHT);
    if (!texture) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Texture: %s", SDL_GetError());
        return false;
    }
    // Keeps pixels sharp under the non-integer 200 -> 240 line stretch.
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_PIXELART);
    return true;
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
        for (int y = 0; y < VID_HEIGHT; y++) {
            uint32_t *row = (uint32_t *)((uint8_t *)pixels + y * pitch);
            for (int x = 0; x < VID_WIDTH; x++)
                row[x] = lut[vid_screen[y][x]];
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
