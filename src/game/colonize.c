#include "game/colonize.h"

#include <SDL3/SDL.h>

#include "game/blit.h"
#include "game/gfx.h"
#include "game/groups.h"
#include "game/lang.h"
#include "game/message.h"
#include "game/mouse.h"
#include "game/planetinfo.h"
#include "game/satellite.h"
#include "game/sound.h"
#include "game/system.h"

// Routines missing from their units' headers.
void Surface_DrawBuildingTiles(int16_t kind, int16_t x, int16_t y); // 2ef2:1b98


static int16_t cur_planet(void)
{
    return ds.Game_CurPlanet[ds.Game_CurSystem - 1];
}

static int16_t cur_moon(void)
{
    return ds.Game_CurMoon[ds.Game_CurSystem - 1];
}

// 2841:02ff..033c and 07bb..07f3 (the same test in both routines).
bool Colonize_TypeAvailable(const TBuildingType *bt)
{
    if (bt->NeedProduct == 0) {
        int16_t need = (int8_t)bt->_10;
        if (need < 0 || ds.Game_StaffExp[1] >= (uint16_t)need)
            return true;
    }
    return (int8_t)bt->NeedProduct > 0 && ds.Research_Products[bt->NeedProduct - 1].State == 5;
}

bool Colonize_SuitsSurface(const TBuildingType *bt, int8_t surfaceType)
{
    return ((const uint8_t *)bt)[0x13 + surfaceType] != 0;
}

// 2841:0060
void Colonize_ColonizeScreen(void)
{
    if (ds.Game_ScreenEntered) {
        if ((ds.Game_StaffHired[1] == 2 && ds.Game_CurSystem < 3) || ds.Game_StaffHired[1] == 3) {
            ds.Colonize_ColonyCost = 100000;
            const TBody *body = ds.Game_CurBody;
            ds.Surface_TileSet = ds.Surface_Tileset[(int8_t)body->SurfaceType - 1];
            Colonize_DrawColonizeScreen();
            Colonize_SetupColonizeHotspots();
            TColonySetup *cs = &ds.Game_ColonySetups[ds.Game_ColonySetupCount++];
            cs->Active = 0;
            for (int k = 0; k < 6; k++)
                cs->Timers[k] = 0;
            cs->System = (uint8_t)ds.Game_CurSystem;
            cs->Planet = (uint8_t)cur_planet();
            cs->Moon = (uint8_t)cur_moon();
        } else {
            if (ds.Game_StaffHired[1] == 0)
                Message_ShowMessageNoLog(Lang_Text("|  You haven't got a builder!  |"));
            else
                Message_ShowMessageNoLog(Lang_Text("|  You builder not able to build this colony  |"));
            int16_t cur = ds.Game_CurrentScreen;
            ds.Game_CurrentScreen = ds.Game_PrevScreen;
            ds.Game_PrevScreen = cur;
            ds.Game_ScreenEntered = false;
            ds.Game_ScreenSwitch = true;
        }
    }
    if (ds.Game_MenuCommand == 0x3B) {
        if (ds.Game_Credits < ds.Colonize_ColonyCost) {
            Sound_PlayButtonSfx("hiba");
            Gfx_FlashCredits();
        } else {
            game_free(ds.Surface_BufC);
            ds.Game_CurrentScreen = 0x14;
            ds.Game_ScreenSwitch = true;
            Satellite_AcquireBody();
            ds.Game_GroupList = 1;
            ds.Game_ShownGroupCount = (int16_t)(ds.Game_PlanetForceCount + 1);
            ds.Game_PlanetForceCount = ds.Game_ShownGroupCount;
            ds.Game_SelGroup = ds.Game_ShownGroupCount;
            ds.Game_SelGroup1 = ds.Game_ShownGroupCount;
            Groups_ClearGroup();
            char name[256], tmp[256];
            PlanetInfo_GetBodyName(name, sizeof name, ds.Game_CurSystem, cur_planet(), cur_moon());
            TGroup *g = Groups_At(1, ds.Game_SelGroup);
            SDL_snprintf(tmp, sizeof tmp, Lang_Format(0x28410228, "%s forces"), name);
            pstr_set(&g->Name, 0x11, tmp);
            g->Kind = 5;
            g->Status = 7;
            g->System = (uint8_t)ds.Game_CurSystem;
            g->Planet = (uint8_t)cur_planet();
            g->Moon = (uint8_t)cur_moon();
            ds.Game_Credits =
                (int32_t)((uint32_t)ds.Game_Credits - (uint32_t)ds.Colonize_ColonyCost);
            Gfx_DrawCredits();
        }
    }
    if (ds.Game_MenuCommand == 0x3A) {
        game_free(ds.Surface_BufC);
        ds.Game_CurrentScreen = 0x14;
        ds.Game_ScreenSwitch = true;
        ds.Game_ColonySetupCount--;
    }

    // Hotspots 18h..1Dh are the six starter buildings.
    if ((uint16_t)ds.Game_HotspotHit < 0x18 || (uint16_t)ds.Game_HotspotHit > 0x1D)
        return;
    int16_t slot = (int16_t)(ds.Game_HotspotHit - 0x17);
    const TStarterBuilding *sb = &ds.Colony_StarterBuildings[slot - 1];
    const TBuildingType *bt = &ds.Surface_BuildingTypes[sb->Building - 1];
    if (!Colonize_TypeAvailable(bt))
        return;
    const TBody *body = ds.Game_CurBody;
    if (!Colonize_SuitsSurface(bt, (int8_t)body->SurfaceType))
        return;
    int16_t *picked = &ds.Game_ColonySetups[ds.Game_ColonySetupCount - 1].Timers[slot - 1];
    if (*picked == 0) {
        // Select: the colony plus the chosen buildings must stay payable.
        if (ds.Colonize_ColonyCost + bt->Cost <= ds.Game_Credits) {
            Sound_PlayButtonSfx("x");
            Gfx_DrawCredits();
            *picked = 1;
            Surface_DrawBuildingTiles(sb->Building, sb->X, sb->Y);
            ds.Colonize_ColonyCost += bt->Cost;
        } else {
            Sound_PlayButtonSfx("hiba");
            Gfx_FlashCredits();
        }
    } else {
        Sound_PlayButtonSfx("x");
        Gfx_DrawCredits();
        *picked = 0;
        Gfx_DrawDitherRect(sb->X, sb->Y, (int16_t)(bt->Width * 16 + sb->X - 1),
                           (int16_t)(bt->Height * 16 + sb->Y - 1), 0x40);
        ds.Colonize_ColonyCost -= bt->Cost;
    }
    char s[16];
    SDL_snprintf(s, sizeof s, "%d", (int)ds.Colonize_ColonyCost);
    Gfx_DrawTextF9(s, 0x114, 0xB9, 6);
}

