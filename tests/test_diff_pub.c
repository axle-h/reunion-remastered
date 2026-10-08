// Differential test of unit Pub (1fd2): the pub screen (graphics, hotspots,
// the people's animations), the dialog (portrait, question menu, replies,
// hover) and the pirate deal dates, original (under Unicorn) against the
// port, from a new game's pub and a dialog opened on it. See
// docs/difftest.md. Needs the game data.
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/pub.h"
#include "game/system.h"

DIFF_PROC(Pub_DrawPubSprite, Pub_DrawPubSprite((int16_t)a[0]));
DIFF_PROC(Pub_LoadPubGraphics, Pub_LoadPubGraphics());
DIFF_PROC(Pub_SetupPubHotspots, Pub_SetupPubHotspots());
DIFF_PROC(Pub_LoadPersonPortrait, Pub_LoadPersonPortrait());
DIFF_PROC(Pub_SetupTalkHotspot, Pub_SetupTalkHotspot());
DIFF_PROC(Pub_ReadQuestionCode, Pub_ReadQuestionCode());
DIFF_PROC(Pub_ShowReply, Pub_ShowReply());
DIFF_PROC(Pub_BuildQuestionMenu, Pub_BuildQuestionMenu());
DIFF_PROC(Pub_DrawQuestionMenu, Pub_DrawQuestionMenu());
DIFF_PROC(Pub_UpdateQuestionHover, Pub_UpdateQuestionHover());
DIFF_PROC(Pub_UpdateTopicAvailability, Pub_UpdateTopicAvailability());
// Pascal order (var year, month, day, hour; then the amounts year, month,
// day, hour): the port's C prototype lists them the other way round.
DIFF_PROC(Pub_AddToDate, Pub_AddToDate((int8_t)a[7], (int8_t)a[6], (int8_t)a[5], (int16_t)a[4],
                                       far_host((FarPtr)a[3]), far_host((FarPtr)a[2]),
                                       far_host((FarPtr)a[1]), far_host((FarPtr)a[0])));
DIFF_PROC(Pub_StartPubDealWindows, Pub_StartPubDealWindows());
DIFF_FUNC(Pub_DateToHours, Pub_DateToHours(a[0], a[1], a[2], a[3]));
DIFF_FUNC(Pub_FindActiveDeal, Pub_FindActiveDeal());
DIFF_PROC(Pub_PlayRandomPubSound, Pub_PlayRandomPubSound());
DIFF_PROC(Pub_AnimatePubPerson, Pub_AnimatePubPerson((int16_t)a[0]));
DIFF_PROC(Pub_UpdatePubAnimations, Pub_UpdatePubAnimations());

static int16_t *w16(uint16_t off) { return (int16_t *)ds_at(off); }
static uint8_t *b8(uint16_t off) { return (uint8_t *)ds_at(off); }
static FarPtr *fp(uint16_t off) { return (FarPtr *)ds_at(off); }

#define PERSON (*w16(0x7738))
#define MENU_IDX (*w16(0x7764))

// Menus per person (Pub::MenuData).
static const int menus[11] = {0, 5, 11, 3, 5, 6, 11, 7, 6, 7, 6};

static void seed(DiffRng *r)
{
    ds.System_RandSeed = (int32_t)diff_rand(r, 0xFFFFFFFFu);
}

static void people(DiffRng *r)
{
    for (int k = 0; k < 10; k++) {
        TPubPerson *p = &ds.Pub_People[k];
        if (!diff_chance(r, 40))
            continue;
        p->State = (uint8_t)diff_pick(r, 0, 0, 1, 2, 2, 3);
        p->Timer = (int16_t)diff_pick(r, 0, 0, 0, 1, 5);
    }
}

// People with dialog files (there is no KERDES8.LOC: nobody talks to 8).
static int talker(DiffRng *r) { return diff_pick(r, 1, 2, 3, 4, 5, 6, 7, 9, 10); }

// Person k's number string (DS:7760, string[2]) as the dialog sets it.
static void person(int k)
{
    PERSON = (int16_t)k;
    PString *s = (PString *)ds_at(0x7760);
    System_StrLong(k, 0, s, 2);
}

// --- Pub screen ---------------------------------------------------------------------------

static void setup_sprite(DiffRng *r, int32_t *a)
{
    people(r);
    a[0] = diff_range(r, 1, 10);
}

static void setup_graphics(DiffRng *r, int32_t *a)
{
    (void)a;
    people(r);
    System_FreeMem(*fp(0x773A), 0xAF00);
    System_FreeMem(*fp(0x7784), 64000);
}

static void setup_people(DiffRng *r, int32_t *a)
{
    (void)a;
    people(r);
}

static void setup_anim(DiffRng *r, int32_t *a)
{
    seed(r);
    ds.Sound_SbStatus = 0;  // as in setup_sound
    people(r);
    for (int k = 0; k < 9; k++) {
        TPubAnim *an = &ds.Pub_Anims[k];
        an->Timer = (int16_t)diff_pick(r, an->Timer, 1, 1, 2, 0);
        if (diff_chance(r, 50))
            an->Frame = (uint8_t)diff_pick(r, an->FrameCount, an->FrameCount, 1, diff_range(r, 1, an->FrameCount));
    }
    a[0] = diff_range(r, 1, 9);
}

