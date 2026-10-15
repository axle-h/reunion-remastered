// Boots the port: SDL, the platform layer and the game's state, then runs the
// original start-up (unit initialisers, Main) until it halts.
//
// Leaving the game: the original ends with Halt, deep inside screen code, and
// many waiting loops never return. The port unwinds instead of returning:
// System_Halt and fatal errors (after the exit hook) and a window-close
// request (seen by timing_wait_retrace at the next retrace, i.e. inside every
// waiting loop) longjmp back to main(), and so does the port menu's Restart.
// Game state lives in DS (game/dseg.h), heap blocks and video memory, which
// boot() rebuilds from scratch, so nothing on the abandoned C stack matters.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <setjmp.h>
#include <stdio.h>

#include "cutscene/cutscene.h"
#include "game/dseg.h"
#include "game/fixes.h"
#include "game/improvements.h"
#include "game/keyplay.h"
#include "game/gfx.h"
#include "game/lang.h"
#include "game/main_program.h"
#include "game/music.h"
#include "game/options.h"
#include "game/state.h"
#include "game/system.h"
#include "platform/audio.h"
#include "platform/autopilot.h"
#include "platform/config.h"
#include "platform/fs.h"
#include "platform/gamedata.h"
#include "platform/import.h"
#include "platform/importscreen.h"
#include "platform/input.h"
#include "platform/menu.h"
#include "platform/playerdir.h"
#include "platform/timing.h"
#include "platform/video.h"
#include "platform/web.h"

// Exit codes of REUNION.PRG that start.exe acts on (cutscene/cutscene.h).
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
// [--import=PATH] [--language=NAME] [DATA_DIR].
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
// --import=PATH imports the player's copy at PATH into the player's folder
// (platform/import.h), prints the result and exits: 0 if it was imported.
static const char *import_path;
// --language=english|german|french|italian sets the language as the port
// menu does (scripted runs read no config file).
static const char *language_arg;
static const char *data_arg;

// A replay that goes on this many frames past the script's last event
// without halting has failed (100,000 retraces: about 40 game days).
enum : uint64_t { REPLAY_MARGIN = 100000 };

static void parse_args(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if (SDL_strncmp(argv[i], "--cutscene=", 11) == 0)
            only_cutscene = argv[i] + 11;
        else if (SDL_strncmp(argv[i], "--replay=", 9) == 0)
            replay = argv[i] + 9;
        else if (SDL_strncmp(argv[i], "--import=", 9) == 0)
            import_path = argv[i] + 9;
        else if (SDL_strncmp(argv[i], "--language=", 11) == 0)
            language_arg = argv[i] + 11;
        else if (!data_arg)
            data_arg = argv[i];
    }
}

// Sets the fs root to `dir` if it holds the game (its GRAFIKA folder).
static bool use_data(const char *dir)
{
    char *grafika = dir && fs_init(dir) ? fs_resolve("GRAFIKA") : nullptr;
    SDL_free(grafika);
    return grafika != nullptr;
}

// The folder of config.language in the game's root (an import's data/DEU,
// platform/import.h), if the root's game is another language and has one.
// Set at start and at every restart, so a change in the port menu applies
// from the next restart.
static void use_language(void)
{
    fs_set_language_dir(nullptr);
    char *dir = config.language != gamedata_language()
                    ? fs_resolve(gamedata_language_folder(config.language))
                    : nullptr;
    fs_set_language_dir(dir);
    SDL_free(dir);
}

// The languages the port menu offers: the root's and its folders'.
static unsigned data_languages(void)
{
    char *keep = fs_language_dir() ? SDL_strdup(fs_language_dir()) : nullptr;
    fs_set_language_dir(nullptr);
    Language own = gamedata_language();
    unsigned in = 1u << own;
    for (Language l = 0; l < LANGUAGE_COUNT; l++) {
        char *dir = l != own ? fs_resolve(gamedata_language_folder(l)) : nullptr;
        if (dir)
            in |= 1u << l;
        SDL_free(dir);
    }
    fs_set_language_dir(keep);
    SDL_free(keep);
    return in;
}

