// Golden state hashes (docs/native-memory-plan.md, Stage 0): plays a script
// and compares the hash of the game's state (game/state.h) every N retraces
// with tests/golden/<case>.txt. The state stream doesn't depend on the
// memory layout, so these hold across changes to the memory model; a
// mismatch means the game behaves differently.
//
//   test_golden DATA_DIR CASE        compare (exit 0 / 1, 77 without data)
//   GOLDEN_WRITE=1 test_golden ...   write the golden file instead
//   GOLDEN_DUMP=FRAME GOLDEN_DUMP_DIR=DIR test_golden ...
//                                    also save the stream at FRAME as
//                                    DIR/CASE-FRAME.state (FRAME "all":
//                                    at every sample)
//   test_golden compare A.state B.state
//                                    names the first difference of two
//                                    dumps (e.g. from two builds)
//   test_golden DATA_DIR write-saves writes the load cases' saves
//                                    (tests/golden/saves/) from the proof
#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdio.h>

#include "cutscene/cutscene.h"
#include "game/fixes.h"
#include "game/saveload.h"
#include "game/state.h"
#include "platform/autopilot.h"
#include "platform/config.h"
#include "platform/fs.h"
#include "platform/input.h"
#include "platform/timing.h"
#include "play/play.h"

#ifndef REUNION_SOURCE_DIR
#define REUNION_SOURCE_DIR "."
#endif

// A new game: the start menu's New game, then the command centre.
#define NEW_GAME "40:click:160,60;120:click:80,100"

typedef struct {
    const char *name;
    const char *script;     // autopilot script; "@file" is relative to the source dir
    int interval;           // retraces between samples
    bool original;          // every fix off (the port menu's All off)
    const char *cutscene;   // plays this cut-scene instead of the game
    const char *load;       // loads tests/golden/saves/<load>.sav ...
    int load_at;            // ... at this frame
} GoldenCase;

// After loading a save on the command centre: each menu-bar screen, then
// time goes by.
#define LOAD_PLAY                                                                                 \
    ";1100:click:24,16;1500:click:24,16;1800:click:72,16;2200:click:24,16;2500:click:120,16;"    \
    "2900:click:24,16;3200:click:168,16;3600:click:24,16;3900:click:216,16;4300:click:24,16;"    \
    "4600:click:264,16;5000:click:24,16;8000:quit"

static const GoldenCase cases[] = {
    // The command centre's menu bar (y=16: x=24 shop, 72 research, 120 ship
    // info, 168 system view, 216 surface, 264 message log), each screen left
    // by its Back button (x=24).
    {.name = "boot",
     .script = "seed:1;" NEW_GAME ";1000:click:24,16;1400:click:24,16;1700:click:72,16;2100:click:24,16;"
               "2400:click:120,16;2800:click:24,16;3100:click:168,16;3500:click:24,16;"
               "3800:click:216,16;4200:click:24,16;4500:click:264,16;4900:click:24,16;6300:quit",
     .interval = 100},
    {.name = "soak1", .script = NEW_GAME ";random:1:30000:1000", .interval = 500},
    {.name = "soak2", .script = NEW_GAME ";random:2:30000:1000", .interval = 500},
    {.name = "soak3", .script = NEW_GAME ";random:3:30000:1000", .interval = 500},
    {.name = "soak4", .script = NEW_GAME ";random:4:30000:1000", .interval = 500},
    {.name = "soak5", .script = "random:5:30000", .interval = 500},
    {.name = "soak1_original", .script = NEW_GAME ";random:1:30000:1000", .interval = 500,
     .original = true},
    {.name = "credits", .script = "", .interval = 100, .cutscene = "credits"},
    {.name = "intro", .script = "", .interval = 100, .cutscene = "intro"},
    {.name = "victory", .script = "", .interval = 100, .cutscene = "victory"},
    // Saves taken from the proof at four of its checkpoints (write-saves),
    // loaded into a new game.
    {.name = "load_J01", .script = "seed:1;" NEW_GAME LOAD_PLAY, .interval = 100,
     .load = "J01_contact", .load_at = 1000},
    {.name = "load_M01", .script = "seed:1;" NEW_GAME LOAD_PLAY, .interval = 100,
     .load = "M01_wave1", .load_at = 1000},
    {.name = "load_L01", .script = "seed:1;" NEW_GAME LOAD_PLAY, .interval = 100,
     .load = "L01_pilot", .load_at = 1000},
    {.name = "load_X01", .script = "seed:1;" NEW_GAME LOAD_PLAY, .interval = 100,
     .load = "X01_rigel1", .load_at = 1000},
    // Release builds only (CMakeLists.txt): the playthrough's proof.
    {.name = "proof", .script = "@tests/play/proof_12345.txt", .interval = 10000},
    {.name = "proof_original", .script = "@tests/play/proof_12345.txt", .interval = 10000,
     .original = true},
};

