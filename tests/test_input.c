// input: key queue and mouse state from synthetic SDL events (dummy video driver).
#include <SDL3/SDL.h>
#include <stdio.h>

#include "platform/input.h"
#include "platform/video.h"

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("FAILED line %d: %s\n", __LINE__, #cond);              \
            ok = false;                                                     \
        }                                                                   \
    } while (0)

static void key(SDL_Keycode k, SDL_Keymod mod)
{
    SDL_Event e = {.key = {.type = SDL_EVENT_KEY_DOWN, .key = k, .mod = mod, .down = true}};
    SDL_PushEvent(&e);
}

static void text(const char *s)
{
    SDL_Event e = {.text = {.type = SDL_EVENT_TEXT_INPUT, .text = s}};
    SDL_PushEvent(&e);
}

static void button(int b, bool down, float x, float y)
{
    SDL_Event e = {.button = {.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP,
                             .button = b, .down = down, .x = x, .y = y}};
    SDL_PushEvent(&e);
}

int main(void)
{
    SDL_setenv_unsafe("SDL_VIDEO_DRIVER", "dummy", 1);
    if (!SDL_Init(SDL_INIT_VIDEO) || !vid_init("test")) {
        printf("no video, skipping\n");
        return 77;
    }
    input_init();
    bool ok = true;

    text("a");
    key(SDLK_RETURN, 0);
    key(SDLK_ESCAPE, 0);
    key(SDLK_UP, 0);
    key(SDLK_F1, 0);
    key(SDLK_X, SDL_KMOD_LALT);
    key(SDLK_DELETE, 0);
    input_pump();
    int expect[] = {'a', 13, 27, 0, 72, 0, 59, 0, 45, 0, 83};
    for (size_t i = 0; i < SDL_arraysize(expect); i++) {
        CHECK(input_key_pressed());
        CHECK(input_read_key() == expect[i]);
    }
    CHECK(!input_key_pressed());
    CHECK(input_read_key() == -1);

    int ww, wh;
    SDL_GetWindowSize(SDL_GetRenderWindow(vid_renderer()), &ww, &wh);
    button(SDL_BUTTON_LEFT, true, ww / 2.0f, wh / 2.0f);
    button(SDL_BUTTON_MIDDLE, true, ww / 2.0f, wh / 2.0f);
    input_pump();
    InputMouse m = input_mouse();
    CHECK(m.left == 1 && m.right == 0 && m.middle == 1);
    CHECK(SDL_abs(m.x - 160) <= 2 && SDL_abs(m.y - 100) <= 2);

    button(SDL_BUTTON_RIGHT, true, (float)ww * 2, (float)wh * 2);
    button(SDL_BUTTON_LEFT, false, (float)ww * 2, (float)wh * 2);
    input_pump();
    m = input_mouse();
    CHECK(m.left == 0 && m.right == 1);
    CHECK(m.x == 318 && m.y == 198);

    CHECK(!input_quit_requested());
    SDL_Event q = {.type = SDL_EVENT_QUIT};
    SDL_PushEvent(&q);
    input_pump();
    CHECK(input_quit_requested());

    vid_shutdown();
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
