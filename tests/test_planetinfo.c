// PlanetInfo, PlanetList, Colonize, Droids and BaseEff: body selection and
// names through the Galaxy tables, the list of bodies, building power
// balancing, the colony cost checks and the system view, with the real data.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/baseeff.h"
#include "game/colonize.h"
#include "game/droids.h"
#include "game/dseg.h"
#include "game/gfx.h"
#include "game/groups.h"
#include "game/mouse.h"
#include "game/planetinfo.h"
#include "game/planetlist.h"
#include "game/rawtext.h"
#include "game/system.h"
#include "platform/fs.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static bool no_wait(void)
{
    return true;
}

// Compares a Pascal string with a C string.
static bool pstr_is(const PString *s, const char *want)
{
    return s->len == strlen(want) && memcmp(s->s, want, s->len) == 0;
}

static TBody *body(int sys, int idx)
{
    return ds.Galaxy_Bodies[sys - 1] + (idx - 1);
}

static void reset(void)
{
    dseg_clear();
    vid_clear();
    dseg_init();
    system_init();
    // The charset lives in the game data block, which this test doesn't
    // load; blank glyphs instead.
    static uint8_t charset[0x2880];
    ds.Gfx_Charset = charset;
    // The surface's building tiles, as Surface's set-up allocates them.
    ds.Surface_BufC = System_GetMem(0xF000);
    ds.Gfx_LineStride = 320;
    ds.Gfx_LineStrideCopy = 320;
    gfx_wait_retrace = no_wait;
    ds.Game_Groups = System_GetMem(2 * 32 * sizeof(TGroup));
    SDL_memset(ds.Game_Groups, 0, 2 * 32 * sizeof(TGroup));
    ds.Game_Buildings = System_GetMem(1000 * sizeof(TBuilding));
    SDL_memset(ds.Game_Buildings, 0, 1000 * sizeof(TBuilding));
}

static void test_bodies(void)
{
    // Sun system (8): Earth is planet 3 and has the Moon; Mars (4) two moons.
    ds.Game_CurSystem = 8;
    ds.Game_CurPlanet[7] = 3;
    ds.Game_CurMoon[7] = 0;
    PlanetInfo_SelectCurBody();
    CHECK(ds.Game_CurBody == body(8, 3));
    PString s;
    PlanetInfo_GetBodyName(&s, 8, 3, 0);
    CHECK(pstr_is(&s, "Earth"));
    PlanetInfo_GetBodyName(&s, 8, 3, 1);
    CHECK(pstr_is(&s, "Moon"));
    PlanetInfo_GetBodyFullName(&s, 8, 3, 0);
    CHECK(pstr_is(&s, "Earth"));
    PlanetInfo_GetBodyFullName(&s, 8, 3, 1);
    CHECK(pstr_is(&s, "Earth Moon"));
    PlanetInfo_GetBodyLongName(&s, 8, 3, 1);
    CHECK(pstr_is(&s, "Sun system,planet Earth,moon Moon"));
    PlanetInfo_GetBodyLocationName(&s, 8, 4, 2);
    CHECK(pstr_is(&s, "Sun system Mars Deimos"));
    PlanetInfo_GetBodyLocationName(&s, 8, 4, 0);
    CHECK(pstr_is(&s, "Sun system Mars"));
    CHECK(PlanetInfo_IsCurrentBody(8, 3, 0));
    CHECK(!PlanetInfo_IsCurrentBody(8, 3, 1));
    CHECK(!PlanetInfo_IsCurrentBody(1, 3, 0));

    // The moon: the body index comes from the planet's moon list.
    ds.Game_CurMoon[7] = 1;
    PlanetInfo_SelectCurBody();
    const TMoonList *ml = ds.Galaxy_MoonLists[7] + 2;
    CHECK(ml->Count == 1);
    CHECK(ds.Game_CurBody == body(8, ml->Moons[0]));
    CHECK(ml->Moons[0] > ds.Galaxy_PlanetCount[7]);
    CHECK(PlanetInfo_IsCurrentBody(8, 3, 1));

    // The colony flag is only the low byte: the player's New Earth is one.
    ds.Game_CurSystem = 1;
    ds.Game_CurPlanet[0] = 5;
    ds.Game_CurMoon[0] = 0;
    ds.Game_CurBodyIsColony = 0xFF00;
    PlanetInfo_SelectCurBody();
    CHECK(ds.Game_CurBody == body(1, 5));
    CHECK(ds.Game_CurBodyIsColony == 0xFF01);
    ds.Game_CurPlanet[0] = 7;  // Jade is an alien colony
    PlanetInfo_SelectCurBody();
    CHECK(ds.Game_CurBodyIsColony == 0xFF00);

    PlanetInfo_SetupPlanetInfoHotspots();
    ds.PlanetInfo_AlienMode = 0;
    PlanetInfo_SetupPlanetInfoHotspots();
    CHECK(ds.Mouse_HotspotCount == 3);
    CHECK(Mouse_Hotspot(3)->Y == 0x91 && Mouse_Hotspot(3)->W == 0x60 && Mouse_Hotspot(3)->H == 0x37);
    CHECK(pstr_is((const PString *)&Mouse_Hotspot(1)->Caption, "SEE OWNER INFO"));
    ds.PlanetInfo_AlienMode = 1;
    PlanetInfo_SetupPlanetInfoHotspots();
    CHECK(ds.Mouse_HotspotCount == 1 && Mouse_Hotspot(1)->W == 0x140);
    ds.PlanetInfo_AlienMode = 0;
}