// The game data: DATA_DIR, else $REUNION_DATA, else the import in the
// player's folder, else the REUNION_DATA_DIR built in (not in release
// builds). False if none of them holds the game.
static bool find_data(void)
{
    if (data_arg) {
        if (use_data(data_arg))
            return true;
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "No game data in %s", data_arg);
    }
    const char *env = SDL_getenv("REUNION_DATA");
    if (env && *env) {
        if (use_data(env))
            return true;
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "No game data in REUNION_DATA (%s)", env);
    }
    char *imported = playerdir() ? import_data_dir(playerdir(), nullptr) : nullptr;
    bool found = use_data(imported);
    SDL_free(imported);
#ifdef REUNION_DEFAULT_DATA_DIR
    found = found || use_data(REUNION_DEFAULT_DATA_DIR);
#endif
    return found;
}

// find_data and the language: what the game reads.
static bool find_game(void)
{
    bool found = find_data();
    if (found)
        use_language();
    return found;
}

// --import=PATH, without a window.
static int import_only(void)
{
    const char *dir = playerdir();
    if (!dir) {
        fprintf(stderr, "No folder for the player's files: %s\n", SDL_GetError());
        return 1;
    }
    ImportReport report;
    bool ok = import_game(import_path, dir, gamedata_files, gamedata_file_count, &report, nullptr);
    char *text = importscreen_describe(&report, 0);
    printf("%s\n", text ? text : "");
    if (ok)
        printf("The game data is in %sdata\n", dir);
    SDL_free(text);
    import_report_free(&report);
    return ok ? 0 : 1;
}

// 1000:01a1 up to Main's body: video memory, DS, the heap and the unit
// initialisers, in the original order. Before them the port picks the
// language and the CD's or the floppy's font for the edition of the data
// (game/gfx.h).
static void boot(void)
{
    use_language();
    Lang_Set(gamedata_language());
    gfx_set_cd_charset(gamedata_edition() == EDITION_CD);
    vid_clear();
    dseg_init();
    system_init();
    Game_InitUnits();
    Options_Init();
}

// The port menu's link to the game: requests go to the main loop. The music
// changes at once, wherever the menu was opened: in a cut-scene, on the title
// screen or in the game.
static void menu_changed(MenuChange change)
{
    static const OptionsRequest requests[] = {
        [MENU_CHANGED_CHEATS] = OPTIONS_CHEATS,
        [MENU_ADD_MONEY] = OPTIONS_ADD_MONEY_ONCE,
        [MENU_ADD_ORE] = OPTIONS_ADD_ORE_ONCE,
        [MENU_CHANGED_SWITCHES] = OPTIONS_SWITCHES,
    };
    if (change == MENU_CHANGED_MUSIC) {
        if (!cutscene_music_changed())
            Music_SwitchDevice();
        return;
    }
    Options_Request(requests[change]);
}