// A run that hasn't ended this long after the script's last event stops.
enum : uint64_t { MARGIN = 100000 };

static const GoldenCase *cur;
static char *out;              // lines written by the run
static size_t out_len, out_cap;
static uint64_t stop_at, dump_at;
static bool load_failed;
static bool dump_every;
static const char *dump_dir;

[[gnu::format(printf, 1, 2)]] static void outf(const char *fmt, ...)
{
    char line[256];
    va_list ap;
    va_start(ap, fmt);
    int n = SDL_vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (out_len + (size_t)n + 1 > out_cap) {
        out_cap = out_cap ? out_cap * 2 : 1 << 16;
        out = SDL_realloc(out, out_cap);
    }
    SDL_memcpy(out + out_len, line, (size_t)n + 1);
    out_len += (size_t)n;
}

static void dump_state(uint64_t frame)
{
    static StateBuf b;
    state_serialize(&b);
    char path[1024];
    SDL_snprintf(path, sizeof path, "%s/%s-%llu.state", dump_dir, cur->name, (unsigned long long)frame);
    if (state_save(&b, path))
        printf("dumped %s\n", path);
    else
        printf("can't write %s\n", path);
}

static void save_path(char *path, size_t size, const char *name)
{
    SDL_snprintf(path, size, "%s/tests/golden/saves/%s.sav", REUNION_SOURCE_DIR, name);
}

// Loads the case's save, then saves it again: the bytes must not change.
static void load_save(void)
{
    char path[1024], again[1024];
    save_path(path, sizeof path, cur->load);
    SDL_snprintf(again, sizeof again, "golden-saves-%s/again.sav", cur->name);
    uint8_t name[0x14];
    size_t a_size = 0, b_size = 0;
    void *a = nullptr, *b = nullptr;
    bool ok = SaveLoad_TransferFile(path, name, false) && SaveLoad_TransferFile(again, name, true)
              && (a = SDL_LoadFile(path, &a_size)) && (b = SDL_LoadFile(again, &b_size));
    if (!ok) {
        printf("%s: can't load %s or save it again\n", cur->name, path);
    } else if (a_size != b_size || SDL_memcmp(a, b, a_size) != 0) {
        printf("%s: saving %s again gives different bytes\n", cur->name, path);
        ok = false;
    }
    load_failed |= !ok;
    SDL_free(a);
    SDL_free(b);
}

static void sample(uint64_t frame)
{
    if (cur->load && frame == (uint64_t)cur->load_at)
        load_save();
    if (dump_dir && frame == dump_at)
        dump_state(frame);
    if (frame % (uint64_t)cur->interval == 0 && dump_dir && dump_every)
        dump_state(frame);
    if (frame % (uint64_t)cur->interval == 0)
        outf("%llu %016llx\n", (unsigned long long)frame, (unsigned long long)state_hash());
    if (stop_at && frame >= stop_at)
        input_request_quit();
}

static void clear_dir(const char *dir)
{
    int n = 0;
    char **names = SDL_GlobDirectory(dir, nullptr, 0, &n);
    for (int i = 0; i < n; i++) {
        char path[1024];
        SDL_snprintf(path, sizeof path, "%s/%s", dir, names[i]);
        SDL_RemovePath(path);
    }
    SDL_free(names);
    SDL_CreateDirectory(dir);
}

