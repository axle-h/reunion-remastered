// Playthrough fixture infrastructure (docs/playthrough.md):
// checkpoints and play_resume, the write barrier, and the recording replayed
// in-process and by `reunion --replay` (REUNION_BIN, set by CMake). Needs
// the game data.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/dseg.h"
#include "platform/fs.h"
#include "play/play.h"
#include "play/run.h"
#include "play/ui.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

enum : uint32_t { SEED = 4242 };

// New game, checkpoint "cc", hire, checkpoint "hired", a while longer, quit
// (by returning).
static void policy(void *unused)
{
    (void)unused;
    const char *at = play_resumed_at();
    if (!at) {
        ui_new_game();
        play_checkpoint("cc");
    }
    if (!at || SDL_strcmp(at, "cc") == 0) {
        ui_hire("BUILDERS", "William Forbs");
        play_checkpoint("hired");
    }
    play_wait(300);
}

static void writes_memory(void *unused)
{
    (void)unused;
    play_wait(10);
    ds.Game_Credits ^= 1;
    play_wait(10);
    play_quit();
}

static void show(const char *what, const PlayResult *r)
{
    printf("%s: exit %d after %llu frames, hash %016llx%s%s\n", what, r->exit_code,
           (unsigned long long)r->frames, (unsigned long long)r->state_hash,
           r->failed ? ", FAILED: " : "", r->failure);
}

static bool same(const PlayResult *a, const PlayResult *b)
{
    return a->exit_code == b->exit_code && a->frames == b->frames && a->state_hash == b->state_hash
           && !a->failed && !b->failed;
}

// The recording's events (lines that aren't comments).
static char *events(const char *path)
{
    char *text = SDL_LoadFile(path, nullptr);
    if (!text)
        return SDL_strdup("");
    char *out = SDL_malloc(SDL_strlen(text) + 1), *o = out;
    for (char *line = text; *line;) {
        char *eol = SDL_strchr(line, '\n');
        size_t n = eol ? (size_t)(eol - line + 1) : SDL_strlen(line);
        if (line[0] != '#') {
            SDL_memcpy(o, line, n);
            o += n;
        }
        line += n;
    }
    *o = 0;
    SDL_free(text);
    return out;
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_EVENTS)) {
        printf("SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    // In the working directory (the build directory under ctest).
    const char *a = "play_resume_a.txt", *b = "play_resume_b.txt", *bad = "play_resume_bad.txt";

    // The whole run, then the same from each checkpoint: same end, same
    // events.
    PlayResult full = play_run(SEED, policy, nullptr, a);
    show("run", &full);
    CHECK(!full.failed && full.exit_code == -1);

    PlayResult r = play_resume(a, "cc", policy, nullptr, b);
    show("resumed at cc", &r);
    CHECK(same(&r, &full));
    char *ea = events(a), *eb = events(b);
    CHECK(SDL_strcmp(ea, eb) == 0);
    SDL_free(eb);

    r = play_resume(b, "hired", policy, nullptr, b);  // into its own recording
    show("resumed at hired", &r);
    CHECK(same(&r, &full));
    eb = events(b);
    CHECK(SDL_strcmp(ea, eb) == 0);
    SDL_free(ea);
    SDL_free(eb);

    // The combined recording is one script from power-on.
    CHECK(play_verify(b, argv[1], SDL_getenv("REUNION_BIN"), &full));

    // Resuming where the game is not as recorded fails.
    r = play_resume(a, "nowhere", policy, nullptr, nullptr);
    show("unknown checkpoint", &r);
    CHECK(r.failed && SDL_strstr(r.failure, "no checkpoint"));
    char *text = SDL_LoadFile(a, nullptr);
    char *hash = text ? SDL_strstr(text, "# checkpoint hired at frame ") : nullptr;
    CHECK(hash != nullptr);
    if (hash) {
        hash = SDL_strstr(hash, "hash ") + 5;
        *hash = *hash == '0' ? '1' : '0';
        SDL_SaveFile(bad, text, SDL_strlen(text));
        r = play_resume(bad, "hired", policy, nullptr, nullptr);
        show("tampered checkpoint", &r);
        CHECK(r.failed && SDL_strstr(r.failure, "not as recorded"));
    }
    SDL_free(text);

    // The write barrier names the byte.
    r = play_run(SEED, writes_memory, nullptr, nullptr);
    show("policy writes memory", &r);
    CHECK(r.failed && SDL_strstr(r.failure, "write barrier") && SDL_strstr(r.failure, "DS Game_Credits"));

    SDL_Quit();
    return ok ? 0 : 1;
}
