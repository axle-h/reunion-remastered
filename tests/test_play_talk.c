// Dialogue drivers (tests/play/ui_talk.h, docs/playthrough.md thread B4):
// alien talks 2-10 along the policy's paths (docs/playthrough-policy.md
// §3B), story scenes, a pub conversation and a university course, each from
// a new game with seed 12345. Talks and scenes come late in a game, so those
// runs start them with the game's own entry (AlienTalk_StartAlienTalk /
// StartAlienScene, as the story timers do) through play_test_setup; the pub
// and the university are reached by play, and the university run is
// replayed. Effects are checked in the game state. Needs the game data.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/alientalk.h"
#include "game/dseg.h"
#include "platform/fs.h"
#include "play/play.h"
#include "play/ui.h"
#include "play/ui_message.h"
#include "play/ui_talk.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

enum : uint32_t { SEED = 12345 };

static const TProduct *product(int k)
{
    return &ds.Research_Products[k - 1];
}

// --- Alien talks ---

typedef struct {
    int race;
    int talk;          // what ui_alien_talk_policy returned
    int32_t credits;   // before and after the talk
    int32_t credits_after;
    int32_t energon_after;
    uint8_t key_abort;
} TalkRun;

static void start_talk(void *arg)
{
    TalkRun *t = arg;
    if (t->race == 3) {
        // Meeting 2's own trigger (sim.c ProcessStoryTimers): the hour after
        // next the timer runs out, MSG 9's box shows, then the talk.
        ds.Intel_Race2Timer2 = 1;
        ds.Intel_Race2Done2 = 0;
        ds.Races_Races[0].Status = 3;            // Jaanosians met
        ds.Research_Products[7].State = 5;       // Communicator done
        return;
    }
    if (t->race == 7)
        ds.Game_HomeStock[1] = 3000;  // Energon for the Phelonians to halve
    if (t->race == 9)
        ds.Pub_People[4].State = 1;   // the Eran not yet in the pub (story)
    AlienTalk_StartAlienTalk((int16_t)t->race);
}

static bool past_midnight(void *unused)
{
    (void)unused;
    return ds.Game_Hour == 1;
}

static void talk_policy(void *arg)
{
    TalkRun *t = arg;
    ui_new_game();
    // Past the daily tax, so that the talk's own payments show in the credits.
    if (!play_wait_until(past_midnight, nullptr, 1000))
        play_fail("the clock does not run");
    t->credits = ds.Game_Credits;
    play_test_setup(start_talk, t);
    t->talk = ui_alien_talk_policy();
    t->credits_after = ds.Game_Credits;
    t->energon_after = ds.Game_HomeStock[1];
    t->key_abort = ds.Gfx_KeyAbort;
    play_dump("talk%d-end", t->race);
    play_quit();
}

static void check_talk_effect(const TalkRun *t)
{
    switch (t->race) {
    case 2:  // scientist away 60-89 h (the Trade ship follows)
        CHECK(ds.Intel_Race2Done1 == 1);
        CHECK((int16_t)ds.Intel_DevAwayTimer >= 60 && (int16_t)ds.Intel_DevAwayTimer < 90);
        break;
    case 3:  // -16,000; the alien item (Hyperspace drive) to analyse
        CHECK(t->credits_after == t->credits - 16000);
        CHECK(product(14)->State == 3);
        break;
    case 4:  // Hunter, Laser cannon, Army groups
        CHECK(product(10)->State == 5);
        CHECK(product(11)->State == 5);
        CHECK(ds.Game_GroupKindAllowed[0] == 1);
        break;
    case 5:  // Starfighter to analyse, 10 in stock
        CHECK(product(16)->State == 3);
        CHECK(product(16)->Stock == 10);
        break;
    case 6:  // the Kall turn cancelled
        CHECK(ds.Intel_KallTurnCancelled == 1);
        CHECK(ds.Intel_KallTurnTimer == 0);
        break;
    case 7:  // Battle tank; New Earth's Energon halved
        CHECK(product(0x15)->State == 5);
        CHECK(t->energon_after == 1500);
        CHECK(t->credits_after == t->credits);
        break;
    case 9:  // the Eran in the pub, Erans allied
        CHECK(ds.Pub_People[4].State == 2);
        CHECK(ds.Races_Races[4].Status == 6);
        break;
    case 10:  // Sun known, Energy shield, Bounty-Hunter in the pub
        CHECK(ds.Galaxy_SystemStatus[7] == 0);
        CHECK(product(0x23)->State == 5);
        CHECK(ds.Pub_People[6].State == 2);
        break;
    default:
        break;
    }
}

