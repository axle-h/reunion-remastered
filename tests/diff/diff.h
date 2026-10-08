// Differential tests: run an ORIGINAL routine of REUNION.PRG under Unicorn and
// the port's C version on the same memory, then compare all of memory, the
// result registers, the VGA palette, the EMS pages and the keys consumed.
// See docs/difftest.md.
//
// The emulated CPU works directly on the port's mem[] (same layout as on DOS:
// the program is loaded at MEM_LOAD_SEG with its relocations applied), so a
// port state is an original state. A check runs the original first, keeps
// its memory, rolls everything back, runs the port and compares.
#pragma once

#include <stdint.h>

#include "game/mem.h"

// --- Set-up ------------------------------------------------------------------

// Opens the game data (argv[1]) and loads REUNION.PRG. false: data missing,
// the test should exit 77.
bool diff_init(int argc, char *argv[]);
// Prints the summary; returns the exit code (0 all matched, 1 otherwise).
int diff_finish(void);
// Cases per function: REUNION_DIFF_CASES if set, else `dflt`.
int diff_cases(int dflt);

// --- Game states ---------------------------------------------------------------

// Everything the port's routines read or write: mem[], the EMS driver's
// pages and the VGA palette.
typedef struct DiffState DiffState;
DiffState *diff_save(void);
void diff_load(const DiffState *s);
void diff_free(DiffState *s);

// Boots the port into a new game (start menu, "new game", first hero) and
// runs the main loop for `frames` retraces in total; then diff_quiesce().
// The state is left in mem[]. false if the game halted or failed to boot.
bool diff_boot_new_game(int frames);
// The same with more autopilot events after "new game" (platform/autopilot.h;
// the command centre is up by frame 900), ending in FRAME:quit; e.g.
// "900:click:72,16;1100:quit" opens the research screen.
bool diff_boot(const char *then);
// Makes a state fit for comparison: sound effects off (the port's sound is
// not memory-faithful), input settled (mouse at DS Mouse_X/Y, no buttons,
// one Mouse_Update so the polling the port adds in wait loops is a no-op).
void diff_quiesce(void);

// --- Deterministic random inputs -----------------------------------------------

typedef struct DiffRng {
    uint64_t s;
} DiffRng;

uint32_t diff_rand(DiffRng *r, uint32_t n);          // 0..n-1 (n > 0)
int32_t diff_range(DiffRng *r, int32_t lo, int32_t hi); // lo..hi inclusive
bool diff_chance(DiffRng *r, int percent);
// One of the n values in `v`.
#define diff_pick(r, ...) \
    ((const int32_t[]){__VA_ARGS__}[diff_rand((r), sizeof((const int32_t[]){__VA_ARGS__}) / sizeof(int32_t))])

// --- Routines under test ---------------------------------------------------------

// A linear address range [lo, hi) left out of the comparison.
typedef struct DiffRange {
    uint32_t lo, hi;
} DiffRange;
#define DIFF_DS(ofs, len) {(uint32_t)MEM_DS_SEG * 16 + (ofs), (uint32_t)MEM_DS_SEG * 16 + (ofs) + (len)}
#define DIFF_LINEAR(lo, hi) {(lo), (hi)}

typedef struct DiffFn {
    // symbols.txt name ("Sim::AdvanceHour"), C name ("Sim_AdvanceHour": the
    // first '_' becomes "::") or Ghidra address ("1b8a:0000"). The prototype
    // (arguments, result, near if its comment says "near" or "nested") comes
    // from symbols.txt.
    const char *name;
    // Calls the port's routine with the arguments a[0], a[1], ... in
    // prototype order (far pointers as FarPtr values); returns AL/AX/DX:AX.
    uint32_t (*port)(const int32_t *a);
    // Extra ranges not compared, terminated by {0, 0}; may be null.
    const DiffRange *ignore;
    // Forces a near call when symbols.txt does not say so.
    bool near;
    // Prototype to use instead of symbols.txt's (which has none or a wrong
    // one): the text after the name, e.g. "(src: pointer; tile: word; x, y: integer)".
    const char *decl;
} DiffFn;

