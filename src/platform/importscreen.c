#include "platform/importscreen.h"

#include <SDL3/SDL.h>

#include "platform/fs.h"
#include "platform/import.h"
#include "platform/input.h"
#include "platform/panel.h"
#include "platform/playerdir.h"
#include "platform/video.h"
#include "platform/web.h"

// Layout, in the panel's 320x240 space (platform/panel.h).
enum : int {
    TEXT_X = 16,
    COLS = 36,
    LINE = 10,
    BUTTON_X = 24,
    BUTTON_W = 272,
    BUTTON_H = 13,
    BUTTONS_Y = 112,
    STATUS_Y = 172,
    SHOWN_PROBLEMS = 3,
    COUNTDOWN = 5,  // seconds from an import at start-up to the game
};

typedef enum : int { B_FILE, B_FOLDER, B_QUIT, BUTTONS } Button;

// A file dialog's answer, which may come on another thread.
static SDL_Mutex *dialog_lock;
static bool dialog_open;
static char *picked;        // the path chosen
static char *dialog_error;  // or why no dialog could open

// The import, on its own thread.
static SDL_Thread *thread;
static SDL_AtomicInt finished;
static char *job_path;
static ImportReport report;
static ImportProgress progress;

static bool from_menu;
static bool running;
static bool imported;
static int focus;
static char *status;  // the last result, or nullptr: the intro
static uint64_t countdown_start;  // ms; 0 until an import at start-up succeeds

// What happened to the DOS game's saves, as sentences ("" if it had none).
static void saves_note(const ImportReport *r, char *out, size_t size)
{
    out[0] = '\0';
    if (r->saves_copied)
        SDL_snprintf(out, size, "Copied %d save%s of the DOS game.", r->saves_copied,
                     r->saves_copied == 1 ? "" : "s");
    char slots[64] = "";
    int skipped = 0;
    for (int n = 1; n <= IMPORT_SAVE_SLOTS; n++) {
        if (!r->save_skipped[n])
            continue;
        size_t len = SDL_strlen(slots);
        SDL_snprintf(slots + len, sizeof slots - len, "%s%d", skipped ? ", " : "", n);
        skipped++;
    }
    if (skipped) {
        size_t len = SDL_strlen(out);
        SDL_snprintf(out + len, size - len,
                     "%sSlot%s %s already held a save, so the DOS game's %s not copied.",
                     len ? " " : "", skipped == 1 ? "" : "s", slots, skipped == 1 ? "was" : "were");
    }
}

char *importscreen_describe(const ImportReport *r, int max_problems)
{
    char *out = nullptr;
    if (r->ok) {
        bool root = r->root && r->root[0];
        char saves[256];
        saves_note(r, saves, sizeof saves);
        // "in English, German, French and Italian"
        char langs[96] = "";
        int n = 0, k = 0;
        for (unsigned bits = r->languages; bits; bits &= bits - 1)
            n++;
        for (Language l = 0; l < LANGUAGE_COUNT && n > 1; l++) {
            if (!(r->languages & 1u << l))
                continue;
            size_t len = SDL_strlen(langs);
            SDL_snprintf(langs + len, sizeof langs - len, "%s%s",
                         k == 0 ? ", in " : k == n - 1 ? " and " : ", ", gamedata_language_label(l));
            k++;
        }
        SDL_asprintf(&out, "Imported the %s edition%s%s%s.%s%s", gamedata_edition_name(r->edition),
                     root ? " from " : "", root ? r->root : "", langs, saves[0] ? " " : "", saves);
        return out;
    }
    if (r->error) {
        SDL_asprintf(&out, "Not imported: %s", r->error);
        return out;
    }
    SDL_asprintf(&out, "Not imported: %d file%s of this copy %s missing or different.",
                 r->problem_count, r->problem_count == 1 ? "" : "s",
                 r->problem_count == 1 ? "is" : "are");
    int shown = max_problems > 0 ? SDL_min(max_problems, r->problem_count) : r->problem_count;
    for (int i = 0; i < shown && out; i++) {
        char *more = nullptr;
        SDL_asprintf(&more, "%s\n%s", out, r->problems[i]);
        SDL_free(out);
        out = more;
    }
    if (out && shown < r->problem_count) {
        char *more = nullptr;
        SDL_asprintf(&more, "%s\n... and %d more.", out, r->problem_count - shown);
        SDL_free(out);
        out = more;
    }
    return out;
}

