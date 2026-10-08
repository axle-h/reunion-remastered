// Differential test of unit AlienTalk (2e4b): the alien visit scenes, the
// conversation screen (backdrop, portraits, question menu, replies, hover),
// original (under Unicorn) against the port, from a new game's command
// centre and a conversation opened on it. See docs/difftest.md. Needs the
// game data.
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/alientalk.h"
#include "game/dseg.h"
#include "game/protect.h"
#include "game/system.h"

DIFF_PROC(AlienTalk_StartAlienScene, AlienTalk_StartAlienScene((int16_t)a[0]));
DIFF_PROC(AlienTalk_PlayAlienScene, AlienTalk_PlayAlienScene((int16_t)a[0]));
DIFF_PROC(AlienTalk_StartAlienTalk, AlienTalk_StartAlienTalk((int16_t)a[0]));
DIFF_PROC(AlienTalk_DrawAlienTalkScreen, AlienTalk_DrawAlienTalkScreen());
DIFF_PROC(AlienTalk_SetupTalkHotspots, AlienTalk_SetupTalkHotspots());
DIFF_PROC(AlienTalk_LoadQuestionAnswerId, AlienTalk_LoadQuestionAnswerId());
DIFF_PROC(AlienTalk_DrawAlienReply, AlienTalk_DrawAlienReply());
DIFF_PROC(AlienTalk_BuildQuestionMenu, AlienTalk_BuildQuestionMenu());
DIFF_PROC(AlienTalk_DrawQuestionMenu, AlienTalk_DrawQuestionMenu());
DIFF_PROC(AlienTalk_HandleTalkMouse, AlienTalk_HandleTalkMouse());
DIFF_PROC(AlienTalk_VerifyManualAnswer, AlienTalk_VerifyManualAnswer());

static int16_t *w16(uint16_t off) { return (int16_t *)ds_at(off); }
static uint8_t *b8(uint16_t off) { return (uint8_t *)ds_at(off); }

#define MENU_NO (*w16(0x78BC))

static void seed(DiffRng *r)
{
    ds.System_RandSeed = (int32_t)diff_rand(r, 0xFFFFFFFFu);
}

// Menu m of the current race: count + 3 question numbers.
static const uint8_t *menu(int m)
{
    return (const uint8_t *)far_host(ds.Alien_MenuLists[ds.AlienTalk_Race - 1]) + m * 4;
}

// A menu number of the current race whose entry is a real menu.
static int pick_menu(DiffRng *r)
{
    for (int tries = 0; tries < 8; tries++) {
        int m = diff_range(r, 1, 8);
        const uint8_t *p = menu(m);
        bool ok = p[0] >= 1 && p[0] <= 3;
        for (int k = 1; k <= p[0] && ok; k++)
            ok = p[k] >= 1 && p[k] < 60;
        if (ok)
            return m;
    }
    return 1;
}

// --- Scenes and screen switches -------------------------------------------------------------

static void setup_start(DiffRng *r, int32_t *a)
{
    a[0] = diff_range(r, 0, 12);
}

static void setup_scene(DiffRng *r, int32_t *a)
{
    seed(r);
    // Not scene 1: it waits for mouse clicks.
    a[0] = diff_pick(r, 2, 9, 10, 10, diff_range(r, 2, 10));
}

// --- The conversation ----------------------------------------------------------------------------

static void race(int k)
{
    ds.AlienTalk_Race = (int16_t)k;
    System_StrLong(k, 0, (PString *)ds_at(0x78B8), 2);
}

static void setup_screen(DiffRng *r, int32_t *a)
{
    (void)a;
    System_FreeMem(ds.AlienTalk_QuestionBuf, 0x300);
    race(diff_range(r, 1, 9));
    ds.Game_Hero = (int16_t)diff_range(r, 1, 2);
    *b8(0x7E3A) = (uint8_t)diff_range(r, 0, 1);
}

static void setup_line(DiffRng *r, int32_t *a)
{
    (void)a;
    race(diff_range(r, 1, 9));
    *w16(0x78BE) = (int16_t)diff_range(r, 1, 30);  // question line
    *w16(0x78C0) = (int16_t)diff_range(r, 1, 30);  // reply line
}

