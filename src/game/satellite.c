#include "game/satellite.h"

#include "game/groups.h"
#include "game/system.h"

// Routines of units that are not ported yet (stubs in stubs.c).
void PlanetInfo_SelectCurBody(void);                     // 357b:3253
void PlanetInfo_DrawPlanetInfoText(void);                // 357b:0359
void PlanetInfo_DrawStoresList(void);                    // 357b:1671
void Message_ShowMessageLine(int16_t lineNo);            // 34b0:011a
void Message_ShowMessageNoLog(const uint8_t *text);      // 34b0:0197
void Message_PlaySatelliteAnim(int16_t satNo);           // 34b0:0953
void Intel_OfferInventionForAnalysis(int16_t idea);      // 29b9:1409
void Intel_ScheduleIdea(int16_t idea, int16_t delayBase, int16_t delayRandom); // 29b9:136e
void AlienTalk_StartAlienScene(int16_t scene);           // 2e4b:0000
void GfxPath_SetToolbar(int16_t screen);                 // 3a1b:0030
void GfxPath_AddToolbarIcon(int16_t id);                 // 3a1b:0366
void GfxPath_ShowToolbar(void);                          // 3a1b:0704
bool GfxPath_ToolbarHasIcon(int16_t id);                 // 3a1b:07ca
void Surface_AutoPlaceBuilding(int16_t kind, int16_t system, int16_t planet, int16_t moon); // 2ef2:29b1

// Research_Products[k] / Game_IdeaTimer[k] hold Pascal Products[k + 1] / IdeaTimer[k + 1]
// (DS:5DAC + 35h*k): the decompile's indices are already 0-based, so they are used as is.
// [2] = Satellite, [6] = Control Centre (idea 7), [0xB] = Radio (idea 0xC).

static TBody *CurBody(void) { return ds.Game_CurBody; }

// The location of the selected body: CurPlanet/CurMoon of CurSystem.
#define CUR_PLANET (ds.Game_CurPlanet[ds.Game_CurSystem - 1])
#define CUR_MOON (ds.Game_CurMoon[ds.Game_CurSystem - 1])

// SurfaceColonisable of the body's tileset (Surface::Tileset[SurfaceType]).
static bool Colonisable(const TBody *b)
{
    return ds.Galaxy_SurfaceColonisable[ds.Surface_Tileset[b->SurfaceType - 1] - 1] != 0;
}

static bool AtLocation(const TGroup *g, int16_t system, int16_t planet)
{
    return g->System == system && g->Planet == planet && (g->Status == 1 || g->Status == 2);
}

// 28cd:0000
void Satellite_DeployStockSatellite(void)
{
    PlanetInfo_SelectCurBody();
    if (ds.Game_CurSystem == 1 && ds.Game_CurPlanet[0] == 5) {
        CurBody()->Satellite = 1;
        ds.Research_Products[2].Stock--; // Products[3] = Satellite (DS:5E31)
    } else {
        CurBody()->Satellite = 0xE2;
        ds.Research_Products[2].Stock--;
        ds.Intel_SatTheftTimer = System_Random(10) + 5;
    }
}

// 28cd:0050: story events at 3:2:1 and 7:1:0
static void CheckStoryLocations(void)
{
    if (ds.Game_CurSystem == 3 && ds.Game_CurPlanet[2] == 2 && ds.Game_CurMoon[2] == 1
        && ds.Research_Products[0x15].State == 0) {
        Message_ShowMessageLine(0x15);
        Intel_OfferInventionForAnalysis(0x16);
        Intel_OfferInventionForAnalysis(0x17);
        AlienTalk_StartAlienScene(4);
    }
    if (ds.Game_CurSystem == 7 && ds.Game_CurPlanet[6] == 1 && ds.Game_CurMoon[6] == 0
        && ds.Research_Products[0x1E].State == 0) {
        Message_ShowMessageLine(0x1F);
        Intel_OfferInventionForAnalysis(0x1F);
        Intel_OfferInventionForAnalysis(0x20);
        Intel_ScheduleIdea(0x22, 100, 0x14);
        AlienTalk_StartAlienScene(6);
    }
}

