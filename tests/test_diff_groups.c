// Differential test of unit Groups (2b8d): the group lists, the group detail
// screen (0x16) and the New Group screen (0x15), original (under Unicorn)
// against the port. Base states: new games on the ship info screen and on
// the group screen of the planet force (reached with the autopilot), and a
// fleet's group screen and the New Group screen entered by the port's own
// handlers (no fleets exist at the start). The selected group, its slots, the
// stocks and the clicked hotspot are perturbed at random. See
// docs/difftest.md. Needs the game data.
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/groups.h"
#include "game/system.h"

DIFF_PROC(Groups_GroupsScreen, Groups_GroupsScreen());
DIFF_PROC(Groups_NewGroupScreen, Groups_NewGroupScreen());
DIFF_PROC(Groups_LoadGroupsGfx, Groups_LoadGroupsGfx());
DIFF_PROC(Groups_DrawGroupPanel, Groups_DrawGroupPanel((uint8_t)a[0] != 0));
DIFF_PROC(Groups_SetupGroupsHotspots, Groups_SetupGroupsHotspots());
DIFF_PROC(Groups_DrawNewGroupNameAndType, Groups_DrawNewGroupNameAndType());
DIFF_PROC(Groups_SetupNewGroupHotspots, Groups_SetupNewGroupHotspots());
DIFF_PROC(Groups_ClearGroup, Groups_ClearGroup());
DIFF_PROC(Groups_InitGroupOfKind, Groups_InitGroupOfKind());
DIFF_PROC(Groups_NextSlotCategory, Groups_NextSlotCategory());
DIFF_PROC(Groups_PrevSlotCategory, Groups_PrevSlotCategory());
DIFF_PROC(Groups_FirstSlotCategory, Groups_FirstSlotCategory());
DIFF_FUNC(Groups_IsGroupEmpty, Groups_IsGroupEmpty((int16_t)a[0], (int16_t)a[1]));
DIFF_FUNC(Groups_IsGroupEmptyOfCargo, Groups_IsGroupEmptyOfCargo((int16_t)a[0], (int16_t)a[1]));
DIFF_FUNC(Groups_FindPlanetForce, Groups_FindPlanetForce((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_PROC(Groups_InitPlanetForce, Groups_InitPlanetForce());
DIFF_PROC(Groups_Init, Groups_Init());

#define PF_PTR (*(FarPtr *)ds_at(0x789C))  // planet force at the shown group's location

static void seed(DiffRng *r)
{
    ds.System_RandSeed = (int32_t)diff_rand(r, 0xFFFFFFFFu);
}

static uint8_t moons_of(int s, int p)
{
    return *((uint8_t *)far_host(ds.Galaxy_MoonLists[s - 1]) + p * 9 - 9);
}

static TBody *body(int s, int p, int m)
{
    TBody *b = far_host(ds.Galaxy_Bodies[s - 1]);
    if (m == 0)
        return &b[p - 1];
    return &b[*((uint8_t *)far_host(ds.Galaxy_MoonLists[s - 1]) + p * 9 - 9 + m) - 1];
}

// New Earth half of the time, else anywhere.
static void location(DiffRng *r, uint8_t *s, uint8_t *p, uint8_t *m)
{
    if (diff_chance(r, 50)) {
        *s = 1;
        *p = 5;
        *m = 0;
        return;
    }
    *s = (uint8_t)diff_pick(r, 1, 1, 2, diff_range(r, 1, 8));
    *p = (uint8_t)diff_range(r, 1, ds.Galaxy_PlanetCount[*s - 1]);
    int n = moons_of(*s, *p);
    *m = (uint8_t)(n > 0 && diff_chance(r, 30) ? diff_range(r, 1, n) : 0);
}

static void random_units(DiffRng *r, TGroup *g)
{
    for (int t = 0; t < 2; t++)
        for (int u = 0; u < 4; u++) {
            TGroupSlot *s = &g->Units[t][u];
            s->ShipType = (int16_t)diff_pick(r, 0, 1, 2, 5, 10, 999, 1000, diff_range(r, 0, 300));
            for (int w = 0; w < 4; w++)
                s->Weapons[w] = (int16_t)diff_pick(r, 0, 0, 0, 1, 3, diff_range(r, 0, 60));
        }
    for (int i = 0; i < 6; i++) {
        g->Cargo[i].A = (int16_t)diff_pick(r, 0, 0, diff_range(r, 0, 100));
        g->Cargo[i].B = (int16_t)diff_pick(r, 0, 0, diff_range(r, 0, 100));
    }
    for (int i = 0; i < 13; i++)
        g->Items[i] = (int16_t)diff_pick(r, 0, diff_range(r, 0, 100));
}

static void random_group(DiffRng *r, TGroup *g, int list)
{
    g->Kind = (uint8_t)(list == 1 ? 5 : diff_range(r, 1, 4));
    static const char *names[] = {"Fleet", "Alpha group", "", "Satellites 17 abc"};
    const char *n = names[diff_rand(r, 4)];
    g->Name.len = (uint8_t)strlen(n);
    memcpy(g->Name.s, n, g->Name.len);
    location(r, &g->System, &g->Planet, &g->Moon);
    g->Status = (uint8_t)(list == 1 ? 7 : diff_pick(r, 1, 1, 2, 2, 4, 5, 6));
    g->Leg1 = (int16_t)diff_range(r, 0, 200);
    g->Jump = (int16_t)diff_range(r, 0, 1);
    g->Leg2 = (int16_t)diff_range(r, 0, 200);
    random_units(r, g);
}

// The planet force at the selected group's location, as SetupGroupsHotspots sets it.
static void set_pf_ptr(void)
{
    const TGroup *g = Groups_Selected();
    int16_t pf = Groups_FindPlanetForce(g->System, g->Planet, g->Moon);
    PF_PTR = ds.Game_Groups + (uint16_t)((32 + pf - 1) * (int)sizeof(TGroup));
}

// A used slot category of the selected group (or, rarely, any 1..4).
static void slot_category(DiffRng *r)
{
    uint8_t kind = Groups_Selected()->Kind;
    int16_t c = (int16_t)diff_range(r, 1, 4);
    for (int k = 0; k < 4 && !Groups_SlotUsed(kind, c); k++)
        c = (int16_t)(c % 4 + 1);
    ds.Groups_SlotCategory = c;
}

// Fleets and planet forces, one of them selected; product states and stocks.
static void random_lists(DiffRng *r, bool keep_selection)
{
    seed(r);
    int fleets = diff_pick(r, 0, 1, 2, 5, diff_range(r, 1, 32));
    int forces = diff_range(r, 1, 6);
    for (int i = 1; i <= fleets; i++)
        random_group(r, Groups_At(0, (int16_t)i), 0);
    for (int i = 2; i <= forces; i++)
        random_group(r, Groups_At(1, (int16_t)i), 1);
    if (diff_chance(r, 70))
        random_units(r, Groups_At(1, 1));
    ds.Game_GroupCount = (int16_t)fleets;
    ds.Game_PlanetForceCount = (int16_t)forces;
    if (!keep_selection) {
        ds.Game_GroupList = (int16_t)(fleets == 0 ? 1 : diff_range(r, 0, 1));
        ds.Game_ShownGroupCount = ds.Game_GroupList == 0 ? ds.Game_GroupCount : ds.Game_PlanetForceCount;
        ds.Game_SelGroup = (int16_t)diff_pick(r, 1, 1, ds.Game_ShownGroupCount,
                                              diff_range(r, 1, ds.Game_ShownGroupCount));
    } else {
        if (ds.Game_GroupList == 0 && ds.Game_GroupCount < ds.Game_SelGroup)
            ds.Game_GroupCount = ds.Game_SelGroup;
        ds.Game_ShownGroupCount = ds.Game_GroupList == 0 ? ds.Game_GroupCount : ds.Game_PlanetForceCount;
    }
    if (ds.Game_GroupList == 0)
        ds.Game_SelGroup0 = ds.Game_SelGroup;
    else
        ds.Game_SelGroup1 = ds.Game_SelGroup;
    for (int k = 0; k < 35; k++) {
        TProduct *p = &ds.Research_Products[k];
        if (diff_chance(r, 50))
            p->State = (int16_t)diff_pick(r, 0, 1, 3, 5, 5, 5);
        p->Stock = (int16_t)diff_pick(r, 0, 0, 1, 7, diff_range(r, 0, 500));
    }
    const TGroup *g = Groups_Selected();
    TBody *b = body(g->System, g->Planet, g->Moon);
    if (diff_chance(r, 50)) {
        b->Owner = 1;
        b->Colony = 1;
    }
    ds.Groups_PlanetForceMode = (uint8_t)(ds.Game_GroupList == 1 && diff_chance(r, 50));
    ds.Groups_AtHomePlanet = g->System == 1 && g->Planet == 5 && g->Moon == 0;
    ds.Groups_CanChangeSlots = (uint8_t)diff_pick(r, 0, 1, 1);
    slot_category(r);
    set_pf_ptr();
}

// --- The screens ------------------------------------------------------------------

static void setup_groups_screen(DiffRng *r, int32_t *a)
{
    (void)a;
    random_lists(r, diff_chance(r, 30));
    ds.Game_HotspotHit = (int16_t)diff_pick(r, 0, 0x15, 0x16, 0x16, 0x17, 0x17, 0x19, 0x1A, 0x1C, 0x1D, 0x1E, 0x1F,
                                            0x20, diff_range(r, 0x21, 0x34), diff_range(r, 0x21, 0x34),
                                            diff_range(r, 0x1C, 0x20), 0x1B, 0x35);
    ds.Game_MenuCommand = (int16_t)diff_pick(r, 0, 0, 0, 0, 0, 0x2E, 0x25);
    if (ds.Game_GroupCount > 0 && diff_chance(r, 8)) {  // back from the first planet force to the fleets
        ds.Game_GroupList = 1;
        ds.Game_SelGroup = 1;
        ds.Game_ShownGroupCount = ds.Game_PlanetForceCount;
        ds.Groups_PlanetForceMode = 0;
        ds.Game_HotspotHit = 0x16;
    }
    if (ds.Game_MenuCommand != 0)  // a menu-bar button was clicked
        ds.Game_HotspotHit = (int16_t)diff_range(r, 1, 12);
    ds.Mouse_HeldLeft = (uint8_t)diff_range(r, 0, 1);
    if (diff_chance(r, 20)) {
        ds.Game_ScreenEntered = 1;
        ds.Game_PrevScreen = (int16_t)diff_pick(r, 0x10, 0x14, 0x11);
    }
}

static void setup_new_group(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    for (int k = 0; k < 4; k++)
        ds.Game_GroupKindAllowed[k] = (uint8_t)diff_pick(r, 0, 1, 1);
    ds.Game_GroupKindAllowed[diff_rand(r, 4)] = 1;
    TGroup *g = Groups_At(0, ds.Game_SelGroup);
    g->Kind = (uint8_t)diff_range(r, 1, 4);
    ds.Game_HotspotHit = (int16_t)diff_pick(r, 0, 0x15, 0x16, 0x16, 0x17);
    ds.Game_MenuCommand = (int16_t)diff_pick(r, 0, 0, 0x3C, 0x3A);
    if (diff_chance(r, 25))
        ds.Game_ScreenEntered = 1;
}

// --- Panels and hotspots ---------------------------------------------------------------

static void setup_panel(DiffRng *r, int32_t *a)
{
    random_lists(r, false);
    a[0] = diff_range(r, 0, 1);
}

static void setup_selected(DiffRng *r, int32_t *a)
{
    (void)a;
    random_lists(r, false);
    if (diff_chance(r, 30))
        ds.Groups_SlotCategory = (int16_t)diff_range(r, 1, 4);
}

static void setup_init_kind(DiffRng *r, int32_t *a)
{
    (void)a;
    random_lists(r, false);
    // Any selection number; list 0 cycles the kinds by SelGroup.
    ds.Game_SelGroup = (int16_t)diff_range(r, 1, 32);
}

static void setup_empty(DiffRng *r, int32_t *a)
{
    random_lists(r, false);
    a[0] = diff_range(r, 0, 1);
    a[1] = diff_range(r, 1, a[0] == 0 ? 32 : 6);
    TGroup *g = Groups_At((int16_t)a[0], (int16_t)a[1]);
    if (diff_chance(r, 30))
        g->Kind = (uint8_t)diff_range(r, 1, 5);
    if (diff_chance(r, 50))
        memset(g->Units, 0, sizeof g->Units);
    else if (diff_chance(r, 50)) {
        memset(g->Units, 0, sizeof g->Units);
        g->Units[diff_rand(r, 2)][diff_rand(r, 4)].ShipType = (int16_t)diff_pick(r, 1, -1, 0x100);
    }
}

static void setup_find(DiffRng *r, int32_t *a)
{
    random_lists(r, false);
    const TGroup *g = Groups_At(1, (int16_t)diff_range(r, 1, ds.Game_PlanetForceCount));
    if (diff_chance(r, 70)) {
        a[0] = g->System;
        a[1] = g->Planet;
        a[2] = g->Moon;
    } else {
        uint8_t s, p, m;
        location(r, &s, &p, &m);
        a[0] = s;
        a[1] = p;
        a[2] = m;
    }
    if (diff_chance(r, 20))
        ds.Game_PlanetForceCount = 0;
}

static void setup_none(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
}

static void setup_lists(DiffRng *r, int32_t *a)
{
    (void)a;
    random_lists(r, false);
}

// --- Base states -------------------------------------------------------------------------

// Enters `screen` from `prev` with the port's handler (as the main loop does on
// a switch) and keeps the state shown afterwards.
static DiffState *enter(int16_t screen, int16_t prev, void (*handler)(void))
{
    ds.Game_PrevScreen = prev;
    ds.Game_CurrentScreen = screen;
    ds.Game_ScreenEntered = 1;
    ds.Game_HotspotHit = 0;
    ds.Game_MenuCommand = 0;
    handler();
    ds.Game_ScreenEntered = 0;
    ds.Game_PrevScreen = screen;
    diff_quiesce();
    return diff_save();
}

// A fleet at New Earth (list 0, group 1), as "New group" leaves it.
static void make_fleet(uint8_t kind, uint8_t status)
{
    ds.Game_GroupList = 0;
    ds.Game_GroupCount = 1;
    ds.Game_ShownGroupCount = 1;
    ds.Game_SelGroup = 1;
    ds.Game_SelGroup0 = 1;
    Groups_ClearGroup();
    TGroup *g = Groups_At(0, 1);
    g->Kind = kind;
    g->Status = status;
    g->Units[0][0].ShipType = 3;
    g->Units[0][0].Weapons[0] = 2;
    for (int k = 0; k < 4; k++)
        ds.Game_GroupKindAllowed[k] = 1;
}

enum { STATES = 5 };

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    // A near routine although symbols.txt does not say so (ends in RET).
    diff_Groups_InitGroupOfKind.near = true;

    DiffState *bases[STATES];
    // 0: ship info (0x10); 1: group screen of the planet force (0x16), by clicks.
    if (!diff_boot("900:click:120,16;1300:quit"))
        return 1;
    bases[0] = diff_save();
    if (!diff_boot("900:click:120,16;1100:click:100,194;1300:click:32,68;1600:quit")
        || ds.Game_CurrentScreen != 0x16) {
        printf("FAILED to reach the group screen\n");
        return 1;
    }
    bases[1] = diff_save();
    // 2, 3: a fleet's group screen (docked, travelling); 4: the New Group screen.
    diff_load(bases[0]);
    make_fleet(1, 1);
    bases[2] = enter(0x16, 0x10, Groups_GroupsScreen);
    diff_load(bases[0]);
    make_fleet(2, 4);
    bases[3] = enter(0x16, 0x10, Groups_GroupsScreen);
    diff_load(bases[0]);
    make_fleet(1, 1);
    bases[4] = enter(0x15, 0x10, Groups_NewGroupScreen);

    // The screen handlers need their own screen's state.
    DiffState *groups_bases[] = {bases[1], bases[2], bases[3]};
    diff_run(&diff_Groups_GroupsScreen, groups_bases, 3, setup_groups_screen, diff_cases(250));
    diff_run(&diff_Groups_NewGroupScreen, &bases[4], 1, setup_new_group, diff_cases(30));

    const struct {
        DiffFn *fn;
        DiffSetup setup;
        int cases;
    } fns[] = {
        {&diff_Groups_LoadGroupsGfx, setup_none, 5},
        {&diff_Groups_DrawGroupPanel, setup_panel, 60},
        {&diff_Groups_SetupGroupsHotspots, setup_selected, 40},
        {&diff_Groups_DrawNewGroupNameAndType, setup_selected, 10},
        {&diff_Groups_SetupNewGroupHotspots, setup_none, 5},
        {&diff_Groups_ClearGroup, setup_selected, 10},
        {&diff_Groups_InitGroupOfKind, setup_init_kind, 30},
        {&diff_Groups_NextSlotCategory, setup_selected, 25},
        {&diff_Groups_PrevSlotCategory, setup_selected, 25},
        {&diff_Groups_FirstSlotCategory, setup_selected, 25},
        {&diff_Groups_IsGroupEmpty, setup_empty, 40},
        {&diff_Groups_IsGroupEmptyOfCargo, setup_empty, 40},
        {&diff_Groups_FindPlanetForce, setup_find, 30},
        {&diff_Groups_InitPlanetForce, setup_lists, 10},
        {&diff_Groups_Init, setup_none, 2},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, bases, STATES, fns[i].setup, diff_cases(fns[i].cases));
    for (int i = 0; i < STATES; i++)
        diff_free(bases[i]);
    return diff_finish();
}
