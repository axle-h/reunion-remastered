#include "play/play.h"

#include <SDL3/SDL.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "game/dseg.h"
#include "game/fixes.h"
#include "game/gfx.h"
#include "game/main_program.h"
#include "game/options.h"
#include "game/state.h"
#include "platform/config.h"
#include "game/system.h"
#include "platform/audio.h"
#include "platform/autopilot.h"
#include "platform/input.h"
#include "platform/timing.h"
#include "platform/video.h"

// Game side: the longjmp out of Halt or a quit.
static jmp_buf game_out;
static int halt_code;
static uint32_t run_id;  // boots so far (play_run_id)

[[noreturn]] static void on_halt(uint8_t code)
{
    halt_code = code;
    longjmp(game_out, 1);
}

[[noreturn]] static void on_quit(void)
{
    halt_code = -1;
    longjmp(game_out, 1);
}

// The hand-off. The game thread owns everything while the policy waits on
// `to_policy`; the policy owns everything while the game waits on `to_game`.
static struct {
    SDL_Semaphore *to_policy, *to_game;
    SDL_Thread *thread;
    PlayPolicy policy;
    void *arg;
    jmp_buf top;        // policy thread: unwinds to policy_main
    bool done;          // don't wake the policy any more
    bool exited;        // the policy thread has left the policy
    bool aborting;      // the game ended first: unwind the policy
    int sleep;          // retraces to skip before waking the policy
    uint64_t frame;     // current retrace (script numbering)
    FILE *record;
    bool tainted;       // play_test_setup changed the game this slice or before
    bool rebaseline;    // ... this slice: skip the write barrier once
    bool failed;
    char failure[256];
    // play_resume: the policy first wakes at the checkpoint's frame, where
    // the game's state must hash as it did when the checkpoint was taken.
    bool check_resume;
    uint64_t resume_hash;
    char checkpoint[64];
} ctl;

// The write barrier: the game's state (game/state.h) as the policy got it
// and as it left it. Comparing the two streams finds the exact field.
static StateBuf shadow, after;

static void set_failure(const char *fmt, va_list ap)
{
    if (ctl.failed)
        return;
    ctl.failed = true;
    SDL_vsnprintf(ctl.failure, sizeof ctl.failure, fmt, ap);
}

[[gnu::format(printf, 1, 2)]] static void fail_from_game(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    set_failure(fmt, ap);
    va_end(ap);
    ctl.done = true;
    input_request_quit();
}

// A cheat of the port menu is on (scripted runs can't open the menu, and
// never load the config file, so this only guards against code that does).
static bool cheating(void)
{
    return config.freeze_money || config.freeze_ore;
}

// PLAY_TRACE=path: one line per retrace with the state hash (debugging
// replays). Files get a .0, .1, ... suffix per boot. PLAY_TRACE_DUMP=frame
// also saves the state stream at that frame (path.state0, ...; compare two
// with `test_golden compare`).
static FILE *trace;

static void trace_frame(uint64_t frame)
{
    if (!trace)
        return;
    InputMouse m = input_mouse();
    fprintf(trace, "%llu %016llx %d,%d,%d%d\n", (unsigned long long)frame,
            (unsigned long long)state_hash(), m.x, m.y, m.left, m.right);
    const char *at = SDL_getenv("PLAY_TRACE_DUMP");
    if (at && SDL_strtoull(at, nullptr, 10) == frame) {
        static int n;
        char path[1024];
        SDL_snprintf(path, sizeof path, "%s.state%d", SDL_getenv("PLAY_TRACE"), n++);
        state_serialize(&after);
        state_save(&after, path);
    }
}

static void check_write_barrier(uint64_t frame)
{
    state_serialize(&after);
    size_t at = state_first_difference(&shadow, &after);
    if (at == SIZE_MAX)
        return;
    char where[128];
    state_describe(&after, at, where, sizeof where);
    fail_from_game("write barrier: the policy changed the game at frame %llu (%s)",
                   (unsigned long long)frame, where);
}