static void test_talk(int race)
{
    TalkRun t = {.race = race};
    PlayResult r = play_run(SEED, talk_policy, &t, nullptr);
    printf("talk %d: exit %d after %llu frames%s%s\n", race, r.exit_code, (unsigned long long)r.frames,
           r.failed ? ", FAILED: " : "", r.failure);
    CHECK(!r.failed);
    CHECK(r.tainted);
    CHECK(t.talk == race);
    CHECK(t.key_abort == 0);
    CHECK(ds.Game_CurrentScreen == 1);
    check_talk_effect(&t);
}

// --- Story scenes ---

typedef struct {
    int scene, shown;
    uint8_t key_abort;
} SceneRun;

static void start_scene(void *arg)
{
    int scene = ((SceneRun *)arg)->scene;
    if (scene == 1) {
        // The fall of the Jaanosian base by its own timer: MSG 11's box,
        // then the scene.
        ds.Intel_Race2Timer4 = 1;
        ds.Intel_Race2Done4 = 0;
        ds.Races_Races[0].Status = 3;
        return;
    }
    AlienTalk_StartAlienScene((int16_t)scene);
}

static void scene_policy(void *arg)
{
    SceneRun *s = arg;
    ui_new_game();
    play_test_setup(start_scene, s);
    s->shown = ui_story_scene();
    s->key_abort = ds.Gfx_KeyAbort;
    play_dump("scene%d-end", s->scene);
    play_quit();
}

static void test_scene(int scene)
{
    SceneRun s = {.scene = scene};
    PlayResult r = play_run(SEED, scene_policy, &s, nullptr);
    printf("scene %d: exit %d after %llu frames%s%s\n", scene, r.exit_code, (unsigned long long)r.frames,
           r.failed ? ", FAILED: " : "", r.failure);
    CHECK(!r.failed);
    CHECK(s.shown == scene);
    CHECK(s.key_abort == 0);
    CHECK(ds.Game_CurrentScreen == 1);
    if (scene == 1)
        CHECK(ds.Game_StoryStage == 2);
}

// --- Pub ---

static int pub_asked[4];
static char informer_reply[TALK_TEXT_MAX];
static int32_t pub_credits[2];
static int pub_day[2];

static void pub_policy(void *unused)
{
    (void)unused;
    ui_new_game();
    pub_asked[0] = ui_pub_ask("Bartender", "Have you heard any news", "Of course, what do you recommend?",
                              "All right, that will do.");
    pub_credits[0] = ds.Game_Credits;
    pub_day[0] = ds.Game_Day;
    // Nothing to sell yet: the talk ends after the first question.
    pub_asked[1] = ui_pub_ask("Informer", "Do you have any information?", "All right, talk.");
    SDL_strlcpy(informer_reply, ui_last_reply(), sizeof informer_reply);
    pub_asked[2] = ui_pub_ask("Spy-Hunter", "Have you seen or heard anything unusual?");
    pub_asked[3] = ui_pub_ask("Stranger", "Would you work for me?");  // not in the pub yet
    pub_credits[1] = ds.Game_Credits;
    pub_day[1] = ds.Game_Day;
    play_dump("pub-end");
    play_quit();
}

static void test_pub(void)
{
    PlayResult r = play_run(SEED, pub_policy, nullptr, nullptr);
    printf("pub: exit %d after %llu frames%s%s\n", r.exit_code, (unsigned long long)r.frames,
           r.failed ? ", FAILED: " : "", r.failure);
    CHECK(!r.failed);
    CHECK(pub_asked[0] == 3);
    CHECK(pub_asked[1] == 1);
    CHECK(SDL_strcmp(informer_reply, "I can't tell you anything new.") == 0);
    CHECK(pub_asked[2] == 1);
    CHECK(ds.Pub_StoryFlags[0] == 1);                     // the Spy-Hunter has been asked
    CHECK(pub_asked[3] == 0);
    CHECK(ds.Game_CurrentScreen == 23);
    CHECK(pub_day[1] == pub_day[0]);                      // no tax day in between
    CHECK(pub_credits[1] == pub_credits[0]);
}