static void run_case(const GoldenCase *c)
{
    cur = c;
    // The run applies the config's fixes when it boots (play.c).
    for (int i = 0; i < Fixes_Count; i++)
        config_set_fix(Fixes_List[i].key, !c->original);
    // The golden files describe the game without the colony stats, which
    // add a hotspot to the planet info screen.
    config.colony_stats = false;
    char script[1100];
    if (c->script[0] == '@')
        SDL_snprintf(script, sizeof script, "@%s/%s", REUNION_SOURCE_DIR, c->script + 1);
    else
        SDL_strlcpy(script, c->script, sizeof script);

    if (c->cutscene) {
        Cutscene which;
        cutscene_from_name(c->cutscene, &which);
        input_reset();
        autopilot_set_script(script[0] ? script : "seed:1");
        autopilot_set_controller(sample);
        uint64_t start = timing_frames();
        cutscene_play(which);
        autopilot_set_controller(nullptr);
        outf("end after %llu frames, hash %016llx\n",
                (unsigned long long)(timing_frames() - start), (unsigned long long)state_hash());
        autopilot_set_script(nullptr);
        return;
    }

    // Saves go to an empty directory, so no run sees another's.
    char saves[1024];
    SDL_snprintf(saves, sizeof saves, "golden-saves-%s", c->name);
    clear_dir(saves);
    SaveLoad_SetSaveDir(saves);
    // The last event's frame, from the script itself.
    autopilot_set_script(script);
    stop_at = autopilot_last_event() + MARGIN;
    autopilot_set_script(nullptr);
    PlayResult r = play_replay_watch(script, sample);
    outf("end: exit %d after %llu frames, hash %016llx\n", r.exit_code,
            (unsigned long long)r.frames, (unsigned long long)r.state_hash);
    SaveLoad_SetSaveDir(nullptr);
}

// --- compare mode -----------------------------------------------------------

static int compare(const char *a_path, const char *b_path)
{
    StateBuf a = {}, b = {};
    if (!state_load(&a, a_path) || !state_load(&b, b_path)) {
        printf("can't read %s or %s\n", a_path, b_path);
        return 1;
    }
    size_t at = 0;
    int shown = 0;
    // Each differing run of bytes, by name.
    while ((at = state_first_difference(&a, &b)) != SIZE_MAX && shown < 40) {
        char an[128], bn[128];
        state_describe(&a, at, an, sizeof an);
        state_describe(&b, at, bn, sizeof bn);
        if (at >= a.len || at >= b.len) {
            printf("lengths differ: %zu | %zu (at %s | %s)\n", a.len, b.len, an, bn);
            return 1;
        }
        printf("%8zu %-40s %02X | %02X%s%s\n", at, an, a.data[at], b.data[at],
               SDL_strcmp(an, bn) ? "  (B: " : "", SDL_strcmp(an, bn) ? bn : "");
        shown++;
        // Skip to the next part.
        size_t next = at + 1;
        for (size_t i = 0; i < a.nparts; i++)
            if (at >= a.parts[i].start && at < a.parts[i].start + a.parts[i].size)
                next = a.parts[i].start + a.parts[i].size;
        if (next >= a.len || next >= b.len)
            break;
        // Compare from there on by pretending the earlier bytes are equal.
        SDL_memcpy(b.data, a.data, next);
    }
    if (!shown)
        printf("same\n");
    return shown ? 1 : 0;
}

// --- write-saves ---------------------------------------------------------------

// The proof's checkpoints the load cases start from.
static const char *const save_points[] = {"J01_contact", "M01_wave1", "L01_pilot", "X01_rigel1"};
static uint64_t save_frames[SDL_arraysize(save_points)];

static void save_watch(uint64_t frame)
{
    for (size_t i = 0; i < SDL_arraysize(save_points); i++) {
        if (frame != save_frames[i])
            continue;
        char path[1024];
        save_path(path, sizeof path, save_points[i]);
        uint8_t name[0x14] = {};
        name[0] = (uint8_t)SDL_snprintf((char *)name + 1, sizeof name - 1, "%s", save_points[i]);
        printf("%s %s\n", SaveLoad_TransferFile(path, name, true) ? "wrote" : "can't write", path);
    }
}