static void test_list(void)
{
    ds.PlanetList_Entries = System_GetMem(8 * 100);
    for (int s = 0; s < 8; s++)
        ds.Galaxy_SystemStatus[s] = 0;
    ds.Galaxy_SystemStatus[0] = 1;  // only systems with status 1 are listed
    ds.PlanetList_Mode = 1;
    PlanetList_BuildList();
    const PlanetListEntry *e = (const PlanetListEntry *)ds.PlanetList_Entries;
    CHECK(ds.PlanetList_Count == 1);
    CHECK(e[0].System == 1 && e[0].Planet == 5 && e[0].Moon == 0 && e[0].Kind == 1);

    // Miner stations: Owner 1, no colony, a droid on it.
    TBody *b = body(1, 3);
    b->Owner = 1;
    b->_0A = 2;
    PlanetList_BuildList();
    CHECK(ds.PlanetList_Count == 2);
    CHECK(e[0].Planet == 3 && e[0].Kind == 2 && e[1].Planet == 5 && e[1].Kind == 1);

    // Accomodable and mineable bodies (mode 2): surveyed, enough info.
    ds.PlanetList_Mode = 2;
    b = body(1, 4);
    b->Surveyed = 1;
    b->InfoLevel = 0x20;
    b->SurfaceType = 1;  // earth-like: colonisable
    PlanetList_BuildList();
    CHECK(ds.PlanetList_Count == 1 && e[0].Planet == 4 && e[0].Kind == 1);
    b->SurfaceType = 9;  // too hot: not colonisable
    b->_03 = 1;
    b->InfoLevel = 12;
    b->Surveyed = 0;
    PlanetList_BuildList();
    CHECK(ds.PlanetList_Count == 1 && e[0].Planet == 4 && e[0].Kind == 2);

    // A moon adds its own row of kind 4 behind the entry.
    ds.PlanetList_Count = 0;
    int16_t moonIdx = ds.Galaxy_MoonLists[0][3].Moons[1];
    TBody *m = body(1, moonIdx);
    m->Surveyed = 1;
    m->InfoLevel = 0x20;
    m->SurfaceType = 1;
    PlanetList_AddListEntry(1, 4, 2, moonIdx);
    CHECK(ds.PlanetList_Count == 2);
    CHECK(e[0].Moon == 2 && e[0].Kind == 1 && e[1].Moon == 2 && e[1].Kind == 4);

    // Aliens (mode 3): Jade belongs to race 2. Known once InfoLevel >= 28h;
    // the race's status picks the kind.
    ds.PlanetList_Mode = 3;
    b = body(1, 7);
    b->InfoLevel = 0x30;
    for (int st = 0; st < 3; st++) {
        static const int8_t status[3] = {4, 2, 6};
        ds.Races_Races[0].Status = status[st];
        PlanetList_BuildList();
        bool found = false;
        for (int i = 0; i < ds.PlanetList_Count; i++)
            found = found || (e[i].Planet == 7 && e[i].Moon == 0 && e[i].Kind == st + 1);
        CHECK(found);
    }
    b->InfoLevel = 0x20;  // unknown race: only a guess, kind 1 if InfoLevel >= 1Eh
    PlanetList_BuildList();
    bool aliens = false;
    for (int i = 0; i < ds.PlanetList_Count; i++)
        aliens = aliens || (e[i].Planet == 7 && e[i].Kind == 1);
    CHECK(aliens);

    PString cap;
    PlanetList_ListKindCaption(&cap, 1, 2);
    CHECK(pstr_is(&cap, "Miner station on "));
    PlanetList_ListKindCaption(&cap, 3, 3);
    CHECK(pstr_is(&cap, "Friendly colony on "));
    PlanetList_ListKindCaption(&cap, 2, 3);
    CHECK(cap.len == 0);
    PlanetList_ListKindCaption(&cap, 4, 1);
    CHECK(cap.len == 0);

    // Scroll bar thumb: 30 entries, top at 5.
    ds.PlanetList_Count = 30;
    ds.PlanetList_Top = 5;
    PlanetList_DrawScrollBarThumb();
    const uint8_t *scr = (const uint8_t *)vid_screen;
    CHECK(scr[(0x38 + 29) * 320 + 0x137] == 0x40);
    CHECK(scr[(0x38 + 30) * 320 + 0x137] == 0x54);
    CHECK(scr[(0x38 + 30 + 22) * 320 + 0x139] == 0x54);
    CHECK(scr[(0x38 + 30 + 23) * 320 + 0x139] == 0x40);
    ds.PlanetList_Count = 10;  // fits on the page: the thumb fills the bar
    PlanetList_DrawScrollBarThumb();
    CHECK(scr[0x38 * 320 + 0x137] == 0x54 && scr[(0x38 + 0x88) * 320 + 0x137] == 0x54
          && scr[(0x38 + 0x89) * 320 + 0x137] == 0x40);
    PlanetList_SetupScrollBarHotspot();
    CHECK(ds.Mouse_HotspotCount == 1 && Mouse_Hotspot(1)->X == 0x134 && Mouse_Hotspot(1)->H == 0x8C);
    ds.Mouse_Buttons = 0;
    PlanetList_DragScrollBar();  // no button down: nothing happens
    CHECK(ds.PlanetList_Top == 5);
}