// 28cd:0117
void Satellite_DeploySatellite(void)
{
    Message_PlaySatelliteAnim(1);
    PlanetInfo_SelectCurBody();
    if (CurBody()->Owner < 2) {
        CurBody()->Satellite = 1;
        Satellite_DeductSatellites(ds.Game_CurSystem, CUR_PLANET, CUR_MOON, 1, 1);
        CheckStoryLocations();
    } else {
        Satellite_DeductSatellites(ds.Game_CurSystem, CUR_PLANET, CUR_MOON, 1, 1);
        Message_ShowMessageNoLog((const uint8_t *)PSTR("| You lost the contact with your satellite ! |"));
    }
}

// 28cd:018d
void Satellite_DeploySpySat(void)
{
    PlanetInfo_SelectCurBody();
    TBody *b = CurBody();
    Message_PlaySatelliteAnim(2);
    b->Satellite = 2;
    Satellite_DeductSatellites(ds.Game_CurSystem, CUR_PLANET, CUR_MOON, 2, 1);
    CheckStoryLocations();
}

// 28cd:01d6
void Satellite_DeploySpyShip(void)
{
    PlanetInfo_SelectCurBody();
    TBody *b = CurBody();
    Message_PlaySatelliteAnim(3);
    b->_09 = 1;
    Satellite_DeductSatellites(ds.Game_CurSystem, CUR_PLANET, CUR_MOON, 3, 1);
    CheckStoryLocations();
}

// 28cd:021f
void Satellite_DeploySolarSat(void)
{
    PlanetInfo_SelectCurBody();
    CurBody()->_0B++;
    Satellite_DeductSatellites(ds.Game_CurSystem, CUR_PLANET, CUR_MOON, 4, 1);
}

// 28cd:0254: takes ownership, consumes a trade ship
void Satellite_ColonizeBody(void)
{
    PlanetInfo_SelectCurBody();
    TBody *b = CurBody();
    b->InfoLevel += 10;
    if (b->InfoLevel > 60)
        b->InfoLevel = 60;
    b->Owner = 1;
    b->_0A = 1;
    Satellite_TakeTradeShips(ds.Game_CurSystem, CUR_PLANET, CUR_MOON, 1);
    if (ds.Research_Products[6].State == 0 && ds.Game_IdeaTimer[6] == -1) {
        if (b->Surveyed != 0 && Colonisable(b)) {
            Intel_ScheduleIdea(7, 0x14, 0x28);
            b->InfoLevel += 10;
            Message_ShowMessageLine(4);
        }
    }
}

// 28cd:02ec: after paying in screen 0x3b
void Satellite_AcquireBody(void)
{
    PlanetInfo_SelectCurBody();
    TBody *b = CurBody();
    b->Owner = 1;
    b->InfoLevel += 10;
    if (b->InfoLevel > 60)
        b->InfoLevel = 60;
    b->Miner = (int8_t)(System_Random(2) + 1);
    // Galaxy::Bodies[1] + 0x186: the 7th body of system 1, field +0x0C.
    if (ds.Game_CurSystem == 1 && ds.Game_CurPlanet[0] == 7 && ds.Game_CurMoon[0] == 0
        && ds.Galaxy_Bodies[0][6].InfoLevel != 2 && ds.Research_Products[0xB].State == 0) {
        Message_ShowMessageLine(0xD);
        Intel_OfferInventionForAnalysis(0xC);
    }
}

// 28cd:0367: toolbar for screen 0x11
void Satellite_SetupGroupScreenMenu(void)
{
    int16_t hit = ds.Game_HotspotHit;
    const TGroup *g = Groups_Selected();
    bool any;
    GfxPath_SetToolbar(0x11);
    if (g->Kind == 2 || g->Kind == 3)
        GfxPath_AddToolbarIcon(3);
    if (g->Status == 1 || g->Status == 2) {
        GfxPath_AddToolbarIcon(0x2E);
        GfxPath_AddToolbarIcon(0x29);
        Satellite_AddBodyActionItems(g->System, g->Planet, g->Moon, &any);
    }
    GfxPath_ShowToolbar();
    ds.Game_HotspotHit = hit;
}

