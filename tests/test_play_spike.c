// Playthrough fixture spike (docs/playthrough.md, thread 0): a policy starts
// a new game and hires William Forbs through clicks only; the recorded input
// then replays without the policy to the same end state. Needs the game data.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/dseg.h"
#include "platform/fs.h"
#include "play/play.h"
#include "play/ui.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static int hired_builder, credits;

static void spike(void *unused)
{
    (void)unused;
    ui_new_game();
    ui_hire("BUILDERS", "William Forbs");
    hired_builder = ds.Game_StaffHired[1];
    credits = ds.Game_Credits;
    play_dump("end");
    play_quit();
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, SDL_getenv("PLAY_AUDIO") ? SDL_getenv("PLAY_AUDIO") : "dummy");
    if (!SDL_Init(SDL_INIT_EVENTS)) {
        printf("SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    char record[1024];
    char *cwd = SDL_GetCurrentDirectory();
    SDL_snprintf(record, sizeof record, "%s/play_spike.txt", cwd);
    SDL_free(cwd);

    PlayResult r = play_run(12345, spike, nullptr, record);
    printf("run: exit %d after %llu frames, hash %016llx%s%s\n", r.exit_code,
           (unsigned long long)r.frames, (unsigned long long)r.state_hash,
           r.failed ? ", FAILED: " : "", r.failure);
    CHECK(!r.failed);
    CHECK(r.exit_code == -1);
    printf("builder %d, credits %d\n", hired_builder, credits);
    CHECK(hired_builder == 3);           // William Forbs
    CHECK(credits == 121620 - 70000);    // after the first day's tax

    char script[1100];
    SDL_snprintf(script, sizeof script, "@%s", record);
    PlayResult p = play_replay(script);
    printf("replay: exit %d after %llu frames, hash %016llx\n", p.exit_code,
           (unsigned long long)p.frames, (unsigned long long)p.state_hash);
    CHECK(p.exit_code == r.exit_code);
    CHECK(p.frames == r.frames);
    CHECK(p.state_hash == r.state_hash);
    PlayResult q = play_replay(script);
    printf("replay 2: exit %d after %llu frames, hash %016llx\n", q.exit_code,
           (unsigned long long)q.frames, (unsigned long long)q.state_hash);
    PlayResult r2 = play_run(12345, spike, nullptr, nullptr);
    printf("run 2: exit %d after %llu frames, hash %016llx\n", r2.exit_code,
           (unsigned long long)r2.frames, (unsigned long long)r2.state_hash);

    SDL_Quit();
    return ok ? 0 : 1;
}
