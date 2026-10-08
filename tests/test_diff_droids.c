// Differential test of unit Droids (2527): the miner droid shop (screen 4),
// original (under Unicorn) against the port. Cases enter the screen from the
// surface of New Earth or of another colony, or run a pass of the entered
// screen (bases made by the port entering it) with the add-droid command or
// the back hotspot, with droids, stock, ores and the buildings that give
// capacity perturbed. See docs/difftest.md. Needs the game data.
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/droids.h"
#include "game/dseg.h"
#include "game/groups.h"
#include "game/planetinfo.h"

DIFF_PROC(Droids_DroidsScreen, Droids_DroidsScreen());
DIFF_PROC(Droids_DrawDroidsScreen, Droids_DrawDroidsScreen());
DIFF_PROC(Droids_SetupDroidsHotspots, Droids_SetupDroidsHotspots());

typedef struct Loc {
    int16_t sys, planet, moon;
} Loc;

static const Loc HOME = {1, 5, 0};
static const Loc COLONY = {1, 3, 0};
static const Loc MOON = {2, 1, 1};

static const TMoonList *moon_list(int sys, int planet)
{
    return (const TMoonList *)far_host(ds.Galaxy_MoonLists[sys - 1]) + (planet - 1);
}

static TBody *body_at(Loc l)
{
    int idx = l.moon == 0 ? l.planet : moon_list(l.sys, l.planet)->Moons[l.moon - 1];
    return (TBody *)far_host(ds.Galaxy_Bodies[l.sys - 1]) + (idx - 1);
}

static void select_loc(Loc l)
{
    if (l.moon > moon_list(l.sys, l.planet)->Count)
        l.moon = 0;
    ds.Game_CurSystem = l.sys;
    ds.Game_CurPlanet[l.sys - 1] = l.planet;
    ds.Game_CurMoon[l.sys - 1] = l.moon;
    PlanetInfo_SelectCurBody();
}

// A colony of ours at l with its planet force (droid stock in Items[13]).
static void make_colony(Loc l)
{
    if (l.moon > moon_list(l.sys, l.planet)->Count)
        l.moon = 0;
    TBody *b = body_at(l);
    b->Owner = 1;
    b->Colony = 1;
    for (int i = 1; i <= ds.Game_PlanetForceCount; i++) {
        TGroup *g = Groups_At(1, (int16_t)i);
        if (g->System == l.sys && g->Planet == l.planet && g->Moon == l.moon)
            return;
    }
    int16_t n = ++ds.Game_PlanetForceCount;
    TGroup *g = Groups_At(1, n);
    memset(g, 0, sizeof *g);
    g->Kind = 5;
    g->Status = 7;
    g->System = (uint8_t)l.sys;
    g->Planet = (uint8_t)l.planet;
    g->Moon = (uint8_t)l.moon;
    g->Items[12] = 3;
}

static void add_building(Loc l, uint8_t kind, uint8_t health)
{
    if (ds.Game_BuildingCount >= 990)
        return;
    TBuilding *b = (TBuilding *)far_host(ds.Game_Buildings) + ds.Game_BuildingCount++;
    memset(b, 0, sizeof *b);
    b->Kind = kind;
    b->System = (uint8_t)l.sys;
    b->Planet = (uint8_t)l.planet;
    b->Moon = (uint8_t)l.moon;
    b->Health = health;
    b->Flag = 1;
}

static int16_t force_at(Loc l)
{
    return Groups_FindPlanetForce(l.sys, l.planet, l.moon);
}

static Loc cur_loc(void)
{
    int16_t s = ds.Game_CurSystem;
    return (Loc){s, ds.Game_CurPlanet[s - 1], ds.Game_CurMoon[s - 1]};
}

