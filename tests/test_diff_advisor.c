// Differential test of unit Advisor (1991): the advisor dialogue (screen 24),
// original (under Unicorn) against the port, from new-game states with the
// advisor, the menu, the story state and the mouse perturbed at random. See
// docs/difftest.md. Needs the game data.
//
// PollAdvisorMenu and AdvisorScreen are driven with the button up (the loop
// ends after one pass): the harness has no button presses to give.
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/advisor.h"
#include "game/dseg.h"

DIFF_PROC(Advisor_AdvisorScreen, Advisor_AdvisorScreen());
DIFF_FUNC(Advisor_FindAdvisorItem, Advisor_FindAdvisorItem(a[0] != 0));
DIFF_PROC(Advisor_LoadAdvisorPictures, Advisor_LoadAdvisorPictures());
DIFF_PROC(Advisor_DrawAdvisorQuestion, Advisor_DrawAdvisorQuestion());
DIFF_PROC(Advisor_ShowAdvisorAnswer, Advisor_ShowAdvisorAnswer());
DIFF_PROC(Advisor_LoadAdvisorMenu, Advisor_LoadAdvisorMenu());
DIFF_PROC(Advisor_DrawAdvisorMenu, Advisor_DrawAdvisorMenu());
DIFF_PROC(Advisor_PollAdvisorMenu, Advisor_PollAdvisorMenu());
DIFF_PROC(Advisor_SetupAdvisorHotspots, Advisor_SetupAdvisorHotspots());

static int16_t *w16(uint16_t off) { return (int16_t *)ds_at(off); }

#define MENU_NO (*w16(0x771C))
#define QUESTION (*w16(0x771E))
#define ANSWER (*w16(0x7720))
#define MENU_LINES (*(FarPtr *)ds_at(0x7722))
#define KIND (*w16(0x7E3C))
#define SEL_LINE (*w16(0x7E3E))

static void seed(DiffRng *r)
{
    ds.System_RandSeed = (int32_t)diff_rand(r, 0xFFFFFFFFu);
}

// The advisor (staff type 1..4, hired), the hints' inventions, the menu
// buffer (5 lines of string[60]) and the story state the answers read.
static void advisor(DiffRng *r)
{
    seed(r);
    KIND = (int16_t)diff_range(r, 1, 4);
    for (int k = 0; k < 4; k++) {
        ds.Game_StaffHired[k] = (uint16_t)diff_range(r, 1, 3);
        ds.Game_StaffExp[k] = (uint16_t)diff_pick(r, 0, 5, 20, diff_range(r, 0, 70));
        ds.Game_DevSkill[k] = (uint16_t)diff_pick(r, 0, 3, 6, diff_range(r, 0, 6), 10, 30);
    }
    ds.Game_Hero = (int16_t)diff_range(r, 1, 2);
    ds.Game_StoryStage = (int16_t)diff_range(r, 1, 6);
    for (int i = 0; i < 9; i++) {
        int p = ds.Advisor_Hints[i].Product;
        if (p >= 1 && p <= 35 && diff_chance(r, 50))
            ds.Research_Products[p - 1].State = (int16_t)diff_pick(r, 0, 1, 2, 3, 4, 5, 5);
    }
    MENU_LINES = DIFF_SCRATCH;
    MENU_NO = (int16_t)diff_range(r, 1, 3);
    QUESTION = (int16_t)diff_range(r, 1, 11);
    ANSWER = (int16_t)diff_range(r, 1, 15);
    SEL_LINE = (int16_t)diff_range(r, 0, 5);
    *w16(0x5D9C) = (int16_t)diff_pick(r, 0, 0, 10);
    *w16(0x5D9E) = (int16_t)diff_pick(r, 0, 0, 1, 4);
    *w16(0x5DA0) = (int16_t)diff_range(r, 0, 4);
    *(int32_t *)ds_at(0x5DA2) = diff_pick(r, 0, 1000, 20000, 75000, 2000000);
    ds.Game_Credits = diff_pick(r, 0, 999, 20000, 75000, 1000000);
}

// The current menu as LoadAdvisorMenu leaves it (lines read into the buffer).
static void menu(DiffRng *r)
{
    MENU_NO = (int16_t)diff_range(r, 1, 3);
    Advisor_LoadAdvisorMenu();
}