// 28cd:03fb: toolbar for screen 0x16; SatButtons records which buttons are shown
void Satellite_UpdateGroupsMenu(void)
{
    int16_t hit = ds.Game_HotspotHit;
    const TGroup *g = Groups_Selected();
    if (g->Status == 1 || g->Status == 2) {
        ds.Game_CurSystem = g->System;
        ds.Game_CurPlanet[ds.Game_CurSystem - 1] = g->Planet;
        ds.Game_CurMoon[ds.Game_CurSystem - 1] = g->Moon;
        PlanetInfo_SelectCurBody();
    }
    int16_t old = ds.Game_SatButtons;
    ds.Game_SatButtons = 0;
    g = Groups_Selected();
    GfxPath_SetToolbar(0x16);
    if (ds.Game_GroupList == 0) {
        GfxPath_AddToolbarIcon(0x10);
        ds.Game_SatButtons += 1;
    }
    if (g->Kind == 2 || g->Kind == 3) {
        GfxPath_AddToolbarIcon(3);
        ds.Game_SatButtons += 2;
    }
    if (g->Status == 1 || g->Status == 2 || g->Status == 7) {
        GfxPath_AddToolbarIcon(0x2E);
        GfxPath_AddToolbarIcon(0x29);
        ds.Game_SatButtons += 4;
    }
    if (g->Status == 1) {
        if (Groups_IsGroupEmptyOfCargo(ds.Game_GroupList, ds.Game_SelGroup)
            && CurBody()->Owner == 1 && CurBody()->Colony != 0) {
            GfxPath_AddToolbarIcon(0x25);
            ds.Game_SatButtons += 8;
        }
    }
    if (ds.Game_SatButtons != old)
        GfxPath_ShowToolbar();
    ds.Game_HotspotHit = hit;
}

// 28cd:0542: toolbar for screen 0x10
void Satellite_UpdateShipInfoMenu(void)
{
    int16_t hit = ds.Game_HotspotHit;
    int16_t old = ds.Game_ShipInfoButtons;
    ds.Game_ShipInfoButtons = 0;
    GfxPath_SetToolbar(0x10);
    if (ds.Game_SelGroup == 0) {
        if (ds.Game_GroupsEnabled != 0 && ds.Game_GroupList == 0) {
            GfxPath_AddToolbarIcon(0xF);
            ds.Game_ShipInfoButtons += 2;
        }
    } else {
        const TGroup *g = Groups_Selected();
        if (ds.Game_GroupList == 0) {
            GfxPath_AddToolbarIcon(0x10);
            ds.Game_ShipInfoButtons += 1;
        }
        GfxPath_AddToolbarIcon(0x31);
        if (ds.Game_GroupsEnabled != 0 && ds.Game_GroupList == 0) {
            GfxPath_AddToolbarIcon(0xF);
            ds.Game_ShipInfoButtons += 2;
        }
        if (g->Kind == 2 || g->Kind == 3) {
            GfxPath_AddToolbarIcon(3);
            ds.Game_ShipInfoButtons += 4;
        }
        if (g->Status == 1 || g->Status == 2 || g->Status == 7) {
            GfxPath_AddToolbarIcon(0x29);
            ds.Game_ShipInfoButtons += 8;
        }
    }
    if (ds.Game_ShipInfoButtons != old)
        GfxPath_ShowToolbar();
    ds.Game_HotspotHit = hit;
}