// autopilot controller: runs on the game thread at every retrace.
static void on_retrace(uint64_t frame)
{
    trace_frame(frame);
    ctl.frame = frame;
    if (ctl.done)
        return;
    if (ctl.sleep > 0) {
        ctl.sleep--;
        return;
    }
    if (ctl.check_resume) {
        ctl.check_resume = false;
        if (state_hash() != ctl.resume_hash) {
            fail_from_game("resume: the game at checkpoint %s (frame %llu) is not as recorded",
                           ctl.checkpoint, (unsigned long long)frame);
            return;
        }
    }
    state_serialize(&shadow);
    SDL_SignalSemaphore(ctl.to_policy);
    SDL_WaitSemaphore(ctl.to_game);
    if (ctl.rebaseline)
        ctl.rebaseline = false;
    else
        check_write_barrier(frame);
    if (cheating())
        fail_from_game("a cheat was on at frame %llu", (unsigned long long)frame);
}

[[gnu::format(printf, 1, 2)]] static void record_line(const char *fmt, ...)
{
    if (!ctl.record)
        return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(ctl.record, fmt, ap);
    va_end(ap);
}

static int policy_main(void *unused)
{
    (void)unused;
    SDL_WaitSemaphore(ctl.to_policy);
    if (!ctl.aborting && setjmp(ctl.top) == 0) {
        ctl.policy(ctl.arg);
        // Returning ends the run as play_quit() does.
        input_request_quit();
        record_line("%llu:quit\n", (unsigned long long)ctl.frame);
    }
    ctl.done = true;
    ctl.exited = true;
    SDL_SignalSemaphore(ctl.to_game);
    return 0;
}

// Hands the game back and sleeps until the next slice.
static void yield(void)
{
    SDL_SignalSemaphore(ctl.to_game);
    SDL_WaitSemaphore(ctl.to_policy);
    if (ctl.aborting)
        longjmp(ctl.top, 1);
}

// Boots as src/main.c does for a scripted run (no intro, fixes on) and plays
// until the game halts or quits.
static PlayResult boot_and_run(const char *script)
{
    run_id++;
    audio_shutdown();
    input_reset();
    input_set_mouse(0, 0, 0, 0);  // as a fresh process starts
    timing_reset();
    Fixes_Init();
    autopilot_set_script(script);
    system_exit_handler = on_halt;
    timing_set_quit_handler(on_quit);
    uint64_t start = timing_frames();
    halt_code = -2;
    const char *tr = SDL_getenv("PLAY_TRACE");
    if (tr && *tr) {
        static int n;
        char path[1024];
        SDL_snprintf(path, sizeof path, "%s.%d", tr, n++);
        trace = fopen(path, "w");
    }
    if (setjmp(game_out) == 0) {
        vid_clear();
        dseg_init();
        system_init();
        Game_InitUnits();
        Options_Init();
        Game_Main();
    }
    const char *dump = SDL_getenv("PLAY_STATE_DUMP");
    if (dump && *dump) {
        static int n;
        char path[1024];
        SDL_snprintf(path, sizeof path, "%s.%d", dump, n++);
        state_serialize(&after);
        state_save(&after, path);
    }
    PlayResult r = {
        .exit_code = halt_code,
        .frames = timing_frames() - start,
        .state_hash = state_hash(),
    };
    if (trace)
        fclose(trace);
    trace = nullptr;
    autopilot_set_script(nullptr);
    system_exit_handler = nullptr;
    timing_set_quit_handler(nullptr);
    return r;
}

static PlayResult failed_result(const char *fmt, ...)
{
    PlayResult r = {.exit_code = -2, .failed = true};
    va_list ap;
    va_start(ap, fmt);
    SDL_vsnprintf(r.failure, sizeof r.failure, fmt, ap);
    va_end(ap);
    return r;
}

// A checkpoint line of a recording (play_checkpoint).
typedef struct {
    char name[64];
    uint64_t frame, hash;
} Checkpoint;

