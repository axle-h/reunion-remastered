// Differential test of unit Launch (18e0): the system map (screen 7) and the
// destination chooser (screen 18), original (under Unicorn) against the port.
// Cases enter a screen, or run a pass of an entered screen (bases made by the
// port entering it: the galaxy, a planet with our fleets and alien forces,
// the destination chooser) with a hotspot clicked or held, or a command.
// See docs/difftest.md. Needs the game data.
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/groups.h"
#include "game/launch.h"
#include "game/planetinfo.h"

DIFF_PROC(Launch_MapScreen, Launch_MapScreen());
DIFF_PROC(Launch_DestinationScreen, Launch_DestinationScreen());

static int16_t *w16(uint16_t off) { return (int16_t *)ds_at(off); }

static void seed(DiffRng *r)
{
    ds.System_RandSeed = (int32_t)diff_rand(r, 0xFFFFFFFFu);
}

static int16_t *cur_planet(void) { return &ds.Game_CurPlanet[ds.Game_CurSystem - 1]; }
static int16_t *cur_moon(void) { return &ds.Game_CurMoon[ds.Game_CurSystem - 1]; }

// Fleets: n groups in list 0, most at 1:5 or 1:3 (on the ground, in orbit or
// arriving), with or without hyperdrives.
static void make_fleets(int n)
{
    static const uint8_t kinds[] = {1, 2, 3, 4, 1, 2};
    static const uint8_t planets[] = {5, 5, 5, 3, 5, 3};
    static const uint8_t moons[] = {0, 1, 0, 0, 2, 1};
    static const uint8_t status[] = {2, 1, 2, 2, 3, 1};
    ds.Game_GroupCount = (int16_t)n;
    for (int i = 1; i <= n; i++) {
        TGroup *g = Groups_At(0, (int16_t)i);
        memset(g, 0, sizeof *g);
        int k = (i - 1) % 6;
        g->Kind = kinds[k];
        g->Name.len = 6;
        memcpy(g->Name.s, "Fleet1", 6);
        g->Name.s[5] = (char)('0' + i);
        g->System = 1;
        g->Planet = planets[k];
        g->Moon = moons[k];
        g->Status = status[k];
        g->Units[0][0].ShipType = 1;
        g->Units[0][0].Weapons[0] = 2;
        if (i % 2)
            g->Units[0][3].ShipType = 2; // a hyperdrive
    }
    ds.Game_SelGroup = 1;
    ds.Game_SelGroup0 = 1;
    ds.Game_GroupList = 0;
}

// Alien forces of races 2 and 5 at 1:5, the race known and at war or not.
static void make_aliens(void)
{
    for (int k = 0; k < 2; k++) {
        TRace *rc = &ds.Races_Races[k == 0 ? 0 : 3];
        rc->Status = (int8_t)(k == 0 ? 6 : 2);
        rc->ForceCount = 2;
        for (int f = 0; f < 2; f++) {
            TRaceForce *fc = &rc->Forces[f];
            fc->Mission = 2;
            fc->Active = 1;
            fc->System = 1;
            fc->Body = 5;
            fc->Moon = (uint8_t)f;
        }
    }
}

// The port enters screen `screen` from `prev`, as the main loop does after a switch.
static void enter(int16_t screen, int16_t prev, void (*handler)(void))
{
    ds.Game_PrevScreen = prev;
    ds.Game_CurrentScreen = screen;
    ds.Game_ScreenEntered = 1;
    ds.Game_ScreenSwitch = 0;
    ds.Game_HotspotHit = 0;
    ds.Game_MenuCommand = 0;
    ds.Mouse_HotspotCount = 0;
    handler();
    ds.Game_PrevScreen = screen;
    ds.Game_ScreenEntered = 0;
    diff_quiesce();
}

// --- Entering --------------------------------------------------------------------------

