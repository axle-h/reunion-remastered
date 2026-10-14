// main() for playthrough policy tests (tests/test_play_*.c): a policy is a
// list of steps, each run from a checkpoint of the same name, so a long
// policy can be iterated on from any step of an earlier run's recording.
//
//   static const PlayStep steps[] = {{"start", start}, {"act1", act1}};
//   int main(int argc, char *argv[])
//   {
//       return play_main(argc, argv, &(PlayPlan){.name = "play_act1", .steps = steps,
//                                                 .count = 2, .expect_exit = -1});
//   }
//
// Environment:
//   PLAY_SEED=N         game seed (default: the plan's, else 12345)
//   PLAY_RECORD=FILE    the recording (default: NAME.txt in the working
//                       directory, i.e. the build directory under ctest;
//                       NAME_LANGUAGE.txt with PLAY_LANGUAGE)
//   PLAY_RESUME=STEP    replays the recording up to step STEP's checkpoint
//                       and plays from there (PLAY_RESUME_FROM=FILE reads
//                       another recording); the new recording replaces it
//   PLAY_STOP=STEP      ends the run (as a quit) when step STEP would start
//   PLAY_VERIFY=0       skips replaying the recording at the end
//   PLAY_LANGUAGE=NAME  plays the CD's German, French or Italian game: the
//                       data is then an import's data/ (English, with the
//                       language's folder; lang.h), or the CD's ISO, which
//                       is imported into NAME_LANGUAGE.import/ first
//   REUNION_BIN=PATH    also replays it with `PATH --replay` (set by CMake)
//   PLAY_DUMP_DIR, PLAY_TRACE, PLAY_STATE_DUMP: see play.c
#pragma once

#include "play/play.h"

typedef struct {
    const char *name;  // checkpoint taken before the step runs
    void (*run)(void);
} PlayStep;

typedef struct {
    const char *name;        // the test, for the default recording name
    const PlayStep *steps;
    int count;
    int expect_exit;         // the end that passes: -1 (the policy quits) or a halt code
    uint32_t seed;           // 0: 12345
} PlayPlan;

// The name of the step running (nullptr outside a plan's step).
const char *play_step_name(void);

// Parses argv[1] (game data directory), runs the plan and verifies the
// recording. Returns the process exit code: 0 passed, 1 failed, 77 no data.
int play_main(int argc, char *argv[], const PlayPlan *plan);

// Replays `recording` in-process and, if `reunion_bin` is set, with
// `reunion_bin --replay=RECORDING DATA_DIR`, and checks that both end as
// `expect`. Logs what differs.
bool play_verify(const char *recording, const char *data_dir, const char *reunion_bin,
                 const PlayResult *expect);