// Places n buildings of the given kinds on 1:5:0 and balances their power.
static void place(const uint8_t *kinds, const uint8_t *outputs, int n)
{
    SDL_memset(ds.Game_Buildings, 0, 1000 * sizeof(TBuilding));
    for (int i = 0; i < n; i++) {
        TBuilding *b = ds.Game_Buildings + i;
        b->Kind = kinds[i];
        b->System = 1;
        b->Planet = 5;
        b->Moon = 0;
        b->Output = outputs[i];
        b->Flag = 0xEE;  // reset by the balancing
    }
    ds.Game_BuildingCount = (int16_t)n;
}

static TBuilding *bld(int i)
{
    return ds.Game_Buildings + (i - 1);
}

static void test_power(void)
{
    ds.Game_PlanetForceCount = 1;
    TGroup *pf = Groups_At(1, 1);
    pf->System = 1;
    pf->Planet = 5;
    pf->Moon = 0;
    pf->Storage = 77;
    TBody *b = body(1, 5);
    b->Population = 30000;
    b->_0B = 0;

    // Nuclear gen. (2: prod 3000), Radar (17: energy 750, prio 2), Stadium
    // (9: energy 445, prio 1), Miner station (25: adds 10 to the supply), a
    // second stadium. Output 20% of 3000 = 600; 2 * 600 < 1640: the later of
    // the two lowest-priority buildings goes first, then 2 * 600 >= 1195.
    static const uint8_t kinds[] = {2, 17, 9, 25, 9};
    static const uint8_t outs[] = {20, 0, 0, 0, 0};
    place(kinds, outs, 5);
    PlanetInfo_BalanceBuildingPower(1, 5, 0);
    CHECK(bld(1)->Flag == 1 && bld(2)->Flag == 1 && bld(3)->Flag == 1 && bld(4)->Flag == 1);
    CHECK(bld(5)->Flag == 0);
    CHECK(pf->Storage == 10);  // the miner station
    CHECK(bld(1)->W9 == 120 && bld(1)->WB == 0 && bld(1)->BD == 100);
    // supply 600, energy 1195, demand = population.
    CHECK(bld(2)->W9 == 0 && bld(2)->WB == 376 && bld(2)->BD == 50);
    CHECK(bld(3)->W9 == 73 && bld(3)->WB == 223 && bld(3)->BD == 50);
    CHECK(*(uint16_t *)body(1, 5)->_04 == 0);

    // With a lower output everything but the plant and the miner station
    // has to go: lowest priority first (stadiums 1, radar 2).
    place(kinds, (const uint8_t[]){10, 0, 0, 0, 0}, 5);
    PlanetInfo_BalanceBuildingPower(1, 5, 0);
    CHECK(bld(1)->Flag == 1 && bld(2)->Flag == 0 && bld(3)->Flag == 0 && bld(4)->Flag == 1
          && bld(5)->Flag == 0);

    // Buildings elsewhere or damaged are not touched / not powered.
    place(kinds, outs, 5);
    bld(2)->Planet = 4;
    bld(3)->Health = 5;
    PlanetInfo_BalanceBuildingPower(1, 5, 0);
    CHECK(bld(2)->Flag == 0xEE && bld(3)->Flag == 0xEE);
    CHECK(bld(1)->Flag == 1 && bld(5)->Flag == 1);

    // Builder plant (22: work 120, energy 890, prod 780): kind 16h adds
    // Output * Production / 100 * BD / 100 in 16-bit arithmetic to the body.
    static const uint8_t k2[] = {2, 22};
    static const uint8_t o2[] = {100, 50};
    place(k2, o2, 2);
    PlanetInfo_BalanceBuildingPower(1, 5, 0);
    CHECK(bld(1)->Flag == 1 && bld(2)->Flag == 1);
    CHECK(bld(2)->WB == 890 && bld(2)->BD == 100);
    // 50 * 780 = 39000 wraps to -26536; / 100 = -265, * 100 / 100.
    CHECK(*(uint16_t *)body(1, 5)->_04 == (uint16_t)-265);

    // A lone consumer cannot run: supply is 0 (made 1), 2 < energy.
    static const uint8_t k3[] = {22};
    static const uint8_t o3[] = {100};
    place(k3, o3, 1);
    b->Population = 0;  // treated as 1
    PlanetInfo_BalanceBuildingPower(1, 5, 0);
    CHECK(bld(1)->Flag == 0);
    CHECK(*(uint16_t *)body(1, 5)->_04 == 0);

    // Solar satellites add 10000 each to the supply.
    b->Population = 30000;
    b->_0B = 1;
    place(k3, o3, 1);
    PlanetInfo_BalanceBuildingPower(1, 5, 0);
    CHECK(bld(1)->Flag == 1);
    b->_0B = 0;

    // A moon: Earth's Moon (8:3:1) has its own Population.
    ds.Game_PlanetForceCount = 2;
    pf = Groups_At(1, 2);
    pf->System = 8;
    pf->Planet = 3;
    pf->Moon = 1;
    pf->Storage = 5;
    const TMoonList *ml = ds.Galaxy_MoonLists[7] + 2;
    TBody *moon = body(8, ml->Moons[0]);
    moon->Population = 1000;
    SDL_memset(ds.Game_Buildings, 0, 1000 * sizeof(TBuilding));
    TBuilding *mb = bld(1);
    mb->Kind = 11;  // storage bay: 114 workers, 450 energy
    mb->System = 8;
    mb->Planet = 3;
    mb->Moon = 1;
    mb->Flag = 0;
    ds.Game_BuildingCount = 1;
    PlanetInfo_BalanceBuildingPower(8, 3, 1);
    CHECK(pf->Storage == 20);  // kind 0Bh adds 20 to the force's supply
    CHECK(mb->Flag == 0);  // no production: switched off
    CHECK(mb->W9 == 0);
}

