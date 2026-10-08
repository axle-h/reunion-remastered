// Boots the port: SDL, the platform layer and the game's emulated DOS memory,
// then runs the original start-up (unit initialisers, Main) until it halts.
//
// Leaving the game: the original ends with Halt, deep inside screen code, and
// many waiting loops never return. The port unwinds instead of returning:
// System_Halt (after the ExitProc chain) and a window-close request (seen by
// timing_wait_retrace at the next retrace, i.e. inside every waiting loop)
// both longjmp back to main(). Game state lives in the emulated memory, which
// boot() rebuilds from scratch, so nothing on the abandoned C stack matters.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <setjmp.h>

#include "cutscene/cutscene.h"
#include "game/dseg.h"
#include "game/fixes.h"
#include "game/gfx.h"
#include "game/main_program.h"
#include "game/mem.h"
#include "game/options.h"
#include "game/system.h"
#include "platform/audio.h"
#include "platform/autopilot.h"
#include "platform/fs.h"
#include "platform/input.h"
#include "platform/timing.h"
#include "platform/video.h"

#ifndef REUNION_DEFAULT_DATA_DIR
#define REUNION_DEFAULT_DATA_DIR "."
#endif

// Exit codes of REUNION.PRG that start.exe acts on (docs/cutscenes.md).
enum : int {
    EXIT_VICTORY = 10,  // start.exe runs VICTORY.PRG
    EXIT_RESTART = 15,  // start.exe runs the game again
    EXIT_QUIT = -1,     // window closed (port only)
};

static jmp_buf exit_jump;
static int exit_code;

[[noreturn]] static void on_halt(uint8_t code)
{
    exit_code = code;
    longjmp(exit_jump, 1);
}

[[noreturn]] static void on_quit(void)
{
    exit_code = EXIT_QUIT;
    longjmp(exit_jump, 1);
}

// Command line: [--skip-intro] [--cutscene=credits|intro|victory] [DATA_DIR].
// REUNION_SKIP_INTRO=1 also skips the credits and intro before the game, and
// so does an autopilot script (its frame numbers count from the game's start).
static bool skip_intro;
static const char *only_cutscene;

static const char *find_data_dir(int argc, char *argv[])
{
    const char *dir = nullptr;
    for (int i = 1; i < argc; i++) {
        if (SDL_strcmp(argv[i], "--skip-intro") == 0)
            skip_intro = true;
        else if (SDL_strncmp(argv[i], "--cutscene=", 11) == 0)
            only_cutscene = argv[i] + 11;
        else if (!dir)
            dir = argv[i];
    }
    if (dir)
        return dir;
    const char *env = SDL_getenv("REUNION_DATA");
    if (env && *env)
        return env;
    return REUNION_DEFAULT_DATA_DIR;
}

// 1000:01a1 up to Main's body: the emulated machine and the unit
// initialisers, in the original order.
static bool boot(void)
{
    mem_init();
    if (!dseg_init())
        return false;
    system_init();
    Game_InitUnits();
    Options_Init();
    system_add_exit_proc(far_ptr(0x405F, 0x1A62), Gfx_ExitHandler);
    return true;
}

int main(int argc, char *argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init: %s", SDL_GetError());
        return 1;
    }
    if (!vid_init("Reunion")) {
        SDL_Quit();
        return 1;
    }
    input_init();
    autopilot_init();
    Fixes_Init();

    const char *data_dir = find_data_dir(argc, argv);
    char *exe = nullptr;
    if (fs_init(data_dir))
        exe = fs_resolve("GRWAR\\REUNION.PRG");
    if (!exe) {
        char msg[1200];
        SDL_snprintf(msg, sizeof msg,
                     "The original game data was not found in\n%s\n\n"
                     "Pass the directory holding GRWAR\\REUNION.PRG, GRAFIKA, ... as the first "
                     "argument or set REUNION_DATA.",
                     data_dir);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", msg);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Reunion", msg, nullptr);
        vid_shutdown();
        SDL_Quit();
        return 1;
    }
    SDL_free(exe);

    // start.exe: CREDITS.PRG and INTRO.PRG, then the game (the cut-scenes
    // return false if the window was closed).
    bool play_game = true;
    Cutscene cutscene;
    const char *skip_env = SDL_getenv("REUNION_SKIP_INTRO");
    skip_intro |= (skip_env && *skip_env && *skip_env != '0') || autopilot_active();
    if (only_cutscene) {
        play_game = false;
        if (cutscene_from_name(only_cutscene, &cutscene))
            cutscene_play(cutscene);
        else
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unknown cut-scene %s", only_cutscene);
    } else if (!skip_intro) {
        play_game = cutscene_play(CUTSCENE_CREDITS) && cutscene_play(CUTSCENE_INTRO);
    }

    system_exit_handler = on_halt;
    timing_set_quit_handler(on_quit);
    int result = 0;
    while (play_game) {
        // volatile: read after longjmp.
        volatile bool booted = false;
        if (setjmp(exit_jump) == 0) {
            booted = boot();
            if (booted)
                Game_Main();
            exit_code = booted ? 0 : 1;
        }
        if (!booted && exit_code == 1) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Reunion",
                                     "GRWAR\\REUNION.PRG could not be read.", nullptr);
            result = 1;
            break;
        }
        // Soak tests (random autopilot) keep playing after the player quits.
        uint32_t seed;
        if (exit_code == 0 && booted && autopilot_random_seed(&seed)) {
            SDL_Log("Autopilot: the game quit at frame %llu, restarting",
                    (unsigned long long)timing_frames());
            exit_code = EXIT_RESTART;
        }
        if (exit_code == EXIT_RESTART) {
            audio_shutdown();  // Sound_Init opens the device again
            input_reset();
            continue;
        }
        if (exit_code == EXIT_VICTORY) {
            system_exit_handler = nullptr;  // VICTORY.PRG has its own
            timing_set_quit_handler(nullptr);
            cutscene_play(CUTSCENE_VICTORY);
        }
        result = exit_code == EXIT_QUIT ? 0 : exit_code;
        break;
    }

    system_exit_handler = nullptr;
    timing_set_quit_handler(nullptr);
    audio_shutdown();
    fs_shutdown();
    vid_shutdown();
    SDL_Quit();
    return result;
}