static void setup_build(DiffRng *r, int32_t *a)
{
    (void)a;
    race(diff_range(r, 1, 9));
    MENU_NO = (int16_t)pick_menu(r);
}

static void setup_draw(DiffRng *r, int32_t *a)
{
    setup_build(r, a);
    AlienTalk_BuildQuestionMenu();
    *w16(0x78B4) = (int16_t)diff_range(r, 0, 4);  // highlighted question
}

static void setup_mouse(DiffRng *r, int32_t *a)
{
    setup_build(r, a);
    AlienTalk_BuildQuestionMenu();
    *b8(0x78B6) = (uint8_t)diff_pick(r, 0, 0, 0, 1);       // locked
    *w16(0x78CC) = (int16_t)diff_pick(r, 0, 1, 0xFFFF);  // previous column
    *w16(0x78CE) = (int16_t)diff_pick(r, 0, 1, 2, -1);    // previous row
    ds.Mouse_ClickX = (int16_t)diff_pick(r, 0, 5, 6, 100, 0xD8, 0xD9, 300);
    ds.Mouse_ClickY = (int16_t)diff_pick(r, 0, 0x7D, 0x86, 0x90, 0x99, 0xA2, 0xB0, 0xC5, 199);
}

// Only the pages the port does not answer differently (see alientalk.c:
// the code check is removed): unarmed or out of range.
static void setup_verify(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    ds.Protect_PageIndex = (int16_t)diff_pick(r, PROTECT_PAGE_UNARMED, 0, 22, -3);
}

static void setup_none(DiffRng *r, int32_t *a)
{
    (void)r;
    (void)a;
}

// The original's file variable of the graphics data file (TP FileRec, used
// by the .ANI loader of the scenes' animations): the port keeps its open
// file on the host side.
static const DiffRange file_rec[] = {DIFF_DS(0x91EE, 0x80), {0, 0}};

int main(int argc, char *argv[])
{
    diff_AlienTalk_PlayAlienScene.ignore = file_rec;
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    if (!diff_boot("1500:quit")) {
        printf("FAILED to reach the command centre\n");
        return 1;
    }
    DiffState *command = diff_save();
    // A conversation with race 2, opened as Main does (screen 22h).
    race(2);
    MENU_NO = 1;
    AlienTalk_DrawAlienTalkScreen();
    AlienTalk_BuildQuestionMenu();
    AlienTalk_DrawQuestionMenu();
    AlienTalk_SetupTalkHotspots();
    DiffState *talk = diff_save();
    DiffState *both[2] = {command, talk};

    const struct {
        DiffFn *fn;
        DiffSetup setup;
        int cases;
        int nbases;
        DiffState *const *bases;
    } fns[] = {
        {&diff_AlienTalk_StartAlienScene, setup_start, 6, 1, &command},
        {&diff_AlienTalk_StartAlienTalk, setup_start, 6, 1, &command},
        {&diff_AlienTalk_PlayAlienScene, setup_scene, 18, 2, both},
        {&diff_AlienTalk_DrawAlienTalkScreen, setup_screen, 12, 1, &talk},
        {&diff_AlienTalk_SetupTalkHotspots, setup_none, 2, 2, both},
        {&diff_AlienTalk_LoadQuestionAnswerId, setup_line, 30, 1, &talk},
        {&diff_AlienTalk_DrawAlienReply, setup_line, 30, 1, &talk},
        {&diff_AlienTalk_BuildQuestionMenu, setup_build, 30, 1, &talk},
        {&diff_AlienTalk_DrawQuestionMenu, setup_draw, 30, 1, &talk},
        {&diff_AlienTalk_HandleTalkMouse, setup_mouse, 40, 1, &talk},
        {&diff_AlienTalk_VerifyManualAnswer, setup_verify, 8, 1, &talk},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, fns[i].bases, fns[i].nbases, fns[i].setup, diff_cases(fns[i].cases));
    diff_free(command);
    diff_free(talk);
    return diff_finish();
}