static void setup_sound(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    *w16(0x7782) = (int16_t)diff_range(r, 0, 8);
    // No sample playing: the port asks its audio (none playing), the
    // original reads the status a past sample left.
    ds.Sound_SbStatus = 0;
    ds.Sound_DeviceType = (uint8_t)diff_pick(r, 0, 1, 2);
}

// --- Dates ----------------------------------------------------------------------------------

static void setup_add_date(DiffRng *r, int32_t *a)
{
    uint8_t *p = far_host(DIFF_SCRATCH);
    *(int16_t *)p = (int16_t)diff_range(r, 2900, 2950);   // year
    p[2] = (uint8_t)diff_pick(r, 1, 12, 13, diff_range(r, 0, 255));  // month
    p[3] = (uint8_t)diff_pick(r, 1, 30, 31, diff_range(r, 0, 255));  // day
    p[4] = (uint8_t)diff_pick(r, 0, 23, 24, diff_range(r, 0, 255));  // hour
    a[0] = (int32_t)DIFF_SCRATCH;
    a[1] = (int32_t)(DIFF_SCRATCH + 2);
    a[2] = (int32_t)(DIFF_SCRATCH + 3);
    a[3] = (int32_t)(DIFF_SCRATCH + 4);
    a[4] = diff_range(r, -2, 3000);
    a[5] = diff_pick(r, 0, 1, 11, 12, diff_range(r, 0, 255));
    a[6] = diff_pick(r, 0, 1, 29, 30, diff_range(r, 0, 255));
    a[7] = diff_pick(r, 0, 1, 23, 24, diff_range(r, 0, 255));
}

static void deals(DiffRng *r)
{
    ds.Game_Year = (uint16_t)diff_range(r, 2927, 2930);
    ds.Game_Month = (uint16_t)diff_range(r, 1, 12);
    ds.Game_Day = (uint16_t)diff_range(r, 1, 30);
    ds.Game_Hour = (uint16_t)diff_range(r, 0, 23);
    for (int k = 0; k < 10; k++) {
        TPubDeal *d = &ds.Pub_Deals[k];
        d->Year = (uint16_t)diff_pick(r, 0, 0, 1, ds.Game_Year, ds.Game_Year - 1);
        d->Month = (uint8_t)diff_range(r, 0, 12);
        d->Day = (uint8_t)diff_range(r, 0, 30);
        d->Hour = (uint8_t)diff_range(r, 0, 23);
        d->Active = (uint16_t)diff_pick(r, 0, 1, ds.Game_Year, ds.Game_Year + 1);
        d->_15[0] = (uint8_t)diff_range(r, 0, 12);
        d->_15[1] = (uint8_t)diff_range(r, 0, 30);
        d->_15[2] = (uint8_t)diff_range(r, 0, 23);
    }
}

static void setup_deals(DiffRng *r, int32_t *a)
{
    (void)a;
    deals(r);
}

static void setup_date_to_hours(DiffRng *r, int32_t *a)
{
    a[0] = diff_pick(r, 0, 2927, 3000, -1, (int32_t)diff_rand(r, 0xFFFFFFFFu));
    a[1] = diff_pick(r, 0, 1, 12, (int32_t)diff_rand(r, 0xFFFFFFFFu));
    a[2] = diff_pick(r, 0, 1, 30, (int32_t)diff_rand(r, 0xFFFFFFFFu));
    a[3] = diff_pick(r, 0, 1, 23, (int32_t)diff_rand(r, 0xFFFFFFFFu));
}

// --- Dialog ------------------------------------------------------------------------------------

static void setup_portrait(DiffRng *r, int32_t *a)
{
    (void)a;
    person(talker(r));
}

static void setup_question_code(DiffRng *r, int32_t *a)
{
    (void)a;
    person(talker(r));
    *w16(0x7766) = (int16_t)diff_range(r, 1, 20);
}

static void setup_reply(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    person(diff_pick(r, 2, 2, talker(r)));
    *w16(0x7768) = (int16_t)(PERSON == 2 ? diff_pick(r, 9, 10, 11, 12, 13, 14, diff_range(r, 1, 15))
                                          : diff_range(r, 1, 15));
    *(int32_t *)ds_at(0x7770) = diff_range(r, 1, 12);  // item offered
    for (int k = 0; k < 12; k++)
        ds.Pub_ItemQty[k] = (uint8_t)diff_range(r, 0, 255);
}

static void setup_build_menu(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    int k = diff_pick(r, 2, 2, 9, talker(r));
    person(k);
    MENU_IDX = (int16_t)diff_range(r, 1, menus[k] - 1);
    if (k == 2 && diff_chance(r, 60))
        MENU_IDX = (int16_t)diff_pick(r, 6, 7);
    if (k == 9 && diff_chance(r, 50))
        MENU_IDX = 4;
    for (int f = 1; f <= 10; f++)
        *b8((uint16_t)(0x774A + f)) = (uint8_t)diff_range(r, 0, 1);
    deals(r);
}