// --- Choosing and importing -------------------------------------------------------

static void SDLCALL dialog_done(void *userdata, const char *const *files, int filter)
{
    (void)userdata;
    (void)filter;
    SDL_LockMutex(dialog_lock);
    dialog_open = false;
    if (!files)
        dialog_error = SDL_strdup(SDL_GetError());
    else if (files[0])
        picked = SDL_strdup(files[0]);
    SDL_UnlockMutex(dialog_lock);
}

static void set_status(char *text)
{
    SDL_free(status);
    status = text;
}

static int SDLCALL import_thread(void *userdata)
{
    const char *dir = userdata;
    import_game(job_path, dir, gamedata_files, gamedata_file_count, &report, &progress);
    SDL_SetAtomicInt(&finished, 1);
    return 0;
}

static void finish_import(void)
{
    SDL_WaitThread(thread, nullptr);
    thread = nullptr;
    char *text = importscreen_describe(&report, 0);
    if (text)
        SDL_Log("%s", text);
    SDL_free(text);
    text = importscreen_describe(&report, SHOWN_PROBLEMS);
    if (report.ok && !from_menu) {
        // The countdown to the game; the status keeps the saves' note.
        imported = true;
        countdown_start = SDL_GetTicks();
        focus = B_QUIT;
        char saves[256];
        saves_note(&report, saves, sizeof saves);
        SDL_free(text);
        text = SDL_strdup(saves);
    } else if (report.ok) {
        imported = true;
        // A running game keeps the data it was started with until a restart.
        char *data = playerdir_path("data");
        const char *root = fs_root();
        char *more = nullptr;
        if (from_menu && root && data && SDL_strcmp(root, data) != 0)
            SDL_asprintf(&more, "%s This run reads the game from %s instead.", text, root);
        else if (from_menu)
            SDL_asprintf(&more, "%s It is used from the next restart (System).", text);
        if (more) {
            SDL_free(text);
            text = more;
        }
        SDL_free(data);
        focus = B_QUIT;
    }
    set_status(text);
}

static void start_import(const char *path)
{
    if (thread || countdown_start)
        return;
    const char *dir = playerdir();
    if (!dir) {
        set_status(SDL_strdup("Not imported: there is no folder for the player's files."));
        return;
    }
    SDL_free(job_path);
    job_path = SDL_strdup(path);
    import_report_free(&report);
    SDL_SetAtomicInt(&finished, 0);
    SDL_SetAtomicInt(&progress.done, 0);
    SDL_SetAtomicInt(&progress.total, 0);
    set_status(nullptr);
    SDL_Log("Importing the game data from %s", path);
    thread = SDL_CreateThread(import_thread, "import", (void *)dir);
    if (!thread) {
        import_thread((void *)dir);
        finish_import();
    }
}

static void open_dialog(Button b)
{
#ifdef __EMSCRIPTEN__
    // The page's file picker, which copies the files chosen into its memory.
    char *path = web_choose_import(b == B_FOLDER);
    if (path)
        start_import(path);
    SDL_free(path);
    return;
#endif
    SDL_LockMutex(dialog_lock);
    bool busy = dialog_open;
    dialog_open = true;
    SDL_UnlockMutex(dialog_lock);
    if (busy)
        return;
    static const SDL_DialogFileFilter filters[] = {
        {"ZIP or ISO image", "zip;iso"},
        {"All files", "*"},
    };
    if (b == B_FILE)
        SDL_ShowOpenFileDialog(dialog_done, nullptr, vid_window(), filters,
                               (int)SDL_arraysize(filters), nullptr, false);
    else
        SDL_ShowOpenFolderDialog(dialog_done, nullptr, vid_window(), nullptr, false);
}