// --- University ---

static bool uni_went, uni_second;
static int32_t uni_credits[3];
static uint16_t uni_exp;

static void university_policy(void *unused)
{
    (void)unused;
    ui_new_game();
    ui_hire("DEVELOPERS", "Thomas Dempsey");
    uni_credits[0] = ds.Game_Credits;
    uni_exp = ds.Game_StaffExp[3];
    uni_went = ui_university("Thomas Dempsey", "Maths & physics.");
    uni_credits[1] = ds.Game_Credits;
    // Only one away at a time: "I cannot leave my work behind right now".
    ui_hire("BUILDERS", "William Forbs");
    uni_second = ui_university("William Forbs", nullptr);
    uni_credits[2] = ds.Game_Credits;
    play_dump("university-end");
    play_quit();
}

static void test_university(void)
{
    char record[1024];
    char *cwd = SDL_GetCurrentDirectory();
    SDL_snprintf(record, sizeof record, "%s/play_talk_university.txt", cwd);
    SDL_free(cwd);
    PlayResult r = play_run(SEED, university_policy, nullptr, record);
    printf("university: exit %d after %llu frames, hash %016llx%s%s\n", r.exit_code,
           (unsigned long long)r.frames, (unsigned long long)r.state_hash, r.failed ? ", FAILED: " : "",
           r.failure);
    CHECK(!r.failed);
    CHECK(!r.tainted);
    CHECK(uni_went);
    CHECK(ds.Advisor_TrainingKind == 4);                  // the developer is away
    CHECK(ds.Advisor_TrainingTimer >= 50 && ds.Advisor_TrainingTimer < 70); // for 50-69 h
    CHECK(ds.Advisor_TrainingCourse == 1);                // Maths & physics
    int32_t paid = uni_credits[0] - uni_credits[1];
    printf("university: paid %d at experience %d\n", (int)paid, uni_exp);
    CHECK(paid % 1000 == 0 && paid >= uni_exp * 1000 && paid < (uni_exp + 10) * 1000);
    CHECK(!uni_second);
    CHECK(uni_credits[2] == uni_credits[1] - 70000);     // only the builder's hire
    CHECK(ds.Game_CurrentScreen == 1);

    char script[1100];
    SDL_snprintf(script, sizeof script, "@%s", record);
    PlayResult p = play_replay(script);
    printf("university replay: exit %d after %llu frames, hash %016llx\n", p.exit_code,
           (unsigned long long)p.frames, (unsigned long long)p.state_hash);
    CHECK(p.exit_code == r.exit_code);
    CHECK(p.frames == r.frames);
    CHECK(p.state_hash == r.state_hash);
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
    const char *only = SDL_getenv("PLAY_TALK_ONLY");  // e.g. "talk4", "scene1", "pub"
    char name[16];
    static const int talks[] = {2, 3, 4, 5, 6, 7, 9, 10};
    for (size_t i = 0; i < SDL_arraysize(talks); i++) {
        SDL_snprintf(name, sizeof name, "talk%d", talks[i]);
        if (!only || SDL_strcmp(only, name) == 0)
            test_talk(talks[i]);
    }
    static const int scenes[] = {1, 2, 3, 4, 9};
    for (size_t i = 0; i < SDL_arraysize(scenes); i++) {
        SDL_snprintf(name, sizeof name, "scene%d", scenes[i]);
        if (!only || SDL_strcmp(only, name) == 0)
            test_scene(scenes[i]);
    }
    if (!only || SDL_strcmp(only, "pub") == 0)
        test_pub();
    if (!only || SDL_strcmp(only, "university") == 0)
        test_university();
    SDL_Quit();
    return ok ? 0 : 1;
}
