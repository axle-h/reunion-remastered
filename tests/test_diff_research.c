// Differential test of unit Research (2d66): the research and design screen,
// original (under Unicorn) against the port, from the research screen and
// other screens of a new game with the inventions, the developer and the
// screen's state perturbed. See docs/difftest.md. Needs the game data.
#include <stdio.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/research.h"

DIFF_PROC(Research_ResearchScreen, Research_ResearchScreen());
DIFF_PROC(Research_SetupResearchHotspots, Research_SetupResearchHotspots());
DIFF_PROC(Research_DrawInventionInfo, Research_DrawInventionInfo());
DIFF_PROC(Research_DrawDeveloperSkills, Research_DrawDeveloperSkills());
DIFF_PROC(Research_AnimateSelectionFrame, Research_AnimateSelectionFrame());
DIFF_PROC(Research_ResetSelectionFrame, Research_ResetSelectionFrame());
DIFF_PROC(Research_DrawInventionIcons, Research_DrawInventionIcons());
// The original's order: DrawIconFrame(seg, dest, frame), Play*(inv, kind).
DIFF_PROC(Research_DrawIconFrame, Research_DrawIconFrame((uint8_t)a[2], (uint16_t)a[1], (uint16_t)a[0]));
DIFF_PROC(Research_AnimateCurrentResearch, Research_AnimateCurrentResearch());
DIFF_PROC(Research_PlayIconTransitionOut, Research_PlayIconTransitionOut((int16_t)a[1], (int16_t)a[0]));
DIFF_PROC(Research_PlayIconTransitionIn, Research_PlayIconTransitionIn((int16_t)a[1], (int16_t)a[0]));

// Unnamed globals (research.c).
#define FRAME_BLINK (*(int16_t *)ds_at(0x78AA))
#define WORK_FRAME (*(int16_t *)ds_at(0x78B0))
#define FLAG_5D52 (*(uint16_t *)ds_at(0x5D52))
#define FLAG_5D62 (*(uint8_t *)ds_at(0x5D62))
#define FLAG_5D9E (*(int16_t *)ds_at(0x5D9E))

// Inventions, the developer and the screen's selection.
static void setup_research(DiffRng *r, int32_t *a)
{
    (void)a;
    for (int k = 0; k < 35; k++) {
        TProduct *p = &ds.Research_Products[k];
        if (diff_chance(r, 40))
            p->State = (int16_t)diff_pick(r, 0, 1, 2, 3, 4, 5, 5);
        p->Progress = (int16_t)diff_pick(r, 0, 1, 99, 100, 5000, 9999, 10000, diff_range(r, 0, 10000));
        if (diff_chance(r, 30))
            for (int j = 0; j < 4; j++)
                p->Skills[j] = (uint8_t)diff_pick(r, 0, 1, 3, 6, 9, diff_range(r, 0, 120));
    }
    for (int j = 0; j < 4; j++)
        ds.Game_DevSkill[j] = (uint16_t)diff_pick(r, 0, 3, 6, diff_range(r, 0, 120));
    ds.Game_StaffExp[3] = (uint16_t)diff_pick(r, 0, 10, 35);
    ds.Game_StaffHired[3] = (uint16_t)diff_pick(r, 0, 1, 2, 3);
    ds.Game_SelInvention = (int16_t)diff_pick(r, 0, 0, 1, 14, 14, 35, diff_range(r, 1, 35));
    ds.Game_CurResearch = (int16_t)diff_pick(r, 0, 0, 1, 14, 35, diff_range(r, 1, 35));
    if (ds.Game_CurResearch > 0 && diff_chance(r, 70))
        ds.Research_Products[ds.Game_CurResearch - 1].State = (int16_t)diff_pick(r, 2, 4);
    FLAG_5D52 = (uint16_t)diff_pick(r, 0, 0, 0, 5);
    FLAG_5D62 = (uint8_t)diff_pick(r, 0, 1);
    FLAG_5D9E = (int16_t)diff_pick(r, 0, 0, 1, 4);
    FRAME_BLINK = (int16_t)diff_pick(r, 0, 1, 0x14, 0x15, 0x28, diff_range(r, 0, 0x28));
    WORK_FRAME = (int16_t)diff_pick(r, 0, 4, 5, 0x2C, 0x2C, diff_range(r, 0, 0x2C));
}

