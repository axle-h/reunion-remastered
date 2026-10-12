#include "game/launch.h"

#include "game/groups.h"
#include "game/intel.h"
#include "game/sound.h"
#include "game/system.h"

// Routines missing from their units' headers.
void GfxPath_SetToolbarAndShow(int16_t screen);  // 3a1b:0019
void GfxPath_SetToolbar(int16_t screen);         // 3a1b:0030
void GfxPath_AddToolbarIcon(int16_t id);         // 3a1b:0366
void GfxPath_ShowToolbar(void);                  // 3a1b:0704
void PlanetInfo_SetupSystemViewHotspots(void);   // 357b:18cc
void PlanetInfo_InitSystemView(void);            // 357b:235a
void PlanetInfo_ReloadSystemView(void);          // 357b:26ee
void PlanetInfo_SelectCurBody(void);             // 357b:3253
void Message_ShowMessageNoLog(const uint8_t *text); // 34b0:0197

// The system view table (Launch::SystemViewTable, filled by PlanetInfo): Kind <= 4 is a
// group, else the force of race Kind - 3; Index is the group or force number.
#define SYS_VIEW(i) (ds.Launch_SystemViewTable[(i) - 1])
// Races::Races[i].Status and Forces[k].Moon (Intel_Race also reaches "race 1").
#define RACE_STATE(i) (Intel_Race(i)->Status)
#define RACE_MOON(i, k) (Intel_Race(i)->Forces[(k) - 1].Moon)

// Game_CurPlanet[CurSystem] / Game_CurMoon[CurSystem] (arrays are 1-based in the original).
#define CUR_PLANET (ds.Game_CurPlanet[ds.Game_CurSystem - 1])
#define CUR_MOON (ds.Game_CurMoon[ds.Game_CurSystem - 1])

#define SFX(lit) ((const uint8_t *)PSTR(lit))
#define MSG(lit) Message_ShowMessageNoLog((const uint8_t *)PSTR(lit))

