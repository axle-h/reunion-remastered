#include "play/play.h"

#include <SDL3/SDL.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "game/dseg.h"
#include "game/gfx.h"
#include "game/main_program.h"
#include "game/mem.h"
#include "game/options.h"
#include "game/system.h"
#include "platform/audio.h"
#include "platform/autopilot.h"
#include "platform/input.h"
#include "platform/timing.h"
#include "platform/video.h"

// Game side: the longjmp out of Halt or a quit.
static jmp_buf game_out;
static int halt_code;

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
    bool failed;
    char failure[256];
} ctl;

static uint64_t hash_memory(void)
{
    uint64_t h = 0x84222325CBF29CE4u;
    for (size_t i = 0; i < MEM_SIZE; i += 8) {
        uint64_t w;
        memcpy(&w, mem + i, 8);
        h = (h ^ w) * 0x9E3779B97F4A7C15u;
        h ^= h >> 29;
    }
    return h;
}

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

// The Options panel (cheats) shows BACK TO DISK OPS in the toolbar.
static bool options_panel_open(void)
{
    for (int slot = 1; slot <= 12; slot++)
        if (*(int16_t *)ds_at((uint16_t)(0x7C64 + slot * 21)) == OPTIONS_BACK)
            return true;
    return options.freeze_money || options.freeze_ore;
}

// PLAY_TRACE=path: one line per retrace with the memory hash (debugging
// replays). Files get a .0, .1, ... suffix per boot.
static FILE *trace;

static void trace_frame(uint64_t frame)
{
    if (!trace)
        return;
    InputMouse m = input_mouse();
    fprintf(trace, "%llu %016llx %d,%d,%d%d\n", (unsigned long long)frame,
            (unsigned long long)hash_memory(), m.x, m.y, m.left, m.right);
    const char *at = SDL_getenv("PLAY_TRACE_DUMP");  // frame to save memory at
    if (at && SDL_strtoull(at, nullptr, 10) == frame) {
        static int n;
        char path[1024];
        SDL_snprintf(path, sizeof path, "%s.mem%d", SDL_getenv("PLAY_TRACE"), n++);
        SDL_SaveFile(path, mem, MEM_SIZE);
    }
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
    uint64_t before = hash_memory();
    SDL_SignalSemaphore(ctl.to_policy);
    SDL_WaitSemaphore(ctl.to_game);
    if (hash_memory() != before)
        fail_from_game("write barrier: the policy changed game memory at frame %llu",
                       (unsigned long long)frame);
    if (options_panel_open())
        fail_from_game("the Options panel (cheats) was opened at frame %llu",
                       (unsigned long long)frame);
}

static int policy_main(void *unused)
{
    (void)unused;
    SDL_WaitSemaphore(ctl.to_policy);
    if (!ctl.aborting && setjmp(ctl.top) == 0)
        ctl.policy(ctl.arg);
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

static PlayResult boot_and_run(const char *script)
{
    audio_shutdown();
    input_reset();
    input_set_mouse(0, 0, 0, 0);  // as a fresh process starts
    timing_reset();
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
        mem_init();
        if (dseg_init()) {
            system_init();
            Game_InitUnits();
            Options_Init();
            system_add_exit_proc(far_ptr(0x405F, 0x1A62), Gfx_ExitHandler);
            Game_Main();
        }
    }
    const char *dump = SDL_getenv("PLAY_MEM_DUMP");
    if (dump && *dump) {
        static int n;
        char path[1024];
        SDL_snprintf(path, sizeof path, "%s.%d", dump, n++);
        SDL_SaveFile(path, mem, MEM_SIZE);
    }
    PlayResult r = {
        .exit_code = halt_code,
        .frames = timing_frames() - start,
        .state_hash = hash_memory(),
    };
    if (trace)
        fclose(trace);
    trace = nullptr;
    autopilot_set_script(nullptr);
    system_exit_handler = nullptr;
    timing_set_quit_handler(nullptr);
    return r;
}

PlayResult play_run(uint32_t seed, PlayPolicy policy, void *arg, const char *record_path)
{
    memset(&ctl, 0, sizeof ctl);
    ctl.policy = policy;
    ctl.arg = arg;
    ctl.to_policy = SDL_CreateSemaphore(0);
    ctl.to_game = SDL_CreateSemaphore(0);
    if (record_path) {
        ctl.record = fopen(record_path, "w");
        if (ctl.record)
            fprintf(ctl.record, "# Recorded by the playthrough fixture (tests/play/).\nseed:%u\n",
                    (unsigned)seed);
    }
    ctl.thread = SDL_CreateThread(policy_main, "policy", nullptr);
    autopilot_set_controller(on_retrace);

    char script[32];
    SDL_snprintf(script, sizeof script, "seed:%u", (unsigned)seed);
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
    if (ctl.record)
        fclose(ctl.record);
    r.failed = ctl.failed;
    SDL_strlcpy(r.failure, ctl.failure, sizeof r.failure);
    return r;
}

PlayResult play_replay(const char *script)
{
    autopilot_set_controller(trace_frame);
    PlayResult r = boot_and_run(script);
    autopilot_set_controller(nullptr);
    return r;
}

// --- Policy side ---

uint64_t play_frame(void)
{
    return ctl.frame;
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
    if (ctl.record)
        fprintf(ctl.record, "%llu:mouse:%d,%d,%d\n", (unsigned long long)ctl.frame, x, y, buttons);
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
    if (ctl.record)
        fprintf(ctl.record, "%llu:key:%d\n", (unsigned long long)ctl.frame, code);
}

void play_quit(void)
{
    input_request_quit();
    if (ctl.record)
        fprintf(ctl.record, "%llu:quit\n", (unsigned long long)ctl.frame);
    longjmp(ctl.top, 1);
}

void play_fail(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    set_failure(fmt, ap);
    va_end(ap);
    play_log("FAILED: %s", ctl.failure);
    input_request_quit();
    if (ctl.record)
        fprintf(ctl.record, "%llu:quit\n", (unsigned long long)ctl.frame);
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
