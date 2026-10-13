#include "play/ui_message.h"

#include <SDL3/SDL.h>

#include "game/dseg.h"
#include "game/gfx.h"
#include "game/main_program.h"
#include "game/mouse.h"
#include "platform/video.h"
#include "play/play.h"
#include "play/ui.h"

enum : int {
    SCREEN_W = 320,
    SCREEN_H = 200,
    GLYPH_W = 6,
    GLYPH_H = 8,
    GLYPHS = 78,              // Gfx_DrawText6x8 looks characters up among 78 glyphs
    BOX_FILL = 2,             // message.c: Blit_FillRect(..., 2)
    BOX_BORDER = 4,           // Gfx_DrawRectOutline(..., 4)
    BOX_CX = 0xA0,            // centre of the box
    BOX_CY = 0x7C,
    STATUS_X = 16,            // Gfx_DrawStatusText
    STATUS_Y = 37,
    STATUS_W = 18,
    STATUS_COLOUR = 0x34,
    CREDITS_X = 144,          // Gfx_DrawCredits, Gfx_DrawGameDate
    DATE_X = 224,
    CREDITS_W = 10,
    DATE_W = 13,
    CURSOR_SIZE = 11,         // mouse.c clip_box
    ENTER = 13,
    CLOSE_TIMEOUT = 200,
    HOLOGRAM_BUSY = 200,      // retraces the command centre may hold before we press
    SCENE = 33,
    SCENE_HOLD = 12,          // retraces a scene press and its release are held
    SCENE_GAP = 20,           // retraces between scene clicks
    SCENE_TIMEOUT = 12000,
    SCENE_WAIT = 3000,
    HISTORY = 32,
};

// --- Screen text ---

static const uint8_t *screen(void)
{
    return &vid_screen[0][0];
}

// Glyph g's pixel (r, c) in grafika\charset1, in either edition's layout.
static uint8_t glyph_pixel(const uint8_t *cs, int g, int r, int c)
{
    return cs[(uint16_t)(gfx_glyph_offset((uint8_t)g) + r * SCREEN_W + c)];
}

// The mouse cursor is drawn into video memory (mouse.c); its pixels hide
// the text below.
static bool under_cursor(int x, int y)
{
    if (ds.Mouse_CursorState != 0 || ds.Mouse_CursorFrozen)
        return false;
    int cx = ds.Mouse_CursorSaveX, cy = ds.Mouse_CursorSaveY;
    return x >= cx && x < cx + CURSOR_SIZE && y >= cy && y < cy + CURSOR_SIZE;
}

// Matches the cell at (x, y) against glyph g: the screen is glyph + colour
// for every visible pixel. Returns the colour, or -1 (also when the cursor
// hides the whole cell); *seen counts the pixels compared.
static int match_glyph(int x, int y, int g, int *seen)
{
    const uint8_t *s = screen(), *cs = ds.Gfx_Charset;
    int colour = -1;
    *seen = 0;
    for (int r = 0; r < GLYPH_H; r++) {
        for (int c = 0; c < GLYPH_W; c++) {
            if (under_cursor(x + c, y + r))
                continue;
            int d = (uint8_t)(s[(y + r) * SCREEN_W + x + c] - glyph_pixel(cs, g, r, c));
            if (colour < 0)
                colour = d;
            else if (d != colour)
                return -1;
            (*seen)++;
        }
    }
    return colour;
}

static bool in_screen(int x, int y)
{
    return x >= 0 && y >= 0 && x + GLYPH_W <= SCREEN_W && y + GLYPH_H <= SCREEN_H;
}

// The glyph Gfx_DrawText6x8 draws for ch (GLYPHS if it has none).
static int glyph_of(char ch)
{
    const PString *chars = &ds.Gfx_CharsetChars;
    for (int g = 0; g < GLYPHS && g < chars->len; g++)
        if (chars->s[g] == ch)
            return g;
    return GLYPHS;
}

// Reads one cell drawn in `want` (UI_ANY_COLOUR: any): its character ('?'
// if none matches) and colour.
static char read_cell(int x, int y, int want, int *colour)
{
    *colour = -1;
    if (!in_screen(x, y))
        return '?';
    const PString *chars = &ds.Gfx_CharsetChars;
    int blank = glyph_of(' '), seen;
    if ((*colour = match_glyph(x, y, blank, &seen)) >= 0 && (want < 0 || *colour == want))
        return ' ';
    for (int g = 0; g < GLYPHS && g < chars->len; g++) {
        if (g != blank && (*colour = match_glyph(x, y, g, &seen)) >= 0 && (want < 0 || *colour == want))
            return chars->s[g];
    }
    *colour = -1;
    return '?';
}