static void test_colonize(void)
{
    // Whether a starter building can be chosen.
    TBuildingType t;
    memset(&t, 0, sizeof t);
    ds.Game_StaffExp[1] = 4;
    t._10 = 5;
    CHECK(!Colonize_TypeAvailable(&t));
    ds.Game_StaffExp[1] = 5;
    CHECK(Colonize_TypeAvailable(&t));
    t._10 = (uint8_t)-1;  // negative requirement: always met
    ds.Game_StaffExp[1] = 0;
    CHECK(Colonize_TypeAvailable(&t));
    t._10 = 0;
    t.NeedProduct = 3;
    ds.Research_Products[2].State = 4;
    CHECK(!Colonize_TypeAvailable(&t));
    ds.Research_Products[2].State = 5;
    CHECK(Colonize_TypeAvailable(&t));
    t.NeedProduct = (uint8_t)-1;
    CHECK(!Colonize_TypeAvailable(&t));
    memset(&t, 0, sizeof t);
    ((uint8_t *)&t)[0x13 + 6] = 1;
    CHECK(Colonize_SuitsSurface(&t, 6) && !Colonize_SuitsSurface(&t, 5));

    // Selecting buildings in the screen: slot 3 is the Builder Plant (its
    // type needs nothing but a suitable surface).
    ds.Game_CurSystem = 1;
    ds.Game_CurPlanet[0] = 4;  // Zeus
    ds.Game_CurMoon[0] = 0;
    PlanetInfo_SelectCurBody();
    TBody *b = ds.Game_CurBody;
    b->SurfaceType = 1;
    ds.Game_ColonySetupCount = 1;
    ds.Game_ScreenEntered = 0;
    ds.Game_MenuCommand = 0;
    ds.Colonize_ColonyCost = 100000;
    ds.Game_Credits = 100000 + 50000;
    const TStarterBuilding *sb = &ds.Colony_StarterBuildings[2];
    const TBuildingType *bt = &ds.Surface_BuildingTypes[sb->Building - 1];
    ds.Game_StaffExp[1] = 100;
    ((uint8_t *)bt)[0x13 + 1] = 1;  // suitable for surface type 1
    ds.Game_HotspotHit = 0x17 + 3;
    int32_t before = ds.Colonize_ColonyCost;
    ds.Game_ColonySetups[0].Timers[2] = 0;
    if (Colonize_TypeAvailable(bt)) {
        Colonize_ColonizeScreen();
        CHECK(ds.Game_ColonySetups[0].Timers[2] == (bt->Cost + before <= ds.Game_Credits ? 1 : 0));
        if (ds.Game_ColonySetups[0].Timers[2] == 1) {
            CHECK(ds.Colonize_ColonyCost == before + bt->Cost);
            Colonize_ColonizeScreen();  // clicking again deselects
            CHECK(ds.Game_ColonySetups[0].Timers[2] == 0);
            CHECK(ds.Colonize_ColonyCost == before);
        }
        // Not enough credits: nothing is picked.
        ds.Game_Credits = before + bt->Cost - 1;
        Colonize_ColonizeScreen();
        CHECK(ds.Game_ColonySetups[0].Timers[2] == 0 && ds.Colonize_ColonyCost == before);
    }

    // Colonise (3Bh) with too little money does nothing; cancel (3Ah) frees
    // the buffer and the set-up slot.
    ds.Game_HotspotHit = 0;
    ds.Game_MenuCommand = 0x3B;
    ds.Game_Credits = 99999;
    ds.Game_CurrentScreen = 0x15;
    ds.Game_ScreenSwitch = false;
    Colonize_ColonizeScreen();
    CHECK(ds.Game_CurrentScreen == 0x15 && !ds.Game_ScreenSwitch);
    ds.Surface_BufC = System_GetMem(0xF000);
    ds.Game_MenuCommand = 0x3A;
    Colonize_ColonizeScreen();
    CHECK(ds.Game_CurrentScreen == 0x14 && ds.Game_ScreenSwitch && ds.Game_ColonySetupCount == 0);

    // Colonise with enough money: a planet force group is created and paid.
    ds.Game_ColonySetupCount = 1;
    ds.Surface_BufC = System_GetMem(0xF000);
    ds.Game_Credits = 120000;
    ds.Colonize_ColonyCost = 100000;
    ds.Game_MenuCommand = 0x3B;
    ds.Game_PlanetForceCount = 1;
    ds.Game_CurrentScreen = 0x15;
    ds.Game_ScreenSwitch = false;
    ds.Game_HotspotHit = 0;
    Colonize_ColonizeScreen();
    CHECK(ds.Game_Credits == 20000);
    CHECK(ds.Game_PlanetForceCount == 2 && ds.Game_SelGroup == 2 && ds.Game_GroupList == 1);
    const TGroup *g = Groups_At(1, 2);
    CHECK(g->Kind == 5 && g->Status == 7 && g->System == 1 && g->Planet == 4 && g->Moon == 0);
    CHECK(pstr_is((const PString *)&g->Name, "Zeus forces"));
    CHECK(ds.Game_CurrentScreen == 0x14 && ds.Game_ScreenSwitch);
    CHECK(ds.Game_CurBody->Owner == 1);

    // Hotspots.
    Colonize_SetupColonizeHotspots();
    CHECK(ds.Mouse_HotspotCount == 9);
    CHECK(Mouse_Hotspot(4)->X == ds.Colony_StarterBuildings[0].X);
    CHECK(Mouse_Hotspot(4)->W == ds.Colony_StarterBuildings[0].W * 16);
    CHECK(pstr_is((const PString *)&Mouse_Hotspot(1)->Caption, "Change people"));
    const TBuildingType *first = &ds.Surface_BuildingTypes[ds.Colony_StarterBuildings[0].Building - 1];
    CHECK(Mouse_Hotspot(4)->Caption.len == first->Name.len);
}