// 18e0:00e8
void Launch_MapScreen(void)
{
    uint16_t hit;

    if (ds.Game_ScreenEntered) {
        if (ds.Game_PrevScreen == 0x12 && ds.Launch_DestinationActive == 0)
            Intel_SetGroupTarget(ds.Game_SelGroup, ds.Game_CurSystem, CUR_PLANET, CUR_MOON);
        int16_t p = ds.Game_PrevScreen;
        if ((p != 8 && p != 0x11 && p != 0x12 && p != 0x13 && p != 0x14 && p != 0x15 && p != 0x16
             && p != 0x1D && p != 0x1F && p != 0x20)
            || ds.Game_MenuCommand == 0x22)
            CUR_PLANET = 0;
        ds.Launch_SelectedEntry = 0;
        ds.Launch_SavedBody = 0;
        ds.Launch_AttackIconShown = 0;
        ds.Launch_InvadeIconShown = 0;
        ds.Launch_MoveIconShown = 0;
        if (CUR_PLANET == 0) {
            GfxPath_SetToolbarAndShow(7);
        } else {
            GfxPath_SetToolbar(7);
            GfxPath_AddToolbarIcon(0x21);
            GfxPath_ShowToolbar();
        }
        PlanetInfo_SetupSystemViewHotspots();
        PlanetInfo_InitSystemView();
    }

    hit = (uint16_t)ds.Game_HotspotHit;

    // A star system was clicked while the galaxy is shown.
    if (hit > 0x14 && hit < 0x1D && CUR_PLANET == 0) {
        if (ds.Game_CurSystem < 0 || ds.Game_CurSystem != ds.Game_HotspotHit - 0x14) {
            Sound_PlayButtonSfx(SFX("x"));
            ds.Game_CurSystem = ds.Game_HotspotHit - 0x14;
            CUR_PLANET = 0;
            PlanetInfo_SetupSystemViewHotspots();
            PlanetInfo_ReloadSystemView();
            ds.Game_HotspotHit = 0;
        } else {
            Sound_PlayButtonSfx(SFX("x"));
        }
    }

    // A fleet/planet-force entry clicked in the system view, button released.
    hit = (uint16_t)ds.Game_HotspotHit;
    if (hit > 0x14 && hit < 0x27 && CUR_PLANET != 0 && ds.Mouse_HeldLeft == 0) {
        int16_t e = ds.Game_HotspotHit - 0x14;
        if (SYS_VIEW(e).Kind < 5) {
            Sound_PlayButtonSfx(SFX("controll"));
            ds.Game_GroupList = 0;
            ds.Game_SelGroup = SYS_VIEW(e).Index;
            ds.Game_ShownGroupCount = ds.Game_GroupCount;
            ds.Game_CurrentScreen = 0x11;
            ds.Game_ScreenSwitch = true;
            ds.Game_SelGroup0 = ds.Game_SelGroup;
        } else {
            Sound_PlayButtonSfx(SFX("x"));
        }
    }

    // Same hotspots with the button held: select the entry and update the toolbar.
    hit = (uint16_t)ds.Game_HotspotHit;
    if (hit > 0x14 && hit < 0x27 && CUR_PLANET != 0 && ds.Mouse_HeldLeft != 0
        && ds.Mouse_Buttons == 0) {
        int16_t e = ds.Game_HotspotHit - 0x14;
        Sound_PlayButtonSfx(SFX("x"));
        ds.Launch_InvadeIcon = 0;
        bool anyFighter = false;
        for (int16_t i = 1; i <= 0x12; i++)
            if (SYS_VIEW(i).Kind == 1)
                anyFighter = true;
        ds.Launch_SelectedEntry = ds.Game_HotspotHit - 0x14;
        if (SYS_VIEW(e).Kind < 5) {
            TGroup *g = Groups_At(0, SYS_VIEW(e).Index);
            ds.Launch_SavedBody = g->Moon + 1;
            ds.Game_GroupList = 0;
            ds.Game_SelGroup = SYS_VIEW(e).Index;
            ds.Game_ShownGroupCount = ds.Game_GroupCount;
            ds.Launch_AttackIcon = 0;
            ds.Launch_MoveIcon = 1;
            ds.Game_SelGroup0 = ds.Game_SelGroup;
            CUR_MOON = g->Moon;
            PlanetInfo_SelectCurBody();
            TBody *body = ds.Game_CurBody;
            if ((int8_t)body->Owner > 1 && body->InfoLevel > 0x27
                && (int8_t)RACE_STATE((int8_t)body->Owner) < 6)
                ds.Launch_InvadeIcon = g->Kind == 1 ? 1 : 0;
        } else {
            ds.Launch_SavedBody = RACE_MOON(SYS_VIEW(e).Kind - 3, SYS_VIEW(e).Index) + 1;
            if (anyFighter && (int8_t)RACE_STATE(SYS_VIEW(e).Kind - 3) < 6)
                ds.Launch_AttackIcon = 1;
            else
                ds.Launch_AttackIcon = 0;
            ds.Launch_InvadeIcon = 0;
            ds.Launch_MoveIcon = 0;
        }
        if (ds.Launch_AttackIcon != ds.Launch_AttackIconShown
            || ds.Launch_MoveIcon != ds.Launch_MoveIconShown
            || ds.Launch_InvadeIcon != ds.Launch_InvadeIconShown) {
            GfxPath_SetToolbar(7);
            GfxPath_AddToolbarIcon(0x21);
            if (ds.Launch_AttackIcon)
                GfxPath_AddToolbarIcon(0x37);
            if (ds.Launch_MoveIcon)
                GfxPath_AddToolbarIcon(0x38);
            if (ds.Launch_InvadeIcon)
                GfxPath_AddToolbarIcon(0x47);
            GfxPath_ShowToolbar();
        }
        ds.Launch_MoveIconShown = ds.Launch_MoveIcon;
        ds.Launch_AttackIconShown = ds.Launch_AttackIcon;
        ds.Launch_InvadeIconShown = ds.Launch_InvadeIcon;
    }

    // A planet was clicked in the system view of an empty selection.
    hit = (uint16_t)ds.Game_HotspotHit;
    if (hit > 0x26 && CUR_PLANET == 0) {
        if (ds.Mouse_HeldLeft == 0) {
            ds.Game_ScreenSwitch = true;
            ds.Game_CurrentScreen = 8;
            CUR_PLANET = ds.Game_HotspotHit - 0x27;
            CUR_MOON = 0;
            Sound_PlayButtonSfx(SFX("planetin"));
        } else {
            CUR_PLANET = ds.Game_HotspotHit - 0x27;
            CUR_MOON = 0;
            ds.Game_HotspotHit = 0;
            Sound_PlayButtonSfx(SFX("planet"));
            GfxPath_SetToolbar(7);
            GfxPath_AddToolbarIcon(0x21);
            GfxPath_ShowToolbar();
            ds.Launch_SelectedEntry = 0;
            ds.Launch_SavedBody = 0;
            ds.Launch_AttackIconShown = 0;
            ds.Launch_InvadeIconShown = 0;
            ds.Launch_MoveIconShown = 0;
            PlanetInfo_SetupSystemViewHotspots();
            PlanetInfo_ReloadSystemView();
        }
    }
    hit = (uint16_t)ds.Game_HotspotHit;
    if (hit > 0x26 && CUR_PLANET > 0 && !ds.Game_ScreenSwitch) {
        ds.Game_ScreenSwitch = true;
        if (ds.Mouse_HeldLeft == 0) {
            ds.Game_CurrentScreen = 8;
            Sound_PlayButtonSfx(SFX("planetin"));
        } else {
            ds.Game_CurrentScreen = 0x14;
            Sound_PlayButtonSfx(SFX("surface"));
        }
        CUR_MOON = ds.Game_HotspotHit - 0x27;
    }

    if (ds.Game_MenuCommand == 0x21 && CUR_PLANET > 0) {
        GfxPath_SetToolbarAndShow(7);
        CUR_PLANET = 0;
        PlanetInfo_SetupSystemViewHotspots();
        PlanetInfo_ReloadSystemView();
    }

    if (ds.Game_MenuCommand == 0x37) {
        if (ds.Game_StaffExp[2] == 0) {
            MSG(" You haven't got a fighter! ");
        } else {
            RACE_STATE(SYS_VIEW(ds.Launch_SelectedEntry).Kind - 3) = 2;
            ds.SpWar_BattleSystem = ds.Game_CurSystem;
            ds.SpWar_BattlePlanet = CUR_PLANET;
            ds.SpWar_BattleMoon = 0;
            ds.SpWar_GroundBattle = 0;
            ds.Sim_PlayerAttacks = 1;
            ds.Game_CurrentScreen = 0x1D;
            ds.Game_ScreenSwitch = true;
        }
    }

    if (ds.Game_MenuCommand == 0x47) {
        if (ds.Game_StaffExp[2] == 0) {
            MSG(" You haven't got a battle advisor! ");
        } else if (ds.Game_StaffHired[2] < 2) {
            MSG(" Your fighter not skilled to lead a ground battle ");
        } else {
            CUR_MOON = Groups_At(0, ds.Game_SelGroup)->Moon;
            PlanetInfo_SelectCurBody();
            RACE_STATE(ds.Game_CurBody->Owner) = 2; // BUG? Owner as a signed char in the original
            ds.SpWar_BattleSystem = ds.Game_CurSystem;
            ds.SpWar_BattlePlanet = CUR_PLANET;
            ds.SpWar_BattleMoon = CUR_MOON;
            ds.SpWar_GroundBattle = 1;
            ds.Sim_PlayerAttacks = 1;
            ds.Game_CurrentScreen = 0x1D;
            ds.Game_ScreenSwitch = true;
        }
    }

    if (ds.Game_MenuCommand == 0x38) {
        int16_t sel = ds.Game_SelGroup;
        TGroup *g = Groups_At(0, sel);
        if (ds.Game_StaffExp[0] == 0 && g->Kind != 4) {
            MSG(" You haven't hired a pilot! ");
        } else if (Groups_IsGroupEmpty(0, sel)) {
            MSG(" No ships in this unit! ");
        } else if (g->Status == 2) {
            Sound_PlayButtonSfx(SFX("x"));
            ds.Game_ScreenSwitch = true;
            ds.Game_CurrentScreen = 0x12;
        } else {
            MSG(" The ships are on ground! ");
        }
    }
}