// The dialog's answer, on this thread.
static void take_dialog_result(void)
{
    SDL_LockMutex(dialog_lock);
    char *path = picked, *error = dialog_error;
    picked = dialog_error = nullptr;
    SDL_UnlockMutex(dialog_lock);
    if (path)
        start_import(path);
    if (error) {
        char *text = nullptr;
        SDL_asprintf(&text,
                     "No file dialog could open (%s). Drop the file or folder onto this window, "
                     "or start the game with --import=PATH.",
                     error);
        set_status(text);
    }
    SDL_free(path);
    SDL_free(error);
}

// --- Buttons ----------------------------------------------------------------------

// During an import and the countdown after it only Quit works.
static bool enabled(Button b)
{
    return !thread && (!countdown_start || b == B_QUIT);
}

static const char *label(Button b)
{
    switch (b) {
    case B_FILE: return "Choose ZIP or ISO...";
    case B_FOLDER: return "Choose folder...";
    default: return from_menu ? "Back" : "Quit";
    }
}

static void press(Button b)
{
    if (!enabled(b))
        return;
    switch (b) {
    case B_FILE:
    case B_FOLDER:
        open_dialog(b);
        break;
    default:
        if (!from_menu)
            input_request_quit();
        running = false;
        break;
    }
}

static void move_focus(int dir)
{
    if (!enabled(B_FILE))
        return;
    focus = (focus + dir + BUTTONS) % BUTTONS;
}

static int button_at(float x, float y)
{
    if (!panel_inside(x, y, BUTTON_X - 2, BUTTONS_Y - 3, BUTTON_W + 4, BUTTONS * BUTTON_H))
        return -1;
    int b = (int)((y - BUTTONS_Y + 3) / BUTTON_H);
    return enabled((Button)b) ? b : -1;
}

static void handle(const SDL_Event *e)
{
    float x, y;
    int b;
    switch (e->type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        input_request_quit();
        running = false;
        break;
    case SDL_EVENT_DROP_FILE:
        start_import(e->drop.data);
        break;
    case SDL_EVENT_KEY_DOWN:
        switch (e->key.key) {
        case SDLK_UP:
            move_focus(-1);
            break;
        case SDLK_DOWN:
        case SDLK_TAB:
            move_focus(1);
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE:
            if (e->key.key == SDLK_RETURN && (e->key.mod & SDL_KMOD_ALT))
                vid_toggle_fullscreen();
            else
                press((Button)focus);
            break;
        case SDLK_ESCAPE:
            press(B_QUIT);
            break;
        default:
            break;
        }
        break;
    case SDL_EVENT_MOUSE_MOTION:
        panel_mouse(e->motion.x, e->motion.y, &x, &y);
        b = button_at(x, y);
        if (b >= 0)
            focus = b;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        panel_mouse(e->button.x, e->button.y, &x, &y);
        b = button_at(x, y);
        if (e->button.button == SDL_BUTTON_LEFT && b >= 0)
            press((Button)b);
        break;
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
        vid_window_event(e);
        break;
    default:
        break;
    }
}

// --- Drawing ----------------------------------------------------------------------

