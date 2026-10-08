// Differential test of unit ShipMove (22ca): screen 0x11 (a fleet at a planet:
// dock / orbit view, star field while travelling), its animations and the
// launch / docking sequences, original (under Unicorn) against the port. A new
// game has no fleets, so the base states put one in list 0 on the ship info
// screen and enter screen 0x11 with the port's handler (as the main loop does
// on a switch); the entry itself is compared too. See docs/difftest.md. Needs
// the game data.
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/groups.h"
#include "game/shipmove.h"

DIFF_PROC(ShipMove_GroupScreen, ShipMove_GroupScreen());
DIFF_PROC(ShipMove_SetupGroupScreenHotspots, ShipMove_SetupGroupScreenHotspots());
DIFF_PROC(ShipMove_InitOrbitView, ShipMove_InitOrbitView());
DIFF_PROC(ShipMove_InitDockView, ShipMove_InitDockView((int16_t)a[0]));
DIFF_PROC(ShipMove_RedrawBackdrop, ShipMove_RedrawBackdrop());
DIFF_PROC(ShipMove_LaunchAnimation, ShipMove_LaunchAnimation((int16_t)a[0]));
DIFF_PROC(ShipMove_DockingAnimation, ShipMove_DockingAnimation((int16_t)a[0]));
DIFF_PROC(ShipMove_BlitViewport, ShipMove_BlitViewport((FarPtr)a[0]));
DIFF_PROC(ShipMove_DrawButtonAnimFrame, ShipMove_DrawButtonAnimFrame((int16_t)a[0]));
DIFF_PROC(ShipMove_DrawDockDoorFrame, ShipMove_DrawDockDoorFrame((int16_t)a[0]));
DIFF_PROC(ShipMove_AnimateBlinkLight, ShipMove_AnimateBlinkLight());
DIFF_PROC(ShipMove_AnimateSpinner, ShipMove_AnimateSpinner());
DIFF_PROC(ShipMove_AnimateLights, ShipMove_AnimateLights());
DIFF_PROC(ShipMove_AnimateStarfield, ShipMove_AnimateStarfield());
DIFF_PROC(ShipMove_InitStarfield, ShipMove_InitStarfield());
DIFF_PROC(ShipMove_AnimateGroupView, ShipMove_AnimateGroupView());

static int16_t *w16(uint16_t off) { return (int16_t *)ds_at(off); }
static uint8_t *b8(uint16_t off) { return (uint8_t *)ds_at(off); }

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

// The selected fleet (list 0, group 1 of 1..3).
static TGroup *fleet(void) { return Groups_Selected(); }

// --- The screen ---------------------------------------------------------------------

// Where the fleet is: New Earth, the story locations (1:7:0, 3:2:1, 7:1:0) or anywhere.
static void place_fleet(DiffRng *r, TGroup *g)
{
    switch (diff_rand(r, 6)) {
    case 0:
        g->System = 1, g->Planet = 7, g->Moon = 0;
        break;
    case 1:
        g->System = 3, g->Planet = 2, g->Moon = 1;
        break;
    case 2:
        g->System = 7, g->Planet = 1, g->Moon = 0;
        break;
    case 3:
        g->System = 1, g->Planet = 5, g->Moon = 0;
        break;
    default:
        g->System = (uint8_t)diff_range(r, 1, 8);
        g->Planet = (uint8_t)diff_range(r, 1, ds.Galaxy_PlanetCount[g->System - 1]);
        g->Moon = (uint8_t)(moons_of(g->System, g->Planet) > 0 && diff_chance(r, 30)
                                ? diff_range(r, 1, moons_of(g->System, g->Planet))
                                : 0);
        break;
    }
}

