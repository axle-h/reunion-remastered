#include "game/droids.h"

#include "game/blit.h"
#include "game/dseg.h"
#include "game/gfx.h"
#include "game/gfxpath.h"
#include "game/groups.h"
#include "game/mouse.h"
#include "game/planetinfo.h"
#include "game/sound.h"
#include "game/system.h"

// Not ported yet: see stubs.c.
int16_t Surface_CountBuildings(int16_t kind, int16_t system, int16_t planet, int16_t moon,
                               bool activeOnly); // 2ef2:2b1a

#define P(lit) ((const uint8_t *)PSTR(lit))

// Unnamed global: the 0x8980-byte picture of grafika\szamok (droid figures,
// face digits at +5780h, small digits at +5963h).
#define DIGITS_BUF (*(FarPtr *)ds_at(0x7812))

static bool at_home_planet(void)
{
    return ds.Game_CurSystem == 1 && ds.Game_CurPlanet[0] == 5 && ds.Game_CurMoon[0] == 0;
}

static int16_t cur_planet(void)
{
    return ds.Game_CurPlanet[ds.Game_CurSystem - 1];
}

static int16_t cur_moon(void)
{
    return ds.Game_CurMoon[ds.Game_CurSystem - 1];
}

static FarPtr far_add(FarPtr p, uint16_t delta)
{
    return far_ptr((uint16_t)(p >> 16), (uint16_t)((uint16_t)p + delta));
}

// 2527:001f
void Droids_DroidsScreen(void)
{
    if (ds.Game_ScreenEntered) {
        PlanetInfo_SelectCurBody();
        Droids_SetupDroidsHotspots();
        GfxPath_ShowScreenBackdrop();
        DIGITS_BUF = System_GetMem(0x8980);
        Gfx_LoadPic(P("grafika\\szamok"), DIGITS_BUF, 1, 0x6E, 0, 0, 0x140, false, Gfx_PalettePtr(),
                    0, 0xB0, 0x40, 0, 0xB0);
        Droids_DrawDroidsScreen();
        Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
    }
    int16_t force = Groups_FindPlanetForce(ds.Game_CurSystem, cur_planet(), cur_moon());
    // The planet force's droid stock is Items[13]; New Earth keeps its own in
    // product 2's stock.
    int16_t stock = at_home_planet() ? ds.Research_Products[1].Stock : Groups_At(1, force)->Items[12];
    if (ds.Game_MenuCommand == 0x19 && stock > 0) {
        TBody *body = far_host(ds.Game_CurBody);
        int16_t a = Surface_CountBuildings(0x19, ds.Game_CurSystem, cur_planet(), cur_moon(), false);
        int16_t b = Surface_CountBuildings(4, ds.Game_CurSystem, cur_planet(), cur_moon(), false);
        if (body->_0A < b + a && body->Colony != 0 && body->Owner == 1 && body->_0A < 9) {
            Sound_PlayButtonSfx(P("addroid"));
            body->_0A++;
            if (at_home_planet())
                ds.Research_Products[1].Stock--;
            else
                Groups_At(1, force)->Items[12]--;
            Droids_DrawDroidsScreen();
        }
    }
    if (ds.Game_HotspotHit == 0x15) {
        Sound_PlayButtonSfx(P("surface"));
        ds.Game_CurrentScreen = 0x14;
        ds.Game_ScreenSwitch = true;
    }
}

// 2527:01e5
void Droids_DrawDroidsScreen(void)
{
    const TBody *body = far_host(ds.Game_CurBody);
    uint16_t droids = body->_0A;
    int16_t stock;
    const int32_t *stores;
    if (at_home_planet()) {
        stock = ds.Research_Products[1].Stock;
        stores = ds.Game_HomeStock;
    } else {
        int16_t force = Groups_FindPlanetForce(ds.Game_CurSystem, cur_planet(), cur_moon());
        stock = body->Colony == 0 ? 0 : Groups_At(1, force)->Items[12];
        stores = body->Stores;
    }
    int16_t cap = (int16_t)(Surface_CountBuildings(0x19, ds.Game_CurSystem, cur_planet(), cur_moon(), false)
                            + Surface_CountBuildings(4, ds.Game_CurSystem, cur_planet(), cur_moon(), false));
    if (stock > 99)
        stock = 99;
    if (cap > 99)
        cap = 99;

    FarPtr scr = far_ptr(ds.Gfx_ScreenSeg, 0);
    Blit_CopyRect(far_add(DIGITS_BUF, (uint16_t)((droids % 5) * 0x40 + (droids / 5) * 0x2BC0)),
                  far_add(scr, 0xB422), 0x30, 0x23);
    Blit_CopyRect(far_add(DIGITS_BUF, (uint16_t)(0x5780 + (stock / 10) * 0x10)), far_add(scr, 0xB470), 0x10, 0x23);
    Blit_CopyRect(far_add(DIGITS_BUF, (uint16_t)(0x5780 + (stock % 10) * 0x10)), far_add(scr, 0xB480), 0x10, 0x23);
    Blit_CopyRect(far_add(DIGITS_BUF, (uint16_t)(0x5963 + (cap / 10) * 13)), far_add(scr, 0x8DB1), 13, 9);
    Blit_CopyRect(far_add(DIGITS_BUF, (uint16_t)(0x5963 + (cap % 10) * 13)), far_add(scr, 0x8DBE), 13, 9);

    PString s;
    for (int16_t k = 1; k <= 6; k++) {
        System_StrLong(stores[k - 1], 6, &s, 0xFF);
        if (stores[k - 1] == 0)
            System_StrStore(PSTR("     -"), &s, 0xFF);
        Gfx_DrawTextF9((const uint8_t *)&s, 0x3B, (int16_t)((k - 1) * 8 + 0x39), 6);
    }
    if (body->Owner == 1 && (body->Colony != 0 || body->_0A != 0)) {
        for (int16_t k = 1; k <= 6; k++) {
            uint8_t deposit = body->Deposits[k - 1];
            System_StrLong((int16_t)((int16_t)(deposit * body->_0A) / 10), 2, &s, 0xFF);
            if (k == 1)
                System_StrStore(PSTR("--"), &s, 0xFF);
            if (deposit < 10)
                System_StrStore(PSTR(" -"), &s, 0xFF);
            Gfx_DrawTextF9((const uint8_t *)&s, 0x71, (int16_t)((k - 1) * 8 + 0x39), 2);
        }
    }
}

// 2527:0578
void Droids_SetupDroidsHotspots(void)
{
    ds.Mouse_HotspotCount = 1;
    Mouse_SetHotspot(1, 0, 0x31, 0x140, 0x97, P("Back to surface"), 1, 0);
}