// Plays `policy` on the game booted with `script`. The recording starts with
// `prefix`. With `resume`, the policy first wakes at its frame.
static PlayResult run_policy(const char *script, const char *prefix, const Checkpoint *resume,
                             PlayPolicy policy, void *arg, const char *record_path)
{
    memset(&ctl, 0, sizeof ctl);
    if (resume) {
        SDL_strlcpy(ctl.checkpoint, resume->name, sizeof ctl.checkpoint);
        ctl.check_resume = true;
        ctl.resume_hash = resume->hash;
        ctl.sleep = (int)resume->frame;
    }
    ctl.policy = policy;
    ctl.arg = arg;
    if (record_path) {
        ctl.record = fopen(record_path, "w");
        if (!ctl.record)
            return failed_result("can't write the recording %s", record_path);
        fputs(prefix, ctl.record);
    }
    ctl.to_policy = SDL_CreateSemaphore(0);
    ctl.to_game = SDL_CreateSemaphore(0);
    ctl.thread = SDL_CreateThread(policy_main, "policy", nullptr);
    autopilot_set_controller(on_retrace);

    PlayResult r = boot_and_run(script);

    autopilot_set_controller(nullptr);
    if (!ctl.exited) {
        // The game ended under the policy: unwind it.
        ctl.aborting = true;
        SDL_SignalSemaphore(ctl.to_policy);
        SDL_WaitSemaphore(ctl.to_game);
    }
    SDL_WaitThread(ctl.thread, nullptr);
    SDL_DestroySemaphore(ctl.to_policy);
    SDL_DestroySemaphore(ctl.to_game);
    r.failed = ctl.failed;
    r.tainted = ctl.tainted;
    SDL_strlcpy(r.failure, ctl.failure, sizeof r.failure);
    if (ctl.record) {
        fprintf(ctl.record, "# end: exit %d after %llu frames, hash %016llx%s\n", r.exit_code,
                (unsigned long long)r.frames, (unsigned long long)r.state_hash,
                r.failed ? " (failed)" : r.tainted ? " (tainted)" : "");
        fclose(ctl.record);
    }
    return r;
}

PlayResult play_run(uint32_t seed, PlayPolicy policy, void *arg, const char *record_path)
{
    char script[64];
    SDL_snprintf(script, sizeof script, "seed:%u", (unsigned)seed);
    char prefix[128];
    SDL_snprintf(prefix, sizeof prefix, "# Recorded by the playthrough fixture (tests/play/).\n%s\n",
                 script);
    return run_policy(script, prefix, nullptr, policy, arg, record_path);
}

// "# checkpoint NAME at frame F, hash H".
static bool parse_checkpoint(const char *line, Checkpoint *c)
{
    unsigned long long f, h;
    if (SDL_sscanf(line, "# checkpoint %63s at frame %llu, hash %llx", c->name, &f, &h) != 3)
        return false;
    c->frame = f;
    c->hash = h;
    return true;
}

PlayResult play_resume(const char *from, const char *checkpoint, PlayPolicy policy, void *arg,
                       const char *record_path)
{
    char *text = SDL_LoadFile(from, nullptr);
    if (!text)
        return failed_result("resume: %s: %s", from, SDL_GetError());
    // The prefix: everything up to and including the checkpoint's line.
    char *end = nullptr;
    bool tainted = false;
    Checkpoint c;
    for (char *line = text; *line && !end;) {
        char *eol = SDL_strchr(line, '\n');
        char *next = eol ? eol + 1 : line + SDL_strlen(line);
        if (eol)
            *eol = 0;
        tainted |= line[0] == '#' && SDL_strstr(line, ": test setup") != nullptr;
        if (parse_checkpoint(line, &c) && SDL_strcmp(c.name, checkpoint) == 0)
            end = next;
        if (eol)
            *eol = '\n';
        line = next;
    }
    PlayResult r;
    if (!end) {
        r = failed_result("resume: no checkpoint %s in %s", checkpoint, from);
    } else if (tainted) {
        r = failed_result("resume: %s used play_test_setup before checkpoint %s", from, checkpoint);
    } else {
        *end = 0;
        size_t size = SDL_strlen(text) + 1024;
        char *prefix = SDL_malloc(size);
        SDL_snprintf(prefix, size, "%s# resumed at checkpoint %s of %s\n", text, checkpoint, from);
        r = run_policy(text, prefix, &c, policy, arg, record_path);
        SDL_free(prefix);
    }
    SDL_free(text);
    return r;
}