static void perturb_fleet(DiffRng *r, bool keep_place)
{
    seed(r);
    TGroup *g = fleet();
    if (!keep_place) {
        place_fleet(r, g);
        g->Status = (uint8_t)diff_pick(r, 1, 1, 2, 2, 4, 5, 6);
    }
    g->Kind = (uint8_t)diff_pick(r, 1, 1, 2, 3, 4, 4);
    if (diff_chance(r, 30))
        memset(g->Units, 0, sizeof g->Units);
    else
        g->Units[0][0].ShipType = (int16_t)diff_pick(r, 1, 5, 20);
    TBody *b = body(g->System, g->Planet, g->Moon);
    b->InfoLevel = (int8_t)diff_pick(r, b->InfoLevel, 0, 5, 6, 30);
    if (diff_chance(r, 20))
        b->SurfaceType = (uint8_t)diff_pick(r, 1, 2, 3, 5, 11);
    TBody *p7 = body(1, 7, 0);
    p7->Owner = (uint8_t)diff_pick(r, p7->Owner, 0, 1, 2, 3);
    *w16(0x6004) = (int16_t)diff_pick(r, 0, 0, 1);  // story flags of the landings
    *w16(0x6216) = (int16_t)diff_pick(r, 0, 0, 1);
    *w16(0x63F3) = (int16_t)diff_pick(r, 0, 0, 1);
    for (int k = 0; k < 35; k++)
        if (diff_chance(r, 20))
            ds.Research_Products[k].State = (int16_t)diff_pick(r, 0, 0, 1, 5);
    ds.Game_StaffExp[0] = (uint16_t)diff_pick(r, 0, 0, 5, 30);
}

static void setup_screen(DiffRng *r, int32_t *a)
{
    (void)a;
    perturb_fleet(r, diff_chance(r, 50));
    ds.Game_HotspotHit = (int16_t)diff_pick(r, 0, 0x15, 0x16, 0x16, 0x16, 0x17, 0x18, 0x18, 0x19, 0x1A);
    ds.Game_MenuCommand = 0;
    if (diff_chance(r, 10)) {  // a menu-bar button
        ds.Game_HotspotHit = (int16_t)diff_range(r, 1, 12);
        ds.Game_MenuCommand = (int16_t)diff_pick(r, 0x2E, 0x2E, 3, 0x29);
    }
    if (diff_chance(r, 20)) {
        ds.Game_ScreenEntered = 1;
        ds.Game_PrevScreen = (int16_t)diff_pick(r, 0x10, 0x16, 0x12, 0x12);
        *b8(0x787C) = (uint8_t)diff_pick(r, 0, 0, 1);
        // A destination chosen on the system view (screen 0x12).
        ds.Game_CurSystem = (int16_t)diff_pick(r, fleet()->System, diff_range(r, 1, 8));
        int s = ds.Game_CurSystem;
        ds.Game_CurPlanet[s - 1] = (int16_t)diff_range(r, 1, ds.Galaxy_PlanetCount[s - 1]);
        ds.Game_CurMoon[s - 1] = 0;
    }
}

static void setup_hotspots(DiffRng *r, int32_t *a)
{
    (void)a;
    perturb_fleet(r, false);
    fleet()->Status = (uint8_t)diff_range(r, 0, 7);
}

// --- Views and animations -------------------------------------------------------------

static void setup_surface(DiffRng *r, int32_t *a)
{
    seed(r);
    a[0] = diff_range(r, 0, 12);
}

static void setup_frame(DiffRng *r, int32_t *a)
{
    seed(r);
    a[0] = diff_range(r, 1, 4);
}

static void setup_blit(DiffRng *r, int32_t *a)
{
    a[0] = (int32_t)diff_pick(r, (int32_t)ds.ShipMove_Backdrop, (int32_t)ds.ShipMove_AnimSheet,
                              (int32_t)far_ptr((uint16_t)(ds.ShipMove_Backdrop >> 16), 0x35C0));
}

static void setup_anim(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    *w16(0x7802) = (int16_t)diff_range(r, 0, 5);   // BlinkCounter
    *w16(0x7804) = (int16_t)diff_range(r, 0, 31);  // SpinnerCounter
    for (int i = 1; i <= 6; i++)
        *b8((uint16_t)(0x7805 + i)) = (uint8_t)diff_range(r, 0, 1);
    TGroup *g = fleet();
    g->Status = (uint8_t)diff_pick(r, g->Status, 1, 2, 3, 4, 5, 6);
}

static void setup_none(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
}

// --- Base states -----------------------------------------------------------------------