// 2841:04c6
void Colonize_SetupColonizeHotspots(void)
{
    ds.Mouse_HotspotCount = 9;
    Mouse_SetHotspot(1, 0x10B, 0x36, 0x30, 0x30, Lang_TextAt(0x284104e6, "Change people"), 1, 0);
    Mouse_SetHotspot(2, 0x10B, 0x69, 0x17, 0x0B, Lang_TextAt(0x28410517, "Previous people"), 1, 0);
    Mouse_SetHotspot(3, 0x123, 0x69, 0x17, 0x0B, Lang_TextAt(0x28410548, "Next people"), 1, 0);
    for (int16_t k = 1; k <= 6; k++) {
        const TStarterBuilding *sb = &ds.Colony_StarterBuildings[k - 1];
        const TBuildingType *bt = &ds.Surface_BuildingTypes[sb->Building - 1];
        THotspot *h = Mouse_Hotspot((int16_t)(k + 3));
        h->X = sb->X;
        h->Y = (uint8_t)sb->Y;
        h->W = (int16_t)(sb->W << 4);
        h->H = (uint8_t)(sb->H << 4);
        char name[256];
        pstr_set(&h->Caption, 0x12, pstr_get(name, sizeof name, &bt->Name));
        h->Repeat_ = 1;
        h->_1A = 0;
    }
}

// 2841:062d
void Colonize_DrawColonizeScreen(void)
{
    PlanetInfo_SelectCurBody();
    uint8_t *buf = game_alloc(0xBCC0);
    Gfx_LoadPic("grafika\\koloniz", buf, 1, 0x97, 0, 0, 0x140, true, Gfx_PalettePtr(),
                0, 0xB0, 0x40, 0, 0xBE);
    Blit_CopyRect(buf, gfx_screen() + 0x3D40, 0x140, 0x97);
    game_free(buf);
    char s[16], name[32];
    ds.Surface_BufC = game_alloc(0xF000);
    SDL_snprintf(name, sizeof name, "planets\\epul%d", ds.Surface_TileSet);
    Gfx_LoadPic(name, ds.Surface_BufC, 1, 0xC0, 0, 0, 0x140, false,
                Gfx_PalettePtr(), 0, 0x3F, 0x90, 0, 0xBE);
    const TBody *body = ds.Game_CurBody;
    for (int16_t k = 1; k <= 6; k++) {
        const TStarterBuilding *sb = &ds.Colony_StarterBuildings[k - 1];
        const TBuildingType *bt = &ds.Surface_BuildingTypes[sb->Building - 1];
        Surface_DrawBuildingTiles(sb->Building, sb->X, sb->Y);
        Gfx_DrawDitherRect(sb->X, sb->Y, (int16_t)(bt->Width * 16 + sb->X - 1),
                           (int16_t)(bt->Height * 16 + sb->Y - 1), 0x40);
        SDL_snprintf(s, sizeof s, "%d", (int)bt->Cost);
        if (!Colonize_TypeAvailable(bt) || !Colonize_SuitsSurface(bt, (int8_t)body->SurfaceType))
            SDL_strlcpy(s, Lang_Text("Sorry"), sizeof s);
        Gfx_DrawTextF9(s, (int16_t)(sb->X + 2), (int16_t)(sb->Y - 0xB), 5);
    }
    Gfx_DrawTextF9(Lang_Text("Cost of colony:"), 0xB4, 0xB9, 0xF);
    Gfx_DrawTextF9("100000", 0x114, 0xB9, 6);
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
}