static void test_droids(void)
{
    // No capacity (no buildings): buying a droid does nothing.
    ds.Game_CurSystem = 1;
    ds.Game_CurPlanet[0] = 5;
    ds.Game_CurMoon[0] = 0;
    ds.Game_BuildingCount = 0;
    PlanetInfo_SelectCurBody();
    TBody *b = ds.Game_CurBody;
    b->_0A = 2;
    ds.Research_Products[1].Stock = 7;
    ds.Game_ScreenEntered = 0;
    ds.Game_MenuCommand = 0x19;
    ds.Game_HotspotHit = 0;
    Droids_DroidsScreen();
    CHECK(b->_0A == 2 && ds.Research_Products[1].Stock == 7);
    ds.Game_HotspotHit = 0x15;
    ds.Game_MenuCommand = 0;
    Droids_DroidsScreen();
    CHECK(ds.Game_CurrentScreen == 0x14 && ds.Game_ScreenSwitch);
    Droids_SetupDroidsHotspots();
    CHECK(ds.Mouse_HotspotCount == 1 && Mouse_Hotspot(1)->W == 0x140
          && pstr_is((const PString *)&Mouse_Hotspot(1)->Caption, "Back to surface"));
}

// The system view with the real pictures.
static void test_system_view(void)
{
    ds.Game_CurSystem = 8;
    ds.Game_CurPlanet[7] = 0;
    ds.Game_CurMoon[7] = 0;
    // The sine table, scaled by 500.
    for (int i = 0; i < 360; i++) {
        static const double pi = 3.14159265358979323846;
        int32_t v = (int32_t)(500.0 * __builtin_sin(i * pi / 180.0) + (i < 180 ? 0.5 : -0.5));
        ds.Vec3D_CosTable[i] = v;
    }
    ds.Galaxy_SystemStatus[7] = 1;
    SDL_memset(vid_screen, 0, 320 * 200);
    PlanetInfo_InitSystemView();
    // One orbit record per planet, from the name table.
    const TBodyName *nm = ds.Galaxy_Names[7];
    const TOrbit *rec = &ds.PlanetInfo_Orbits[3];
    CHECK(rec->RX == nm[2].X && rec->RY == nm[2].Y && rec->Speed == nm[2].Size);
    CHECK(rec->Angle >= 0 && rec->Angle < 3600);

    // Hand-placed planet 1 at 90 degrees: x = 160 + rx, y = 75, size 24.
    TOrbit *r1 = &ds.PlanetInfo_Orbits[1];
    r1->RX = 60;
    r1->RY = 30;
    r1->Angle = 900;
    r1->Speed = 10;
    ds.Galaxy_PlanetStatus[7][0] = 0;  // Mercury known (the Sun starts all -1)
    PlanetInfo_UpdateOrbitPositions();
    CHECK(r1->X == 220);
    CHECK(r1->Y == 75);
    CHECK(r1->Size == 24 && r1->Half == 12);
    CHECK(r1->Angle == 905);  // advanced by speed / 2
    CHECK(Mouse_Hotspot(0x14)->X == 220 - 12 - 3);
    CHECK(Mouse_Hotspot(0x14)->Y == (uint8_t)(75 - 12 + 0x2E));
    CHECK(Mouse_Hotspot(0x14)->W == 24 && Mouse_Hotspot(0x14)->H == 24);

    // The frame reached the screen: the sprite area is not all background.
    int nonzero = 0;
    for (int i = 49 * 320; i < 200 * 320; i++)
        nonzero += ((const uint8_t *)vid_screen)[i] != 0;
    CHECK(nonzero > 5000);

    // Hotspots of the system map.
    ds.Game_CurrentScreen = 7;
    PlanetInfo_SetupSystemViewHotspots();
    CHECK(ds.Mouse_HotspotCount >= 0x12);
    CHECK(pstr_is((const PString *)&Mouse_Hotspot(0x14)->Caption, "Mercury"));
    CHECK(Mouse_Hotspot(0x14)->W == 0x20 && Mouse_Hotspot(0x14)->_1A == 5);

    // Planet view with moons: the moon records follow the moon list.
    ds.Game_CurPlanet[7] = 4;  // Mars
    PlanetInfo_SetupSystemViewHotspots();
    CHECK(pstr_is((const PString *)&Mouse_Hotspot(0x13)->Caption, "Mars"));
    CHECK(pstr_is((const PString *)&Mouse_Hotspot(0x14)->Caption, "Phobos"));
    CHECK(pstr_is((const PString *)&Mouse_Hotspot(0x15)->Caption, "Deimos"));
    PlanetInfo_ReloadSystemView();
    const TOrbit *m1 = &ds.PlanetInfo_Orbits[1];
    const TMoonList *ml = ds.Galaxy_MoonLists[7] + 3;
    CHECK(m1->RX == nm[ml->Moons[0] - 1].X);
}

