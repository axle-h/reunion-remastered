#include "platform/autopilot.h"

#include <SDL3/SDL.h>

#include "platform/input.h"
#include "platform/video.h"

enum : int { MAX_EVENTS = 4096, CLICK_FRAMES = 4 };

typedef enum { EV_CLICK, EV_RCLICK, EV_MOVE, EV_KEY, EV_DUMP, EV_QUIT } EventKind;

typedef struct {
    uint64_t frame;
    EventKind kind;
    int x, y;
    char path[512];
} Event;

static Event events[MAX_EVENTS];
static int event_count;
static bool active;
static bool initialised;
// A click in progress: release at this frame.
static uint64_t release_at;
static bool pressed;
static int mouse_x, mouse_y;
// Event frames count from the first retrace after the script was set.
static uint64_t base;
static bool have_base;

// Random input (soak tests): "random:SEED:FRAMES[:START]".
static struct {
    bool on;
    uint64_t state;     // splitmix64, independent of the game's RandSeed
    uint32_t seed;
    uint64_t start, end;
    uint8_t left, right;  // buttons held by the current random click
} rnd;

static bool parse_event(const char *s, Event *ev)
{
    char kind[16] = "";
    unsigned long long frame;
    int n = 0;
    if (SDL_sscanf(s, "%llu:%15[a-z]%n", &frame, kind, &n) < 2)
        return false;
    ev->frame = frame;
    const char *rest = s + n;
    if (*rest == ':')
        rest++;
    if (SDL_strcmp(kind, "click") == 0 || SDL_strcmp(kind, "rclick") == 0
        || SDL_strcmp(kind, "move") == 0) {
        if (SDL_sscanf(rest, "%d,%d", &ev->x, &ev->y) != 2)
            return false;
        ev->kind = kind[0] == 'c' ? EV_CLICK : kind[0] == 'r' ? EV_RCLICK : EV_MOVE;
        return true;
    }
    if (SDL_strcmp(kind, "key") == 0) {
        ev->kind = EV_KEY;
        return SDL_sscanf(rest, "%d", &ev->x) == 1;
    }
    if (SDL_strcmp(kind, "dump") == 0) {
        ev->kind = EV_DUMP;
        SDL_strlcpy(ev->path, rest, sizeof ev->path);
        return ev->path[0] != 0;
    }
    if (SDL_strcmp(kind, "quit") == 0) {
        ev->kind = EV_QUIT;
        return true;
    }
    return false;
}

// "random:SEED:FRAMES[:START]": random input from frame START (default 0),
// quit at frame FRAMES.
static bool parse_random(const char *s)
{
    unsigned long seed;
    unsigned long long frames, start = 0;
    int n = SDL_sscanf(s, "random:%lu:%llu:%llu", &seed, &frames, &start);
    if (n < 2 || start > frames)
        return false;
    rnd.on = true;
    rnd.seed = (uint32_t)seed;
    rnd.state = seed;
    rnd.start = start;
    rnd.end = frames;
    return true;
}

static uint64_t rnd_next(void)
{
    uint64_t z = (rnd.state += 0x9E3779B97F4A7C15u);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9u;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBu;
    return z ^ (z >> 31);
}

static int rnd_below(int n)
{
    return (int)(rnd_next() % (uint64_t)n);
}

void autopilot_set_script(const char *script)
{
    initialised = true;
    event_count = 0;
    pressed = false;
    have_base = false;
    rnd.on = false;
    active = script && *script;
    if (!active)
        return;
    char *copy = SDL_strdup(script);
    char *save = nullptr;
    for (char *tok = SDL_strtok_r(copy, ";", &save); tok; tok = SDL_strtok_r(nullptr, ";", &save)) {
        while (*tok == ' ' || *tok == '\n')
            tok++;
        if (!*tok)
            continue;
        if (SDL_strncmp(tok, "random:", 7) == 0) {
            if (!parse_random(tok))
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Autopilot: bad event '%s'", tok);
            continue;
        }
        if (event_count < MAX_EVENTS && parse_event(tok, &events[event_count]))
            event_count++;
        else
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Autopilot: bad event '%s'", tok);
    }
    SDL_free(copy);
}