// The config file (not for scripted runs, which must replay the same
// anywhere) and what it sets before the game starts.
static void setup_config(void)
{
    bool scripted = replay || autopilot_init();
    if (!scripted) {
        char *path = config_default_path();
        config_load(path);
        SDL_free(path);
    }
    if (language_arg) {
        Language l = gamedata_language_from_name(language_arg);
        if (l < LANGUAGE_COUNT)
            config.language = l;
        else
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Unknown language %s", language_arg);
    }
    // Every fix and improvement gets a line in the file.
    for (int i = 0; i < Fixes_Count; i++)
        config_set_fix(Fixes_List[i].key, config_fix(Fixes_List[i].key));
    for (int i = 0; i < Improvements_Count; i++)
        config_set_improvement(Improvements_List[i].key, config_improvement(Improvements_List[i].key));
    Fixes_Init();
    Improvements_Init(scripted);
    for (int i = 0; i < KEY_ACTION_COUNT; i++)
        config_set_binding(KeyPlay_Actions[i].key, KeyPlay_Keys((KeyAction)i));
    KeyPlay_Init();
    audio_set_user_volumes(config.music_volume / 100.0f,
                           config.effects ? config.effects_volume / 100.0f : 0.0f);
    skip_intro |= config.skip_intro;

    static MenuSwitch fixes[CONFIG_MAX_SWITCHES], improvements[CONFIG_MAX_SWITCHES];
    int n = SDL_min(Fixes_Count, CONFIG_MAX_SWITCHES);
    for (int i = 0; i < n; i++)
        fixes[i] = (MenuSwitch){Fixes_List[i].key, Fixes_List[i].label, Fixes_List[i].help, false};
    int m = SDL_min(Improvements_Count, CONFIG_MAX_SWITCHES);
    for (int i = 0; i < m; i++)
        improvements[i] = (MenuSwitch){Improvements_List[i].key, Improvements_List[i].label,
                                       Improvements_List[i].help, Improvements_NeedsPortSaves(i)};
    static MenuKey keys[KEY_ACTION_COUNT];
    for (int i = 0; i < KEY_ACTION_COUNT; i++)
        keys[i] = (MenuKey){KeyPlay_Actions[i].label, KeyPlay_Keys((KeyAction)i),
                            KeyPlay_Help((KeyAction)i)};
    menu_set_hooks(&(MenuHooks){
        .game_running = Options_GameRunning,
        .changed = menu_changed,
        .add_money = OPTIONS_ADD_MONEY,
        .add_ore = OPTIONS_ADD_ORE,
        .fixes = fixes,
        .fix_count = n,
        .improvements = improvements,
        .improvement_count = m,
        .keys = keys,
        .key_count = KEY_ACTION_COUNT,
        .languages = data_languages,
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

// start.exe's cut-scenes before the game: CREDITS.PRG and INTRO.PRG, or the
// one --cutscene names. False if the game is not to follow (--cutscene, or
// the window was closed).
static bool play_cutscenes(void)
{
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
    return play_game;
}

// start.exe: the cut-scenes, then the game until the player leaves. Returns
// the exit status.
static int play(void)
{
    if (!play_cutscenes())
        return 0;
    int result = 0;
    uint64_t replay_start = timing_frames();
    for (;;) {
        system_exit_handler = on_halt;
        timing_set_quit_handler(on_quit);
        timing_set_restart_handler(on_restart);
        if (setjmp(exit_jump) == 0) {
            boot();
            Game_Main();
            exit_code = 0;
        }
        KeyPlay_Stop();
        if (replay) {
            // The state as the game left it, before any restart or VICTORY.
            uint64_t frames = timing_frames() - replay_start;
            if (autopilot_limit_reached()) {
                printf("replay: no end after %llu frames\n", (unsigned long long)frames);
                result = 1;
            } else {
                printf("replay: exit %d after %llu frames, hash %016llx\n", exit_code,
                       (unsigned long long)frames,
                       (unsigned long long)state_hash());
                result = exit_code == EXIT_QUIT ? 0 : exit_code;
            }
            fflush(stdout);
            break;
        }
        // Soak tests (random autopilot) keep playing after the player quits.
        uint32_t seed;
        if (exit_code == 0 && autopilot_random_seed(&seed)) {
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
    return result;
}

#ifdef __EMSCRIPTEN__
// A page has no window to close: it starts with the click-to-play screen,
// and leaving the game (Exit to DOS, the port menu's Quit, the end of
// VICTORY) or the import screen goes back to it. The click also lets the
// browser play sound from the first cut-scene.
[[noreturn]] static void play_in_page(void)
{
    for (;;) {
        web_click_to_play();
        input_reset();
        if (find_game() || (importscreen_run(false) && find_game()))
            play();
        input_reset();
        audio_shutdown();
    }
}
#endif

int main(int argc, char *argv[])
{
    parse_args(argc, argv);
    if (import_path)
        return import_only();
#ifdef __EMSCRIPTEN__
    // Scripted runs leave the player's files alone.
    if (!replay && !autopilot_init())
        web_open_storage();
#endif
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

#ifdef __EMSCRIPTEN__
    if (!replay && !autopilot_active())
        play_in_page();
#endif
    // No game data: a player chooses their copy; scripted runs fail.
    bool found = find_game();
    if (!found && vid_visible() && !replay && !autopilot_active()) {
        if (!importscreen_run(false)) {
            vid_shutdown();
            SDL_Quit();
            return 0;
        }
        found = find_game();
    }
    if (!found) {
        const char *msg = "The game data of the DOS game was not found. Import your copy with "
                          "--import=PATH (a folder, a ZIP or the CD's ISO), or pass the folder "
                          "holding GRAFIKA, GRWAR, PICS, ... as the first argument or in "
                          "REUNION_DATA.";
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", msg);
        vid_shutdown();
        SDL_Quit();
        return 1;
    }

    int result = play();
    config_save();  // the window's size
    audio_shutdown();
    fs_shutdown();
    vid_shutdown();
    SDL_Quit();
    return result;
}