static void test_baseeff(void)
{
    ds.Game_CurSystem = 1;
    ds.Game_CurPlanet[0] = 5;
    ds.Game_CurMoon[0] = 0;
    PlanetInfo_SelectCurBody();
    TBody *b = ds.Game_CurBody;
    ds.Game_ScreenEntered = 0;
    ds.Game_HotspotHit = 0;
    ds.PlanetInfo_AlienMode = 0;
    b->TechLevel = 6;
    ds.Game_MenuCommand = 0x26;
    BaseEff_BaseEffScreen();
    CHECK(b->TechLevel == 7);
    BaseEff_BaseEffScreen();
    CHECK(b->TechLevel == 7);  // capped
    ds.Game_MenuCommand = 0x27;
    for (int i = 0; i < 9; i++)
        BaseEff_BaseEffScreen();
    CHECK(b->TechLevel == 0);  // floored (the byte wraps to FFh, a signed -1)
    ds.Game_MenuCommand = 0;

    // The owner page needs a known alien body; the info page of New Earth
    // refuses and clears the click.
    b->InfoLevel = 100;
    ds.Game_HotspotHit = 0x15;
    BaseEff_BaseEffScreen();
    CHECK(ds.PlanetInfo_AlienMode == 0 && ds.Game_HotspotHit == 0);
    // Jade is an alien colony: its page opens and the click returns to the info.
    ds.Game_CurPlanet[0] = 7;
    PlanetInfo_SelectCurBody();
    b = ds.Game_CurBody;
    b->InfoLevel = 0x30;
    RawText_LoadSzFaj();
    ds.Game_HotspotHit = 0x15;
    BaseEff_BaseEffScreen();
    CHECK(ds.PlanetInfo_AlienMode == 1 && ds.Game_HotspotHit == 0 && ds.Mouse_HotspotCount == 1);
    ds.Game_HotspotHit = 0x15;
    BaseEff_BaseEffScreen();
    CHECK(ds.PlanetInfo_AlienMode == 0 && ds.Mouse_HotspotCount == 3);
    // Ships button, surface button.
    ds.Game_HotspotHit = 0x16;
    BaseEff_BaseEffScreen();
    CHECK(ds.Game_CurrentScreen == 0x10 && ds.Game_ScreenSwitch);
    b->InfoLevel = 3;
    ds.Game_HotspotHit = 0x17;
    BaseEff_BaseEffScreen();
    CHECK(ds.PlanetInfo_AlienMode == 0);  // too little known about the surface
    b->InfoLevel = 8;
    ds.Game_HotspotHit = 0x17;
    BaseEff_BaseEffScreen();
    CHECK(ds.PlanetInfo_AlienMode == 3 && ds.Game_HotspotHit == 0);
    ds.Game_MenuCommand = 0x2E;
    BaseEff_BaseEffScreen();
    CHECK(ds.Game_CurMoon[0] == 0 && ds.Game_CurrentScreen == 7);
    ds.Game_MenuCommand = 0;
    ds.PlanetInfo_AlienMode = 0;
}