bool ui_read_text_in(int x, int y, int width, int colour, char *out, size_t size)
{
    bool ok = true;
    size_t n = 0;
    for (int i = 0; i < width && n + 1 < size; i++) {
        int c;
        char ch = read_cell(x + i * GLYPH_W, y, colour, &c);
        ok = ok && ch != '?';
        out[n++] = ch;
    }
    while (n > 0 && out[n - 1] == ' ')
        n--;
    out[n] = 0;
    // 'O' and '0' share a glyph (the first match reads 'O'): a run of them
    // next to a digit or '%' is a number.
    for (size_t i = 0; i < n;) {
        size_t end = i;
        while (end < n && (out[end] == 'O' || out[end] == '0'))
            end++;
        if (end == i) {
            i++;
            continue;
        }
        bool number = (i > 0 && SDL_isdigit((unsigned char)out[i - 1]))
                      || (end < n && (SDL_isdigit((unsigned char)out[end]) || out[end] == '%'));
        for (; i < end; i++)
            if (number)
                out[i] = '0';
    }
    return ok;
}

bool ui_read_text(int x, int y, int width, char *out, size_t size)
{
    return ui_read_text_in(x, y, width, UI_ANY_COLOUR, out, size);
}

int ui_text_colour(int x, int y)
{
    int colour;
    read_cell(x, y, UI_ANY_COLOUR, &colour);
    return colour;
}

bool ui_text_at(int x, int y, const char *s)
{
    int seen_all = 0;
    for (int i = 0; s[i]; i++) {
        int cx = x + i * GLYPH_W, seen;
        if (!in_screen(cx, y))
            return false;
        if (match_glyph(cx, y, glyph_of(s[i]), &seen) < 0 && seen > 0)
            return false;
        seen_all += seen;
    }
    return seen_all > 0;
}

bool ui_read_number(int x, int y, int width, int32_t *value)
{
    char t[64];
    ui_read_text(x, y, width, t, sizeof t);
    for (char *c = t; *c; c++)
        if (*c == 'O')
            *c = '0';
    const char *p = t;
    while (*p == ' ' || *p == ':')
        p++;
    if (!(*p >= '0' && *p <= '9') && !(*p == '-' && p[1] >= '0' && p[1] <= '9'))
        return false;
    *value = (int32_t)SDL_strtol(p, nullptr, 10);
    return true;
}

// --- Status bar ---

void ui_status_text(char *out, size_t size)
{
    ui_read_text_in(STATUS_X, STATUS_Y, STATUS_W, STATUS_COLOUR, out, size);
}

bool ui_credits(int32_t *credits)
{
    return ui_read_number(CREDITS_X, STATUS_Y, CREDITS_W, credits);
}

bool ui_date(UiDate *date)
{
    char t[32];
    if (!ui_read_text(DATE_X, STATUS_Y, DATE_W, t, sizeof t))
        return false;
    for (char *c = t; *c; c++)
        if (*c == 'O')
            *c = '0';
    UiDate d;
    if (SDL_sscanf(t, "%d-%d-%d-%d", &d.year, &d.month, &d.day, &d.hour) != 4)
        return false;
    *date = d;
    return true;
}

int64_t ui_date_hours(const UiDate *d)
{
    return (((int64_t)(d->year - 2900) * 12 + d->month - 1) * 30 + d->day - 1) * 24 + d->hour;
}

int64_t ui_clock_hours(void)
{
    // The date Gfx_DrawGameDate draws on the status bar.
    UiDate d = {ds.Game_Year, ds.Game_Month, ds.Game_Day, ds.Game_Hour};
    return ui_date_hours(&d);
}

bool ui_pixel_is(int x, int y, int colour)
{
    return under_cursor(x, y) || screen()[y * SCREEN_W + x] == colour;
}

// --- Message boxes ---