// The mouse somewhere over the menu (lines at y = 9 * i + 54h, x 70h..D0h).
static void mouse(DiffRng *r)
{
    ds.Mouse_X = (int16_t)diff_pick(r, 0x6F, 0x70, 0x90, 0xD0, 0xD1, diff_range(r, 0x60, 0xE0),
                                    diff_range(r, 0x70, 0xD0), diff_range(r, 0x70, 0xD0));
    ds.Mouse_Y = (int16_t)diff_pick(r, 0x53, 0x54, 0x5D, 0x66, 0x7F, 0x83, diff_range(r, 0x50, 0x90),
                                    diff_range(r, 0x5D, 0x85), diff_range(r, 0x5D, 0x85));
    diff_quiesce();  // the cursor where the mouse now is
}

static void setup_screen(DiffRng *r, int32_t *a)
{
    (void)a;
    advisor(r);
    ds.Game_ScreenEntered = (uint8_t)diff_pick(r, 0, 0, 0, 1);
    menu(r);
    mouse(r);
    ds.Game_HotspotHit = (int16_t)diff_pick(r, 0x15, 0x15, 0x15, 0, 0x16);
    // PollAdvisorMenu compares with locals the original never initialises
    // (1991:0a4e, [bp-6]/[bp-8]); on entry they hold what DrawAdvisorMenu
    // left at that stack depth, so whether it redraws the menu is up to the
    // stack. Not both on one pass then.
    if (ds.Game_ScreenEntered)
        ds.Game_HotspotHit = (int16_t)diff_pick(r, 0, 0x16);
    ds.Game_CurrentScreen = 24;
    ds.Game_ScreenSwitch = 0;
}

static void setup_find(DiffRng *r, int32_t *a)
{
    advisor(r);
    a[0] = diff_range(r, 0, 1);
}

static void setup_advisor(DiffRng *r, int32_t *a)
{
    (void)a;
    advisor(r);
}

static void setup_menu(DiffRng *r, int32_t *a)
{
    (void)a;
    advisor(r);
    memset(far_host(DIFF_SCRATCH), 0xCC, 5 * 61);
}

static void setup_draw_menu(DiffRng *r, int32_t *a)
{
    (void)a;
    advisor(r);
    menu(r);
}

static void setup_poll(DiffRng *r, int32_t *a)
{
    (void)a;
    advisor(r);
    menu(r);
    mouse(r);
}

static const struct {
    const char *name;
    const char *script;
} states[] = {
    {"command centre", "1500:quit"},
    {"research (3)", "900:click:72,16;1300:quit"},
};
enum { STATES = sizeof states / sizeof states[0] };

int main(int argc, char *argv[])
{
    // On entry the screen sets up its hotspot under the pointer; the port's
    // Mouse_Poll (Mouse_Update) then finds it, the original only in its INT
    // 33h callback, which the emulator never runs (docs/difftest.md).
    diff_Advisor_AdvisorScreen.ignore =
        (const DiffRange[]){DIFF_DS(offsetof(DSeg, Mouse_HotspotUnder), 2), {0, 0}};
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

    const struct {
        DiffFn *fn;
        DiffSetup setup;
        int cases;
    } fns[] = {
        {&diff_Advisor_AdvisorScreen, setup_screen, 300},
        {&diff_Advisor_FindAdvisorItem, setup_find, 30},
        {&diff_Advisor_LoadAdvisorPictures, setup_advisor, 12},
        {&diff_Advisor_DrawAdvisorQuestion, setup_advisor, 22},
        {&diff_Advisor_ShowAdvisorAnswer, setup_advisor, 150},
        {&diff_Advisor_LoadAdvisorMenu, setup_menu, 12},
        {&diff_Advisor_DrawAdvisorMenu, setup_draw_menu, 20},
        {&diff_Advisor_PollAdvisorMenu, setup_poll, 30},
        {&diff_Advisor_SetupAdvisorHotspots, setup_advisor, 4},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, bases, STATES, fns[i].setup, diff_cases(fns[i].cases));
    for (int i = 0; i < STATES; i++)
        diff_free(bases[i]);
    return diff_finish();
}