static void (*replay_watch)(uint64_t frame);

static void replay_frame(uint64_t frame)
{
    trace_frame(frame);
    if (replay_watch)
        replay_watch(frame);
}

PlayResult play_replay(const char *script)
{
    return play_replay_watch(script, nullptr);
}

PlayResult play_replay_watch(const char *script, void (*watch)(uint64_t frame))
{
    replay_watch = watch;
    autopilot_set_controller(replay_frame);
    PlayResult r = boot_and_run(script);
    autopilot_set_controller(nullptr);
    replay_watch = nullptr;
    return r;
}

// --- Policy side ---

uint64_t play_frame(void)
{
    return ctl.frame;
}

uint32_t play_run_id(void)
{
    return run_id;
}

const char *play_resumed_at(void)
{
    return ctl.checkpoint[0] ? ctl.checkpoint : nullptr;
}

void play_wait(int frames)
{
    ctl.sleep = frames > 1 ? frames - 1 : 0;
    yield();
}

bool play_wait_until(bool (*pred)(void *arg), void *arg, int timeout)
{
    for (int i = 0; i <= timeout; i++) {
        if (pred(arg))
            return true;
        if (i < timeout)
            yield();
    }
    return false;
}

void play_mouse(int x, int y, int buttons)
{
    input_set_mouse(x, y, buttons & 1, (buttons >> 1) & 1);
    record_line("%llu:mouse:%d,%d,%d\n", (unsigned long long)ctl.frame, x, y, buttons);
}

void play_click(int x, int y)
{
    play_mouse(x, y, 1);
    play_wait(4);
    play_mouse(x, y, 0);
    play_wait(2);
}

void play_key(int code)
{
    input_push_key(code);
    record_line("%llu:key:%d\n", (unsigned long long)ctl.frame, code);
}

void play_checkpoint(const char *name)
{
    bool ok = *name && SDL_strlen(name) < sizeof ctl.checkpoint;
    for (const char *p = name; *p; p++)
        ok &= SDL_isalnum(*p) || *p == '_' || *p == '-' || *p == '.';
    if (!ok)
        play_fail("play_checkpoint: bad name '%s'", name);
    record_line("# checkpoint %s at frame %llu, hash %016llx\n", name,
                (unsigned long long)ctl.frame, (unsigned long long)state_hash());
    if (ctl.record)
        fflush(ctl.record);
    play_log("checkpoint %s%s", name, ctl.tainted ? " (tainted: can't be resumed)" : "");
}

void play_test_setup(void (*fn)(void *arg), void *arg)
{
    fn(arg);
    ctl.tainted = true;
    ctl.rebaseline = true;
    play_log("test setup: this run is not a proof");
    record_line("# %llu: test setup (game state changed outside input, won't replay)\n",
                (unsigned long long)ctl.frame);
}

void play_quit(void)
{
    input_request_quit();
    record_line("%llu:quit\n", (unsigned long long)ctl.frame);
    longjmp(ctl.top, 1);
}

void play_fail(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    set_failure(fmt, ap);
    va_end(ap);
    play_log("FAILED: %s", ctl.failure);
    play_dump("failed");
    input_request_quit();
    record_line("%llu:quit\n", (unsigned long long)ctl.frame);
    longjmp(ctl.top, 1);
}

void play_dump(const char *fmt, ...)
{
    const char *dir = SDL_getenv("PLAY_DUMP_DIR");
    if (!dir || !*dir)
        return;
    char name[256], path[1024];
    va_list ap;
    va_start(ap, fmt);
    SDL_vsnprintf(name, sizeof name, fmt, ap);
    va_end(ap);
    SDL_snprintf(path, sizeof path, "%s/%08llu-%s.png", dir, (unsigned long long)ctl.frame, name);
    vid_save_image(path);
}

void play_log(const char *fmt, ...)
{
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    SDL_vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    printf("[%8llu %04d-%02d-%02d %02d:00] %s\n", (unsigned long long)ctl.frame, ds.Game_Year,
           ds.Game_Month, ds.Game_Day, ds.Game_Hour, msg);
    fflush(stdout);
}
