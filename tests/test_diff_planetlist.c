// Differential test of unit PlanetList (1390): the scrolling list of bodies on
// screen 37, original (under Unicorn) against the port, from new-game states
// with the bodies, races and list position perturbed at random. See
// docs/difftest.md. Needs the game data.
//
// AddListEntry and ListKindCaption are nested (covered through BuildList and
// DrawList).
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/planetlist.h"

DIFF_PROC(PlanetList_SetupScrollBarHotspot, PlanetList_SetupScrollBarHotspot());
DIFF_PROC(PlanetList_BuildList, PlanetList_BuildList());
DIFF_PROC(PlanetList_DrawList, PlanetList_DrawList());
DIFF_PROC(PlanetList_RefreshList, PlanetList_RefreshList());
DIFF_PROC(PlanetList_DrawScrollBarThumb, PlanetList_DrawScrollBarThumb());
DIFF_PROC(PlanetList_DragScrollBar, PlanetList_DragScrollBar());

static TBody *body_at(int sys, int idx)
{
    return (TBody *)far_host(ds.Galaxy_Bodies[sys - 1]) + (idx - 1);
}

static void perturb_body(DiffRng *r, TBody *b)
{
    b->Owner = (uint8_t)diff_pick(r, 0, 0, 1, 1, diff_range(r, 2, 12));
    b->Colony = (uint8_t)diff_pick(r, 0, 0, 1);
    b->_0A = (uint8_t)diff_pick(r, 0, 0, 2);
    b->Miner = (int8_t)diff_pick(r, 0, 0, 0, 1);
    b->Surveyed = (uint8_t)diff_pick(r, 0, 1, 1);
    b->_03 = (uint8_t)diff_pick(r, 0, 1, 1);
    b->InfoLevel = (int8_t)diff_pick(r, 0, 9, 10, 29, 30, 39, 40, 60);
    b->SurfaceType = (uint8_t)diff_range(r, 1, 10);
}

// The list buffer: 150 entries in the game (0x4B0 bytes); the scratch area
// holds every body of the galaxy twice.
static void list_buffer(void)
{
    ds.PlanetList_Entries = DIFF_SCRATCH;
}

static void perturb_galaxy(DiffRng *r)
{
    list_buffer();
    ds.PlanetList_Mode = (int16_t)diff_pick(r, 1, 2, 3, 3, 0);
    for (int s = 1; s <= 8; s++) {
        if (s > 1 && diff_chance(r, 40))
            ds.Galaxy_SystemStatus[s - 1] = (int8_t)diff_pick(r, 1, 1, 0, -1);
        if (ds.Galaxy_SystemStatus[s - 1] != 1)
            continue;
        int n = ds.Galaxy_PlanetCount[s - 1] + ds.Galaxy_MoonCount[s - 1];
        for (int i = 1; i <= n; i++)
            if (diff_chance(r, 35))
                perturb_body(r, body_at(s, i));
    }
    for (int k = 0; k < 11; k++)
        if (diff_chance(r, 40))
            ds.Races_Races[k].Status = (int8_t)diff_pick(r, 0, 2, 4, 6);
    for (int o = 2; o <= 12; o++)  // races met (DS:6517 + owner)
        if (diff_chance(r, 40))
            *(uint8_t *)ds_at((uint16_t)(0x6517 + o)) = (uint8_t)diff_pick(r, 0, 1, 2);
}

static void setup_hotspot(DiffRng *r, int32_t *a)
{
    (void)a;
    ds.Mouse_HotspotCount = (int16_t)diff_range(r, 0, 30);
}

static void setup_build(DiffRng *r, int32_t *a)
{
    (void)a;
    perturb_galaxy(r);
    ds.PlanetList_Count = (int16_t)diff_pick(r, 0, 5, 100);
}

// A built list scrolled somewhere (Top past the end too).
static void setup_draw(DiffRng *r, int32_t *a)
{
    (void)a;
    perturb_galaxy(r);
    if (ds.PlanetList_Mode == 0)
        ds.PlanetList_Mode = 1;
    PlanetList_BuildList();
    int n = ds.PlanetList_Count;
    // Top is at least 1 in the game (DragScrollBar, the mode buttons).
    ds.PlanetList_Top = (int16_t)diff_pick(r, 1, 1, diff_range(r, 1, n > 0 ? n : 1), n - 14 > 1 ? n - 14 : 1,
                                           n > 0 ? n : 1, n + 1);
}

static void setup_thumb(DiffRng *r, int32_t *a)
{
    (void)a;
    ds.PlanetList_Count = (int16_t)diff_pick(r, 0, 1, 15, 16, 17, 30, 150, diff_range(r, 0, 200));
    int n = ds.PlanetList_Count;
    ds.PlanetList_Top = (int16_t)diff_pick(r, 1, diff_range(r, 1, n > 0 ? n : 1), n - 14 > 1 ? n - 14 : 1);
}

// The button is down on entry; Mouse_Poll then reads it released, so the loop
// runs once with ClickY where the mouse is.
static void setup_drag(DiffRng *r, int32_t *a)
{
    setup_draw(r, a);
    ds.Mouse_Buttons = (uint8_t)diff_pick(r, 0, 1, 1, 1);
    ds.Mouse_X = (int16_t)diff_range(r, 0x134, 0x13D);
    ds.Mouse_Y = (int16_t)diff_pick(r, 0, 0x37, 0x38, 0x80, 0xC2, 0xC3, 199, diff_range(r, 0x37, 0xC3));
    uint8_t buttons = ds.Mouse_Buttons;
    diff_quiesce();  // the cursor where the mouse now is
    ds.Mouse_Buttons = buttons;
}

static const struct {
    const char *name;
    const char *script;
} states[] = {
    {"command centre", "1500:quit"},
    {"system view (7)", "900:click:168,16;1300:quit"},
};
enum { STATES = sizeof states / sizeof states[0] };

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    diff_PlanetList_BuildList.near = true;  // 1390:037c ret
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
        {&diff_PlanetList_SetupScrollBarHotspot, setup_hotspot, 4},
        {&diff_PlanetList_BuildList, setup_build, 60},
        {&diff_PlanetList_DrawList, setup_draw, 60},
        {&diff_PlanetList_RefreshList, setup_draw, 20},
        {&diff_PlanetList_DrawScrollBarThumb, setup_thumb, 30},
        {&diff_PlanetList_DragScrollBar, setup_drag, 40},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, bases, STATES, fns[i].setup, diff_cases(fns[i].cases));
    for (int i = 0; i < STATES; i++)
        diff_free(bases[i]);
    return diff_finish();
}
