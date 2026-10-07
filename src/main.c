#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "platform/video.h"

// Temporary test card until the game's own screens are ported: a 16x16 grid
// of all 256 palette entries.
static void draw_test_card(void)
{
    for (int i = 0; i < 256; i++) {
        vid_palette[i][0] = (uint8_t)((i & 7) * 9);
        vid_palette[i][1] = (uint8_t)((i >> 3 & 7) * 9);
        vid_palette[i][2] = (uint8_t)((i >> 6) * 21);
    }
    for (int y = 0; y < VID_HEIGHT; y++)
        for (int x = 0; x < VID_WIDTH; x++)
            vid_screen[y][x] = (uint8_t)(y * 16 / VID_HEIGHT * 16 + x * 16 / VID_WIDTH);
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init: %s", SDL_GetError());
        return 1;
    }
    if (!vid_init("Reunion")) {
        SDL_Quit();
        return 1;
    }

    draw_test_card();

    bool running = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT)
                running = false;
            else if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_ESCAPE)
                running = false;
            else if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_RETURN
                     && (ev.key.mod & SDL_KMOD_ALT))
                vid_toggle_fullscreen();
        }
        vid_present();
    }

    vid_shutdown();
    SDL_Quit();
    return 0;
}
