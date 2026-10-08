// Differential test of unit ShipInfo (26fe): the ship info screen (0x10) with
// its grid of group icons and the info panel, original (under Unicorn)
// against the port, from new-game states with the groups, the selection and
// the mouse perturbed at random. See docs/difftest.md. Needs the game data.
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/groups.h"
#include "game/shipinfo.h"
#include "game/system.h"

DIFF_PROC(ShipInfo_ShipInfoScreen, ShipInfo_ShipInfoScreen());
DIFF_PROC(ShipInfo_InitShipInfoScreen, ShipInfo_InitShipInfoScreen());
DIFF_PROC(ShipInfo_DrawGroupIcon, ShipInfo_DrawGroupIcon((int16_t)a[0], (int16_t)a[1]));
DIFF_PROC(ShipInfo_DrawGroupIconBuffered, ShipInfo_DrawGroupIconBuffered((int16_t)a[0], (int16_t)a[1]));
DIFF_PROC(ShipInfo_DrawAllGroupIcons, ShipInfo_DrawAllGroupIcons());
DIFF_PROC(ShipInfo_SwitchGroupList, ShipInfo_SwitchGroupList());
DIFF_PROC(ShipInfo_DrawGroupInfo, ShipInfo_DrawGroupInfo());
DIFF_PROC(ShipInfo_DrawGroupFlightInfo, ShipInfo_DrawGroupFlightInfo());
DIFF_PROC(ShipInfo_DrawSelectionMarker, ShipInfo_DrawSelectionMarker());
DIFF_PROC(ShipInfo_EraseSelectionMarker, ShipInfo_EraseSelectionMarker());
DIFF_PROC(ShipInfo_DrawSelectionMarkerBuffered, ShipInfo_DrawSelectionMarkerBuffered());
DIFF_PROC(ShipInfo_EraseSelectionMarkerBuffered, ShipInfo_EraseSelectionMarkerBuffered());
DIFF_PROC(ShipInfo_SetupShipInfoHotspots, ShipInfo_SetupShipInfoHotspots());

static const char *const fleet_names[] = {"Fleet", "Alpha squadron", "X", "Miners of the deep"};

// A group of list `list` somewhere it can be: the Sun's system mostly.
static void random_group(DiffRng *r, TGroup *g, int list)
{
    if (list == 0) {
        g->Kind = (uint8_t)diff_range(r, 1, 4);
        g->Status = (uint8_t)diff_pick(r, 1, 2, 2, 3, 4, 5, 6);
    } else {
        g->Status = 7;
    }
    const char *name = fleet_names[diff_rand(r, 4)];
    g->Name.len = (uint8_t)strlen(name);
    memcpy(g->Name.s, name, g->Name.len);
    int sys = diff_pick(r, 1, 1, 1, diff_range(r, 1, 8));
    g->System = (uint8_t)sys;
    g->Planet = (uint8_t)diff_range(r, 1, ds.Galaxy_PlanetCount[sys - 1]);
    int moons = ((const TMoonList *)far_host(ds.Galaxy_MoonLists[sys - 1]))[g->Planet - 1].Count;
    g->Moon = (uint8_t)(moons > 0 && diff_chance(r, 40) ? diff_range(r, 1, moons) : 0);
    g->Leg1 = (int16_t)diff_pick(r, 0, 1, 50, diff_range(r, 0, 3000));
    g->Jump = (int16_t)diff_pick(r, 0, 0, 1, 2);
    g->Leg2 = (int16_t)diff_pick(r, 0, 1, 50, diff_range(r, 0, 3000));
    for (int row = 0; row < 2; row++)
        for (int j = 0; j < 4; j++) {
            TGroupSlot *s = &g->Units[row][j];
            s->ShipType = (int16_t)diff_pick(r, 0, 0, 1, 5, diff_range(r, 0, 999), -1);
            for (int w = 0; w < 4; w++)
                s->Weapons[w] = (int16_t)diff_pick(r, 0, 0, 1, 3, diff_range(r, -5, 100));
        }
}

// Groups in both lists, the list shown and the selection.
static void random_groups(DiffRng *r)
{
    if (diff_chance(r, 70)) {
        int n = diff_pick(r, 0, 1, 3, 8, 9, 20, 31, 32);
        ds.Game_GroupCount = (int16_t)n;
        for (int i = 1; i <= n; i++)
            random_group(r, Groups_At(0, (int16_t)i), 0);
    }
    if (diff_chance(r, 50)) {
        int n = ds.Game_PlanetForceCount;
        for (int i = 1; i <= n; i++)
            if (diff_chance(r, 50))
                random_group(r, Groups_At(1, (int16_t)i), 1);
    }
    ds.Game_GroupList = (int16_t)diff_pick(r, 0, 0, 1);
    ds.Game_ShownGroupCount = ds.Game_GroupList == 0 ? ds.Game_GroupCount : ds.Game_PlanetForceCount;
    int n = ds.Game_ShownGroupCount;
    ds.Game_SelGroup = (int16_t)diff_pick(r, 0, 1, diff_range(r, 0, n), n);
    if (ds.Game_SelGroup > n)
        ds.Game_SelGroup = (int16_t)n;
    ds.Game_SelGroup0 = (int16_t)diff_range(r, 0, ds.Game_GroupCount);
    ds.Game_SelGroup1 = (int16_t)diff_range(r, 0, ds.Game_PlanetForceCount);
    ds.Game_StaffExp[0] = (uint16_t)diff_pick(r, 0, 1, 10, 70, 0xFFFF);
}