bool autopilot_init(void)
{
    if (!initialised)
        autopilot_set_script(SDL_getenv("REUNION_AUTOPILOT"));
    return active;
}

bool autopilot_active(void)
{
    return active;
}

bool autopilot_random_seed(uint32_t *seed)
{
    autopilot_init();
    if (active && rnd.on)
        *seed = rnd.seed;
    return active && rnd.on;
}

// Keys a player might press: ReadKey codes, 0x100 + scan code = extended.
static const int random_keys[] = {
    13, 13, 13, 27, 27, 32, 32, 8, 9, 'y', 'n', 'Y', 'N', '0', '1', '2', '3', '5', '9',
    'a', 'e', 'q', 's', 'x', 'z', '+', '-',
    0x100 + 72, 0x100 + 80, 0x100 + 75, 0x100 + 77,             // arrows
    0x100 + 71, 0x100 + 79, 0x100 + 73, 0x100 + 81,             // Home End PgUp PgDn
    0x100 + 59, 0x100 + 60, 0x100 + 61, 0x100 + 62, 0x100 + 68, // F1-F4, F10
};

static void random_frame(uint64_t frame)
{
    if (frame >= rnd.end) {
        if (frame == rnd.end) {
            SDL_Log("Autopilot: random run finished at frame %llu", (unsigned long long)frame);
            input_request_quit();
        }
        return;
    }
    if (frame < rnd.start)
        return;
    if (pressed) {
        // Sometimes drag while the button is down.
        if (rnd_below(4) == 0) {
            mouse_x = SDL_clamp(mouse_x + rnd_below(9) - 4, 0, 319);
            mouse_y = SDL_clamp(mouse_y + rnd_below(9) - 4, 0, 199);
            input_set_mouse(mouse_x, mouse_y, rnd.left, rnd.right);
        }
        return;
    }
    int r = rnd_below(1000);
    if (r < 75) {
        // Click: mostly left, short; now and then held long (scrolling, drags).
        mouse_x = rnd_below(320);
        mouse_y = rnd_below(200);
        rnd.left = r >= 10;
        rnd.right = !rnd.left;
        input_set_mouse(mouse_x, mouse_y, rnd.left, rnd.right);
        pressed = true;
        release_at = frame + 1 + (uint64_t)(rnd_below(8) == 0 ? rnd_below(60) : rnd_below(6));
    } else if (r < 90) {
        mouse_x = rnd_below(320);
        mouse_y = rnd_below(200);
        input_set_mouse(mouse_x, mouse_y, 0, 0);
    } else if (r < 105) {
        int key = random_keys[rnd_below((int)SDL_arraysize(random_keys))];
        if (key >= 0x100) {
            input_push_key(0);
            input_push_key(key - 0x100);
        } else {
            input_push_key(key);
        }
    }
}

void autopilot_frame(uint64_t frame)
{
    if (!active)
        return;
    if (!have_base) {
        base = frame;
        have_base = true;
    }
    frame -= base;
    if (pressed && frame >= release_at) {
        input_set_mouse(mouse_x, mouse_y, 0, 0);
        pressed = false;
    }
    for (int i = 0; i < event_count; i++) {
        const Event *ev = &events[i];
        if (ev->frame != frame)
            continue;
        switch (ev->kind) {
        case EV_CLICK:
        case EV_RCLICK:
            mouse_x = ev->x;
            mouse_y = ev->y;
            input_set_mouse(ev->x, ev->y, ev->kind == EV_CLICK, ev->kind == EV_RCLICK);
            pressed = true;
            release_at = frame + CLICK_FRAMES;
            break;
        case EV_MOVE:
            mouse_x = ev->x;
            mouse_y = ev->y;
            input_set_mouse(ev->x, ev->y, 0, 0);
            break;
        case EV_KEY:
            input_push_key(ev->x);
            break;
        case EV_DUMP:
            if (vid_save_image(ev->path))
                SDL_Log("Autopilot: frame %llu saved to %s", (unsigned long long)frame, ev->path);
            break;
        case EV_QUIT:
            input_request_quit();
            break;
        }
    }
    if (rnd.on)
        random_frame(frame);
}