static int screen_pixels(void)
{
    int n = 0;
    for (int i = 49 * 320; i < 200 * 320; i++)
        n += ((const uint8_t *)vid_screen)[i] != 0;
    return n;
}

// The info screens draw without trouble and use up their buffers.
static void test_screens(void)
{
    uint32_t used0 = system_heap_used();
    ds.Game_CurSystem = 1;
    ds.Game_CurPlanet[0] = 5;
    ds.Game_CurMoon[0] = 0;
    ds.Game_CurrentScreen = 8;
    PlanetInfo_SelectCurBody();
    SDL_memset(vid_screen, 0, 320 * 200);
    ds.PlanetInfo_AlienMode = 0;
    PlanetInfo_DrawPlanetInfoScreen();
    CHECK(screen_pixels() > 20000);
    CHECK(system_heap_used() == used0);

    // The text only: unchanged pixel count except the text area.
    PlanetInfo_DrawPlanetInfoText();
    CHECK(system_heap_used() == used0);

    // An alien body.
    ds.Game_CurPlanet[0] = 7;
    PlanetInfo_SelectCurBody();
    ds.Game_CurBody->InfoLevel = 50;
    RawText_LoadSzFaj();
    used0 = system_heap_used();  // the race texts stay loaded
    ds.PlanetInfo_AlienMode = 1;
    SDL_memset(vid_screen, 0, 320 * 200);
    PlanetInfo_DrawAlienInfoScreen();
    CHECK(screen_pixels() > 5000);
    ds.PlanetInfo_AlienMode = 0;
    SDL_memset(vid_screen, 0, 320 * 200);
    PlanetInfo_DrawPlanetInfoScreen();
    CHECK(screen_pixels() > 20000);
    PlanetInfo_DrawPlanetSurfaceBackdrop();
    CHECK(system_heap_used() == used0);

    // Colony set-up screen.
    ds.Game_CurPlanet[0] = 4;
    PlanetInfo_SelectCurBody();
    ds.Game_CurBody->SurfaceType = 1;
    ds.Surface_TileSet = 1;
    SDL_memset(vid_screen, 0, 320 * 200);
    Colonize_DrawColonizeScreen();
    CHECK(screen_pixels() > 20000);
    System_FreeMem(ds.Surface_BufC, 0xF000);
    CHECK(system_heap_used() == used0);
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    reset();
    test_bodies();
    test_list();
    test_power();
    test_colonize();
    test_droids();
    test_system_view();
    test_screens();
    test_baseeff();
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