// `static DiffFn diff_<cname>` for a procedure: `call` is the port's call,
// e.g. DIFF_PROC(Intel_SetGroupTarget, Intel_SetGroupTarget(a[0], a[1], a[2], a[3])).
#define DIFF_PROC(cname, call)                                              \
    static uint32_t diff_port_##cname(const int32_t *a)                    \
    {                                                                       \
        (void)a;                                                            \
        call;                                                               \
        return 0;                                                           \
    }                                                                       \
    static DiffFn diff_##cname = {#cname, diff_port_##cname, nullptr, false, nullptr}
// The same for a function: the call's value is the result.
#define DIFF_FUNC(cname, call)                                              \
    static uint32_t diff_port_##cname(const int32_t *a)                    \
    {                                                                       \
        (void)a;                                                            \
        return (uint32_t)(call);                                            \
    }                                                                       \
    static DiffFn diff_##cname = {#cname, diff_port_##cname, nullptr, false, nullptr}

// Runs the original and the port on the current state with `args`, and
// compares. Prints the differences under `label` and returns false on a
// mismatch. Afterwards mem[] holds the port's result.
bool diff_call(const DiffFn *fn, const int32_t *args, const char *label);

// Perturbs the state in mem[] (and fills the arguments) for one case.
typedef void (*DiffSetup)(DiffRng *rng, int32_t *args);

// n cases: case k loads bases[k % nbases], runs setup with a generator seeded
// from the routine's name and k, then diff_call. Returns the number of
// mismatches. REUNION_DIFF_CASE=k runs only case k (to reproduce one);
// REUNION_DIFF_ONLY=text skips routines whose name doesn't contain text.
int diff_run(const DiffFn *fn, DiffState *const *bases, int nbases, DiffSetup setup, int n);

// The keys queued for both sides of the next checks instead of the 32
// Enters (13; 0 then a scan code for an extended key; at most 64); n < 0
// restores the Enters. diff_run restores them before each case's setup.
void diff_set_keys(const int *keys, int n);

// Runs only the ORIGINAL routine on the current state and keeps what it
// leaves in mem[] (no port run, no comparison): for setting up a state the
// port does not model. false (with a message) unless it returned.
bool diff_run_original(const char *name, const int32_t *args);
// Opens Output as the RTL's InitSystem does (Assign(Output, ''); Rewrite),
// which the port never does: without it the original's Write(Output) in the
// exit paths is run-time error 105. Call after diff_boot, before diff_save.
bool diff_open_output(void);

// --- Reports -----------------------------------------------------------------------

// Names a block outside DS in reports, e.g. ("Game_Groups^", ds.Game_Groups,
// 64 * sizeof(TGroup), "TGroup"): offsets then print as Name[i].Field. The
// game data block (Groups, Buildings) is named by default. record may be null.
void diff_name_region(const char *label, FarPtr base, uint32_t size, const char *record);
// Writes the name of linear address `lin` to buf (e.g. "DS:95A6 Game_StaffExp[1]").
void diff_describe(uint32_t lin, char *buf, int size);

// Port I/O the original may do in the routines under test without effect on
// what is compared (e.g. a sound card's ports), on top of the VGA ports the
// harness emulates. Any other port stops the run as an error.
void diff_allow_port(uint16_t port);

// A DS variable holding a far procedure pointer. The port stores those in
// Ghidra numbering (far_ptr(0x4758, 0x012D)); around the original's run the
// harness converts them to the loaded segment and back. The procedure
// variables the port sets are registered already (diff.c).
void diff_code_ptr(uint16_t ds_offset);

// A scratch buffer in otherwise unused low memory (0060:0000, 4 KB), for
// var parameters and string results.
enum : uint32_t { DIFF_SCRATCH = 0x00600000u, DIFF_SCRATCH_SIZE = 0x1000 };