// Saves the game at the proof's save points (frames from its checkpoint
// lines) as tests/golden/saves/<checkpoint>.sav.
static int write_saves(void)
{
    char path[1024];
    SDL_snprintf(path, sizeof path, "%s/tests/play/proof_12345.txt", REUNION_SOURCE_DIR);
    char *text = SDL_LoadFile(path, nullptr);
    if (!text)
        return 1;
    for (size_t i = 0; i < SDL_arraysize(save_points); i++) {
        char key[96];
        SDL_snprintf(key, sizeof key, "# checkpoint %s at frame ", save_points[i]);
        const char *at = SDL_strstr(text, key);
        if (!at)
            return 1;
        save_frames[i] = SDL_strtoull(at + SDL_strlen(key), nullptr, 10);
    }
    SDL_free(text);
    char dir[1024];
    SDL_snprintf(dir, sizeof dir, "%s/tests/golden/saves", REUNION_SOURCE_DIR);
    SDL_CreateDirectory(dir);
    char script[1100];
    SDL_snprintf(script, sizeof script, "@%s", path);
    play_replay_watch(script, save_watch);
    return 0;
}

// --- main -------------------------------------------------------------------

static bool same_files(const char *expect_path, const char *got, const GoldenCase *c)
{
    char *expect = SDL_LoadFile(expect_path, nullptr);
    if (!expect) {
        printf("%s: no golden file %s (GOLDEN_WRITE=1 writes it)\n", c->name, expect_path);
        return false;
    }
    // Skip the comment line.
    const char *e = SDL_strchr(expect, '\n');
    e = e ? e + 1 : expect;
    const char *g = got;
    int line = 2;
    bool ok = true;
    while (*e || *g) {
        const char *ee = SDL_strchr(e, '\n'), *ge = SDL_strchr(g, '\n');
        size_t el = ee ? (size_t)(ee - e) : SDL_strlen(e), gl = ge ? (size_t)(ge - g) : SDL_strlen(g);
        if (el != gl || SDL_memcmp(e, g, el) != 0) {
            printf("%s: first difference at line %d\n  golden: %.*s\n  now:    %.*s\n", c->name, line,
                   (int)el, e, (int)gl, g);
            ok = false;
            break;
        }
        e += el + (ee != nullptr);
        g += gl + (ge != nullptr);
        line++;
    }
    SDL_free(expect);
    return ok;
}

int main(int argc, char *argv[])
{
    if (argc == 4 && SDL_strcmp(argv[1], "compare") == 0)
        return compare(argv[2], argv[3]);
    if (argc < 3) {
        printf("usage: test_golden DATA_DIR CASE | compare A.state B.state\n");
        return 2;
    }
    const GoldenCase *c = nullptr;
    for (size_t i = 0; i < SDL_arraysize(cases); i++)
        if (SDL_strcmp(cases[i].name, argv[2]) == 0)
            c = &cases[i];
    if (!c && SDL_strcmp(argv[2], "write-saves") != 0) {
        printf("no case %s\n", argv[2]);
        return 2;
    }
    if (!fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_EVENTS)) {
        printf("SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (argc == 3 && SDL_strcmp(argv[2], "write-saves") == 0)
        return write_saves();
    const char *d = SDL_getenv("GOLDEN_DUMP");
    dump_dir = SDL_getenv("GOLDEN_DUMP_DIR");
    if (d && *d && dump_dir && *dump_dir) {
        dump_every = SDL_strcmp(d, "all") == 0;
        dump_at = SDL_strtoull(d, nullptr, 10);
    }
    else
        dump_dir = nullptr;

    outf("%s", "");
    uint64_t t0 = SDL_GetTicksNS();
    run_case(c);
    printf("%s: %.1f s\n", c->name, (double)(SDL_GetTicksNS() - t0) / 1e9);

    char path[1024];
    SDL_snprintf(path, sizeof path, "%s/tests/golden/%s.txt", REUNION_SOURCE_DIR, c->name);
    bool ok;
    const char *w = SDL_getenv("GOLDEN_WRITE");
    if (w && *w && *w != '0') {
        FILE *f = fopen(path, "w");
        ok = f != nullptr;
        if (f) {
            fprintf(f, "# %s: state hash every %d retraces%s (tests/test_golden.c)\n", c->name,
                    c->interval, c->original ? ", fixes off" : "");
            fputs(out, f);
            fclose(f);
            printf("wrote %s\n", path);
        }
    } else {
        ok = same_files(path, out, c);
    }
    ok &= !load_failed;
    SDL_free(out);
    SDL_Quit();
    printf("%s\n", ok ? "PASSED" : "FAILED");
    return ok ? 0 : 1;
}