static void setup_screen(DiffRng *r, int32_t *a)
{
    (void)a;
    random_groups(r);
    int n = ds.Game_ShownGroupCount;
    ds.Game_ScreenEntered = (uint8_t)diff_pick(r, 0, 0, 0, 0, 1);
    ds.Game_HotspotHit = (int16_t)diff_pick(r, 0, 0x15, 0x15, 0x16, 0x17, diff_range(r, 0x17, 0x17 + n),
                                            0x36, 0x37, diff_range(r, 0x37, 0x37 + n), 0x37 + n);
    if (n == 0 && ds.Game_HotspotHit > 0x36)
        ds.Game_HotspotHit = 0x15;
    ds.Mouse_Buttons = (uint8_t)diff_pick(r, 0, 1, 1);
    ds.Mouse_ButtonsChanged = (uint8_t)diff_range(r, 0, 1);
    ds.Mouse_HeldLeft = (uint8_t)diff_range(r, 0, 1);
    if (n > 0 && diff_chance(r, 25)) {  // released over a name: open the group
        ds.Game_HotspotHit = (int16_t)diff_range(r, 0x37, 0x36 + n);
        ds.Mouse_Buttons = 0;
        ds.Mouse_HeldLeft = 1;
    }
    ds.Game_MenuCommand = (int16_t)diff_pick(r, 0, 0, 0xF);
    ds.Game_ScreenSwitch = 0;
    ds.Game_CurrentScreen = 0x10;
}

static void setup_groups(DiffRng *r, int32_t *a)
{
    (void)a;
    random_groups(r);
}

static void setup_icon(DiffRng *r, int32_t *a)
{
    random_groups(r);
    a[0] = diff_pick(r, 0, 0, 1, 2, 7, 0xA1, 0x142, 0x1E6);
    a[1] = diff_range(r, 1, 32);
    if (diff_chance(r, 30))
        ds.Game_SelGroup = (int16_t)a[1];
}

// The off-screen copy exists only while SwitchGroupList runs.
static void buffer(void)
{
    ds.ShipInfo_Buffer = System_GetMem(0xFB40);
}

static void setup_icon_buffered(DiffRng *r, int32_t *a)
{
    setup_icon(r, a);
    buffer();
}

static void setup_groups_buffered(DiffRng *r, int32_t *a)
{
    setup_groups(r, a);
    buffer();
}

static const struct {
    const char *name;
    const char *script;
} states[] = {
    {"ship info (16)", "900:click:120,16;1300:quit"},
    // The planet forces (list 1): "Change mode".
    {"ship info, list 1", "900:click:120,16;1100:click:100,194;1500:quit"},
};
enum { STATES = sizeof states / sizeof states[0] };

int main(int argc, char *argv[])
{
    diff_ShipInfo_DrawGroupIconBuffered.near = true;  // 26fe:0683 ret 4
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
        {&diff_ShipInfo_ShipInfoScreen, setup_screen, 80},
        {&diff_ShipInfo_InitShipInfoScreen, setup_groups, 8},
        {&diff_ShipInfo_DrawGroupIcon, setup_icon, 30},
        {&diff_ShipInfo_DrawGroupIconBuffered, setup_icon_buffered, 20},
        {&diff_ShipInfo_DrawAllGroupIcons, setup_groups, 20},
        {&diff_ShipInfo_SwitchGroupList, setup_groups, 8},
        {&diff_ShipInfo_DrawGroupInfo, setup_groups, 80},
        {&diff_ShipInfo_DrawGroupFlightInfo, setup_groups, 30},
        {&diff_ShipInfo_DrawSelectionMarker, setup_groups, 10},
        {&diff_ShipInfo_EraseSelectionMarker, setup_groups, 10},
        {&diff_ShipInfo_DrawSelectionMarkerBuffered, setup_groups_buffered, 10},
        {&diff_ShipInfo_EraseSelectionMarkerBuffered, setup_groups_buffered, 10},
        {&diff_ShipInfo_SetupShipInfoHotspots, setup_groups, 20},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, bases, STATES, fns[i].setup, diff_cases(fns[i].cases));
    for (int i = 0; i < STATES; i++)
        diff_free(bases[i]);
    return diff_finish();
}