// A fleet in list 0 (group `sel` of 3) at a body, with a few ships.
static void make_fleet(int16_t sel, uint8_t status, uint8_t s, uint8_t p, uint8_t m)
{
    ds.Game_GroupList = 0;
    ds.Game_GroupCount = 3;
    ds.Game_ShownGroupCount = 3;
    for (int16_t i = 1; i <= 3; i++) {
        ds.Game_SelGroup = i;
        Groups_ClearGroup();
    }
    ds.Game_SelGroup = sel;
    ds.Game_SelGroup0 = sel;
    TGroup *g = fleet();
    g->Kind = 1;
    g->Status = status;
    g->System = s;
    g->Planet = p;
    g->Moon = m;
    g->Units[0][0].ShipType = 3;
    if (status > 3) {
        g->Leg1 = 0;
        g->Jump = 0;
        g->Leg2 = 100;
    }
}

// Enters screen 0x11 from the ship info screen with the port's handler, then
// runs `frames` passes of its animation as the main loop does.
static DiffState *enter(int frames)
{
    ds.Game_PrevScreen = 0x10;
    ds.Game_CurrentScreen = 0x11;
    ds.Game_ScreenEntered = 1;
    ds.Game_HotspotHit = 0;
    ds.Game_MenuCommand = 0;
    ShipMove_GroupScreen();
    ds.Game_ScreenEntered = 0;
    ds.Game_PrevScreen = 0x11;
    for (int i = 0; i < frames; i++)
        ShipMove_AnimateGroupView();
    diff_quiesce();
    return diff_save();
}

enum { STATES = 4 };

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    // The ship info screen (0x10) of a new game.
    if (!diff_boot("900:click:120,16;1300:quit"))
        return 1;
    DiffState *ship_info = diff_save();
    DiffState *bases[STATES];
    make_fleet(1, 1, 1, 5, 0);  // docked at New Earth
    bases[0] = enter(10);
    diff_load(ship_info);
    make_fleet(2, 2, 1, 3, 0);  // in orbit of another planet
    bases[1] = enter(10);
    diff_load(ship_info);
    make_fleet(3, 4, 1, 4, 0);  // flying within the system
    bases[2] = enter(30);
    diff_load(ship_info);
    make_fleet(1, 6, 2, 1, 0);  // in hyperspace
    bases[3] = enter(30);

    // Entering from the ship info screen.
    for (int i = 0; i < STATES; i++) {
        diff_load(bases[i]);
        ds.Game_PrevScreen = 0x10;
        ds.Game_ScreenEntered = 1;
        char label[32];
        snprintf(label, sizeof label, "entry %d", i);
        diff_call(&diff_ShipMove_GroupScreen, nullptr, label);
    }

    const struct {
        DiffFn *fn;
        DiffSetup setup;
        int cases;
    } fns[] = {
        {&diff_ShipMove_GroupScreen, setup_screen, 150},
        {&diff_ShipMove_SetupGroupScreenHotspots, setup_hotspots, 24},
        {&diff_ShipMove_InitOrbitView, setup_none, 4},
        {&diff_ShipMove_InitDockView, setup_surface, 13},
        {&diff_ShipMove_RedrawBackdrop, setup_none, 4},
        {&diff_ShipMove_LaunchAnimation, setup_surface, 13},
        {&diff_ShipMove_DockingAnimation, setup_surface, 13},
        {&diff_ShipMove_BlitViewport, setup_blit, 8},
        {&diff_ShipMove_DrawButtonAnimFrame, setup_frame, 8},
        {&diff_ShipMove_DrawDockDoorFrame, setup_frame, 8},
        {&diff_ShipMove_AnimateBlinkLight, setup_anim, 24},
        {&diff_ShipMove_AnimateSpinner, setup_anim, 16},
        {&diff_ShipMove_AnimateLights, setup_anim, 16},
        {&diff_ShipMove_AnimateStarfield, setup_anim, 24},
        {&diff_ShipMove_InitStarfield, setup_none, 8},
        {&diff_ShipMove_AnimateGroupView, setup_anim, 24},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, bases, STATES, fns[i].setup, diff_cases(fns[i].cases));
    diff_free(ship_info);
    for (int i = 0; i < STATES; i++)
        diff_free(bases[i]);
    return diff_finish();
}