// Droids, stock, ores and capacity of the current body.
static void perturb(DiffRng *r)
{
    Loc l = cur_loc();
    TBody *b = far_host(ds.Game_CurBody);
    b->_0A = (uint8_t)diff_pick(r, 0, 1, 2, 4, 5, 8, 9, 9, diff_range(r, 0, 14));
    if (diff_chance(r, 20))
        b->Colony = (uint8_t)diff_pick(r, 0, 1);
    if (diff_chance(r, 20))
        b->Owner = (uint8_t)diff_pick(r, 0, 1, 2);
    if (diff_chance(r, 8)) { // ours, but neither a colony nor droids
        b->Owner = 1;
        b->Colony = 0;
        b->_0A = 0;
    }
    for (int k = 0; k < 6; k++) {
        if (diff_chance(r, 50))
            b->Deposits[k] = (uint8_t)diff_pick(r, 0, 5, 9, 10, 11, 50, 99, 100, 255);
        if (diff_chance(r, 50))
            b->Stores[k] = diff_pick(r, 0, 0, 1, 9, 99999, 999999, 1234567, -5);
    }
    for (int k = 0; k < 6; k++)
        if (diff_chance(r, 30))
            ds.Game_HomeStock[k] = diff_pick(r, 0, 1, 45, 999999, 1000000);
    ds.Research_Products[1].Stock = (int16_t)diff_pick(r, 0, 1, 2, 9, 10, 99, 100, 150);
    int16_t f = force_at(l);
    if (f > 0)
        Groups_At(1, f)->Items[12] = (int16_t)diff_pick(r, 0, 1, 2, 9, 10, 99, 100, 150);
    int n = diff_pick(r, 0, 0, 1, 2, 3, 6);
    for (int i = 0; i < n; i++)
        add_building(l, (uint8_t)diff_pick(r, 0x19, 0x19, 4, 4, 3), (uint8_t)diff_pick(r, 0, 0, 0, 40));
    if (diff_chance(r, 10))
        for (int i = 0; i < 100; i++)
            add_building(l, 0x19, 0);
}

// --- Entering the screen ------------------------------------------------------------------

static void setup_enter(DiffRng *r, int32_t *a)
{
    (void)a;
    Loc l = diff_pick(r, 0, 0, 1, 2) == 0 ? HOME : diff_chance(r, 50) ? COLONY : MOON;
    if (l.sys != 1 || l.planet != 5)
        make_colony(l);
    select_loc(l);
    perturb(r);
    ds.Game_CurrentScreen = 4;
    ds.Game_PrevScreen = 0x14;
    ds.Game_ScreenEntered = 1;
    ds.Game_HotspotHit = 0;
    ds.Game_MenuCommand = 0;
    ds.Mouse_HotspotCount = 0;
}

// --- A pass of the entered screen --------------------------------------------------------------

static void setup_pass(DiffRng *r, int32_t *a)
{
    (void)a;
    perturb(r);
    ds.Game_MenuCommand = (int16_t)diff_pick(r, 0, 0x19, 0x19, 0x19);
    ds.Game_HotspotHit = (int16_t)(ds.Game_MenuCommand != 0 ? diff_range(r, 1, 12) : diff_pick(r, 0, 0x15, 0x16));
}

static void setup_none(DiffRng *r, int32_t *a)
{
    (void)a;
    ds.Mouse_HotspotCount = (int16_t)diff_range(r, 0, 9);
}

static void enter_at(Loc l)
{
    if (l.sys != 1 || l.planet != 5)
        make_colony(l);
    select_loc(l);
    ds.Game_PrevScreen = 0x14;
    ds.Game_CurrentScreen = 4;
    ds.Game_ScreenEntered = 1;
    ds.Game_ScreenSwitch = 0;
    ds.Game_HotspotHit = 0;
    ds.Game_MenuCommand = 0;
    ds.Mouse_HotspotCount = 0;
    Droids_DroidsScreen();
    ds.Game_PrevScreen = 4;
    ds.Game_ScreenEntered = 0;
    diff_quiesce();
}

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    if (!diff_boot("900:click:216,16;1300:quit")) {
        printf("FAILED to reach the surface\n");
        return 1;
    }
    DiffState *surface = diff_save();
    static const Loc at[] = {{1, 5, 0}, {1, 3, 0}, {2, 1, 1}};
    enum { PASSES = sizeof at / sizeof at[0] };
    DiffState *pass[PASSES];
    for (int i = 0; i < PASSES; i++) {
        diff_load(surface);
        enter_at(at[i]);
        pass[i] = diff_save();
    }

    diff_run(&diff_Droids_DroidsScreen, &surface, 1, setup_enter, diff_cases(30));
    diff_run(&diff_Droids_DroidsScreen, pass, PASSES, setup_pass, diff_cases(120));
    diff_run(&diff_Droids_DrawDroidsScreen, pass, PASSES, setup_pass, diff_cases(120));
    diff_run(&diff_Droids_SetupDroidsHotspots, pass, PASSES, setup_none, diff_cases(6));
    diff_free(surface);
    for (int i = 0; i < PASSES; i++)
        diff_free(pass[i]);
    return diff_finish();
}