// The box of `lines` x `cols` cells as message.c places it: border colour 4
// around a fill of colour 2, centred on (160, 124).
static bool box_frame(int lines, int cols, int *left, int *top)
{
    int x0 = BOX_CX - cols * GLYPH_W / 2, y0 = BOX_CY - lines * 9 / 2;
    int l = x0 - 3, t = y0 - 3, r = x0 + cols * GLYPH_W + 2, b = y0 + lines * 9 + 2;
    *left = l;
    *top = t;
    if (l < 0 || r >= SCREEN_W)
        return false;
    if (!ui_pixel_is(l, t, BOX_BORDER) || !ui_pixel_is(r, b, BOX_BORDER)
        || !ui_pixel_is(l + 1, t + 1, BOX_FILL) || !ui_pixel_is(r - 1, b - 1, BOX_FILL))
        return false;
    for (int x = l; x <= r; x++)
        if (!ui_pixel_is(x, t, BOX_BORDER) || !ui_pixel_is(x, b, BOX_BORDER))
            return false;
    for (int y = t; y <= b; y++)
        if (!ui_pixel_is(l, y, BOX_BORDER) || !ui_pixel_is(r, y, BOX_BORDER))
            return false;
    return true;
}

static bool status_says_press(void)
{
    return ui_text_at(STATUS_X, STATUS_Y, "Press mouse button");
}

// Retraces since the main loop last waited for input (as seen by the
// policy): a box, scene or hologram holds the game while this grows.
static struct { uint64_t frame, ewf; int busy; } loop;

// Boxes closed, latest last.
static struct {
    struct { uint64_t frame; char text[UI_BOX_TEXT]; } seen[HISTORY];
    int count;
    void (*hook)(const char *text, void *arg);
    void *arg;
} boxes;

static struct { int count, last; } scenes;
static int handled;  // UI_INT_* bits since ui_interrupts_handled()

// The state above belongs to one run; a new run (play_run, play_resume) in
// this process starts it afresh.
static void check_run(void)
{
    static uint32_t last_run;
    if (play_run_id() != last_run) {
        loop = (typeof(loop)){};
        boxes.count = 0;
        scenes = (typeof(scenes)){};
        handled = 0;
    }
    last_run = play_run_id();
}

static void track_loop(void)
{
    uint64_t f = play_frame();
    if (f == loop.frame)
        return;
    if (Game_EventWaitFrames == loop.ewf)
        loop.busy += (int)(f - loop.frame);
    else
        loop.busy = 0;
    loop.frame = f;
    loop.ewf = Game_EventWaitFrames;
}

// After acting on what held the game: whatever holds it next is measured
// afresh.
static void reset_busy(void)
{
    loop.frame = play_frame();
    loop.ewf = Game_EventWaitFrames;
    loop.busy = 0;
}

static bool loop_busy(void)
{
    track_loop();
    if (loop.busy == 0) {
        play_wait(1);
        track_loop();
    }
    return loop.busy > 0;
}

static bool find_box(UiBox *box)
{
    for (int lines = 1; lines <= UI_BOX_LINES; lines++) {
        for (int cols = 1; cols <= UI_BOX_COLS; cols++) {
            int l, t;
            if (!box_frame(lines, cols, &l, &t))
                continue;
            *box = (UiBox){.lines = lines, .cols = cols, .x = l + 3, .y = t + 4};
            size_t n = 0;
            for (int i = 0; i < lines; i++) {
                ui_read_text(box->x, box->y + i * 9, cols, box->line[i], sizeof box->line[i]);
                const char *s = box->line[i];
                while (*s == ' ')
                    s++;
                if (!*s)
                    continue;
                n += (size_t)SDL_snprintf(box->text + n, sizeof box->text - n, "%s%s",
                                          n ? "|" : "", s);
                if (n >= sizeof box->text)
                    n = sizeof box->text - 1;
            }
            box->rename = SDL_strstr(box->text, "new planet's name") != nullptr;
            return true;
        }
    }
    return false;
}

bool ui_box(UiBox *box)
{
    UiBox b;
    if (find_box(&b)) {
        if (!(status_says_press() || b.rename || loop_busy()))
            return false;
    } else {
        // Without the memory to save the background the box is not drawn
        // (message.c), and only the status line asks for the click.
        if (!status_says_press() || !loop_busy())
            return false;
        b = (UiBox){};
    }
    if (box)
        *box = b;
    return true;
}

static void note_box(const UiBox *b)
{
    play_log("box: %s", b->text);
    play_dump("box");
    if (boxes.hook)
        boxes.hook(b->text, boxes.arg);
    int i = boxes.count++ % HISTORY;
    boxes.seen[i].frame = play_frame();
    SDL_strlcpy(boxes.seen[i].text, b->text, sizeof boxes.seen[i].text);
}

const char *ui_last_box(void)
{
    check_run();
    return boxes.count ? boxes.seen[(boxes.count - 1) % HISTORY].text : "";
}

