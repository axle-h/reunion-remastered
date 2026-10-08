// Differential test of unit BaseEff (1413): the planet info screen (8) with
// the tech-level buttons, original (under Unicorn) against the port. Cases
// enter the screen (ScreenEntered) from a new game's screens, or run a pass of
// a screen already entered (bases made by the port entering it at several
// bodies) with a command or hotspot. See docs/difftest.md. Needs the game data.
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/baseeff.h"
#include "game/dseg.h"
#include "game/groups.h"
#include "game/planetinfo.h"

DIFF_PROC(BaseEff_BaseEffScreen, BaseEff_BaseEffScreen());

static void seed(DiffRng *r)
{
    ds.System_RandSeed = (int32_t)diff_rand(r, 0xFFFFFFFFu);
}

typedef struct Loc {
    int16_t sys, planet, moon;
} Loc;

static const TMoonList *moon_list(int sys, int planet)
{
    return (const TMoonList *)far_host(ds.Galaxy_MoonLists[sys - 1]) + (planet - 1);
}

static TBody *body_at(Loc l)
{
    int idx = l.moon == 0 ? l.planet : moon_list(l.sys, l.planet)->Moons[l.moon - 1];
    return (TBody *)far_host(ds.Galaxy_Bodies[l.sys - 1]) + (idx - 1);
}

static Loc random_loc(DiffRng *r)
{
    Loc l;
    l.sys = (int16_t)diff_pick(r, 1, 1, 1, 2, 3, diff_range(r, 1, 8));
    l.planet = (int16_t)diff_range(r, 1, ds.Galaxy_PlanetCount[l.sys - 1]);
    if (l.sys == 1 && diff_chance(r, 40))
        l.planet = 5;
    int nm = moon_list(l.sys, l.planet)->Count;
    l.moon = (nm > 0 && diff_chance(r, 30)) ? (int16_t)diff_range(r, 1, nm) : 0;
    return l;
}

static void select_loc(Loc l)
{
    ds.Game_CurSystem = l.sys;
    ds.Game_CurPlanet[l.sys - 1] = l.planet;
    ds.Game_CurMoon[l.sys - 1] = l.moon;
    PlanetInfo_SelectCurBody();
}

static void perturb_body(DiffRng *r, TBody *b)
{
    b->Owner = (uint8_t)diff_pick(r, b->Owner, b->Owner, 0, 1, 1, 2, 4);
    b->Colony = (uint8_t)diff_pick(r, b->Colony, 0, 1);
    b->InfoLevel = (int8_t)diff_pick(r, b->InfoLevel, 0, 5, 6, 30, 39, 40, 60);
    b->TechLevel = (uint8_t)diff_pick(r, b->TechLevel, 0, 1, 6, 7, 7);
    b->Satellite = (uint8_t)diff_pick(r, b->Satellite, 0, 1);
    b->Surveyed = (uint8_t)diff_pick(r, b->Surveyed, 0, 1);
}

// --- Entering the screen ----------------------------------------------------------------

static void setup_enter(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    Loc l = random_loc(r);
    perturb_body(r, body_at(l));
    select_loc(l);
    ds.Game_CurrentScreen = 8;
    ds.Game_PrevScreen = (int16_t)diff_pick(r, 7, 7, 0x11, 1, 0x14);
    ds.Game_ScreenEntered = 1;
    ds.Game_HotspotHit = 0;
    ds.Game_MenuCommand = 0;
    ds.Mouse_HotspotCount = 0;
    ds.PlanetInfo_AlienMode = (uint8_t)diff_pick(r, 0, 1, 3);
}

// --- A pass of the entered screen -------------------------------------------------------

static void setup_pass(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    TBody *b = far_host(ds.Game_CurBody);
    if (diff_chance(r, 50))
        perturb_body(r, b);
    ds.Game_MenuCommand =
        (int16_t)(diff_chance(r, 50) ? 0 : diff_pick(r, 0x26, 0x26, 0x27, 0x27, 0x2E, 10, 0x46, 0x14, 5));
    // A command comes from a menu-bar button (hotspots 1..12), not with a screen hotspot.
    if (ds.Game_MenuCommand != 0)
        ds.Game_HotspotHit = (int16_t)diff_range(r, 1, 12);
    else
        ds.Game_HotspotHit = (int16_t)diff_pick(r, 0, 0x15, 0x15, 0x15, 0x16, 0x17, 0x17, 0x18);
    ds.PlanetInfo_AlienMode = (uint8_t)diff_pick(r, ds.PlanetInfo_AlienMode, 0, 0, 1, 3);
    ds.Research_Products[2].Stock = (int16_t)diff_pick(r, 0, 1, 3);
}

// The port enters screen 8 at l from the current state, as the main loop does
// after a switch.
static void enter_at(Loc l, uint8_t owner, int8_t info)
{
    if (l.moon > moon_list(l.sys, l.planet)->Count)
        l.moon = 0;
    TBody *b = body_at(l);
    if (owner != 0xFF) {
        b->Owner = owner;
        b->InfoLevel = info;
    }
    select_loc(l);
    ds.Game_PrevScreen = 7;
    ds.Game_CurrentScreen = 8;
    ds.Game_ScreenEntered = 1;
    ds.Game_ScreenSwitch = 0;
    ds.Game_HotspotHit = 0;
    ds.Game_MenuCommand = 0;
    ds.Mouse_HotspotCount = 0;
    BaseEff_BaseEffScreen();
    ds.Game_PrevScreen = 8;
    ds.Game_ScreenEntered = 0;
    diff_quiesce();
}

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    static const struct {
        const char *name;
        const char *script;
    } boots[] = {
        {"command centre", "1500:quit"},
        {"system view (7)", "900:click:168,16;1300:quit"},
        {"surface (20)", "900:click:216,16;1300:quit"},
    };
    enum { BOOTS = sizeof boots / sizeof boots[0] };
    DiffState *enter[BOOTS];
    for (int i = 0; i < BOOTS; i++) {
        if (!diff_boot(boots[i].script)) {
            printf("FAILED to reach %s\n", boots[i].name);
            return 1;
        }
        enter[i] = diff_save();
    }
    // Screen 8 entered from the system view at: New Earth (ours, a colony),
    // another planet of Amnesty, a moon, an alien planet well known.
    static const struct {
        Loc l;
        uint8_t owner;
        int8_t info;
    } at[] = {
        {{1, 5, 0}, 0xFF, 0},
        {{1, 3, 0}, 0xFF, 0},
        {{1, 7, 1}, 0, 10},
        {{2, 2, 0}, 3, 45},
        {{3, 1, 0}, 2, 39},
    };
    enum { PASSES = sizeof at / sizeof at[0] };
    DiffState *pass[PASSES];
    for (int i = 0; i < PASSES; i++) {
        diff_load(enter[1]);
        enter_at(at[i].l, at[i].owner, at[i].info);
        pass[i] = diff_save();
    }

    diff_run(&diff_BaseEff_BaseEffScreen, enter, BOOTS, setup_enter, diff_cases(30));
    diff_run(&diff_BaseEff_BaseEffScreen, pass, PASSES, setup_pass, diff_cases(150));
    for (int i = 0; i < BOOTS; i++)
        diff_free(enter[i]);
    for (int i = 0; i < PASSES; i++)
        diff_free(pass[i]);
    return diff_finish();
}
