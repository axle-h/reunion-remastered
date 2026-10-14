#include "play/run.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#include "platform/fs.h"
#include "platform/gamedata.h"
#include "platform/import.h"
#include "play/lang.h"

static const PlayPlan *plan;
static const char *stop_at;
static const char *current;

const char *play_step_name(void)
{
    return current;
}

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
        current = s->name;
        s->run();
        current = nullptr;
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

// PLAY_LANGUAGE, or nullptr.
static const char *language;

static bool verify_with_reunion(const char *recording, const char *data_dir,
                                const char *reunion_bin, const PlayResult *expect)
{
    char arg[1100], lang[64];
    SDL_snprintf(arg, sizeof arg, "--replay=%s", recording);
    SDL_snprintf(lang, sizeof lang, "--language=%s", language);
    const char *with[] = {reunion_bin, arg, lang, data_dir, nullptr};
    const char *without[] = {reunion_bin, arg, data_dir, nullptr};
    const char **args = language ? with : without;
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

// A copy of the game in a file (the CD's ISO) is imported into
// <name>.import/ in the working directory, or taken from there if it was
// already. The data directory, or nullptr.
static char *game_data(const char *path, const char *name)
{
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(path, &info))
        return nullptr;
    if (info.type == SDL_PATHTYPE_DIRECTORY)
        return SDL_strdup(path);
    char *player = nullptr;
    SDL_asprintf(&player, "%s.import/", name);
    char *data = player ? import_data_dir(player, nullptr) : nullptr;
    if (player && !data) {
        SDL_CreateDirectory(player);
        ImportReport r;
        if (import_game(path, player, gamedata_files, gamedata_file_count, &r, nullptr))
            data = import_data_dir(player, nullptr);
        else
            printf("importing %s: %s\n", path, r.error ? r.error : "files missing or different");
        import_report_free(&r);
    }
    SDL_free(player);
    return data;
}

int play_main(int argc, char *argv[], const PlayPlan *p)
{
    language = SDL_getenv("PLAY_LANGUAGE");
    if (!language || !*language)
        language = nullptr;
    char name[96];
    SDL_snprintf(name, sizeof name, "%s%s%s", p->name, language ? "_" : "", language ? language : "");
    char *data = argc >= 2 ? game_data(argv[1], name) : nullptr;
    if (!data || !fs_init(data)) {
        printf("no game data, skipping\n");
        SDL_free(data);
        return 77;
    }
    if (language) {
        // As main.c with --language: the language's folder in the root.
        Language l = gamedata_language_from_name(language);
        char *dir = l < LANGUAGE_COUNT ? fs_resolve(gamedata_language_folder(l)) : nullptr;
        if (!dir) {
            printf("no %s game in %s, skipping\n", language, data);
            SDL_free(data);
            return 77;
        }
        fs_set_language_dir(dir);
        SDL_free(dir);
    }
    if (!lang_init()) {
        SDL_free(data);
        return 1;
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
        SDL_snprintf(record, sizeof record, "%s.txt", name);
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
        ok = play_verify(record, data, SDL_getenv("REUNION_BIN"), &r);
    SDL_free(data);
    SDL_Quit();
    printf("%s\n", ok ? "PASSED" : "FAILED");
    return ok ? 0 : 1;
}
