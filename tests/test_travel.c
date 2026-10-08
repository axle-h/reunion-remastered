// Travel time (port bug 5): how many game hours a group's flight takes, and
// how many frames a game hour takes in the main loop.
//
// The trips are differential checks (docs/difftest.md): Intel::SetGroupTarget
// (29b9:0e66) and then Sim::ProcessTravellingGroups (1b8a:2a0e) once per
// game hour, each run by the original under Unicorn and by the port from the
// same state, until the fleet arrives. Needs the original game data.
#include <SDL3/SDL.h>
#include <setjmp.h>
#include <stdio.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/gfx.h"
#include "game/groups.h"
#include "game/intel.h"
#include "game/main_program.h"
#include "game/sim.h"
#include "game/system.h"
#include "platform/audio.h"
#include "platform/autopilot.h"
#include "platform/input.h"
#include "platform/timing.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

DIFF_PROC(Intel_SetGroupTarget, Intel_SetGroupTarget((int16_t)a[0], (int16_t)a[1], (int16_t)a[2], (int16_t)a[3]));
DIFF_PROC(Sim_ProcessTravellingGroups, Sim_ProcessTravellingGroups());

static void test_trips(void)
{
    // From New Earth (1, 5, 0) to a moon of the same planet, another planet
    // and another system.
    static const int16_t dest[3][4] = {{1, 1, 5, 1}, {1, 1, 3, 0}, {1, 2, 1, 0}};
    static const uint32_t seeds[] = {1, 12345, 0x7FFFFFFF};
    static const uint16_t exps[] = {0, 10, 40};  // Game::StaffExp[1], the pilot
    if (!diff_boot_new_game(1500)) {
        ok = false;
        return;
    }
    DiffState *base = diff_save();
    for (int si = 0; si < 3; si++) {
        for (int ei = 0; ei < 3; ei++) {
            for (int d = 0; d < 3; d++) {
                diff_load(base);
                ds.Game_GroupCount = 1;
                ds.Game_SelGroup = 0;
                ds.Game_StaffExp[0] = exps[ei];
                *(uint8_t *)ds_at(0x5D48) = 1;  // first hyperspace jump already shown
                *(uint8_t *)ds_at(0x5D71) = 0;
                ds.System_RandSeed = (int32_t)seeds[si];
                TGroup *g = Groups_At(0, 1);
                System_FillChar(g, sizeof *g, 0);
                g->Kind = 4;  // satellite carrier: flies without a pilot
                g->System = 1;
                g->Planet = 5;
                g->Moon = 0;
                g->Status = 2;
                char label[64];
                SDL_snprintf(label, sizeof label, "seed %u exp %u dest %d", (unsigned)seeds[si],
                             exps[ei], d);
                const int32_t args[4] = {dest[d][0], dest[d][1], dest[d][2], dest[d][3]};
                CHECK(diff_call(&diff_Intel_SetGroupTarget, args, label));
                CHECK(g->Status == (d == 2 ? 6 : 4));
                int hours = 0;
                while (g->Status != 2 && hours < 1000) {
                    char hl[96];
                    SDL_snprintf(hl, sizeof hl, "%s hour %d", label, hours);
                    if (!diff_call(&diff_Sim_ProcessTravellingGroups, nullptr, hl)) {
                        ok = false;
                        break;
                    }
                    hours++;
                }
                printf("%s: %d hours\n", label, hours);
                CHECK(hours > 0 && hours < 1000);
            }
        }
    }
    diff_free(base);
}

// --- The main loop's clock: one game hour every 101 frames --------------------------

static jmp_buf out;

[[noreturn]] static void on_halt(uint8_t) { longjmp(out, 1); }
[[noreturn]] static void on_quit(void) { longjmp(out, 1); }

// New game, then the command centre until `quit_frame`; game hours since 2927-8-13 0:00.
static int hours_after(int quit_frame)
{
    char script[128];
    snprintf(script, sizeof script, "40:click:160,60;120:click:80,100;%d:quit", quit_frame);
    audio_shutdown();
    input_reset();
    autopilot_set_script(script);
    if (setjmp(out) == 0) {
        mem_init();
        if (!dseg_init())
            return -1;
        system_init();
        Game_InitUnits();
        system_add_exit_proc(far_ptr(0x405F, 0x1A62), Gfx_ExitHandler);
        Game_Main();
    }
    return ((ds.Game_Year - 2927) * 360 + (ds.Game_Month - 8) * 30 + ds.Game_Day - 13) * 24
           + ds.Game_Hour;
}

static void test_clock(void)
{
    system_exit_handler = on_halt;
    timing_set_quit_handler(on_quit);
    int a = hours_after(1500);
    int b = hours_after(1500 + 10 * 101);
    int c = hours_after(1500 + 10 * 101 + 1);
    printf("hours: %d, +1010 frames %d, +1011 frames %d\n", a, b, c);
    CHECK(a > 23);  // the clock has started
    CHECK(b - a == 10);
    CHECK(c - b == 0 || c - b == 1);
}

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    test_trips();
    test_clock();
    if (diff_finish() != 0)
        ok = false;
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