static void draw(SDL_Renderer *r)
{
    panel_begin(r, "REUNION", "Game data");
    panel_color(r, 215, 215, 228, 255);
    panel_wrap(r, TEXT_X, PANEL_TOP, COLS, LINE, BUTTONS_Y - 4,
               from_menu ? "Replace the game data with another copy of the DOS game, one of:"
                         : "Reunion requires the DOS game's data. Please provide one of:");
    panel_color(r, 170, 190, 225, 255);
    panel_wrap(r, TEXT_X, PANEL_TOP + 3 * LINE, COLS, LINE, BUTTONS_Y - 4,
               "1. The CD release, as an ISO image\n"
               "\n"
               "2. The floppy release, as a ZIP or\n"
               "   as a folder");

    for (int b = 0; b < BUTTONS; b++) {
        float by = BUTTONS_Y + (float)b * BUTTON_H;
        bool focused = b == focus && enabled((Button)b);
        if (focused) {
            panel_color(r, 50, 70, 130, 255);
            panel_fill(r, BUTTON_X - 2, by - 3, BUTTON_W + 4, BUTTON_H);
        }
        if (!enabled((Button)b))
            panel_color(r, 110, 110, 125, 255);
        else
            panel_color(r, focused ? 255 : 215, focused ? 255 : 215, focused ? 255 : 228, 255);
        char text[64];
        SDL_snprintf(text, sizeof text, "> %s", label((Button)b));
        panel_text(r, BUTTON_X, by, text);
    }

    panel_color(r, 60, 72, 110, 255);
    panel_fill(r, PANEL_X + 4, STATUS_Y - 7, PANEL_W - 8, 1);
    char line[512];
    const char *msg = status;
    if (countdown_start) {
        // "Floppy version imported, starting in 5..4..3..2..1"
        static const char *const names[] = {
            [EDITION_FLOPPY] = "Floppy version",
            [EDITION_CD] = "CD version",
        };
        const char *name = report.edition ? names[report.edition] : "Game data";
        int len = SDL_snprintf(line, sizeof line, "%s imported, starting in ", name);
        int seconds = (int)((SDL_GetTicks() - countdown_start) / 1000);
        for (int n = COUNTDOWN; n >= SDL_max(COUNTDOWN - seconds, 1) && len < 200; n--)
            len += SDL_snprintf(line + len, sizeof line - (size_t)len, n < COUNTDOWN ? "..%d" : "%d", n);
        if (status && status[0])
            SDL_snprintf(line + len, sizeof line - (size_t)len, "\n\n%s", status);
        msg = line;
        panel_color(r, 150, 225, 150, 255);
    } else if (thread) {
        SDL_snprintf(line, sizeof line, "Checking and copying... %d of %d",
                     SDL_GetAtomicInt(&progress.done), SDL_GetAtomicInt(&progress.total));
        msg = line;
        panel_color(r, 255, 215, 120, 255);
    } else if (status && imported) {
        panel_color(r, 150, 225, 150, 255);
    } else if (status) {
        panel_color(r, 255, 160, 140, 255);
    } else {
#ifdef __EMSCRIPTEN__
        msg = "Or drop the ZIP or ISO onto this page.";
#else
        msg = "Or drop the folder or file onto this window.";
#endif
        panel_color(r, 140, 150, 180, 255);
    }
    panel_wrap(r, TEXT_X, STATUS_Y, COLS, LINE, PANEL_Y + PANEL_H - 1, msg);
    panel_end(r);
}

bool importscreen_run(bool menu)
{
    from_menu = menu;
    running = true;
    imported = false;
    countdown_start = 0;
    focus = B_FILE;
    set_status(nullptr);
    if (!dialog_lock)
        dialog_lock = SDL_CreateMutex();
    playerdir();  // made here, not on the import's thread
    SDL_ShowCursor();
    vid_set_overlay(draw);
    while (running || thread) {
        SDL_Event e;
        while (SDL_PollEvent(&e))
            handle(&e);
        take_dialog_result();
        if (thread && SDL_GetAtomicInt(&finished))
            finish_import();
        if (countdown_start && SDL_GetTicks() - countdown_start >= COUNTDOWN * 1000)
            running = false;
        vid_present();
        SDL_Delay(15);
    }
    vid_set_overlay(nullptr);
    if (!from_menu)
        SDL_HideCursor();
    input_release();
    import_report_free(&report);
    set_status(nullptr);
    countdown_start = 0;
    return imported;
}