// The dialog's menu built (port), then drawn / hovered.
static void setup_draw_menu(DiffRng *r, int32_t *a)
{
    setup_build_menu(r, a);
    Pub_BuildQuestionMenu();
    *w16(0x7758) = (int16_t)diff_range(r, 0, 5);  // hovered item
}

static void setup_hover(DiffRng *r, int32_t *a)
{
    setup_build_menu(r, a);
    Pub_BuildQuestionMenu();
    *b8(0x775A) = (uint8_t)diff_pick(r, 0, 0, 0, 1);       // frozen
    *w16(0x775C) = (int16_t)diff_pick(r, 0, 1, 0xFFFF);  // previous column
    *w16(0x775E) = (int16_t)diff_pick(r, 0, 1, 2, -1);    // previous row
    ds.Mouse_ClickX = (int16_t)diff_pick(r, 0, 5, 6, 100, 0xD8, 0xD9, 300);
    ds.Mouse_ClickY = (int16_t)diff_pick(r, 0, 0x7D, 0x86, 0x90, 0x99, 0xA2, 0xB0, 0xC5, 199);
}

static void setup_topics(DiffRng *r, int32_t *a)
{
    (void)a;
    for (int k = 0; k < 11; k++)
        if (diff_chance(r, 50))
            ds.Races_Races[k].Status = (int8_t)diff_pick(r, -1, 0, 1, 2, 3, 4);
    ds.Galaxy_SystemStatus[2] = (int8_t)diff_pick(r, -1, 0, 1);
    *b8(0x5D90) = (uint8_t)diff_range(r, 0, 1);
}

static void setup_none(DiffRng *r, int32_t *a)
{
    (void)r;
    (void)a;
}

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    // The pub: the SPACE LOCAL hotspot of the command centre.
    if (!diff_boot("900:click:100,70;1300:quit") || ds.Game_CurrentScreen != 23) {
        printf("FAILED to reach the pub (screen %d)\n", ds.Game_CurrentScreen);
        return 1;
    }
    DiffState *pub = diff_save();
    // A dialog with the trader (person 2), as Main opens it.
    person(2);
    MENU_IDX = 1;
    Pub_LoadPersonPortrait();
    Pub_BuildQuestionMenu();
    Pub_DrawQuestionMenu();
    Pub_SetupTalkHotspot();
    DiffState *dialog = diff_save();
    DiffState *both[2] = {pub, dialog};

    diff_Pub_DrawPubSprite.decl = "(idx: integer)";
    diff_Pub_AnimatePubPerson.decl = "(idx: integer)";
    diff_Pub_AddToDate.decl = "(var year: integer; var month, day, hour: byte; dyear: integer; "
                              "dmonth, dday, dhour: byte)";
    diff_Pub_FindActiveDeal.decl = ": integer";

    const struct {
        DiffFn *fn;
        DiffSetup setup;
        int cases;
        int nbases;
        DiffState *const *bases;
    } fns[] = {
        {&diff_Pub_DrawPubSprite, setup_sprite, 30, 1, &pub},
        {&diff_Pub_LoadPubGraphics, setup_graphics, 4, 1, &pub},
        {&diff_Pub_SetupPubHotspots, setup_people, 20, 1, &pub},
        {&diff_Pub_AnimatePubPerson, setup_anim, 40, 1, &pub},
        {&diff_Pub_UpdatePubAnimations, setup_anim, 30, 1, &pub},
        {&diff_Pub_PlayRandomPubSound, setup_sound, 10, 1, &pub},
        {&diff_Pub_AddToDate, setup_add_date, 40, 1, &pub},
        {&diff_Pub_StartPubDealWindows, setup_deals, 20, 1, &pub},
        {&diff_Pub_DateToHours, setup_date_to_hours, 30, 1, &pub},
        {&diff_Pub_FindActiveDeal, setup_deals, 40, 1, &pub},
        {&diff_Pub_UpdateTopicAvailability, setup_topics, 30, 1, &pub},
        {&diff_Pub_LoadPersonPortrait, setup_portrait, 10, 1, &pub},
        {&diff_Pub_SetupTalkHotspot, setup_none, 2, 2, both},
        {&diff_Pub_ReadQuestionCode, setup_question_code, 20, 1, &dialog},
        {&diff_Pub_ShowReply, setup_reply, 40, 1, &dialog},
        {&diff_Pub_BuildQuestionMenu, setup_build_menu, 40, 1, &dialog},
        {&diff_Pub_DrawQuestionMenu, setup_draw_menu, 30, 1, &dialog},
        {&diff_Pub_UpdateQuestionHover, setup_hover, 40, 1, &dialog},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, fns[i].bases, fns[i].nbases, fns[i].setup, diff_cases(fns[i].cases));
    diff_free(pub);
    diff_free(dialog);
    return diff_finish();
}
