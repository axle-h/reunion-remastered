#include "platform/autopilot.h"

#include <SDL3/SDL.h>

#include "platform/input.h"
#include "platform/video.h"

enum : int { CLICK_FRAMES = 4 };

typedef enum { EV_CLICK, EV_RCLICK, EV_MOVE, EV_MOUSE, EV_KEY, EV_DUMP, EV_QUIT } EventKind;

typedef struct {
    uint64_t frame;
    EventKind kind;
    int x, y, buttons;
    int order;   // position in the script
    char *path;  // EV_DUMP
} Event;

// Sorted by frame (stably, so same-frame events keep their order); `next` is
// the first one not played yet.
static Event *events;
static int event_count, event_cap, next_event;
static bool active;
static bool have_seed;
static uint32_t seed;
static void (*controller)(uint64_t frame);
static bool initialised;
// A click in progress: release at this frame.
static uint64_t release_at;
static bool pressed;
static int mouse_x, mouse_y;
// Event frames count from the first retrace after the script was set.
static uint64_t base;
static bool have_base;
// autopilot_set_frame_limit.
static uint64_t limit;
static bool limit_reached;

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
    if (SDL_strcmp(kind, "mouse") == 0) {
        ev->kind = EV_MOUSE;
        return SDL_sscanf(rest, "%d,%d,%d", &ev->x, &ev->y, &ev->buttons) == 3;
    }
    if (SDL_strcmp(kind, "key") == 0) {
        ev->kind = EV_KEY;
        return SDL_sscanf(rest, "%d", &ev->x) == 1;
    }
    if (SDL_strcmp(kind, "dump") == 0) {
        ev->kind = EV_DUMP;
        if (!*rest)
            return false;
        ev->path = SDL_strdup(rest);
        return true;
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

static void free_events(void)
{
    for (int i = 0; i < event_count; i++)
        SDL_free(events[i].path);
    event_count = 0;
    next_event = 0;
}

static bool add_event(const char *tok)
{
    if (event_count == event_cap) {
        int cap = event_cap ? event_cap * 2 : 256;
        Event *e = SDL_realloc(events, (size_t)cap * sizeof *e);
        if (!e)
            return false;
        events = e;
        event_cap = cap;
    }
    Event ev = {0};
    if (!parse_event(tok, &ev))
        return false;
    ev.order = event_count;
    events[event_count++] = ev;
    return true;
}

static int by_frame(const void *a, const void *b)
{
    const Event *x = a, *y = b;
    if (x->frame != y->frame)
        return x->frame < y->frame ? -1 : 1;
    return x->order - y->order;  // keep the script's order
}

void autopilot_set_script(const char *script)
{
    initialised = true;
    free_events();
    pressed = false;
    have_base = false;
    have_seed = false;
    limit = 0;
    limit_reached = false;
    rnd.on = false;
    active = script && *script;
    if (!active)
        return;
    char *copy = nullptr;
    if (script[0] == '@') {
        copy = SDL_LoadFile(script + 1, nullptr);
        if (!copy)
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Autopilot: %s: %s", script + 1, SDL_GetError());
    } else {
        copy = SDL_strdup(script);
    }
    if (!copy)
        return;
    char *save_line = nullptr;
    for (char *line = SDL_strtok_r(copy, "\n", &save_line); line;
         line = SDL_strtok_r(nullptr, "\n", &save_line)) {
        while (*line == ' ' || *line == '\r')
            line++;
        if (*line == '#')
            continue;  // the whole line, ';' and all
        char *save = nullptr;
        for (char *tok = SDL_strtok_r(line, ";", &save); tok; tok = SDL_strtok_r(nullptr, ";", &save)) {
            while (*tok == ' ' || *tok == '\r')
                tok++;
            if (!*tok || *tok == '#')
                continue;
            if (SDL_strncmp(tok, "random:", 7) == 0) {
                if (!parse_random(tok))
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Autopilot: bad event '%s'", tok);
                continue;
            }
            unsigned long s32;
            if (SDL_sscanf(tok, "seed:%lu", &s32) == 1) {
                seed = (uint32_t)s32;
                have_seed = true;
                continue;
            }
            if (!add_event(tok))
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Autopilot: bad event '%s'", tok);
        }
    }
    SDL_free(copy);
    SDL_qsort(events, (size_t)event_count, sizeof *events, by_frame);
}

void autopilot_set_controller(void (*fn)(uint64_t frame))
{
    controller = fn;
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

bool autopilot_random_seed(uint32_t *out)
{
    autopilot_init();
    if (active && rnd.on)
        *out = rnd.seed;
    return active && rnd.on;
}

bool autopilot_seed(uint32_t *out)
{
    autopilot_init();
    if (active && have_seed)
        *out = seed;
    else if (active && rnd.on)
        *out = rnd.seed;
    return active && (have_seed || rnd.on);
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
    while (next_event < event_count && events[next_event].frame < frame)
        next_event++;  // set before the script's base frame: never played
    for (; next_event < event_count && events[next_event].frame == frame; next_event++) {
        const Event *ev = &events[next_event];
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
        case EV_MOUSE:
            mouse_x = ev->x;
            mouse_y = ev->y;
            input_set_mouse(ev->x, ev->y, ev->buttons & 1, (ev->buttons >> 1) & 1);
            pressed = false;
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
    if (limit && frame >= limit && !limit_reached) {
        SDL_Log("Autopilot: frame limit %llu reached", (unsigned long long)limit);
        limit_reached = true;
        input_request_quit();
    }
    if (controller)
        controller(frame);
}

uint64_t autopilot_last_event(void)
{
    return event_count ? events[event_count - 1].frame : 0;
}

void autopilot_set_frame_limit(uint64_t frame)
{
    limit = frame;
}

bool autopilot_limit_reached(void)
{
    return limit_reached;
}

uint64_t autopilot_hash(const void *data, size_t size)
{
    const uint8_t *p = data;
    uint64_t h = 0x84222325CBF29CE4u;
    for (size_t i = 0; i + 8 <= size; i += 8) {
        uint64_t w;
        SDL_memcpy(&w, p + i, 8);
        h = (h ^ w) * 0x9E3779B97F4A7C15u;
        h ^= h >> 29;
    }
    for (size_t i = size & ~(size_t)7; i < size; i++)
        h = (h ^ p[i]) * 0x9E3779B97F4A7C15u;
    return h;
}