const char *ui_box_seen(const char *text, uint64_t since)
{
    check_run();
    int first = boxes.count > HISTORY ? boxes.count - HISTORY : 0;
    for (int i = boxes.count - 1; i >= first; i--) {
        const char *t = boxes.seen[i % HISTORY].text;
        if (boxes.seen[i % HISTORY].frame >= since && (!*text || SDL_strcasestr(t, text)))
            return t;
    }
    return nullptr;
}

void ui_on_box(void (*fn)(const char *text, void *arg), void *arg)
{
    boxes.hook = fn;
    boxes.arg = arg;
}

static void (*screen_hook)(void);

void ui_on_interrupt_screen(void (*fn)(void))
{
    screen_hook = fn;
}

void ui_screen_lost(const char *what)
{
    play_log("%s: screen %d shown instead", what, ui_screen());
    if (screen_hook)
        screen_hook();
    play_fail("%s: the screen was replaced", what);
}

// Toolbar strip and its toggle, the clock button (mouse.c Mouse_HitTest).
static bool on_fixed_button(int x, int y)
{
    return y <= 0x20 || (x >= 0xDA && y >= 0x22 && y <= 0x2D);
}

static bool on_hotspot(int x, int y)
{
    for (int j = 1; j <= ds.Mouse_HotspotCount; j++) {
        const THotspot *h = Mouse_Hotspot((int16_t)j);
        if (h->X <= x && x <= h->X + h->W - 1 && h->Y <= y && y <= h->Y + h->H - 1)
            return true;
    }
    return false;
}

void ui_inert_point(int after_screen, int *x, int *y)
{
    // Between the status line (y 37..44) and the screen body: on the
    // command centre its hotspots start at y 49; the cursor drawn there
    // covers no status text.
    if (after_screen == 1 && ui_screen() != 1) {
        *x = 130;
        *y = 46;
        return;
    }
    static const int candidates[][2] = {
        {130, 46}, {300, 195}, {160, 197}, {2, 197}, {316, 120},
    };
    for (size_t i = 0; i < SDL_arraysize(candidates); i++) {
        int cx = candidates[i][0], cy = candidates[i][1];
        if (!on_fixed_button(cx, cy) && !on_hotspot(cx, cy)) {
            *x = cx;
            *y = cy;
            return;
        }
    }
    play_fail("no inert point on screen %d", ui_screen());
}

typedef struct { const UiBox *box; uint64_t ewf; } Closing;

// The box closed: another one (or none) is drawn, or the main loop waited
// for input since.
static bool box_gone(void *arg)
{
    const Closing *c = arg;
    UiBox b;
    return !find_box(&b) || SDL_strcmp(b.text, c->box->text) != 0 || Game_EventWaitFrames != c->ewf;
}

static void close_box(UiBox *b)
{
    int x, y;
    ui_inert_point(0, &x, &y);
    // message.c: waits for the button to be up, down, then up again. The
    // text is read once the cursor is off the box: a cell it mostly hides
    // can match another character ("scient? t" for "scientist").
    play_mouse(x, y, 0);
    play_wait(2);
    UiBox now;
    if (find_box(&now))
        *b = now;
    note_box(b);
    play_mouse(x, y, 1);
    play_wait(4);
    play_mouse(x, y, 0);
    play_wait(2);
    reset_busy();
}

void ui_close_box(void)
{
    UiBox b;
    if (!ui_box(&b) || b.rename)
        play_fail("ui_close_box: no message box shown");
    Closing c = {&b, Game_EventWaitFrames};
    close_box(&b);
    // The same text still drawn after the timeout may be the same box
    // again; ui_handle_interrupts gives up on one that keeps coming back.
    play_wait_until(box_gone, &c, CLOSE_TIMEOUT);
}

void ui_accept_name(void)
{
    UiBox b;
    if (!ui_box(&b) || !b.rename)
        play_fail("ui_accept_name: no rename prompt shown");
    note_box(&b);
    play_key(ENTER);
    play_wait(2);
    UiBox now;
    for (int i = 0; i < CLOSE_TIMEOUT; i++) {
        if (!find_box(&now) || SDL_strcmp(now.text, b.text) != 0) {
            reset_busy();
            return;
        }
        play_wait(1);
    }
    play_fail("rename prompt did not take Enter: %s", b.text);
}

// --- Story scenes ---

int ui_story_scene_shown(void)
{
    // AlienTalk_PlayAlienScene shows pics\pic<AlienTalk_Scene>.
    return ds.Game_CurrentScreen == SCENE ? ds.AlienTalk_Scene : 0;
}

