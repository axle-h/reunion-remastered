// Boots the port: SDL, the platform layer and the game's emulated DOS memory,
// then runs the original start-up (unit initialisers, Main) until it halts.
//
// Leaving the game: the original ends with Halt, deep inside screen code, and
// many waiting loops never return. The port unwinds instead of returning:
// System_Halt (after the ExitProc chain) and a window-close request (seen by
// timing_wait_retrace at the next retrace, i.e. inside every waiting loop)
// both longjmp back to main(), and so does the port menu's Restart. Game state
// lives in the emulated memory, which boot() rebuilds from scratch, so
// nothing on the abandoned C stack matters.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <setjmp.h>
#include <stdio.h>

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
#include "platform/config.h"
#include "platform/fs.h"
#include "platform/input.h"
#include "platform/menu.h"
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

[[noreturn]] static void on_restart(void)
{
    exit_code = EXIT_RESTART;
    longjmp(exit_jump, 1);
}

// Command line: [--cutscene=credits|intro|victory] [--replay=SCRIPT]
// [DATA_DIR].
// The port menu's Skip intro skips the credits and intro before the game, and
// so does an autopilot script (its frame numbers count from the game's
// start).
static bool skip_intro;
static const char *only_cutscene;
// --replay=SCRIPT checks a recorded autopilot script (the playthrough
// fixture's, docs/playthrough.md): plays it without a window or sound until
// the game halts or quits, prints "replay: exit CODE after FRAMES frames,
// hash HASH" as the fixture reports its runs, and exits with the halt code
// (0 for a quit, 1 if the script can't be read or the game never ends).
static const char *replay;

// A replay that goes on this many frames past the script's last event
// without halting has failed (100,000 retraces: about 40 game days).
enum : uint64_t { REPLAY_MARGIN = 100000 };

static const char *find_data_dir(int argc, char *argv[])
{
    const char *dir = nullptr;
    for (int i = 1; i < argc; i++) {
        if (SDL_strncmp(argv[i], "--cutscene=", 11) == 0)
            only_cutscene = argv[i] + 11;
        else if (SDL_strncmp(argv[i], "--replay=", 9) == 0)
            replay = argv[i] + 9;
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

// The port menu's link to the game: requests go to the main loop.
static void menu_changed(MenuChange change)
{
    static const OptionsRequest requests[] = {
        [MENU_CHANGED_CHEATS] = OPTIONS_CHEATS,
        [MENU_ADD_MONEY] = OPTIONS_ADD_MONEY_ONCE,
        [MENU_ADD_ORE] = OPTIONS_ADD_ORE_ONCE,
        [MENU_CHANGED_MUSIC] = OPTIONS_MUSIC,
        [MENU_CHANGED_FIXES] = OPTIONS_FIXES,
    };
    Options_Request(requests[change]);
}

// The config file (not for scripted runs, which must replay the same
// anywhere) and what it sets before the game starts.
static void setup_config(void)
{
    if (!replay && !autopilot_init()) {
        char *path = config_default_path();
        config_load(path);
        SDL_free(path);
    }
    // Every fix gets a line in the file.
    for (int i = 0; i < Fixes_Count; i++)
        config_set_fix(Fixes_List[i].key, config_fix(Fixes_List[i].key));
    Fixes_Init();
    audio_set_user_volumes(config.music_volume / 100.0f,
                           config.effects ? config.effects_volume / 100.0f : 0.0f);
    audio_set_amiga_mixer(config.music == CONFIG_MUSIC_AMIGA);
    skip_intro |= config.skip_intro;

    static MenuFix fixes[CONFIG_MAX_FIXES];
    int n = SDL_min(Fixes_Count, CONFIG_MAX_FIXES);
    for (int i = 0; i < n; i++)
        fixes[i] = (MenuFix){Fixes_List[i].key, Fixes_List[i].label, Fixes_List[i].help};
    menu_set_hooks(&(MenuHooks){
        .game_running = Options_GameRunning,
        .changed = menu_changed,
        .add_money = OPTIONS_ADD_MONEY,
        .add_ore = OPTIONS_ADD_ORE,
        .fixes = fixes,
        .fix_count = n,
        .fixes_locked = Fixes_Locked(),
    });
}

// --replay: the script, with a frame limit so that a replay that goes wrong
// still ends.
static bool start_replay(void)
{
    if (!SDL_GetPathInfo(replay, nullptr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Replay %s: %s", replay, SDL_GetError());
        return false;
    }
    char script[1100];
    SDL_snprintf(script, sizeof script, "@%s", replay);
    autopilot_set_script(script);
    autopilot_set_frame_limit(autopilot_last_event() + REPLAY_MARGIN);
    return true;
}

int main(int argc, char *argv[])
{
    const char *data_dir = find_data_dir(argc, argv);
    setup_config();
    // Headless unless SDL_VIDEO_DRIVER says otherwise (the replay is then
    // shown fast-forward).
    if (replay)
        SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "offscreen,dummy", SDL_HINT_DEFAULT);
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init: %s", SDL_GetError());
        return 1;
    }
    if (!vid_init("Reunion")) {
        SDL_Quit();
        return 1;
    }
    input_init();
    if (replay && !start_replay()) {
        vid_shutdown();
        SDL_Quit();
        return 1;
    }

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
    skip_intro |= autopilot_active();
    if (only_cutscene) {
        play_game = false;
        if (cutscene_from_name(only_cutscene, &cutscene))
            cutscene_play(cutscene);
        else
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unknown cut-scene %s", only_cutscene);
    } else if (!skip_intro) {
        // Restart in a cut-scene goes on to the game.
        play_game = cutscene_play(CUTSCENE_CREDITS)
                    && (input_restart_requested() || cutscene_play(CUTSCENE_INTRO));
    }
    if (input_restart_requested())
        input_reset();

    int result = 0;
    uint64_t replay_start = timing_frames();
    while (play_game) {
        system_exit_handler = on_halt;
        timing_set_quit_handler(on_quit);
        timing_set_restart_handler(on_restart);
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
        if (replay) {
            // The state as the game left it, before any restart or VICTORY.
            uint64_t frames = timing_frames() - replay_start;
            if (autopilot_limit_reached()) {
                printf("replay: no end after %llu frames\n", (unsigned long long)frames);
                result = 1;
            } else {
                printf("replay: exit %d after %llu frames, hash %016llx\n", exit_code,
                       (unsigned long long)frames,
                       (unsigned long long)autopilot_hash(mem, MEM_SIZE));
                result = exit_code == EXIT_QUIT ? 0 : exit_code;
            }
            fflush(stdout);
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
            timing_set_restart_handler(nullptr);
            cutscene_play(CUTSCENE_VICTORY);
            if (input_restart_requested()) {
                input_reset();
                continue;
            }
        }
        result = exit_code == EXIT_QUIT ? 0 : exit_code;
        break;
    }

    system_exit_handler = nullptr;
    timing_set_quit_handler(nullptr);
    timing_set_restart_handler(nullptr);
    config_save();  // the window's size
    audio_shutdown();
    fs_shutdown();
    vid_shutdown();
    SDL_Quit();
    return result;
}