static void setup_enter_common(DiffRng *r)
{
    seed(r);
    ds.Game_CurSystem = (int16_t)diff_pick(r, 1, 1, 1, 2, 3, diff_range(r, 1, 8));
    *cur_planet() = (int16_t)diff_pick(r, 0, 0, 1, 3, 5);
    if (*cur_planet() > ds.Galaxy_PlanetCount[ds.Game_CurSystem - 1])
        *cur_planet() = 1;
    *cur_moon() = (int16_t)diff_pick(r, 0, 0, 1);
    ds.Game_SelGroup = (int16_t)diff_range(r, 1, ds.Game_GroupCount);
    ds.Game_CurrentScreen = 7;
    ds.Game_ScreenEntered = 1;
    ds.Game_ScreenSwitch = 0;
    ds.Game_HotspotHit = 0;
    ds.Mouse_HotspotCount = 0;
    *w16(0x7C36) = (int16_t)diff_pick(r, 0, 1, 3);
}

static void setup_enter_map(DiffRng *r, int32_t *a)
{
    (void)a;
    setup_enter_common(r);
    ds.Game_PrevScreen = (int16_t)diff_pick(r, 1, 8, 0x11, 0x12, 0x12, 0x14, 0x16, 0x1D, 0x10, 3);
    ds.Game_MenuCommand = (int16_t)diff_pick(r, 0, 0, 0x22, 7);
    *(uint8_t *)ds_at(0x787C) = (uint8_t)diff_pick(r, 0, 0, 1);
}

static void setup_enter_dest(DiffRng *r, int32_t *a)
{
    (void)a;
    setup_enter_common(r);
    ds.Game_CurrentScreen = 0x12;
    ds.Game_PrevScreen = (int16_t)diff_pick(r, 7, 7, 0x11, 0x10, 0x16);
    ds.Game_MenuCommand = 0;
}

// --- Passes --------------------------------------------------------------------------------

// A click (button released) or a hold on a hotspot, or a menu command.
static void click(DiffRng *r, const int32_t *cmds, int ncmds)
{
    ds.Mouse_HeldLeft = (uint8_t)diff_pick(r, 0, 0, 1);
    ds.Mouse_Buttons = (uint8_t)diff_pick(r, 0, 0, 0, 1);
    if (diff_chance(r, 40) && ncmds > 0) {
        ds.Game_MenuCommand = (int16_t)cmds[diff_rand(r, (uint32_t)ncmds)];
        ds.Game_HotspotHit = (int16_t)diff_range(r, 1, 12);
    } else {
        ds.Game_MenuCommand = 0;
        ds.Game_HotspotHit = (int16_t)diff_pick(r, 0, 0x14, diff_range(r, 0x15, 0x1C), diff_range(r, 0x15, 0x1C),
                                                diff_range(r, 0x15, 0x26), diff_range(r, 0x15, 0x26), 0x27,
                                                diff_range(r, 0x27, 0x2F), diff_range(r, 0x28, 0x2F));
    }
}

static void perturb_staff(DiffRng *r)
{
    ds.Game_StaffExp[0] = (uint16_t)diff_pick(r, 0, 5, 5);
    ds.Game_StaffExp[2] = (uint16_t)diff_pick(r, 0, 5, 5);
    ds.Game_StaffHired[0] = (uint16_t)diff_pick(r, 0, 1, 2, 2, 3);
    ds.Game_StaffHired[2] = (uint16_t)diff_pick(r, 0, 1, 2, 3);
    ds.Research_Products[0xD].State = (int16_t)diff_pick(r, 0, 0, 5);
    ds.Research_Products[0xE].State = (int16_t)diff_pick(r, 0, 0, 5);
}

