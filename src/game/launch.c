#include "game/launch.h"

#include "game/groups.h"
#include "game/sound.h"
#include "game/system.h"

// Routines of units that are not ported yet (stubs in stubs.c).
void Intel_SetGroupTarget(int16_t group, int16_t system, int16_t planet, int16_t moon); // 29b9:0e66
void GfxPath_SetToolbarAndShow(int16_t screen);  // 3a1b:0019
void GfxPath_SetToolbar(int16_t screen);         // 3a1b:0030
void GfxPath_AddToolbarIcon(int16_t id);         // 3a1b:0366
void GfxPath_ShowToolbar(void);                  // 3a1b:0704
void PlanetInfo_SetupSystemViewHotspots(void);   // 357b:18cc
void PlanetInfo_InitSystemView(void);            // 357b:235a
void PlanetInfo_ReloadSystemView(void);          // 357b:26ee
void PlanetInfo_SelectCurBody(void);             // 357b:3253
void Message_ShowMessageNoLog(const uint8_t *text); // 34b0:0197

// Unnamed globals.
#define W(off) (*(int16_t *)ds_at(off))
#define B(off) (*(uint8_t *)ds_at(off))
#define DS_787C B(0x787C)    // set while the destination screen is active (map must not set a target)
#define SYS_TYPE(i) W(0x7BC4 + (i) * 6) // system view table, 1..18 entries of {type, index}; type <= 4: group
#define SYS_IDX(i) W(0x7BC6 + (i) * 6)
#define SEL_ENTRY W(0x7C36)  // selected system-view table entry
#define SEL_MOON W(0x7C38)   // moon number + 1 for the system view cursor
#define TB_A B(0x7C3A)       // toolbar state last shown (icons 0x37 / 0x38 / 0x47)
#define TB_B B(0x7C3B)
#define TB_C B(0x7C3C)
#define TB_D B(0x7C3D)
#define TB_E B(0x7C3E)
#define TB_F B(0x7C3F)
#define SAVED_SCREEN W(0x7C40) // destination screen: where to return, and the map position to restore
#define SAVED_SYSTEM W(0x7C42)
#define SAVED_PLANET W(0x7C44)
// Race records of 0xE4 bytes at DS:6A1C; byte +5 is a state, +0x1B*i bytes of per-entry data.
#define RACE_STATE(i) B(0x6A21 + (i) * 0xE4)
#define RACE_DATA(i, k) B(0x6A1C + (i) * 0xE4 + (k) * 0x1B)

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
        if (ds.Game_PrevScreen == 0x12 && DS_787C == 0)
            Intel_SetGroupTarget(ds.Game_SelGroup, ds.Game_CurSystem, CUR_PLANET, CUR_MOON);
        int16_t p = ds.Game_PrevScreen;
        if ((p != 8 && p != 0x11 && p != 0x12 && p != 0x13 && p != 0x14 && p != 0x15 && p != 0x16
             && p != 0x1D && p != 0x1F && p != 0x20)
            || ds.Game_MenuCommand == 0x22)
            CUR_PLANET = 0;
        SEL_ENTRY = 0;
        SEL_MOON = 0;
        TB_A = 0;
        TB_E = 0;
        TB_C = 0;
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
        if (SYS_TYPE(e) < 5) {
            Sound_PlayButtonSfx(SFX("controll"));
            ds.Game_GroupList = 0;
            ds.Game_SelGroup = SYS_IDX(e);
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
        TB_F = 0;
        bool anyFighter = false;
        for (int16_t i = 1; i <= 0x12; i++)
            if (SYS_TYPE(i) == 1)
                anyFighter = true;
        SEL_ENTRY = ds.Game_HotspotHit - 0x14;
        if (SYS_TYPE(e) < 5) {
            TGroup *g = Groups_At(0, SYS_IDX(e));
            SEL_MOON = g->Moon + 1;
            ds.Game_GroupList = 0;
            ds.Game_SelGroup = SYS_IDX(e);
            ds.Game_ShownGroupCount = ds.Game_GroupCount;
            TB_B = 0;
            TB_D = 1;
            ds.Game_SelGroup0 = ds.Game_SelGroup;
            CUR_MOON = g->Moon;
            PlanetInfo_SelectCurBody();
            TBody *body = (TBody *)far_host(ds.Game_CurBody);
            if ((int8_t)body->Owner > 1 && body->InfoLevel > 0x27
                && (int8_t)RACE_STATE((int8_t)body->Owner) < 6)
                TB_F = g->Kind == 1 ? 1 : 0;
        } else {
            SEL_MOON = RACE_DATA(SYS_TYPE(e) - 3, SYS_IDX(e)) + 1;
            if (anyFighter && (int8_t)RACE_STATE(SYS_TYPE(e) - 3) < 6)
                TB_B = 1;
            else
                TB_B = 0;
            TB_F = 0;
            TB_D = 0;
        }
        if (TB_B != TB_A || TB_D != TB_C || TB_F != TB_E) {
            GfxPath_SetToolbar(7);
            GfxPath_AddToolbarIcon(0x21);
            if (TB_B)
                GfxPath_AddToolbarIcon(0x37);
            if (TB_D)
                GfxPath_AddToolbarIcon(0x38);
            if (TB_F)
                GfxPath_AddToolbarIcon(0x47);
            GfxPath_ShowToolbar();
        }
        TB_C = TB_D;
        TB_A = TB_B;
        TB_E = TB_F;
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
            SEL_ENTRY = 0;
            SEL_MOON = 0;
            TB_A = 0;
            TB_E = 0;
            TB_C = 0;
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
            RACE_STATE(SYS_TYPE(SEL_ENTRY) - 3) = 2;
            W(0x7798) = ds.Game_CurSystem;
            W(0x779A) = CUR_PLANET;
            W(0x779C) = 0;
            B(0x7788) = 0;
            B(0x778B) = 1;
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
            RACE_STATE(((TBody *)far_host(ds.Game_CurBody))->Owner) = 2; // BUG? Owner as a signed char in the original
            W(0x7798) = ds.Game_CurSystem;
            W(0x779A) = CUR_PLANET;
            W(0x779C) = CUR_MOON;
            B(0x7788) = 1;
            B(0x778B) = 1;
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
        DS_787C = 1;
        SAVED_SCREEN = ds.Game_PrevScreen;
        SAVED_SYSTEM = ds.Game_CurSystem;
        SAVED_PLANET = CUR_PLANET;
        if (ds.Game_PrevScreen != 7)
            SEL_ENTRY = 0;
        SEL_MOON = Groups_At(0, ds.Game_SelGroup)->Moon + 1;
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
            ds.Game_CurrentScreen = SAVED_SCREEN;
            CUR_PLANET = ds.Game_HotspotHit - 0x27;
            CUR_MOON = 0;
            DS_787C = 0;
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
        ds.Game_CurrentScreen = SAVED_SCREEN;
        CUR_PLANET = 0;
        CUR_MOON = 0;
        DS_787C = 0;
    }

    hit = (uint16_t)ds.Game_HotspotHit;
    if (hit > 0x26 && CUR_PLANET > 0 && !ds.Game_ScreenSwitch) {
        Sound_PlayButtonSfx(SFX("moveship"));
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = SAVED_SCREEN;
        CUR_MOON = ds.Game_HotspotHit - 0x27;
        DS_787C = 0;
    }

    if (ds.Game_MenuCommand == 0x39) {
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = SAVED_SCREEN;
        DS_787C = 1;
        ds.Game_CurSystem = SAVED_SYSTEM;
        CUR_PLANET = SAVED_PLANET;
    }

    if (ds.Game_MenuCommand == 0x21 && CUR_PLANET > 0) {
        GfxPath_SetToolbarAndShow(0x12);
        CUR_PLANET = 0;
        PlanetInfo_SetupSystemViewHotspots();
        PlanetInfo_ReloadSystemView();
    }
}