// 18e0:07ec
void Launch_DestinationScreen(void)
{
    uint16_t hit;

    if (ds.Game_ScreenEntered) {
        if (CUR_PLANET == 0) {
            GfxPath_SetToolbarAndShow(0x12);
        } else {
            GfxPath_SetToolbar(0x12);
            GfxPath_AddToolbarIcon(0x21);
            GfxPath_ShowToolbar();
        }
        ds.Launch_DestinationActive = 1;
        ds.Launch_SavedScreen = ds.Game_PrevScreen;
        ds.Launch_SavedSystem = ds.Game_CurSystem;
        ds.Launch_SavedPlanet = CUR_PLANET;
        if (ds.Game_PrevScreen != 7)
            ds.Launch_SelectedEntry = 0;
        ds.Launch_SavedBody = Groups_At(0, ds.Game_SelGroup)->Moon + 1;
        PlanetInfo_SetupSystemViewHotspots();
        PlanetInfo_InitSystemView();
    }

    hit = (uint16_t)ds.Game_HotspotHit;
    if (hit > 0x14 && hit < 0x1D
        && (ds.Game_CurSystem < 0 || ds.Game_CurSystem != ds.Game_HotspotHit - 0x14)
        && CUR_PLANET == 0) {
        TGroup *g = Groups_At(0, ds.Game_SelGroup);
        if ((g->Kind == 1 && (g->Units[0][2].ShipType > 0 || g->Units[0][3].ShipType > 0))
            || (g->Kind == 2 && g->Units[0][3].ShipType > 0)
            || (g->Kind == 3 && (g->Units[0][2].ShipType > 0 || g->Units[0][3].ShipType > 0))
            || (g->Kind == 4
                && (ds.Research_Products[0xD].State == 5 || ds.Research_Products[0xE].State == 5))) {
            if ((ds.Game_StaffHired[0] == 2 && ds.Game_CurSystem < 4
                 && (uint16_t)(ds.Game_HotspotHit - 0x14) < 4)
                || ds.Game_StaffHired[0] == 3 || g->Kind == 4) {
                Sound_PlayButtonSfx(SFX("x"));
                ds.Game_CurSystem = ds.Game_HotspotHit - 0x14;
                CUR_PLANET = 0;
                PlanetInfo_SetupSystemViewHotspots();
                PlanetInfo_ReloadSystemView();
                ds.Game_HotspotHit = 0;
            } else {
                MSG(" Your pilot is unable to undertake |        such a long voyage!");
            }
        } else {
            MSG(" No hyperdrive in this unit! ");
        }
    }

    hit = (uint16_t)ds.Game_HotspotHit;
    if (CUR_PLANET == 0 && hit > 0x27) {
        if (ds.Mouse_HeldLeft == 0) {
            Sound_PlayButtonSfx(SFX("moveship"));
            ds.Game_ScreenSwitch = true;
            ds.Game_CurrentScreen = ds.Launch_SavedScreen;
            CUR_PLANET = ds.Game_HotspotHit - 0x27;
            CUR_MOON = 0;
            ds.Launch_DestinationActive = 0;
        } else {
            Sound_PlayButtonSfx(SFX("planet"));
            CUR_PLANET = ds.Game_HotspotHit - 0x27;
            GfxPath_SetToolbar(0x12);
            GfxPath_AddToolbarIcon(0x21);
            GfxPath_ShowToolbar();
            PlanetInfo_SetupSystemViewHotspots();
            PlanetInfo_ReloadSystemView();
        }
    }

    if (CUR_PLANET == 0 && ds.Game_HotspotHit == 0x27) {
        Sound_PlayButtonSfx(SFX("moveship"));
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = ds.Launch_SavedScreen;
        CUR_PLANET = 0;
        CUR_MOON = 0;
        ds.Launch_DestinationActive = 0;
    }

    hit = (uint16_t)ds.Game_HotspotHit;
    if (hit > 0x26 && CUR_PLANET > 0 && !ds.Game_ScreenSwitch) {
        Sound_PlayButtonSfx(SFX("moveship"));
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = ds.Launch_SavedScreen;
        CUR_MOON = ds.Game_HotspotHit - 0x27;
        ds.Launch_DestinationActive = 0;
    }

    if (ds.Game_MenuCommand == 0x39) {
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = ds.Launch_SavedScreen;
        ds.Launch_DestinationActive = 1;
        ds.Game_CurSystem = ds.Launch_SavedSystem;
        CUR_PLANET = ds.Launch_SavedPlanet;
    }

    if (ds.Game_MenuCommand == 0x21 && CUR_PLANET > 0) {
        GfxPath_SetToolbarAndShow(0x12);
        CUR_PLANET = 0;
        PlanetInfo_SetupSystemViewHotspots();
        PlanetInfo_ReloadSystemView();
    }
}
