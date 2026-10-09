#include "play/run.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#include "platform/fs.h"

static const PlayPlan *plan;
static const char *stop_at;

static int step_index(const char *name)
{
    for (int i = 0; i < plan->count; i++)
        if (SDL_strcmp(plan->steps[i].name, name) == 0)
            return i;
    return -1;
}

// The policy: the steps in order, each after its checkpoint. A resumed run
// starts with the step it was resumed at, whose checkpoint is in the prefix.
static void run_steps(void *unused)
{
    (void)unused;
    const char *resumed = play_resumed_at();
    int first = resumed ? step_index(resumed) : 0;
    if (first < 0)
        play_fail("resumed at %s, which is no step of %s", resumed, plan->name);
    for (int i = first; i < plan->count; i++) {
        const PlayStep *s = &plan->steps[i];
        if (i > first || !resumed)
            play_checkpoint(s->name);
        if (stop_at && SDL_strcmp(stop_at, s->name) == 0) {
            play_log("PLAY_STOP: stopping before %s", s->name);
            play_quit();
        }
        s->run();
    }
}

static bool same_end(const char *what, const PlayResult *r, const PlayResult *expect)
{
    bool same = r->exit_code == expect->exit_code && r->frames == expect->frames
                && r->state_hash == expect->state_hash;
    printf("%s: exit %d after %llu frames, hash %016llx%s\n", what, r->exit_code,
           (unsigned long long)r->frames, (unsigned long long)r->state_hash,
           same ? "" : " -- DIFFERENT");
    return same;
}

static bool verify_with_reunion(const char *recording, const char *data_dir,
                                const char *reunion_bin, const PlayResult *expect)
{
    char arg[1100];
    SDL_snprintf(arg, sizeof arg, "--replay=%s", recording);
    const char *args[] = {reunion_bin, arg, data_dir, nullptr};
    SDL_Process *p = SDL_CreateProcess(args, true);
    if (!p) {
        printf("reunion --replay: %s\n", SDL_GetError());
        return false;
    }
    int status = -1;
    char *out = SDL_ReadProcess(p, nullptr, &status);
    SDL_DestroyProcess(p);
    const char *line = out ? SDL_strstr(out, "replay: exit ") : nullptr;
    PlayResult r = {.exit_code = -2};
    unsigned long long frames = 0, hash = 0;
    if (!line || SDL_sscanf(line, "replay: exit %d after %llu frames, hash %llx", &r.exit_code,
                            &frames, &hash) != 3) {
        printf("reunion --replay: no result (status %d): %s\n", status, out ? out : "");
        SDL_free(out);
        return false;
    }
    SDL_free(out);
    r.frames = frames;
    r.state_hash = hash;
    // The process exits with the halt code, 0 for a quit.
    int want_status = expect->exit_code == -1 ? 0 : expect->exit_code;
    bool ok = same_end("reunion --replay", &r, expect);
    if (status != want_status) {
        printf("reunion --replay: exit status %d, expected %d\n", status, want_status);
        ok = false;
    }
    return ok;
}

bool play_verify(const char *recording, const char *data_dir, const char *reunion_bin,
                 const PlayResult *expect)
{
    char script[1100];
    SDL_snprintf(script, sizeof script, "@%s", recording);
    uint64_t t0 = SDL_GetTicksNS();
    PlayResult r = play_replay(script);
    double s = (double)(SDL_GetTicksNS() - t0) / 1e9;
    bool ok = same_end("replay", &r, expect);
    printf("replay: %.1f s, %.0f frames/s\n", s, (double)r.frames / s);
    if (reunion_bin && *reunion_bin)
        ok &= verify_with_reunion(recording, data_dir, reunion_bin, expect);
    return ok;
}

static bool env_flag(const char *name, bool def)
{
    const char *v = SDL_getenv(name);
    return v && *v ? *v != '0' : def;
}

int play_main(int argc, char *argv[], const PlayPlan *p)
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    const char *audio = SDL_getenv("PLAY_AUDIO");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, audio && *audio ? audio : "dummy");
    if (!SDL_Init(SDL_INIT_EVENTS)) {
        printf("SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    plan = p;
    const char *env = SDL_getenv("PLAY_SEED");
    uint32_t seed = env && *env ? (uint32_t)SDL_strtoul(env, nullptr, 10) : p->seed ? p->seed : 12345;
    char record[1024];
    env = SDL_getenv("PLAY_RECORD");
    if (env && *env)
        SDL_strlcpy(record, env, sizeof record);
    else
        SDL_snprintf(record, sizeof record, "%s.txt", p->name);
    stop_at = SDL_getenv("PLAY_STOP");
    if (stop_at && !*stop_at)
        stop_at = nullptr;
    const char *resume = SDL_getenv("PLAY_RESUME");
    const char *from = SDL_getenv("PLAY_RESUME_FROM");
    if (!from || !*from)
        from = record;

    uint64_t t0 = SDL_GetTicksNS();
    PlayResult r;
    if (resume && *resume) {
        printf("%s: resuming at %s of %s\n", p->name, resume, from);
        r = play_resume(from, resume, run_steps, nullptr, record);
    } else {
        printf("%s: seed %u\n", p->name, (unsigned)seed);
        r = play_run(seed, run_steps, nullptr, record);
    }
    double s = (double)(SDL_GetTicksNS() - t0) / 1e9;
    printf("run: exit %d after %llu frames, hash %016llx, %.1f s%s%s\n", r.exit_code,
           (unsigned long long)r.frames, (unsigned long long)r.state_hash, s,
           r.failed ? ", FAILED: " : "", r.failure);
    printf("recording: %s\n", record);

    int expect = stop_at ? -1 : p->expect_exit;
    bool ok = !r.failed;
    if (r.exit_code != expect) {
        printf("expected exit %d\n", expect);
        ok = false;
    }
    if (r.tainted)
        printf("tainted by play_test_setup: not verified by replay\n");
    else if (ok && env_flag("PLAY_VERIFY", true))
        ok = play_verify(record, argv[1], SDL_getenv("REUNION_BIN"), &r);
    SDL_Quit();
    printf("%s\n", ok ? "PASSED" : "FAILED");
    return ok ? 0 : 1;
}