static void setup_map(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    perturb_staff(r);
    static const int32_t cmds[] = {0x21, 0x37, 0x37, 0x47, 0x47, 0x38, 0x38, 0x11};
    click(r, cmds, sizeof cmds / sizeof cmds[0]);
    // The selected entry and the toolbar state last shown.
    for (uint16_t off = 0x7C3A; off <= 0x7C3F; off++)
        *(uint8_t *)ds_at(off) = (uint8_t)diff_pick(r, 0, 0, 1);
    // Commands 0x37 and 0x47 are only offered for an alien entry, 0x38 for a group.
    int16_t sel = 0;
    for (int16_t i = 1; i <= 18; i++) {
        int16_t kind = *w16((uint16_t)(0x7BC4 + i * 6));
        if ((ds.Game_MenuCommand == 0x37 && kind >= 5) || (ds.Game_MenuCommand != 0x37 && kind >= 1 && kind < 5))
            if (sel == 0 || diff_chance(r, 40))
                sel = i;
    }
    *w16(0x7C36) = sel;
    if (sel > 0 && *w16((uint16_t)(0x7BC4 + sel * 6)) < 5)
        ds.Game_SelGroup = *w16((uint16_t)(0x7BC6 + sel * 6));
    else
        ds.Game_SelGroup = (int16_t)diff_range(r, 1, ds.Game_GroupCount);
    if (ds.Game_MenuCommand == 0x37 && sel == 0)
        ds.Game_MenuCommand = 0x21;
    TGroup *g = Groups_At(0, ds.Game_SelGroup);
    if (diff_chance(r, 20))
        g->Status = (uint8_t)diff_pick(r, 1, 2, 4);
    if (diff_chance(r, 15))
        memset(g->Units, 0, sizeof g->Units); // no ships
    // The body of the selected group, for the alien checks.
    if (diff_chance(r, 50)) {
        TBody *b = (TBody *)far_host(ds.Galaxy_Bodies[0]) + 4;
        b->Owner = (uint8_t)diff_pick(r, 1, 2, 5);
        b->InfoLevel = (int8_t)diff_pick(r, 10, 39, 40, 60);
    }
    for (int k = 0; k < 11; k++)
        if (diff_chance(r, 20))
            ds.Races_Races[k].Status = (int8_t)diff_pick(r, 0, 2, 5, 6);
}

static void setup_dest(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    perturb_staff(r);
    static const int32_t cmds[] = {0x39, 0x21, 0x21};
    click(r, cmds, sizeof cmds / sizeof cmds[0]);
    TGroup *g = Groups_At(0, ds.Game_SelGroup);
    if (diff_chance(r, 50)) {
        g->Kind = (uint8_t)diff_pick(r, 1, 2, 3, 4);
        g->Units[0][2].ShipType = (int16_t)diff_pick(r, 0, 0, 1);
        g->Units[0][3].ShipType = (int16_t)diff_pick(r, 0, 1, -1);
    }
    if (diff_chance(r, 30))
        ds.Game_CurSystem = (int16_t)diff_range(r, 1, 8);
}

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    if (!diff_boot("900:click:168,16;1300:quit")) {
        printf("FAILED to reach the system view\n");
        return 1;
    }
    make_fleets(6);
    make_aliens();
    DiffState *sysview = diff_save();

    // Bases of a pass: the system view of Amnesty (the boot's, re-entered
    // with the fleets); the planet view of New Earth (1:5) and of 1:3; the
    // destination chooser over the system and over New Earth.
    enum { MAP = 3, DEST = 2 };
    DiffState *map[MAP], *dest[DEST];
    // Each entry allocates the view's buffers (the main loop frees them when
    // the screen is left), so each starts from the boot's state.
    static const int16_t planets[MAP] = {0, 5, 3};
    for (int i = 0; i < MAP; i++) {
        diff_load(sysview);
        ds.Game_CurSystem = 1;
        *cur_planet() = planets[i];
        enter(7, 8, Launch_MapScreen);
        map[i] = diff_save();
    }
    for (int i = 0; i < DEST; i++) {
        diff_load(sysview);
        ds.Game_CurSystem = 1;
        *cur_planet() = planets[i];
        enter(0x12, 7, Launch_DestinationScreen);
        dest[i] = diff_save();
    }

    diff_run(&diff_Launch_MapScreen, &sysview, 1, setup_enter_map, diff_cases(30));
    diff_run(&diff_Launch_MapScreen, map, MAP, setup_map, diff_cases(240));
    diff_run(&diff_Launch_DestinationScreen, &sysview, 1, setup_enter_dest, diff_cases(20));
    diff_run(&diff_Launch_DestinationScreen, dest, DEST, setup_dest, diff_cases(160));
    diff_free(sysview);
    for (int i = 0; i < MAP; i++)
        diff_free(map[i]);
    for (int i = 0; i < DEST; i++)
        diff_free(dest[i]);
    return diff_finish();
}