// Plays out the story scene shown (alientalk.c AlienTalk_PlayAlienScene).
// Its waits end on a press: scenes 3-8 one; scene 2 one after its
// animation, which ignores presses; scene 1 a press, a release and a press,
// each polled only every 10 retraces. Scenes 9 and 10 wait for none, they
// end by themselves. Scenes 4 and 9 open with a message box. So: boxes are
// closed, and press and release, each held SCENE_HOLD retraces, repeat
// until the scene is gone, at a point inert on the command centre (the
// scene fades out in 6 retraces, and a press still down lands there; talk
// 6 follows its scene the same way). Returns the UI_INT_* bits.
static int play_scene(void)
{
    int done = UI_INT_SCENE, scene = ui_story_scene_shown(), clicks = 0;
    bool waits = scene != 9 && scene != 10;
    int x, y;
    ui_inert_point(1, &x, &y);
    play_log("story scene %d: clicking through", scene);
    play_dump("scene%d", scene);
    uint64_t end = play_frame() + SCENE_TIMEOUT;
    while (ui_screen() == SCENE) {
        if (play_frame() > end)
            play_fail("story scene %d did not end", scene);
        UiBox b;
        if (ui_box(&b)) {
            close_box(&b);
            done |= UI_INT_BOX;
            continue;
        }
        if (waits) {
            play_mouse(x, y, 1);
            play_wait(SCENE_HOLD);
            play_mouse(x, y, 0);
            play_wait(SCENE_HOLD);
            clicks++;
        }
        for (int t = 0; t < SCENE_GAP && ui_screen() == SCENE && !find_box(&b); t++)
            play_wait(1);
    }
    reset_busy();
    scenes.count++;
    scenes.last = scene;
    play_log("story scene %d over after %d clicks", scene, clicks);
    return done;
}

static bool scene_played(void *arg)
{
    return scenes.count != *(int *)arg;
}

int ui_scenes_played(void)
{
    check_run();
    return scenes.count;
}

int ui_story_scene(void)
{
    check_run();
    int before = scenes.count;
    if (!ui_wait_until(scene_played, &before, SCENE_WAIT))
        play_fail("no story scene");
    if (!ui_wait_screen(1, SCENE_WAIT))
        play_fail("story scene %d: no command centre after it", scenes.last);
    return scenes.last;
}

// --- Interrupts ---

static bool unhandled_screen(int s)
{
    return s == 19 || s == 29 || s == 32 || s == 34 || s == 35 || s == 36;
}

bool ui_interrupt_screen(void)
{
    return unhandled_screen(ui_screen());
}

int ui_interrupts_handled(void)
{
    check_run();
    int bits = handled;
    handled = 0;
    return bits;
}

int ui_handle_interrupts(void)
{
    check_run();
    int done = 0;
    for (int guard = 0; guard < 100; guard++) {
        track_loop();
        UiBox b;
        if (ui_box(&b)) {
            if (b.rename) {
                ui_accept_name();
                done |= UI_INT_RENAME;
            } else {
                ui_close_box();
                done |= UI_INT_BOX;
            }
            continue;
        }
        int s = ui_screen();
        if (s == SCENE) {
            done |= play_scene();
            continue;
        }
        if (unhandled_screen(s)) {
            done |= UI_INT_SCREEN;
            handled |= done;
            if (screen_hook)
                screen_hook();
        } else if (s == 1 && !ds.Game_ScreenSwitch && loop.busy >= HOLOGRAM_BUSY) {
            // Research finished on the command centre: the hologram waits
            // for a press (command.c:291-299), which the room then sees.
            int x, y;
            ui_inert_point(1, &x, &y);
            play_log("command centre held (hologram): pressing");
            play_dump("hologram");
            play_mouse(x, y, 1);
            play_wait(4);
            play_mouse(x, y, 0);
            play_wait(2);
            reset_busy();
            done |= UI_INT_HOLOGRAM;
            continue;
        }
        handled |= done;
        // A story scene ends on the command centre: the driver sequence it
        // came into is left as for an interrupt screen (the supernova's
        // comes at any hour, in the middle of a shop or surface sequence).
        if ((done & UI_INT_SCENE) && screen_hook)
            screen_hook();
        return done;
    }
    play_fail("interrupts kept coming (last box: %s)", ui_last_box());
}

// --- Game data block ---

const TGroup *ui_group_record(int list, int k)
{
    return &ds.Game_Groups[list][k - 1];
}

const TBuilding *ui_building_record(int i)
{
    return &ds.Game_Buildings[i - 1];
}
