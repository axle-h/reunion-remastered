#include "platform/importscreen.h"

#include <SDL3/SDL.h>

#include "platform/fs.h"
#include "platform/import.h"
#include "platform/input.h"
#include "platform/panel.h"
#include "platform/playerdir.h"
#include "platform/video.h"

// Layout, in the panel's 320x240 space (platform/panel.h).
enum : int {
    TEXT_X = 16,
    COLS = 36,
    LINE = 10,
    BUTTON_X = 24,
    BUTTON_W = 272,
    BUTTON_H = 13,
    BUTTONS_Y = 98,
    STATUS_Y = 160,
    SHOWN_PROBLEMS = 4,
};

typedef enum : int { B_FILE, B_FOLDER, B_PLAY, B_QUIT, BUTTONS } Button;

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

char *importscreen_describe(const ImportReport *r, int max_problems)
{
    char *out = nullptr;
    if (r->ok) {
        bool root = r->root && r->root[0];
        SDL_asprintf(&out, "Imported the %s edition%s%s.", gamedata_edition_name(r->edition),
                     root ? " from " : "", root ? r->root : "");
        if (r->saves_copied) {
            char *more = nullptr;
            SDL_asprintf(&more, "%s Copied %d save%s of the DOS game.", out, r->saves_copied,
                         r->saves_copied == 1 ? "" : "s");
            SDL_free(out);
            out = more;
        }
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
            char *more = nullptr;
            SDL_asprintf(&more, "%s Slot%s %s already held a save, so the DOS game's %s not copied.",
                         out, skipped == 1 ? "" : "s", slots, skipped == 1 ? "was" : "were");
            SDL_free(out);
            out = more;
        }
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
    if (report.ok) {
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
        focus = from_menu ? B_QUIT : B_PLAY;
    }
    set_status(text);
}

static void start_import(const char *path)
{
    if (thread)
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

static bool shown(Button b)
{
    return b != B_PLAY || (imported && !from_menu);
}

static const char *label(Button b)
{
    switch (b) {
    case B_FILE: return "Choose ZIP or ISO...";
    case B_FOLDER: return "Choose folder...";
    case B_PLAY: return "Play";
    default: return from_menu ? "Back" : "Quit";
    }
}

static void press(Button b)
{
    if (thread || !shown(b))
        return;
    switch (b) {
    case B_FILE:
    case B_FOLDER:
        open_dialog(b);
        break;
    case B_PLAY:
        running = false;
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
    do
        focus = (focus + dir + BUTTONS) % BUTTONS;
    while (!shown((Button)focus));
}

// The shown buttons are drawn one below the other.
static float button_y(Button b)
{
    int row = 0;
    for (int i = 0; i < b; i++)
        row += shown((Button)i);
    return BUTTONS_Y + (float)row * BUTTON_H;
}

static int button_at(float x, float y)
{
    for (int b = 0; b < BUTTONS; b++)
        if (shown((Button)b) && panel_inside(x, y, BUTTON_X - 2, button_y((Button)b) - 3,
                                             BUTTON_W + 4, BUTTON_H))
            return b;
    return -1;
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
    float y = panel_wrap(r, TEXT_X, PANEL_TOP, COLS, LINE, BUTTONS_Y - 4,
                         from_menu ? "Import another copy of the DOS game: its folder, a ZIP of "
                                     "it, or the CD's ISO image. It is checked and copied to:"
                                   : "Reunion needs the files of the DOS game. Choose your copy: "
                                     "its folder, a ZIP of it, or the CD's ISO image. It is "
                                     "checked and copied to:");
    char *data = playerdir_path("data");
    panel_color(r, 170, 190, 225, 255);
    panel_wrap(r, TEXT_X, y, COLS, LINE, BUTTONS_Y - 4, data ? data : "");
    SDL_free(data);

    for (int b = 0; b < BUTTONS; b++) {
        if (!shown((Button)b))
            continue;
        float by = button_y((Button)b);
        bool focused = b == focus && !thread;
        if (focused) {
            panel_color(r, 50, 70, 130, 255);
            panel_fill(r, BUTTON_X - 2, by - 3, BUTTON_W + 4, BUTTON_H);
        }
        if (thread)
            panel_color(r, 110, 110, 125, 255);
        else
            panel_color(r, focused ? 255 : 215, focused ? 255 : 215, focused ? 255 : 228, 255);
        char text[64];
        SDL_snprintf(text, sizeof text, "> %s", label((Button)b));
        panel_text(r, BUTTON_X, by, text);
    }

    panel_color(r, 60, 72, 110, 255);
    panel_fill(r, PANEL_X + 4, STATUS_Y - 7, PANEL_W - 8, 1);
    char line[64];
    const char *msg = status;
    if (thread) {
        SDL_snprintf(line, sizeof line, "Checking and copying... %d of %d",
                     SDL_GetAtomicInt(&progress.done), SDL_GetAtomicInt(&progress.total));
        msg = line;
        panel_color(r, 255, 215, 120, 255);
    } else if (status && imported) {
        panel_color(r, 150, 225, 150, 255);
    } else if (status) {
        panel_color(r, 255, 160, 140, 255);
    } else {
        msg = "Or drop the folder or file onto this window.";
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
        vid_present();
        SDL_Delay(15);
    }
    vid_set_overlay(nullptr);
    if (!from_menu)
        SDL_HideCursor();
    input_release();
    import_report_free(&report);
    set_status(nullptr);
    return imported;
}
