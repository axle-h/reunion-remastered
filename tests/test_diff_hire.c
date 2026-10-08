// Differential test of unit Hire (3fab): the staff hiring screen, original
// (under Unicorn) against the port, from the hire screen of a new game with
// the staff, credits and the screen's state perturbed. See docs/difftest.md.
#include <stdio.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/hire.h"

DIFF_PROC(Hire_HireScreen, Hire_HireScreen());
DIFF_PROC(Hire_LoadHireBackground, Hire_LoadHireBackground());
DIFF_PROC(Hire_DrawCandidateFrames, Hire_DrawCandidateFrames());
DIFF_PROC(Hire_DrawCandidateInfo, Hire_DrawCandidateInfo());
DIFF_PROC(Hire_SetupHireHotspots, Hire_SetupHireHotspots());

// Unnamed screen state (hire.h).
#define CANDIDATE (*(int16_t *)ds_at(0x899C))
#define KIND (*(int16_t *)ds_at(0x899E))
#define INFO_SHOWN (*(uint8_t *)ds_at(0x89A0))
#define KIND_5D9E (*(int16_t *)ds_at(0x5D9E))

// Staff, credits and the screen's selection.
static void setup_staff(DiffRng *r, int32_t *a)
{
    (void)a;
    KIND = (int16_t)diff_range(r, 1, 4);
    CANDIDATE = (int16_t)diff_pick(r, 0, 1, 2, 3, 3);
    INFO_SHOWN = (uint8_t)diff_range(r, 0, 1);
    for (int k = 0; k < 4; k++) {
        ds.Game_StaffHired[k] = (uint16_t)diff_pick(r, 0, 0, 1, 2, 3);
        ds.Game_StaffExp[k] = (uint16_t)diff_pick(r, 0, 10, 35, diff_range(r, 0, 99));
        ds.Game_DevSkill[k] = (uint16_t)diff_range(r, 0, 6);
        for (int c = 0; c < 3; c++) {
            if (diff_chance(r, 30))
                ds.Staff_Level[k][c] = (int16_t)diff_range(r, 0, 99);
            ds.Staff_DevSkills[0][c][k] = (uint8_t)diff_range(r, 0, 6);
        }
    }
    ds.Game_Credits = diff_pick(r, 0, 4500, 4499, 25000, 120000, 1000000, diff_range(r, 0, 200000));
    KIND_5D9E = (int16_t)diff_pick(r, 0, 0, 1, 4, KIND);
}

static void setup_screen(DiffRng *r, int32_t *a)
{
    setup_staff(r, a);
    ds.Game_ScreenEntered = (uint8_t)diff_chance(r, 15);
    // Kind buttons 32h..35h, the hire button 28h, others.
    ds.Game_MenuCommand = (int16_t)diff_pick(r, 0, 0x28, 0x28, 0x28, 0x32, 0x33, 0x34, 0x35, 0x31, 0x36,
                                             diff_range(r, 0x32, 0x35));
    if (diff_chance(r, 30))
        ds.Game_MenuCommand = (int16_t)(0x31 + KIND);  // the kind already shown
    // Candidate frames 15h..17h, outside them.
    ds.Game_HotspotHit = (int16_t)diff_pick(r, 0, 0, 0x14, 0x15, 0x16, 0x17, 0x18, 3);
}

// The hire button (28h) on a picked candidate: every outcome of the hire.
static void setup_hire(DiffRng *r, int32_t *a)
{
    setup_staff(r, a);
    ds.Game_ScreenEntered = 0;
    ds.Game_MenuCommand = 0x28;
    ds.Game_HotspotHit = 0;
    CANDIDATE = (int16_t)diff_pick(r, 0, 1, 2, 3, 3, 3);
    if (diff_chance(r, 50))
        ds.Game_Credits = ds.Staff_Price[KIND - 1][CANDIDATE > 0 ? CANDIDATE - 1 : 0] + diff_pick(r, -1, 0, 0, 1);
}

static const struct {
    const char *name;
    const char *script;
} states[] = {
    {"hire (2)", "900:click:175,70;1400:quit"},
    {"hire (2), a candidate picked", "900:click:175,70;1400:click:150,100;1700:quit"},
};
enum { STATES = sizeof states / sizeof states[0] };

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    DiffState *bases[STATES];
    for (int i = 0; i < STATES; i++) {
        if (!diff_boot(states[i].script) || ds.Game_CurrentScreen != 2) {
            printf("FAILED to reach %s\n", states[i].name);
            return 1;
        }
        bases[i] = diff_save();
    }

    const struct {
        DiffFn *fn;
        DiffSetup setup;
        int cases;
    } fns[] = {
        {&diff_Hire_SetupHireHotspots, setup_staff, 8},
        {&diff_Hire_DrawCandidateFrames, setup_staff, 12},
        {&diff_Hire_DrawCandidateInfo, setup_staff, 40},
        {&diff_Hire_LoadHireBackground, setup_staff, 8},
        {&diff_Hire_HireScreen, setup_screen, 80},
        {&diff_Hire_HireScreen, setup_hire, 60},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, bases, STATES, fns[i].setup, diff_cases(fns[i].cases));
    for (int i = 0; i < STATES; i++)
        diff_free(bases[i]);
    return diff_finish();
}