static void setup_screen(DiffRng *r, int32_t *a)
{
    setup_research(r, a);
    ds.Game_ScreenEntered = (uint8_t)diff_chance(r, 15);
    ds.Game_PrevScreen = (int16_t)diff_pick(r, 1, 3, 5);
    ds.Shop_Sel = (int16_t)diff_range(r, 1, 10);
    // Invention hotspots 15h..37h (most), the menu bar, nothing.
    ds.Game_HotspotHit = (int16_t)diff_pick(r, 0, 3, 0x14, 0x15, 0x22, 0x37, diff_range(r, 0x15, 0x37),
                                            diff_range(r, 0x15, 0x37), diff_range(r, 0x15, 0x37));
    if (diff_chance(r, 30) && ds.Game_CurResearch > 0)  // click the running project
        ds.Game_HotspotHit = (int16_t)(ds.Game_CurResearch + 0x14);
    if (ds.Game_HotspotHit > 0x14 && ds.Game_HotspotHit <= 0x37 && diff_chance(r, 60)) {
        TProduct *p = &ds.Research_Products[ds.Game_HotspotHit - 0x15];
        if (p->State == 0)
            p->State = (int16_t)diff_range(r, 1, 5);
    }
}

static void setup_icon_frame(DiffRng *r, int32_t *a)
{
    setup_research(r, a);
    a[0] = ds.Gfx_ScreenSeg;
    a[1] = diff_pick(r, 0, 0x4100, 0x4120, 0x9100, diff_range(r, 0, 0xE000));
    a[2] = diff_pick(r, 0, 10, 0x13, 0x1E, 0x27, 0x28, diff_range(r, 0, 0x30));
}

static void setup_transition(DiffRng *r, int32_t *a)
{
    setup_research(r, a);
    a[0] = diff_pick(r, 1, 5, 14, 35, diff_range(r, 1, 35), ds.Game_CurResearch > 0 ? ds.Game_CurResearch : 2);
    a[1] = diff_range(r, 1, 4);
}

// Base states: the research screen (its CDS picture loaded), and others.
static const struct {
    const char *name;
    const char *script;
} states[] = {
    {"research (3)", "900:click:72,16;1300:quit"},
    {"research (3), later", "900:click:72,16;1300:click:40,60;1700:quit"},
    {"command centre", "1500:quit"},
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
        if (!diff_boot(states[i].script)) {
            printf("FAILED to reach %s\n", states[i].name);
            return 1;
        }
        bases[i] = diff_save();
    }

    // The research screen states only, for the routines drawing from its CDS picture.
    const struct {
        DiffFn *fn;
        DiffSetup setup;
        int cases;
        int nbases;
    } fns[] = {
        {&diff_Research_SetupResearchHotspots, setup_research, 12, STATES},
        {&diff_Research_DrawInventionInfo, setup_research, 60, STATES},
        {&diff_Research_DrawDeveloperSkills, setup_research, 30, 2},
        {&diff_Research_AnimateSelectionFrame, setup_research, 40, STATES},
        {&diff_Research_ResetSelectionFrame, setup_research, 30, STATES},
        {&diff_Research_DrawInventionIcons, setup_research, 20, 2},
        {&diff_Research_DrawIconFrame, setup_icon_frame, 30, 2},
        {&diff_Research_AnimateCurrentResearch, setup_research, 60, 2},
        {&diff_Research_PlayIconTransitionOut, setup_transition, 16, 2},
        {&diff_Research_PlayIconTransitionIn, setup_transition, 16, 2},
        {&diff_Research_ResearchScreen, setup_screen, 120, 2},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, bases, fns[i].nbases, fns[i].setup, diff_cases(fns[i].cases));
    for (int i = 0; i < STATES; i++)
        diff_free(bases[i]);
    return diff_finish();
}