// 28cd:062b: adds the body action items that apply to the body at system:planet:moon
void Satellite_AddBodyActionItems(int16_t system, int16_t planet, int16_t moon, bool *any)
{
    uint16_t idx = planet;
    if (moon != 0) {
        const uint8_t *moons = (const uint8_t *)ds.Galaxy_MoonLists[system - 1];
        idx = moons[(uint16_t)(planet * 9 + moon - 9)];
    }
    const TBody *b = ds.Galaxy_Bodies[system - 1] + (idx - 1);
    int16_t cs = ds.Game_CurSystem, cp = CUR_PLANET, cm = CUR_MOON;

    *any = false;
    if ((ds.Intel_SatTheftDone == 0 || (system == 1 && planet == 5)) && b->Satellite == 0
        && ((b->Colony == 0 && b->_0A == 0) || b->Owner >= 2)
        && ds.Research_Products[2].Stock > 0 && !GfxPath_ToolbarHasIcon(0x46)) {
        GfxPath_AddToolbarIcon(0x46);
        *any = true;
    }
    if (b->Satellite == 0 && ((b->Colony == 0 && b->_0A == 0) || b->Owner >= 2)
        && Satellite_CountSatellites(cs, cp, cm, 1) > 0 && !GfxPath_ToolbarHasIcon(0x14)
        && !GfxPath_ToolbarHasIcon(0x46)) {
        GfxPath_AddToolbarIcon(0x14);
        *any = true;
    }
    if (b->Satellite == 0 && ((b->Colony == 0 && b->_0A == 0) || b->Owner >= 2)
        && Satellite_CountSatellites(cs, cp, cm, 2) > 0 && !GfxPath_ToolbarHasIcon(0x43)) {
        GfxPath_AddToolbarIcon(0x43);
        *any = true;
    }
    if (b->_09 == 0 && b->Owner >= 2 && b->InfoLevel > 29
        && Satellite_CountSatellites(cs, cp, cm, 3) > 0 && !GfxPath_ToolbarHasIcon(0x44)) {
        GfxPath_AddToolbarIcon(0x44);
        *any = true;
    }
    if (b->Owner == 1 && b->Colony != 0 && b->_0B < 5
        && Satellite_CountSatellites(cs, cp, cm, 4) > 0 && !GfxPath_ToolbarHasIcon(0x45)) {
        GfxPath_AddToolbarIcon(0x45);
        *any = true;
    }
    if (b->Owner == 0 && b->Colony == 0 && b->_0A == 0 && b->InfoLevel > 9 && b->Miner == 0
        && Satellite_CountTradeShips(cs, cp, cm) > 0 && b->_03 != 0 && !GfxPath_ToolbarHasIcon(10)) {
        GfxPath_AddToolbarIcon(10);
        *any = true;
    }
    if (ds.Research_Products[6].State == 5 && b->Colony == 0 && b->Miner == 0 && b->InfoLevel > 29
        && b->Surveyed != 0 && b->Owner < 2 && Colonisable(b)
        && ds.Game_ColonySetupCount < 10 && !GfxPath_ToolbarHasIcon(5)) {
        GfxPath_AddToolbarIcon(5);
        *any = true;
    }
}

// 28cd:0948: executes the body action menu commands and refreshes the menu
void Satellite_HandleBodyActions(void)
{
    ds.Satellite_ActionDone = 0;
    if (ds.Game_MenuCommand == 10) {
        Satellite_ColonizeBody();
        ds.Satellite_ActionDone = 1;
        Surface_AutoPlaceBuilding(0x19, ds.Game_CurSystem, CUR_PLANET, CUR_MOON);
    }
    if (ds.Game_MenuCommand == 0x46) {
        Satellite_DeployStockSatellite();
        ds.Satellite_ActionDone = 1;
    }
    if (ds.Game_MenuCommand == 0x14) {
        Satellite_DeploySatellite();
        ds.Satellite_ActionDone = 1;
    }
    if (ds.Game_MenuCommand == 0x43) {
        Satellite_DeploySpySat();
        ds.Satellite_ActionDone = 1;
    }
    if (ds.Game_MenuCommand == 0x44) {
        Satellite_DeploySpyShip();
        ds.Satellite_ActionDone = 1;
    }
    if (ds.Game_MenuCommand == 0x45) {
        Satellite_DeploySolarSat();
        ds.Satellite_ActionDone = 1;
    }
    const TBody *b = CurBody(); // read before the redraw, as the original does
    if (ds.Satellite_ActionDone != 0) {
        if (ds.Game_CurrentScreen == 8 && ds.PlanetInfo_AlienMode == 0)
            PlanetInfo_DrawPlanetInfoText();
        if (ds.Game_CurrentScreen == 8 && ds.PlanetInfo_AlienMode == 1)
            PlanetInfo_DrawStoresList();
        if (ds.Game_CurrentScreen == 0x11) {
            Satellite_SetupGroupScreenMenu();
        } else {
            bool any;
            GfxPath_SetToolbar(ds.Game_CurrentScreen);
            if (b->Owner == 1 && b->Colony != 0) {
                if (ds.Game_CurrentScreen == 0x14)
                    GfxPath_AddToolbarIcon(0x42);
                if (ds.Game_CurrentScreen == 8)
                    GfxPath_AddToolbarIcon(0x26);
                if (ds.Game_CurrentScreen == 8)
                    GfxPath_AddToolbarIcon(0x27);
            }
            Satellite_AddBodyActionItems(ds.Game_CurSystem, CUR_PLANET, CUR_MOON, &any);
            GfxPath_ShowToolbar();
        }
    }
    if (ds.Game_MenuCommand == 5) {
        ds.Game_ScreenSwitch = 1;
        ds.Game_CurrentScreen = 0x1C;
    }
}

