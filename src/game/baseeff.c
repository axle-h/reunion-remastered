#include "game/baseeff.h"

#include "game/dseg.h"
#include "game/gfxpath.h"
#include "game/planetinfo.h"
#include "game/rawtext.h"
#include "game/satellite.h"
#include "game/sound.h"
#include "game/system.h"

#define P(lit) ((const uint8_t *)PSTR(lit))

// 1413:000a
void BaseEff_BaseEffScreen(void)
{
    if (ds.Game_ScreenEntered) {
        PlanetInfo_SelectCurBody();
        GfxPath_SetToolbar(8);
        const TBody *body = ds.Game_CurBody;
        if (ds.Game_PrevScreen == 0x11)
            GfxPath_AddToolbarIcon(0x10);
        if (body->Owner == 1 && body->Colony != 0) {
            GfxPath_AddToolbarIcon(0x26);
            GfxPath_AddToolbarIcon(0x27);
        }
        bool any;
        Satellite_AddBodyActionItems(ds.Game_CurSystem, ds.Game_CurPlanet[ds.Game_CurSystem - 1],
                                     ds.Game_CurMoon[ds.Game_CurSystem - 1], &any);
        ds.PlanetInfo_AlienMode = 0;
        GfxPath_ShowToolbar();
        RawText_LoadSzFaj();
        PlanetInfo_DrawPlanetInfoScreen();
        PlanetInfo_SetupPlanetInfoHotspots();
    }
    Satellite_HandleBodyActions();
    if (ds.Game_MenuCommand == 0x2E) {
        ds.Game_CurMoon[ds.Game_CurSystem - 1] = 0;
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = 7;
    }
    if (ds.Game_MenuCommand == 0x26) {
        TBody *body = ds.Game_CurBody;
        body->TechLevel++;
        if ((int8_t)body->TechLevel > 7)
            body->TechLevel = 7;
        if (ds.PlanetInfo_AlienMode == 0)
            PlanetInfo_DrawPlanetInfoText();
    }
    if (ds.Game_MenuCommand == 0x27) {
        TBody *body = ds.Game_CurBody;
        body->TechLevel--;
        if ((int8_t)body->TechLevel < 0)
            body->TechLevel = 0;
        if (ds.PlanetInfo_AlienMode == 0)
            PlanetInfo_DrawPlanetInfoText();
    }
    const TBody *body = ds.Game_CurBody;
    if (ds.PlanetInfo_AlienMode == 0 && ds.Game_HotspotHit == 0x15) {
        Sound_PlayButtonSfx(P("x"));
        if ((int8_t)body->Owner > 1 && body->InfoLevel > 0x27) {
            ds.PlanetInfo_AlienMode = 1;
            PlanetInfo_DrawAlienInfoScreen();
            PlanetInfo_SetupPlanetInfoHotspots();
        }
        ds.Game_HotspotHit = 0;
    }
    if (ds.PlanetInfo_AlienMode == 0 && ds.Game_HotspotHit == 0x16) {
        Sound_PlayButtonSfx(P("baseeff"));
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = 0x10;
    }
    if (ds.PlanetInfo_AlienMode == 0 && ds.Game_HotspotHit == 0x17) {
        Sound_PlayButtonSfx(P("x"));
        if (body->InfoLevel > 5) {
            ds.PlanetInfo_AlienMode = 3;
            PlanetInfo_DrawPlanetSurfaceBackdrop();
            PlanetInfo_SetupPlanetInfoHotspots();
            ds.Game_HotspotHit = 0;
        }
    }
    if (ds.PlanetInfo_AlienMode != 0 && ds.Game_HotspotHit == 0x15) {
        Sound_PlayButtonSfx(P("x"));
        ds.PlanetInfo_AlienMode = 0;
        PlanetInfo_DrawPlanetInfoScreen();
        PlanetInfo_SetupPlanetInfoHotspots();
    }
}
