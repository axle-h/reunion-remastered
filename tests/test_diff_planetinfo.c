// Differential test of unit PlanetInfo (357b): the planet info screens, the
// system view, the body names and the building power balancing, original
// (under Unicorn) against the port, from new-game states with the current
// body and its fields perturbed at random. See docs/difftest.md. Needs the
// game data.
//
// Not tested directly: DrawOrbitingPlanet / DrawOrbitingMoon and the power
// helpers 3644..3ae5 are nested (covered through RenderSystemView and
// BalanceBuildingPower).
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/groups.h"
#include "game/planetinfo.h"

DIFF_PROC(PlanetInfo_SetupPlanetInfoHotspots, PlanetInfo_SetupPlanetInfoHotspots());
DIFF_PROC(PlanetInfo_DrawPlanetInfoText, PlanetInfo_DrawPlanetInfoText());
DIFF_PROC(PlanetInfo_DrawPlanetInfoScreen, PlanetInfo_DrawPlanetInfoScreen());
DIFF_PROC(PlanetInfo_LoadPlanetFajPic, PlanetInfo_LoadPlanetFajPic((int16_t)a[0], a[1] != 0));
DIFF_PROC(PlanetInfo_LoadPlanetPlanetPic, PlanetInfo_LoadPlanetPlanetPic((int16_t)a[0], a[1] != 0));
DIFF_PROC(PlanetInfo_DrawAlienInfoScreen, PlanetInfo_DrawAlienInfoScreen());
DIFF_PROC(PlanetInfo_DrawStoresList, PlanetInfo_DrawStoresList());
DIFF_PROC(PlanetInfo_DrawPlanetSurfaceBackdrop, PlanetInfo_DrawPlanetSurfaceBackdrop());
DIFF_PROC(PlanetInfo_SetupSystemViewHotspots, PlanetInfo_SetupSystemViewHotspots());
DIFF_PROC(PlanetInfo_InitSystemView, PlanetInfo_InitSystemView());
DIFF_PROC(PlanetInfo_LoadSystemViewAssets, PlanetInfo_LoadSystemViewAssets());
DIFF_PROC(PlanetInfo_ReloadSystemView, PlanetInfo_ReloadSystemView());
DIFF_PROC(PlanetInfo_RenderSystemView, PlanetInfo_RenderSystemView());
DIFF_PROC(PlanetInfo_UpdateOrbitPositions, PlanetInfo_UpdateOrbitPositions());
DIFF_PROC(PlanetInfo_SelectCurBody, PlanetInfo_SelectCurBody());
DIFF_FUNC(PlanetInfo_IsCurrentBody, PlanetInfo_IsCurrentBody((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_PROC(PlanetInfo_GetBodyName,
          PlanetInfo_GetBodyName(far_host((FarPtr)a[0]), (int16_t)a[1], (int16_t)a[2], (int16_t)a[3]));
DIFF_PROC(PlanetInfo_GetBodyFullName,
          PlanetInfo_GetBodyFullName(far_host((FarPtr)a[0]), (int16_t)a[1], (int16_t)a[2], (int16_t)a[3]));
DIFF_PROC(PlanetInfo_GetBodyLongName,
          PlanetInfo_GetBodyLongName(far_host((FarPtr)a[0]), (int16_t)a[1], (int16_t)a[2], (int16_t)a[3]));
DIFF_PROC(PlanetInfo_GetBodyLocationName,
          PlanetInfo_GetBodyLocationName(far_host((FarPtr)a[0]), (int16_t)a[1], (int16_t)a[2], (int16_t)a[3]));
DIFF_PROC(PlanetInfo_BalanceBuildingPower,
          PlanetInfo_BalanceBuildingPower((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_PROC(PlanetInfo_RandomizeProtectPage, PlanetInfo_RandomizeProtectPage());

static void seed(DiffRng *r)
{
    ds.System_RandSeed = (int32_t)diff_rand(r, 0xFFFFFFFFu);
}

static TBody *body_at(int sys, int idx)
{
    return (TBody *)far_host(ds.Galaxy_Bodies[sys - 1]) + (idx - 1);
}

static const TMoonList *moons_of(int sys, int planet)
{
    return (const TMoonList *)far_host(ds.Galaxy_MoonLists[sys - 1]) + (planet - 1);
}

// A body: system (mostly the Sun's), planet, moon (0 = the planet).
static void pick_location(DiffRng *r, int16_t *sys, int16_t *planet, int16_t *moon)
{
    *sys = (int16_t)diff_pick(r, 1, 1, 1, diff_range(r, 1, 8));
    *planet = (int16_t)diff_range(r, 1, ds.Galaxy_PlanetCount[*sys - 1]);
    int n = moons_of(*sys, *planet)->Count;
    *moon = (int16_t)(n > 0 && diff_chance(r, 40) ? diff_range(r, 1, n) : 0);
}

static int body_index(int sys, int planet, int moon)
{
    return moon == 0 ? planet : moons_of(sys, planet)->Moons[moon - 1];
}

static void perturb_body(DiffRng *r, TBody *b)
{
    b->Owner = (uint8_t)diff_pick(r, 0, 0, 1, 1, 1, 2, 3, diff_range(r, 2, 12));
    b->Colony = (uint8_t)diff_pick(r, 0, 0, 1);
    b->Miner = (int8_t)diff_pick(r, 0, 0, 1, 2);
    b->Surveyed = (uint8_t)diff_range(r, 0, 1);
    b->_03 = (uint8_t)diff_pick(r, 0, 1, 1);
    b->Satellite = (uint8_t)diff_pick(r, 0, 0, 1, 2, 0xF6);
    b->_09 = (uint8_t)diff_pick(r, 0, 0, 0, 1);
    b->_0A = (uint8_t)diff_pick(r, 0, 0, 1, 3);
    b->_0B = (uint8_t)diff_pick(r, 0, 0, 2);
    b->InfoLevel = (int8_t)diff_pick(r, 0, 2, 3, 5, 6, 9, 10, 19, 20, 29, 30, 31, 39, 40, 41, 60);
    b->Population = diff_pick(r, 0, 0, 1, 500, 40000, diff_range(r, 0, 250000));
    b->Attitude = (int8_t)diff_pick(r, 9, 10, 69, 70, -1, diff_range(r, 0, 69), diff_range(r, 0, 69));
    b->Resistance = (int8_t)diff_pick(r, 9, 10, 69, 70, -1, diff_range(r, 0, 69), diff_range(r, 0, 69));
    b->TechLevel = (uint8_t)diff_pick(r, 0, 3, 7, 8, diff_range(r, 0, 7));
    b->SurfaceType = (uint8_t)diff_range(r, 1, 10);
    b->Diameter = (uint16_t)diff_range(r, 0, 60000);
    b->Temperature = (int16_t)diff_range(r, -300, 900);
    for (int j = 0; j < 8; j++)
        b->Stores[j] = diff_pick(r, 0, diff_range(r, 0, 200000));
    for (int j = 0; j < 6; j++)
        b->Deposits[j] = (uint8_t)diff_pick(r, 0, 9, 10, diff_range(r, 0, 100));
}

// Selects a random body as the current one; perturbs it most of the time.
static TBody *pick_current(DiffRng *r, bool perturb)
{
    int16_t sys, planet, moon;
    pick_location(r, &sys, &planet, &moon);
    ds.Game_CurSystem = sys;
    ds.Game_CurPlanet[sys - 1] = planet;
    ds.Game_CurMoon[sys - 1] = moon;
    PlanetInfo_SelectCurBody();
    TBody *b = far_host(ds.Game_CurBody);
    if (perturb && diff_chance(r, 85))
        perturb_body(r, b);
    return b;
}

// Some buildings on the current body, some of them energy shields (kind 14h).
static void buildings_on(DiffRng *r, int sys, int planet, int moon, int n)
{
    TBuilding *bl = far_host(ds.Game_Buildings);
    for (int k = 0; k < n; k++) {
        TBuilding *b = &bl[ds.Game_BuildingCount++];
        memset(b, 0, sizeof *b);
        b->Kind = (uint8_t)diff_pick(r, 0x14, 0x16, 4, 5, 0x0B, 0x19, diff_range(r, 1, 25));
        b->System = (uint8_t)sys;
        b->Planet = (uint8_t)planet;
        b->Moon = (uint8_t)moon;
        b->X = (uint8_t)diff_range(r, 0, 30);
        b->Y = (uint8_t)diff_range(r, 0, 30);
        b->Health = (uint8_t)diff_pick(r, 0, 0, 0, 1, 50);
        b->Output = (uint8_t)diff_pick(r, 0, 50, 100, diff_range(r, 0, 200));
        b->Flag = (uint8_t)diff_range(r, 0, 1);
    }
}

static void setup_hotspots(DiffRng *r, int32_t *a)
{
    (void)a;
    ds.PlanetInfo_AlienMode = (uint8_t)diff_pick(r, 0, 1, 3);
}

static void setup_text(DiffRng *r, int32_t *a)
{
    (void)a;
    pick_current(r, true);
    if (diff_chance(r, 40))
        buildings_on(r, ds.Game_CurSystem, ds.Game_CurPlanet[ds.Game_CurSystem - 1],
                     ds.Game_CurMoon[ds.Game_CurSystem - 1], diff_range(r, 1, 4));
    if (diff_chance(r, 30)) {  // the space station line (New Earth only)
        ds.Game_CurSystem = 1;
        ds.Game_CurPlanet[0] = 5;
        ds.Game_CurMoon[0] = 0;
        PlanetInfo_SelectCurBody();
        TBody *b = far_host(ds.Game_CurBody);
        b->Owner = 1;
        ds.Research_Products[0x17].State = (int16_t)diff_pick(r, 5, 5, 4);
        ds.Research_Products[0x17].Stock = (int16_t)diff_pick(r, 0, 1, 3);
    }
}

static void setup_screen(DiffRng *r, int32_t *a)
{
    (void)a;
    pick_current(r, true);
    ds.Game_CurrentScreen = 8;
    ds.PlanetInfo_AlienMode = (uint8_t)diff_pick(r, 0, 0, 0, 1);
}

static void setup_pic(DiffRng *r, int32_t *a)
{
    ds.PlanetInfo_AlienMode = (uint8_t)diff_pick(r, 0, 0, 0, 1);
    a[0] = diff_range(r, 0, 10);
    a[1] = diff_range(r, 0, 1);
}

static void setup_faj(DiffRng *r, int32_t *a)
{
    setup_pic(r, a);
    a[0] = diff_range(r, 0, 13);
}

static void setup_alien(DiffRng *r, int32_t *a)
{
    (void)a;
    TBody *b = pick_current(r, true);
    b->Owner = (uint8_t)diff_range(r, 2, 12);
    *(uint8_t *)ds_at((uint16_t)(0x6517 + b->Owner)) = (uint8_t)diff_pick(r, 0, 1, 2, 2);
    // The race record's resource flags (bytes 13h..1Ah).
    uint8_t *race = ds_at((uint16_t)(0x6A06 + b->Owner * 0xE4));
    for (int k = 0x13; k <= 0x1A; k++)
        if (diff_chance(r, 30))
            race[k] = (uint8_t)diff_range(r, 0, 1);
}

static void setup_surface(DiffRng *r, int32_t *a)
{
    (void)a;
    pick_current(r, true);
}

// --- System view -----------------------------------------------------------------

static void random_groups(DiffRng *r)
{
    int n = diff_range(r, 0, 12);
    ds.Game_GroupCount = (int16_t)n;
    int sys = ds.Game_CurSystem;
    for (int i = 1; i <= n; i++) {
        TGroup *g = Groups_At(0, (int16_t)i);
        g->Kind = (uint8_t)diff_range(r, 1, 4);
        g->Name.len = 5;
        memcpy(g->Name.s, "Fleet", 5);
        g->System = (uint8_t)(diff_chance(r, 80) ? sys : diff_range(r, 1, 8));
        g->Planet = (uint8_t)(diff_chance(r, 70) ? ds.Game_CurPlanet[sys - 1] : diff_range(r, 0, 3));
        g->Moon = (uint8_t)diff_pick(r, 0, 0, 1);
        g->Status = (uint8_t)diff_pick(r, 1, 2, 3, 4, 7);
    }
    for (int k = 0; k < 11; k++) {
        TRace *rc = &ds.Races_Races[k];
        if (!diff_chance(r, 30))
            continue;
        rc->Status = (int8_t)diff_pick(r, 0, 2, 6, 6);
        rc->ForceCount = (uint8_t)diff_range(r, 0, 4);
        for (int f = 0; f < rc->ForceCount; f++) {
            TRaceForce *fc = &rc->Forces[f];
            fc->Active = (uint8_t)diff_pick(r, 0, 1, 1);
            fc->Mission = (uint8_t)diff_range(r, 0, 3);
            fc->System = (uint8_t)sys;
            fc->Body = (uint8_t)(diff_chance(r, 70) ? ds.Game_CurPlanet[sys - 1] : 1);
            fc->Moon = 0;
        }
    }
}

// The system or planet shown (screen 7 or 18), with bodies, fleets and the
// system list perturbed.
static void setup_view(DiffRng *r, bool planet_view)
{
    seed(r);
    int sys = diff_chance(r, 75) ? 1 : diff_range(r, 1, 8);
    ds.Game_CurSystem = (int16_t)sys;
    int planet = planet_view ? diff_range(r, 1, ds.Galaxy_PlanetCount[sys - 1]) : 0;
    ds.Game_CurPlanet[sys - 1] = (int16_t)planet;
    ds.Game_CurMoon[sys - 1] = 0;
    ds.Game_CurrentScreen = (int16_t)diff_pick(r, 7, 7, 0x12);
    ds.Galaxy_SystemStatus[sys - 1] = (int8_t)diff_pick(r, 1, 1, 1, 0, -1);
    for (int p = 0; p < 8; p++)
        if (diff_chance(r, 25))
            ds.Galaxy_PlanetStatus[sys - 1][p] = (int8_t)diff_pick(r, -1, 0, 1, 1);
    int n = ds.Galaxy_PlanetCount[sys - 1] + ds.Galaxy_MoonCount[sys - 1];
    for (int i = 1; i <= n; i++)
        if (diff_chance(r, 30))
            perturb_body(r, body_at(sys, i));
    if (diff_chance(r, 50)) {
        ds.Research_Products[0xD].State = (int16_t)diff_pick(r, 1, 5);
        ds.Research_Products[0xE].State = (int16_t)diff_pick(r, 1, 5);
    }
    *(uint8_t *)ds_at(0x5D90) = (uint8_t)diff_pick(r, 0, 0, 0, 1);
    *(int16_t *)ds_at(0x7C36) = (int16_t)diff_pick(r, 0, 0, 1, 2, 5);  // selected fleet
    *(int16_t *)ds_at(0x7C38) = (int16_t)diff_pick(r, 0, 1, 2, 3);     // marked body
    *(int16_t *)ds_at(0x7C42) = (int16_t)sys;
    *(int16_t *)ds_at(0x7C44) = (int16_t)diff_pick(r, planet, planet, 1);
    *(int16_t *)ds_at(0x7C46) = (int16_t)diff_pick(r, 0, 5, 8, 9, 20);  // blink countdown
    random_groups(r);
}

// The orbit records at DS:7B28 + 18 * k.
static void perturb_orbits(DiffRng *r)
{
    for (int k = 0; k <= 8; k++) {
        int16_t *o = (int16_t *)ds_at((uint16_t)(0x7B28 + k * 18));
        if (diff_chance(r, 50))
            o[2] = (int16_t)diff_range(r, 0, 3599);  // angle
        uint8_t *flag = (uint8_t *)ds_at((uint16_t)(0x7B28 + k * 18 + 17));
        if (diff_chance(r, 30))
            *flag = (uint8_t)diff_pick(r, 0, 1, 2, 5);
    }
}

static void setup_sv_hotspots(DiffRng *r, int32_t *a)
{
    (void)a;
    setup_view(r, diff_chance(r, 60));
    if (diff_chance(r, 15)) {  // the sun of a known, unexplored system (screen 18)
        ds.Game_CurPlanet[ds.Game_CurSystem - 1] = 0;
        ds.Game_CurrentScreen = 0x12;
        ds.Galaxy_SystemStatus[ds.Game_CurSystem - 1] = 0;
    }
}

static void setup_sv_load(DiffRng *r, int32_t *a)
{
    (void)a;
    setup_view(r, diff_chance(r, 40));
}

static void setup_sv_render(DiffRng *r, int32_t *a)
{
    (void)a;
    setup_view(r, diff_chance(r, 50));
    PlanetInfo_SetupSystemViewHotspots();
    perturb_orbits(r);
    *(uint8_t *)ds_at(0x7B26) = (uint8_t)diff_pick(r, 0, 0, 0, 1);
}

// RenderSystemView draws the orbit records as UpdateOrbitPositions left them
// (stale records of the other view can make sizes that take the original
// minutes to blit).
static void setup_sv_frame(DiffRng *r, int32_t *a)
{
    setup_sv_render(r, a);
    PlanetInfo_UpdateOrbitPositions();
    for (int k = 0; k <= 8; k++)
        if (diff_chance(r, 30))
            *(uint8_t *)ds_at((uint16_t)(0x7B28 + k * 18 + 17)) = (uint8_t)diff_pick(r, 0, 1, 2, 5);
}

// --- Names, current body ------------------------------------------------------------

static void setup_select(DiffRng *r, int32_t *a)
{
    (void)a;
    int16_t sys, planet, moon;
    pick_location(r, &sys, &planet, &moon);
    ds.Game_CurSystem = sys;
    ds.Game_CurPlanet[sys - 1] = planet;
    ds.Game_CurMoon[sys - 1] = moon;
    TBody *b = body_at(sys, body_index(sys, planet, moon));
    b->Owner = (uint8_t)diff_pick(r, 0, 1, 1, 2);
    b->Colony = (uint8_t)diff_pick(r, 0, 1, 2);
    ds.Game_CurBodyIsColony = (uint16_t)diff_pick(r, 0, 0x100, 0xFF01);
}

static void setup_is_current(DiffRng *r, int32_t *a)
{
    int16_t sys, planet, moon;
    pick_location(r, &sys, &planet, &moon);
    ds.Game_CurSystem = sys;
    ds.Game_CurPlanet[sys - 1] = planet;
    ds.Game_CurMoon[sys - 1] = moon;
    a[0] = diff_chance(r, 70) ? sys : diff_range(r, 1, 8);
    a[1] = diff_chance(r, 70) ? planet : diff_range(r, 0, 8);
    a[2] = diff_chance(r, 70) ? moon : diff_range(r, 0, 3);
}

static void setup_name(DiffRng *r, int32_t *a)
{
    int16_t sys, planet, moon;
    pick_location(r, &sys, &planet, &moon);
    a[0] = (int32_t)DIFF_SCRATCH;
    a[1] = sys;
    a[2] = planet;
    a[3] = moon;
    memset(far_host(DIFF_SCRATCH), 0xCC, 256);
}

// --- Power -------------------------------------------------------------------------

static void setup_power(DiffRng *r, int32_t *a)
{
    int16_t sys, planet, moon;
    pick_location(r, &sys, &planet, &moon);
    if (diff_chance(r, 50)) {  // New Earth, with the colony's buildings
        sys = 1;
        planet = 5;
        moon = 0;
    }
    TBody *b = body_at(sys, body_index(sys, planet, moon));
    b->Population = diff_pick(r, 0, 1, 500, 40000, diff_range(r, 0, 250000));
    b->_0B = (uint8_t)diff_pick(r, 0, 0, 1, 3);
    TBuilding *bl = far_host(ds.Game_Buildings);
    for (int i = 0; i < ds.Game_BuildingCount; i++) {
        if (diff_chance(r, 20))
            bl[i].Health = (uint8_t)diff_pick(r, 0, 0, 1);
        if (diff_chance(r, 20))
            bl[i].Output = (uint8_t)diff_range(r, 0, 200);
    }
    buildings_on(r, sys, planet, moon, diff_pick(r, 0, 1, 3, 8, 20));
    // A planet force there gets the energy supply.
    if (diff_chance(r, 70) && ds.Game_PlanetForceCount < 32) {
        TGroup *g = Groups_At(1, ++ds.Game_PlanetForceCount);
        g->System = (uint8_t)sys;
        g->Planet = (uint8_t)planet;
        g->Moon = (uint8_t)moon;
        g->Status = 7;
    }
    a[0] = sys;
    a[1] = planet;
    a[2] = moon;
}

static void setup_protect(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    ds.Protect_Key2 = diff_pick(r, 0x1F5F, 0x1F5F, 0, 0x11F5F);
    ds.Protect_PageIndex = diff_pick(r, 0, 7, 0x12345678);
}

static const struct {
    const char *name;
    const char *script;
} states[] = {
    {"command centre", "1500:quit"},
    {"system view (7)", "900:click:168,16;1300:quit"},
    {"ship info (16)", "900:click:120,16;1300:quit"},
    {"surface (20)", "900:click:216,16;1300:quit"},
};
enum { STATES = sizeof states / sizeof states[0] };

int main(int argc, char *argv[])
{
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
    DiffState *sysview = bases[1];
    diff_PlanetInfo_RenderSystemView.near = true;  // 357b:300f ret

    const struct {
        DiffFn *fn;
        DiffSetup setup;
        int cases;
        bool sysview_only;
    } fns[] = {
        {&diff_PlanetInfo_SetupPlanetInfoHotspots, setup_hotspots, 6, false},
        {&diff_PlanetInfo_DrawPlanetInfoText, setup_text, 160, false},
        {&diff_PlanetInfo_DrawPlanetInfoScreen, setup_screen, 16, false},
        {&diff_PlanetInfo_LoadPlanetFajPic, setup_faj, 16, false},
        {&diff_PlanetInfo_LoadPlanetPlanetPic, setup_pic, 16, false},
        {&diff_PlanetInfo_DrawAlienInfoScreen, setup_alien, 16, false},
        {&diff_PlanetInfo_DrawStoresList, setup_alien, 40, false},
        {&diff_PlanetInfo_DrawPlanetSurfaceBackdrop, setup_surface, 10, false},
        {&diff_PlanetInfo_SetupSystemViewHotspots, setup_sv_hotspots, 60, false},
        {&diff_PlanetInfo_InitSystemView, setup_sv_load, 8, false},
        {&diff_PlanetInfo_LoadSystemViewAssets, setup_sv_load, 12, true},
        {&diff_PlanetInfo_ReloadSystemView, setup_sv_render, 12, true},
        {&diff_PlanetInfo_RenderSystemView, setup_sv_frame, 40, true},
        {&diff_PlanetInfo_UpdateOrbitPositions, setup_sv_render, 40, true},
        {&diff_PlanetInfo_SelectCurBody, setup_select, 20, false},
        {&diff_PlanetInfo_IsCurrentBody, setup_is_current, 20, false},
        {&diff_PlanetInfo_GetBodyName, setup_name, 12, false},
        {&diff_PlanetInfo_GetBodyFullName, setup_name, 12, false},
        {&diff_PlanetInfo_GetBodyLongName, setup_name, 12, false},
        {&diff_PlanetInfo_GetBodyLocationName, setup_name, 12, false},
        {&diff_PlanetInfo_BalanceBuildingPower, setup_power, 60, false},
        {&diff_PlanetInfo_RandomizeProtectPage, setup_protect, 8, false},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++) {
        if (fns[i].sysview_only)
            diff_run(fns[i].fn, &sysview, 1, fns[i].setup, diff_cases(fns[i].cases));
        else
            diff_run(fns[i].fn, bases, STATES, fns[i].setup, diff_cases(fns[i].cases));
    }
    for (int i = 0; i < STATES; i++)
        diff_free(bases[i]);
    return diff_finish();
}