// 28cd:0a98: satellites of satType in the carrier groups docked or in orbit at system:planet
int32_t Satellite_CountSatellites(int16_t system, int16_t planet, int16_t moon, int16_t satType)
{
    (void)moon; // not compared by the original
    int32_t sum = 0;
    for (int16_t i = 1; i <= ds.Game_GroupCount; i++) {
        const TGroup *g = Groups_At(0, i);
        if (AtLocation(g, system, planet) && g->Kind == 4)
            sum += g->Units[0][0].Weapons[satType - 1];
    }
    return sum;
}

// Takes min(amount, *field) off a 16-bit count and returns the rest of amount.
static int32_t Take(int16_t *field, int32_t amount)
{
    int32_t n = *field < amount ? *field : amount;
    *field = (int16_t)(*field - (uint16_t)n);
    return amount - n;
}

// 28cd:0b2c: the group selected in screen 0x11 (SelGroup0) pays first
void Satellite_DeductSatellites(int16_t system, int16_t planet, int16_t moon, int16_t satType,
                                int32_t amount)
{
    (void)moon;
    if (ds.Game_CurrentScreen == 0x11) {
        TGroup *g = Groups_At(0, ds.Game_SelGroup0);
        if (AtLocation(g, system, planet) && g->Kind == 4)
            amount = Take(&g->Units[0][0].Weapons[satType - 1], amount);
    }
    int16_t count = ds.Game_GroupCount;
    for (int16_t i = 1; i <= count; i++) {
        TGroup *g = Groups_At(0, i);
        if (AtLocation(g, system, planet) && g->Kind == 4)
            amount = Take(&g->Units[0][0].Weapons[satType - 1], amount);
    }
}

// 28cd:0c81: the trade ships are the 4th weapon field of the four slots in the first row
int32_t Satellite_CountTradeShips(int16_t system, int16_t planet, int16_t moon)
{
    (void)moon;
    int32_t sum = 0;
    for (int16_t i = 1; i <= ds.Game_GroupCount; i++) {
        const TGroup *g = Groups_At(0, i);
        if (AtLocation(g, system, planet) && g->Kind == 2)
            for (int k = 0; k < 4; k++)
                sum += g->Units[0][k].Weapons[3];
    }
    return sum;
}

// 28cd:0d27
void Satellite_TakeTradeShips(int16_t system, int16_t planet, int16_t moon, int32_t amount)
{
    (void)moon;
    if (ds.Game_CurrentScreen == 0x11) {
        TGroup *g = Groups_At(0, ds.Game_SelGroup0);
        if (AtLocation(g, system, planet) && g->Kind == 2)
            for (int k = 0; k < 4; k++)
                amount = Take(&g->Units[0][k].Weapons[3], amount);
    }
    int16_t count = ds.Game_GroupCount;
    for (int16_t i = 1; i <= count; i++) {
        TGroup *g = Groups_At(0, i);
        if (AtLocation(g, system, planet) && g->Kind == 2)
            for (int k = 0; k < 4; k++)
                amount = Take(&g->Units[0][k].Weapons[3], amount);
    }
}

// 28cd:0ea4: Groups::UpdateSatMenu := 28cd:03fb
void Satellite_Init(void)
{
    ds.Groups_UpdateSatMenu = far_ptr(0x28CD, 0x03FB);
}
