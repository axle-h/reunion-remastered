#include "platform/input.h"

#include <SDL3/SDL.h>

#include "platform/autopilot.h"
#include "platform/video.h"

enum : int {
    MOUSE_MAX_X = 0x13E,
    MOUSE_MAX_Y = 0xC6,
    QUEUE_SIZE = 64,
    // Mode 13h lines are stretched to this many display lines.
    DISPLAY_HEIGHT = VID_WIDTH * 3 / 4,
};

static InputMouse mouse;
static bool quit;
static bool restart;
static bool menu_request;
static int queue[QUEUE_SIZE];
static int queue_head, queue_count;

static void push_key(int code)
{
    if (queue_count < QUEUE_SIZE)
        queue[(queue_head + queue_count++) % QUEUE_SIZE] = code;
}

void input_push_key(int code)
{
    push_key(code);
}

static void push_extended(int scan)
{
    push_key(0);
    push_key(scan);
}

static void set_position(float wx, float wy)
{
    float lx = wx, ly = wy;
    SDL_Renderer *renderer = vid_renderer();
    if (renderer)
        SDL_RenderCoordinatesFromWindow(renderer, wx, wy, &lx, &ly);
    mouse.x = SDL_clamp((int)lx, 0, MOUSE_MAX_X);
    mouse.y = SDL_clamp((int)(ly * VID_HEIGHT / DISPLAY_HEIGHT), 0, MOUSE_MAX_Y);
}

static int alt_scan(SDL_Keycode key)
{
    static const char rows[] = "qwertyuiop" "asdfghjkl" "zxcvbnm";
    static const uint8_t scans[] = {16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                                    30, 31, 32, 33, 34, 35, 36, 37, 38,
                                    44, 45, 46, 47, 48, 49, 50};
    if (key < 'a' || key > 'z')
        return 0;
    for (int i = 0; rows[i]; i++)
        if ((SDL_Keycode)rows[i] == key)
            return scans[i];
    return 0;
}

static void key_down(const SDL_KeyboardEvent *e)
{
    SDL_Keycode key = e->key;
    if (key == SDLK_RETURN && (e->mod & SDL_KMOD_ALT)) {
        vid_toggle_fullscreen();
        return;
    }
    if (key == SDLK_F12) {
        vid_screenshot();
        return;
    }
    // The port menu; scripted runs keep F1 as the game key it was.
    if (key == SDLK_F1 && !autopilot_active()) {
        if (!e->repeat)
            menu_request = true;
        return;
    }
    if (e->mod & SDL_KMOD_ALT) {
        int scan = alt_scan(key);
        if (scan)
            push_extended(scan);
        return;
    }
    if (key >= SDLK_F1 && key <= SDLK_F10) {
        push_extended(59 + (int)(key - SDLK_F1));
        return;
    }
    switch (key) {
    case SDLK_RETURN:
    case SDLK_KP_ENTER: push_key(13); break;
    case SDLK_ESCAPE: push_key(27); break;
    case SDLK_BACKSPACE: push_key(8); break;
    case SDLK_TAB: push_key(9); break;
    case SDLK_HOME: push_extended(71); break;
    case SDLK_UP: push_extended(72); break;
    case SDLK_PAGEUP: push_extended(73); break;
    case SDLK_LEFT: push_extended(75); break;
    case SDLK_RIGHT: push_extended(77); break;
    case SDLK_END: push_extended(79); break;
    case SDLK_DOWN: push_extended(80); break;
    case SDLK_PAGEDOWN: push_extended(81); break;
    case SDLK_INSERT: push_extended(82); break;
    case SDLK_DELETE: push_extended(83); break;
    default: break;
    }
}

void input_init(void)
{
    SDL_Renderer *renderer = vid_renderer();
    if (renderer)
        SDL_StartTextInput(SDL_GetRenderWindow(renderer));
}

void input_pump(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            quit = true;
            break;
        case SDL_EVENT_MOUSE_MOTION:
            set_position(e.motion.x, e.motion.y);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            uint8_t down = e.button.down;
            set_position(e.button.x, e.button.y);
            if (e.button.button == SDL_BUTTON_LEFT)
                mouse.left = down;
            else if (e.button.button == SDL_BUTTON_RIGHT)
                mouse.right = down;
            else if (e.button.button == SDL_BUTTON_MIDDLE)
                mouse.middle = down;
            break;
        }
        case SDL_EVENT_KEY_DOWN:
            key_down(&e.key);
            break;
        case SDL_EVENT_TEXT_INPUT:
            for (const char *p = e.text.text; *p; p++)
                if ((uint8_t)*p >= 32 && (uint8_t)*p < 127)
                    push_key(*p);
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            vid_window_event(&e);
            break;
        default:
            break;
        }
    }
}

bool input_quit_requested(void) { return quit; }

void input_request_quit(void) { quit = true; }

void input_request_restart(void) { restart = true; }

bool input_restart_requested(void) { return restart; }

bool input_take_menu_request(void)
{
    bool r = menu_request;
    menu_request = false;
    return r;
}

void input_reset(void)
{
    quit = false;
    restart = false;
    menu_request = false;
    queue_head = queue_count = 0;
    mouse.left = mouse.right = mouse.middle = 0;
}

void input_release(void)
{
    queue_head = queue_count = 0;
    mouse.left = mouse.right = mouse.middle = 0;
    float x, y;
    SDL_GetMouseState(&x, &y);
    set_position(x, y);
}

void input_set_mouse(int x, int y, uint8_t left, uint8_t right)
{
    mouse.x = SDL_clamp(x, 0, MOUSE_MAX_X);
    mouse.y = SDL_clamp(y, 0, MOUSE_MAX_Y);
    mouse.left = left;
    mouse.right = right;
}

InputMouse input_mouse(void) { return mouse; }

bool input_key_pressed(void) { return queue_count > 0; }

int input_read_key(void)
{
    if (!queue_count)
        return -1;
    int code = queue[queue_head];
    queue_head = (queue_head + 1) % QUEUE_SIZE;
    queue_count--;
    return code;
}
