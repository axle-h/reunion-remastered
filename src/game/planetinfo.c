#include "game/planetinfo.h"

#include <string.h>

#include "game/blit.h"
#include "game/gfx.h"
#include "game/groups.h"
#include "game/mouse.h"
#include "game/rawtext.h"
#include "game/scale.h"
#include "game/system.h"

// Not ported yet: see stubs.c.
int16_t Surface_CountBuildings(int16_t kind, int16_t system, int16_t planet, int16_t moon,
                               bool activeOnly); // 2ef2:2b1a

#define P(lit) ((const uint8_t *)PSTR(lit))
#define PS(p) ((const uint8_t *)(p))

// ---------------------------------------------------------------------------
// Unnamed globals of the unit.
// ---------------------------------------------------------------------------
// System view buffers (far pointers): the background picture (hatter1/2,
// 320x151), the icon sheet (hatter3/4, 320x75), the planet and moon sprite
// sheet (planets\napr<system>), the frame being composed and the two scaled
// sprite buffers.
#define SV_BACKDROP (*(FarPtr *)ds_at(0x7B0E))
#define SV_ICONS (*(FarPtr *)ds_at(0x7B12))
#define SV_SPRITES (*(FarPtr *)ds_at(0x7B16))
#define SV_FRAME (*(FarPtr *)ds_at(0x7B1A))
#define SV_MOON (*(FarPtr *)ds_at(0x7B1E))
#define SV_PLANET (*(FarPtr *)ds_at(0x7B22))
// Set by SetupSystemViewHotspots when CurSystem is 4 and the byte at DS:5D90
// (just before Research_Products) is set: the bodies are not drawn and the
// body hotspots are not active.
#define SV_HIDDEN (*(uint8_t *)ds_at(0x7B26))
// Selected body marker: 7C38 = body number (1 = the planet, k + 1 = moon k),
// 7C42/7C44 = its system and planet.
#define SV_MARK_BODY (*(int16_t *)ds_at(0x7C38))
#define SV_MARK_SYS (*(int16_t *)ds_at(0x7C42))
#define SV_MARK_PLANET (*(int16_t *)ds_at(0x7C44))
// Selected entry (1..18) of the fleet/group hotspot table, 0 = none.
#define SV_SELECTED (*(int16_t *)ds_at(0x7C36))
// Countdown (20..0) of the blinking frame in the unknown-system view.
#define SV_BLINK (*(int16_t *)ds_at(0x7C46))
// The body has fleets / a base the player can see, to draw the group table.
#define SV_HAS_GROUPS (*(uint8_t *)ds_at(0x7C48))
#define SV_HAS_BASE (*(uint8_t *)ds_at(0x7C49))
// Hotspot found by Mouse_HitTestCursor in RenderSystemView.
#define SV_HIT (*(int16_t *)ds_at(0x919C))

// Per-body orbit record: DS:7B28 + 18 * k (k = 1 for the planet or moon 1 ...
// 8). rx/ry: orbit radii from the name table, ang: tenths of a degree, speed:
// the body's Size, x/y/size/half: where and how big it is drawn, icon: the
// BodyIcon byte, flag: marker for the status (1 = known, owner + 1 =
// alien, 2 = player's).
typedef struct OrbitRec {
    int16_t rx;
    int16_t ry;
    int16_t ang;
    uint16_t speed;
    int16_t x;
    uint16_t y;
    int16_t size;
    int16_t half;
    uint8_t icon;
    uint8_t flag;
} OrbitRec;
static_assert(sizeof(OrbitRec) == 18);

static OrbitRec *orbit(int16_t k)
{
    return (OrbitRec *)ds_at((uint16_t)(0x7B28 + k * 18));
}

// Fleet table entries at DS:7BC4 + 6 * i (i = 1..18): the group kind (0 =
// none), its number and its moon.
typedef struct GroupTab {
    int16_t kind;
    int16_t idx;
    int16_t moon;
} GroupTab;
static GroupTab *group_tab(int16_t i)
{
    return (GroupTab *)ds_at((uint16_t)(0x7BC4 + i * 6));
}

// Sine table, 360 longints (DS:7FD4), scaled by 500.
static int32_t sin_tab(int16_t i)
{
    return *(int32_t *)ds_at((uint16_t)(0x7FD4 + i * 4));
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static int16_t cur_planet(void)
{
    return ds.Game_CurPlanet[ds.Game_CurSystem - 1];
}

static int16_t cur_moon(void)
{
    return ds.Game_CurMoon[ds.Game_CurSystem - 1];
}

static FarPtr far_add(FarPtr p, int32_t delta)
{
    return far_ptr((uint16_t)(p >> 16), (uint16_t)((uint16_t)p + delta));
}

// Host pointer into a heap block, for the blitters.
static uint8_t *buf_at(FarPtr p, int32_t delta)
{
    return (uint8_t *)far_host(p) + delta;
}

// The same, keeping the original's 16-bit offset wrap: a sprite at the top
// edge of the frame has a negative origin.
static uint8_t *buf_at_wrapped(FarPtr p, int32_t delta)
{
    return far_host(far_add(p, delta));
}

static uint8_t *screen_at(uint16_t off)
{
    return gfx_screen() + off;
}

static TBody *body_at(int16_t sys, int16_t idx)
{
    return (TBody *)far_host(ds.Galaxy_Bodies[sys - 1]) + (idx - 1);
}

static TBodyName *name_at(int16_t sys, int16_t idx)
{
    return (TBodyName *)far_host(ds.Galaxy_Names[sys - 1]) + (idx - 1);
}

// Galaxy::PlanetStatus[sys, planet] (DS:47CD + 8*sys + planet): -1 planet
// unknown, 0 known, 1 its moons explored. Ghidra prints this address as
// "BodyIcon[6] + 7 + sys*8 + planet", which is not a BodyIcon element.
static int8_t planet_status(int16_t sys, int16_t planet)
{
    return ds.Galaxy_PlanetStatus[sys - 1][planet - 1];
}

static TMoonList *moons_of(int16_t sys, int16_t planet)
{
    return (TMoonList *)far_host(ds.Galaxy_MoonLists[sys - 1]) + (planet - 1);
}

// Index in the body array of planet `planet`'s moon number `moon`.
static int16_t moon_body(int16_t sys, int16_t planet, int16_t moon)
{
    return moons_of(sys, planet)->Moons[moon - 1];
}

// Surface::Tileset[SurfaceType], then SurfaceColonisable of that.
static bool colonisable(const TBody *b)
{
    int16_t tileset = *(int16_t *)ds_at((uint16_t)(0x1A38 + (int8_t)b->SurfaceType * 2));
    return ds.Galaxy_SurfaceColonisable[tileset - 1] != 0;
}

// Races[owner].Status, read raw as owners 0 and 1 index below the array.
static int8_t race_status(uint8_t owner)
{
    return *(int8_t *)ds_at((uint16_t)(0x6A21 + owner * 0xE4));
}

// dst := a + b (the original's StrLoad, StrConcat, StrStore sequence).
static void str_cat2(PString *dst, uint16_t maxLen, const PString *a, const PString *b)
{
    PString tmp;
    System_StrLoad(&tmp, a);
    System_StrConcat(&tmp, b);
    System_StrStore(&tmp, dst, maxLen);
}

// dst := a + b + c
static void str_cat3(PString *dst, uint16_t maxLen, const PString *a, const PString *b, const PString *c)
{
    PString tmp;
    System_StrLoad(&tmp, a);
    System_StrConcat(&tmp, b);
    System_StrConcat(&tmp, c);
    System_StrStore(&tmp, dst, maxLen);
}

// dst := prefix + Str(n)
static void numbered_name(PString *dst, const PString *prefix, int32_t n)
{
    PString s;
    System_StrLong(n, 0, &s, 0xFF);
    str_cat2(dst, 0xFF, prefix, &s);
}

// dst := the C string s (at most 0x3C characters, as the StrStore calls here).
static void store_c(PString *dst, const char *s)
{
    PString t;
    t.len = (uint8_t)strlen(s);
    memcpy(t.s, s, t.len);
    System_StrStore(&t, dst, 0x3C);
}

// 32-bit multiply and signed divide as System's LongMul / LongDivMod.
static int32_t long_mul(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a * (uint32_t)b);
}

static int32_t long_div(int32_t a, int32_t b)
{
    if (b == 0)
        System_RunError(200);
    if (b == -1)
        return (int32_t)(0u - (uint32_t)a);
    return a / b;
}

// ---------------------------------------------------------------------------
// Hotspots
// ---------------------------------------------------------------------------
// 357b:0031
void PlanetInfo_SetupPlanetInfoHotspots(void)
{
    if (ds.PlanetInfo_AlienMode == 0) {
        ds.Mouse_HotspotCount = 3;
        Mouse_SetHotspot(1, 0, 0x31, 0x39, 0x2F, P("SEE OWNER INFO"), 1, 0);
        Mouse_SetHotspot(2, 0, 0x61, 0x39, 0x2F, P("SHIPS"), 1, 0);
        Mouse_SetHotspot(3, 0, 0x91, 0x60, 0x37, P("SEE SURFACE"), 1, 0);
    } else {
        ds.Mouse_HotspotCount = 1;
        Mouse_SetHotspot(1, 0, 0x31, 0x140, 0x97, P("SEE PLANET INFO"), 1, 0);
    }
}

// ---------------------------------------------------------------------------
// Planet info screen
// ---------------------------------------------------------------------------
typedef struct PStr40 {
    uint8_t len;
    char s[40];
} PStr40;
static_assert(sizeof(PStr40) == 41);

// The 0..69 attitude and resistance scales have ten values per step; other
// values keep the text unchanged.
static int scale_step(int8_t v)
{
    return v >= 0 && v < 70 ? v / 10 : -1;
}

// 357b:0359
void PlanetInfo_DrawPlanetInfoText(void)
{
    const TBody *b = far_host(ds.Game_CurBody);
    PString text, name, desc;
    PStr40 lines[9];

    uint8_t owner = b->Owner;
    // The original leaves the string undefined for owners above 14h; harmless.
    text.len = 0;
    if (owner == 0) {
        System_StrStore(PSTR("  "), &text, 0x3C);
    } else if (owner == 1) {
        System_StrStore(PSTR("YOUR "), &text, 0x3C);
    } else if ((int8_t)owner > 1 && (int8_t)owner < 0x15) {
        if (b->InfoLevel < 0x1E) {
            text.len = 0;
        } else if (b->InfoLevel < 0x28) {
            System_StrStore(PSTR("ALIEN "), &text, 0x3C);
        } else {
            // Races[owner].Name; owner can exceed the 12 races only in damaged data.
            str_cat2(&text, 0x3C, (const PString *)ds_at((uint16_t)(0x6A06 + owner * 0xE4)), PSTR(" "));
        }
    }
    if (cur_moon() == 0) {
        PlanetInfo_GetBodyName(&name, ds.Game_CurSystem, cur_planet(), 0);
        str_cat3(&text, 0x3C, &text, PSTR("PLANET'S NAME:  "), &name);
    } else {
        PlanetInfo_GetBodyFullName(&name, ds.Game_CurSystem, cur_planet(), cur_moon());
        str_cat3(&text, 0x3C, &text, PSTR("MOON'S NAME: "), &name);
    }
    Gfx_DrawTextF9(PS(&text), 0x5E, 0x35, 0x23);

    // Population and attitude.
    if (b->Population == 0)
        System_StrStore(PSTR("No"), &text, 0x3C);
    else
        System_StrLong(b->Population, 0, &text, 0x3C);
    if (b->Surveyed != 0 && colonisable(b) && b->InfoLevel > 0x1D
        && (b->Owner == 0 || (b->Owner == 1 && b->Colony == 0 && b->Miner == 0)))
        System_StrStore(PSTR("Empty planet, ready for colonysation"), &text, 0x3C);
    if ((int8_t)b->Owner > 1) {
        if (b->InfoLevel < 0x1E)
            System_StrStore(PSTR("No"), &text, 0x3C);
        if (b->InfoLevel > 0x1D && b->InfoLevel < 0x28)
            System_StrStore(PSTR("Unknown"), &text, 0x3C);
        if (b->InfoLevel > 0x27)
            str_cat2(&text, 0x3C, &text, PSTR(" alien"));
    }
    if (b->Owner == 1 && b->Population > 0) {
        static const char *const mood[7] = {" revolting men",
                                            " people hate you",
                                            " people don't like you",
                                            " neutral men",
                                            " people find you sympathetic",
                                            " people like you",
                                            " loyal people"};
        int step = scale_step(b->Attitude);
        if (step >= 0) {
            PString m;
            store_c(&m, mood[step]);
            str_cat2(&text, 0x3C, &text, &m);
        }
    }
    Gfx_DrawTextF9(PS(&text), 0x61, 0x45, 0x24);

    // Tech level.
    if (b->Owner == 1) {
        static const char *const tech[8] = {"None",   "Very low",       "Low",        "Normal",
                                            "High",   "Difficult",      "Very difficult", "Oppressing"};
        if (b->TechLevel < 8) {
            store_c(&text, tech[b->TechLevel]);
        }
    } else {
        System_StrStore(PSTR("No"), &text, 0x3C);
    }
    Gfx_DrawTextF9(PS(&text), 0x61, 0x55, 0x23);

    // Resistance. Below 10 it reads "No"; unmatched values keep the tech text.
    if (b->Owner == 1 || b->InfoLevel > 0x27) {
        static const char *const res[7] = {"No",       "Under developed", "Poor",      "Developing",
                                           "Enhanced", "High-tech",       "Super-tech"};
        int step = scale_step(b->Resistance);
        if (step >= 0) {
            store_c(&text, res[step]);
        }
    }
    if ((int8_t)b->Owner > 1 && b->InfoLevel > 0x1D && b->InfoLevel < 0x28)
        System_StrStore(PSTR("Unknown"), &text, 0x3C);
    if ((int8_t)b->Owner > 1 && b->InfoLevel < 0x1E)
        System_StrStore(PSTR("No"), &text, 0x3C);
    Gfx_DrawTextF9(PS(&text), 0x61, 0x65, 0x23);

    // Surface type and what the body is used for.
    if (b->InfoLevel < 6) {
        System_StrStore(PSTR("Unknown"), &text, 0x3C);
    } else {
        static const char *const surf[11] = {"No data", "Earth-like", "Gaseous", "Icy",
                                             "Watery",  "Tropical",   "Desert",  "Swampy",
                                             "Rocky",   "Too hot",    "No atmosphere"};
        if (b->SurfaceType < 11) {
            store_c(&text, surf[b->SurfaceType]);
        }
    }
    desc.len = 0;
    if (b->InfoLevel > 0x13) {
        if (b->Surveyed == 0 || !colonisable(b))
            System_StrStore(PSTR("not accomodable"), &desc, 0x3C);
        else
            System_StrStore(PSTR("life supporting"), &desc, 0x3C);
    }
    if (b->InfoLevel > 0x1D && b->InfoLevel < 0x28 && (int8_t)b->Owner > 1)
        System_StrStore(PSTR("enemy movement"), &desc, 0x3C);
    if (b->Surveyed != 0 && colonisable(b) && b->InfoLevel > 0x27) {
        if (b->Owner == 1) {
            if (b->Colony != 0)
                System_StrStore(PSTR("colony"), &desc, 0x3C);
            if (b->Colony == 0 && b->Miner > 0)
                System_StrStore(PSTR("under colonization"), &desc, 0x3C);
        } else if ((int8_t)b->Owner > 1 && (int8_t)b->Owner < 0x15 && b->Colony != 0) {
            System_StrStore(PSTR("Alien colony"), &desc, 0x3C);
        }
    }
    if (desc.len != 0)
        str_cat3(&text, 0x3C, &text, PSTR(", "), &desc);
    Gfx_DrawTextF9(PS(&text), 0x61, 0x75, 0x24);

    // Ore deposits.
    for (int16_t k = 1; k <= 6; k++) {
        int16_t y = (int16_t)((k - 1) * 8 + 0x95);
        if (b->InfoLevel < 10)
            Gfx_DrawTextF9(P("???"), 0xA0, y, 3);
        else if (b->Deposits[k - 1] < 10 || b->_03 == 0)
            Gfx_DrawTextF9(P("NO "), 0xA0, y, 3);
        else
            Gfx_DrawTextF9(P("YES"), 0xA0, y, 3);
    }

    System_StrLong(b->Diameter, 6, &text, 0x3C);
    if (b->InfoLevel < 3)
        System_StrStore(PSTR("??????"), &text, 0x3C);
    Gfx_DrawTextF9(PS(&text), 0x7C, 0x85, 6);
    System_StrLong(b->Temperature, 4, &text, 0x3C);
    if (b->InfoLevel < 5)
        System_StrStore(PSTR("????"), &text, 0x3C);
    Gfx_DrawTextF9(PS(&text), 0x110, 0x85, 4);

    // The list of things on the body, up to eight lines.
    int16_t n = 0;
#define LINE(lit) System_StrStore(PSTR(lit), (PString *)&lines[n], 0x28)
    if (b->Owner == 0 && (int8_t)b->Satellite > 0) {
        n = 1;
        if (b->Satellite == 1)
            LINE("Satellite");
        else
            LINE("Spy satellite");
    }
    if (b->Owner == 1) {
        if (b->Colony != 0) {
            n++;
            LINE("Colony");
        }
        if (Surface_CountBuildings(0x14, ds.Game_CurSystem, cur_planet(), cur_moon(), true) > 0) {
            n++;
            LINE("Energy Shield");
        }
        if (ds.Game_CurSystem == 1 && ds.Game_CurPlanet[0] == 5 && ds.Game_CurMoon[0] == 0
            && ds.Research_Products[0x17].State == 5 && ds.Research_Products[0x17].Stock > 0) {
            n++;
            System_StrLong(ds.Research_Products[0x17].Stock, 0, &text, 0x3C);
            str_cat2((PString *)&lines[n], 0x28, &text, PSTR(" Space Station"));
        }
        System_StrLong(b->_0A, 0, &desc, 0x3C);
        if (b->_0A != 0) {
            n++;
            if (b->Colony == 0)
                LINE("Miner Station");
            else
                str_cat2((PString *)&lines[n], 0x28, &desc, PSTR(" Miner Droid"));
        }
        if (b->_0B != 0 && b->Colony != 0) {
            System_StrLong(b->_0B, 0, &text, 0x3C);
            n++;
            str_cat2((PString *)&lines[n], 0x28, &text, PSTR(" Solar Sat"));
        }
        if ((int8_t)b->Satellite > 0) {
            n++;
            if (b->Satellite == 1)
                LINE("Satellite");
            else
                LINE("Spy Satellite");
        }
    }
    if ((int8_t)b->Owner > 1) {
        if (b->InfoLevel > 0x1E && b->Colony != 0) {
            n++;
            LINE("Colony");
        }
        System_StrLong(b->_0A, 0, &desc, 0x3C);
        if (b->InfoLevel > 0x28 && b->_0A != 0 && b->Colony == 0) {
            n++;
            LINE("Miner Station");
        }
        if (b->InfoLevel > 0x28
            && Surface_CountBuildings(0x14, ds.Game_CurSystem, cur_planet(), cur_moon(), true) > 0) {
            n++;
            LINE("Energy Shield");
        }
        if ((int8_t)b->Satellite > 0) {
            n++;
            if (b->Satellite == 1)
                LINE("Satellite");
            else
                LINE("Spy Satellite");
        }
        if (b->_09 != 0) {
            n++;
            LINE("Spy Ship");
        }
    }
#undef LINE
    for (int16_t k = 1; k <= n; k++)
        Gfx_DrawTextF9(PS(&lines[k]), 0xC2, (int16_t)((k - 1) * 8 + 0x95), 0x14);
}

// 357b:10d9
void PlanetInfo_DrawPlanetInfoScreen(void)
{
    const TBody *b = far_host(ds.Game_CurBody);
    FarPtr buf = System_GetMem(0xBCC0);
    PString name, tmp;
    str_cat2(&name, 0xFF, PSTR("grafika\\"), (const PString *)&ds.Ui_Screens[ds.Game_CurrentScreen - 1].Name);
    Gfx_LoadPicScreen(PS(&name), far_host(buf), 0x97, Gfx_PalettePtr());
    Blit_CopyRect(far_host(buf), screen_at(0x3D40), 0x140, 0x97);
    int16_t n = b->InfoLevel < 6 ? 0 : (int8_t)b->SurfaceType;
    PlanetInfo_LoadPlanetPlanetPic(n, false);
    if ((int8_t)b->Owner < 2)
        n = (int8_t)b->Owner;
    else if (b->InfoLevel < 0x28)
        n = b->InfoLevel < 0x1E ? 0 : 0xD;
    else
        n = (int8_t)b->Owner;
    PlanetInfo_LoadPlanetFajPic(n, false);
    System_FreeMem(buf, 0xBCC0);

    // The sprite sheet is only needed here; the global pointer is reused as in
    // the original (InitSystemView allocates it again).
    SV_SPRITES = System_GetMem(0x5500);
    numbered_name(&tmp, PSTR("planets\\napr"), ds.Game_CurSystem);
    Gfx_LoadPic(PS(&tmp), far_host(SV_SPRITES), 1, 0x44, 0, 0, 0x140, false, Gfx_PalettePtr(), 0, 0x6F, 0x80, 0, 0xBE);
    Mouse_HideCursor();
    if (cur_moon() == 0) {
        Blit_BlitTransparent(buf_at(SV_SPRITES, (cur_planet() - 1) * 0x20 + 0x180), screen_at(0x834D), 0x20,
                             0x20, 0x80);
    } else {
        int16_t m = (int16_t)(moon_body(ds.Game_CurSystem, cur_planet(), cur_moon())
                              - ds.Galaxy_PlanetCount[ds.Game_CurSystem - 1]);
        Blit_BlitTransparent(buf_at(SV_SPRITES, ((m - 1) / 0xE) * 0x1540 + ((m - 1) % 0xE) * 0x11 + 0x2AC2),
                             screen_at(0x8D55), 0x10, 0x10, 0x80);
    }
    System_FreeMem(SV_SPRITES, 0x5500);
    Mouse_ShowCursor();
    PlanetInfo_DrawPlanetInfoText();
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
}

// 357b:134b
void PlanetInfo_LoadPlanetFajPic(int16_t n, bool setPalette)
{
    if (ds.PlanetInfo_AlienMode != 0)
        return;
    FarPtr buf = System_GetMem(0xAA6);
    PString tmp;
    numbered_name(&tmp, PSTR("planets\\faj"), n);
    Gfx_LoadPic(PS(&tmp), far_host(buf), 1, 0x2F, 0, 0, 0x3A, false, Gfx_PalettePtr(), 0, 0xF, 0x70, 0, 0xBE);
    Mouse_HideCursor();
    Blit_CopyRectStrided(buf_at(buf, 0x76), screen_at(0x3FC2), 0x35, 0x2B, 0x3A, 0x140);
    Mouse_ShowCursor();
    System_FreeMem(buf, 0xAA6);
    if (setPalette)
        RawText_SetPaletteRange(0x70, 0x7F, Gfx_PalettePtr());
}

// 357b:1429
void PlanetInfo_LoadPlanetPlanetPic(int16_t n, bool setPalette)
{
    if (ds.PlanetInfo_AlienMode != 0)
        return;
    FarPtr buf = System_GetMem(0x14A0);
    PString tmp;
    numbered_name(&tmp, PSTR("planets\\planet"), n);
    Gfx_LoadPic(PS(&tmp), far_host(buf), 1, 0x37, 0, 0, 0x60, false, Gfx_PalettePtr(), 0, 0x1F, 0x50, 0, 0xBE);
    Mouse_HideCursor();
    Blit_CopyRectStrided(buf_at(buf, 0xC2), screen_at(0xB7C2), 0x5C, 0x33, 0x60, 0x140);
    Mouse_ShowCursor();
    System_FreeMem(buf, 0x14A0);
    if (setPalette)
        RawText_SetPaletteRange(0x50, 0x6F, Gfx_PalettePtr());
}

// 357b:1515
void PlanetInfo_DrawAlienInfoScreen(void)
{
    const TBody *b = far_host(ds.Game_CurBody);
    FarPtr buf = System_GetMem(0xBCC0);
    PString tmp;
    Gfx_LoadPicScreen(P("grafika\\aliennfo"), far_host(buf), 0x97, Gfx_PalettePtr());
    Blit_CopyRect(far_host(buf), screen_at(0x3D40), 0x140, 0x97);
    numbered_name(&tmp, PSTR("alien\\alien"), (int8_t)b->Owner);
    Gfx_LoadPic(PS(&tmp), far_host(buf), 1, 0x97, 0, 0, 0x62, false, Gfx_PalettePtr(), 0, 0x8F, 0x60, 0, 0xBE);
    Mouse_HideCursor();
    Blit_CopyRectStrided(buf_at(buf, 0x63), screen_at(0x4382), 0x5E, 0x8F, 0x62, 0x140);
    Mouse_ShowCursor();
    System_FreeMem(buf, 0xBCC0);
    // The race's text: 12 strings of 36 bytes, 1B0h bytes per race.
    const uint8_t *text = (const uint8_t *)far_host(ds.RawText_SzFaj) + ((int8_t)b->Owner - 1) * 0x1B0;
    Gfx_DrawTextFC(text, 0x69, 0x36, 0x23);
    for (int16_t k = 2; k <= 12; k++)
        Gfx_DrawTextF9(text + (k - 1) * 0x24, 0x69, (int16_t)(k * 9 + 0x36), 0x23);
    PlanetInfo_DrawStoresList();
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
}

// 357b:1671
void PlanetInfo_DrawStoresList(void)
{
    const TBody *b = far_host(ds.Game_CurBody);
    if (b->_09 != 0 || ((int8_t)b->Owner > 1 && *(uint8_t *)ds_at((uint16_t)(0x6517 + b->Owner)) == 2)) {
        // BUG? Bytes 13h..1Ah of the race record (typed as KnownSystems) are
        // read as "has this resource / product" flags.
        const uint8_t *race = (const uint8_t *)ds_at((uint16_t)(0x6A06 + (int8_t)b->Owner * 0xE4));
        PString s;
        for (int16_t k = 1; k <= 4; k++) {
            int16_t y = (int16_t)((k - 1) * 9 + 0x9C);
            if (race[k + 0x12] != 0) {
                System_StrLong(b->Stores[k - 1], 0, &s, 0xFF);
                Gfx_DrawTextF9(PS(&ds.Research_Products[ds.Research_ShipProduct[k - 1] - 1].Name), 0x68, y, 0x10);
                Gfx_DrawTextF9(P(":"), 0xAA, y, 1);
                Gfx_DrawTextFC(PS(&s), 0xB0, y, 3);
            }
        }
        for (int16_t k = 1; k <= 4; k++) {
            int16_t y = (int16_t)((k - 1) * 9 + 0x9C);
            if (race[k + 0x16] != 0) {
                System_StrLong(b->Stores[k + 3], 0, &s, 0xFF);
                Gfx_DrawTextF9(PS(&ds.Research_Products[ds.Research_GroundProduct[k - 1] - 1].Name), 0xC5, y,
                               0x10);
                Gfx_DrawTextF9(P(":"), 0x125, y, 1);
                Gfx_DrawTextFC(PS(&s), 299, y, 3);
            }
        }
    }
}

// 357b:1825
void PlanetInfo_DrawPlanetSurfaceBackdrop(void)
{
    const TBody *b = far_host(ds.Game_CurBody);
    FarPtr buf = System_GetMem(0xBCC0);
    PString tmp;
    numbered_name(&tmp, PSTR("planets\\nagy"), (int8_t)b->SurfaceType);
    Gfx_LoadPicScreen(PS(&tmp), far_host(buf), 0x97, Gfx_PalettePtr());
    Blit_CopyRect(far_host(buf), screen_at(0x3D40), 0x140, 0x97);
    System_FreeMem(buf, 0xBCC0);
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
}

// ---------------------------------------------------------------------------
// Current body and names
// ---------------------------------------------------------------------------
// 357b:3253
void PlanetInfo_SelectCurBody(void)
{
    int16_t sys = ds.Game_CurSystem;
    int16_t idx = cur_moon() == 0 ? cur_planet() : moon_body(sys, cur_planet(), cur_moon());
    ds.Game_CurBody = far_add(ds.Galaxy_Bodies[sys - 1], (idx - 1) * (int32_t)sizeof(TBody));
    const TBody *b = far_host(ds.Game_CurBody);
    // Only the low byte of the word is written.
    *(uint8_t *)&ds.Game_CurBodyIsColony = (b->Colony == 0 || b->Owner != 1) ? 0 : 1;
}

// 357b:32f1
bool PlanetInfo_IsCurrentBody(int16_t sys, int16_t planet, int16_t moon)
{
    return ds.Game_CurSystem == sys && cur_planet() == planet && cur_moon() == moon;
}

static const PString *planet_name(int16_t sys, int16_t planet)
{
    return (const PString *)&name_at(sys, planet)->Name;
}

static const PString *moon_name(int16_t sys, int16_t planet, int16_t moon)
{
    return (const PString *)&name_at(sys, moon_body(sys, planet, moon))->Name;
}

// 357b:332c
void PlanetInfo_GetBodyName(PString *result, int16_t sys, int16_t planet, int16_t moon)
{
    PString local;
    System_StrStore(moon == 0 ? planet_name(sys, planet) : moon_name(sys, planet, moon), &local, 0xFF);
    System_StrStore(&local, result, 0xFF);
}

// 357b:33b3
void PlanetInfo_GetBodyFullName(PString *result, int16_t sys, int16_t planet, int16_t moon)
{
    PString local;
    System_StrStore(planet_name(sys, planet), &local, 0xFF);
    if (moon > 0)
        str_cat3(&local, 0xFF, &local, PSTR(" "), moon_name(sys, planet, moon));
    System_StrStore(&local, result, 0xFF);
}

// 357b:346d
void PlanetInfo_GetBodyLongName(PString *result, int16_t sys, int16_t planet, int16_t moon)
{
    PString local;
    str_cat2(&local, 0xFF, (const PString *)&ds.Names_Systems[sys - 1], PSTR(" system,planet "));
    str_cat2(&local, 0xFF, &local, planet_name(sys, planet));
    if (moon > 0)
        str_cat3(&local, 0xFF, &local, PSTR(",moon "), moon_name(sys, planet, moon));
    System_StrStore(&local, result, 0xFF);
}

// 357b:355e
void PlanetInfo_GetBodyLocationName(PString *result, int16_t sys, int16_t planet, int16_t moon)
{
    PString local;
    str_cat2(&local, 0xFF, (const PString *)&ds.Names_Systems[sys - 1], PSTR(" system "));
    str_cat2(&local, 0xFF, &local, planet_name(sys, planet));
    if (moon > 0)
        str_cat3(&local, 0xFF, &local, PSTR(" "), moon_name(sys, planet, moon));
    System_StrStore(&local, result, 0xFF);
}

// 357b:3d77
void PlanetInfo_RandomizeProtectPage(void)
{
    if (ds.Protect_Key2 == 0x1F5F)
        *(uint16_t *)&ds.Protect_PageIndex = (uint16_t)(System_Random(20) + 1);
}

// ---------------------------------------------------------------------------
// Building power balancing
// ---------------------------------------------------------------------------
// The original's BalanceBuildingPower keeps these in its stack frame, where
// the nested procedures reach them.
typedef struct PowerCtx {
    int16_t sys, planet, moon;
    TBody *body;
    int32_t pop;      // the body's population (at least 1 once demand is summed)
    int32_t demand;   // total of the powered buildings' Workers, at least pop
    int32_t supply;   // weighted production of the powered buildings (+ solar satellites)
    int32_t energy;   // weighted Energy of the powered buildings, at least supply
    int32_t shields;  // output total of kind 16h, goes to body bytes 4..5
} PowerCtx;

static TBuilding *building_at(int16_t i)
{
    return (TBuilding *)far_host(ds.Game_Buildings) + (i - 1);
}

// TBuildingType of a building kind; kind 0 is not a real type but the
// original indexes it all the same (Kind * 3Fh + 6553h).
static const TBuildingType *kind_type(uint8_t kind)
{
    return (const TBuildingType *)ds_at((uint16_t)(0x6592 - 0x3F + kind * 0x3F));
}

static bool on_body(const PowerCtx *c, const TBuilding *b)
{
    return b->System == c->sys && b->Planet == c->planet && b->Moon == c->moon && b->Health == 0;
}

// 357b:3644: every intact building is switched on, and the planet force's
// Storage (its energy supply) gets the kind-specific total.
static void power_all_buildings(PowerCtx *c)
{
    int16_t force = Groups_FindPlanetForce(c->sys, c->planet, c->moon);
    int16_t *supply = &Groups_At(1, force)->Storage;
    *supply = 0;
    for (int16_t i = 1; i <= ds.Game_BuildingCount; i++) {
        TBuilding *b = building_at(i);
        if (!on_body(c, b))
            continue;
        b->Flag = 1;
        if (b->Kind == 0x0B)
            *supply = (int16_t)(*supply + 0x14);
        if (b->Kind == 0x04)
            *supply = (int16_t)(*supply + 10);
        if (b->Kind == 0x05)
            *supply = (int16_t)(*supply + 5);
        if (b->Kind == 0x19)
            *supply = (int16_t)(*supply + 10);
    }
}

// 357b:3746
static void sum_energy_demand(PowerCtx *c)
{
    c->demand = 0;
    for (int16_t i = 1; i <= ds.Game_BuildingCount; i++) {
        const TBuilding *b = building_at(i);
        if (on_body(c, b) && b->Flag != 0)
            c->demand = (int32_t)((uint32_t)c->demand + (uint32_t)(int32_t)kind_type(b->Kind)->Workers);
    }
    if (c->pop == 0)
        c->pop = 1;
    if (c->demand < c->pop)
        c->demand = c->pop;
}

// 357b:3846
static void sum_outputs(PowerCtx *c)
{
    c->supply = 0;
    c->energy = 0;
    for (int16_t i = 1; i <= ds.Game_BuildingCount; i++) {
        const TBuilding *b = building_at(i);
        const TBuildingType *t = kind_type(b->Kind);
        if (!on_body(c, b) || b->Flag == 0)
            continue;
        if (t->Category == 1 || t->Category == 8) {
            int32_t share = long_div(long_mul(t->Production, c->pop), c->demand);
            share = long_div(long_mul(b->Output, share), 100);
            c->supply = (int32_t)((uint32_t)c->supply + (uint32_t)share);
        }
        int32_t e = long_div(long_mul(t->Energy, c->pop), c->demand);
        c->energy = (int32_t)((uint32_t)c->energy + (uint32_t)e);
    }
    c->supply = (int32_t)((uint32_t)c->supply + (uint32_t)long_mul(c->body->_0B, 10000));
    if (c->supply == 0)
        c->supply = 1;
    if (c->energy < c->supply)
        c->energy = c->supply;
}

// 357b:3a05: switches off the powered building with the lowest priority
// (type byte 2Bh; the last one wins a tie).
static void unpower_building(PowerCtx *c)
{
    uint8_t best = 0;
    int16_t which = 0;
    for (int16_t i = 1; i <= ds.Game_BuildingCount; i++) {
        const TBuilding *b = building_at(i);
        uint8_t prio = kind_type(b->Kind)->_2B;
        if (on_body(c, b) && b->Flag != 0 && (which == 0 || prio <= best)) {
            best = prio;
            which = i;
        }
    }
    if (which > 0)
        building_at(which)->Flag = 0;
}

// 357b:3ae5
static void apply_building_efficiency(PowerCtx *c)
{
    c->shields = 0;
    for (int16_t i = 1; i <= ds.Game_BuildingCount; i++) {
        TBuilding *b = building_at(i);
        const TBuildingType *t = kind_type(b->Kind);
        if (!on_body(c, b) || b->Flag == 0)
            continue;
        b->W9 = (int16_t)long_div(long_mul(t->Workers, c->pop), c->demand);
        b->WB = (int16_t)long_div(long_mul(t->Energy, c->supply), c->energy);
        if (t->Category == 1 || t->Category == 8) {
            b->BD = (uint8_t)long_div(long_mul(c->pop, 100), c->demand);
        } else {
            int32_t ratio = long_div(long_mul(c->supply, 100), c->energy);
            b->BD = (uint8_t)long_div(long_mul(ratio, c->pop), c->demand);
        }
        if (b->Kind == 0x16) {
            // 16-bit arithmetic throughout.
            int16_t a = (int16_t)((int16_t)(b->Output * t->Production) / 100);
            int16_t v = (int16_t)((int16_t)(a * b->BD) / 100);
            c->shields += v;
        }
    }
}

// 357b:3cc0
void PlanetInfo_BalanceBuildingPower(int16_t sys, int16_t planet, int16_t moon)
{
    PowerCtx c = {.sys = sys, .planet = planet, .moon = moon};
    c.body = body_at(sys, moon == 0 ? planet : moon_body(sys, planet, moon));
    c.pop = c.body->Population;
    power_all_buildings(&c);
    sum_energy_demand(&c);
    sum_outputs(&c);
    while (long_mul(c.supply, 2) < c.energy) {
        unpower_building(&c);
        sum_energy_demand(&c);
        sum_outputs(&c);
    }
    apply_building_efficiency(&c);
    *(uint16_t *)c.body->_04 = (uint16_t)c.shields;
}

// ---------------------------------------------------------------------------
// System view
// ---------------------------------------------------------------------------
// The marker/flag byte of a body in its orbit record: 1 = something known
// there, owner + 1 = an alien owner is known, 2 = the player's.
static uint8_t body_flag(const TBody *b)
{
    uint8_t flag = 0;
    if (b->Owner > 1 && b->InfoLevel > 0x1D)
        flag = 1;
    if (b->Owner > 1 && b->InfoLevel > 0x27)
        flag = (uint8_t)(b->Owner + 1);
    if (b->Owner == 1)
        flag = 2;
    return flag;
}

// Sets SV_HAS_BASE when the body has something the player can see.
static void note_base(const TBody *b)
{
    if (b->Owner > 1 && b->Colony != 0 && race_status(b->Owner) == 6)
        SV_HAS_BASE = 1;
    if (b->Owner == 1 && b->Colony != 0)
        SV_HAS_BASE = 1;
    if (b->Owner == 1 && b->_0A != 0)
        SV_HAS_BASE = 1;
    if (b->_09 != 0)
        SV_HAS_BASE = 1;
}

static void body_hotspot(int16_t j, const TBodyName *nm, int16_t size, int16_t sys, int16_t planet,
                         int16_t moon)
{
    THotspot *h = Mouse_Hotspot(j);
    PString name;
    h->X = nm->X;
    h->Y = (uint8_t)(nm->Y + 0x31);
    h->W = size;
    h->H = (uint8_t)size;
    PlanetInfo_GetBodyName(&name, sys, planet, moon);
    System_StrStore(&name, (PString *)&h->Caption, 0x12);
    h->Repeat_ = 0;
    h->_1A = 5;
}

// 357b:18cc
void PlanetInfo_SetupSystemViewHotspots(void)
{
    int16_t sys = ds.Game_CurSystem;
    SV_HIDDEN = (sys == 4 && *(uint8_t *)ds_at(0x5D90) != 0) ? 1 : 0;

    // Hotspots 1..8: the system list down the right side, once product 14 or
    // 15 is developed and the system is known.
    for (int16_t k = 1; k <= 8; k++) {
        THotspot *h = Mouse_Hotspot(k);
        if (cur_planet() == 0 && ds.Galaxy_SystemStatus[k - 1] >= 0
            && (ds.Research_Products[0xD].State == 5 || ds.Research_Products[0xE].State == 5)) {
            h->X = 0x100;
            h->Y = (uint8_t)((k - 1) * 0x13 + 0x31);
            h->W = 0x40;
            h->H = 0x13;
            System_StrStore((const PString *)&ds.Names_Systems[k - 1], (PString *)&h->Caption, 0x12);
            h->Repeat_ = 1;
            h->_1A = 0;
        } else {
            h->X = 1000;
            h->Y = 0xDE;
            h->W = 0;
            h->H = 0;
            h->Caption.len = 0;
            h->Repeat_ = 1;
            h->_1A = 1;
        }
    }

    if (cur_planet() == 0) {
        // The whole system: a hotspot per planet.
        Mouse_ClearHotspots(9, 0x13);
        if (ds.Galaxy_SystemStatus[sys - 1] < 1)
            ds.Mouse_HotspotCount = 0x13;
        else
            ds.Mouse_HotspotCount = (int16_t)(ds.Galaxy_PlanetCount[sys - 1] + 0x13);
        if (SV_HIDDEN != 0)
            ds.Mouse_HotspotCount = 0x12;
        int16_t planets = ds.Galaxy_PlanetCount[sys - 1];
        for (int16_t k = 1; k <= planets; k++)
            body_hotspot((int16_t)(k + 0x13), name_at(sys, k), 0x20, sys, k, 0);
        for (int16_t k = 1; k <= 8; k++) {
            if (planet_status(sys, k) < 0)
                Mouse_ClearHotspots((int16_t)(k + 0x13), (int16_t)(k + 0x13));
        }
        if (ds.Game_CurrentScreen == 0x12 && ds.Galaxy_SystemStatus[sys - 1] == 0) {
            // The sun, when the system is known but not open.
            THotspot *h = Mouse_Hotspot(0x13);
            h->X = 0x80;
            h->Y = 0x5C;
            h->W = 0x40;
            h->H = 0x40;
            System_StrStore((const PString *)&ds.Names_Systems[sys - 1], (PString *)&h->Caption, 0x12);
            h->Repeat_ = 0;
            h->_1A = 5;
            // BUG? k is 8 here, so this writes record 8's flag; and the body
            // is Bodies[sys][CurPlanet - 1] = Bodies[sys][-1] as CurPlanet is
            // 0 in this branch.
            const TBody *b = body_at(sys, cur_planet());
            orbit(8)->flag = 0;
            if (b->Owner > 1 && b->InfoLevel > 0x1D)
                orbit(8)->flag = 1;
            if (b->Owner > 1 && b->InfoLevel > 0x27)
                orbit(8)->flag = (uint8_t)(b->Owner + 1);
            if (b->Owner == 1)
                orbit(8)->flag = 2;
        }
        return;
    }

    // One planet and its moons.
    int16_t planet = cur_planet();
    SV_HAS_BASE = 0;
    THotspot *h = Mouse_Hotspot(0x13);
    PString name;
    h->X = 0x90;
    h->Y = 0x6C;
    h->W = 0x20;
    h->H = 0x20;
    PlanetInfo_GetBodyName(&name, sys, planet, 0);
    System_StrStore(&name, (PString *)&h->Caption, 0x12);
    h->Repeat_ = 0;
    h->_1A = 5;
    const TBody *pb = body_at(sys, planet);
    note_base(pb);
    orbit(0)->flag = body_flag(pb);

    int16_t moons = moons_of(sys, planet)->Count;
    for (int16_t k = 1; moons != 0 && k <= moons; k++) {
        int16_t bodyIdx = moon_body(sys, planet, k);
        body_hotspot((int16_t)(k + 0x13), name_at(sys, bodyIdx), 0x10, sys, planet, k);
        const TBody *mb = body_at(sys, bodyIdx);
        note_base(mb);
        orbit(k)->flag = body_flag(mb);
    }
    if (planet_status(sys, planet) < 1)
        ds.Mouse_HotspotCount = 0x13;
    else
        ds.Mouse_HotspotCount = (int16_t)(moons_of(sys, planet)->Count + 0x13);
    Mouse_ClearHotspots(1, 0x12);
    if (SV_HIDDEN != 0)
        ds.Mouse_HotspotCount = 0x12;

    // Remember which fleet was selected, to find it again after the rebuild.
    GroupTab saved = {0, 0, 0};
    if (SV_SELECTED > 0)
        saved = *group_tab(SV_SELECTED);

    // Hotspots 1..18: the player's fleets here, then the aliens' forces.
    int16_t j = 1;
    for (int16_t g = 1; g <= ds.Game_GroupCount; g++) {
        const TGroup *grp = Groups_At(0, g);
        if (grp->System == ds.Game_CurSystem && grp->Planet == cur_planet()
            && (grp->Status == 1 || grp->Status == 2 || grp->Status == 3) && j < 0x13) {
            Mouse_SetHotspot(j, (int16_t)(((j - 1) % 2) * 0x20 + 0x101), (uint8_t)(((j - 1) / 2) * 0x10 + 0x32),
                             0x20, 0x10, PS(&grp->Name), 0, 0);
            GroupTab *t = group_tab(j);
            t->kind = grp->Kind;
            t->idx = g;
            t->moon = grp->Moon;
            j++;
        }
    }
    SV_HAS_GROUPS = j > 1;
    for (int16_t r = 2; r <= 12; r++) {
        const TRace *race = &ds.Races_Races[r - 2];
        if (race->Status == 6 && (int8_t)race->ForceCount > 0) {
            for (int16_t f = 1; f <= (int8_t)race->ForceCount; f++) {
                const TRaceForce *rf = &race->Forces[f - 1];
                if (rf->System == ds.Game_CurSystem && rf->Body == cur_planet() && rf->Active == 1)
                    SV_HAS_GROUPS = 1;
            }
        }
    }
    if (SV_HAS_BASE != 0 || SV_HAS_GROUPS != 0) {
        for (int16_t r = 2; r <= 12; r++) {
            const TRace *race = &ds.Races_Races[r - 2];
            // Each race starts at a new row (an odd slot).
            if (!(j & 1))
                group_tab(j++)->kind = 0;
            // BUG? j can reach 19, and entry 19 of the table is SV_SELECTED.
            int16_t nf = (int8_t)race->ForceCount;
            for (int16_t f = 1; nf > 0 && f <= nf; f++) {
                const TRaceForce *rf = &race->Forces[f - 1];
                if (rf->System == ds.Game_CurSystem && rf->Body == cur_planet() && rf->Active == 1
                    && rf->Mission > 1 && j < 0x13) {
                    PString num, fleet;
                    System_StrLong(f, 0, &num, 0xFF);
                    str_cat3(&fleet, 0xFF, (const PString *)&ds.Names_Races[r - 1], PSTR(" Fleet "), &num);
                    Mouse_SetHotspot(j, (int16_t)(((j - 1) % 2) * 0x20 + 0x101),
                                     (uint8_t)(((j - 1) / 2) * 0x10 + 0x32), 0x20, 0x10, PS(&fleet), 0, 0);
                    GroupTab *t = group_tab(j);
                    t->kind = r + 3;
                    t->idx = f;
                    t->moon = rf->Moon;
                    j++;
                }
            }
        }
    }
    if (j < 0x13) {
        for (int16_t i = j; i <= 0x12; i++)
            group_tab(i)->kind = 0;
    }
    if (SV_SELECTED > 0) {
        SV_SELECTED = 0;
        for (int16_t i = 1; i <= 0x12; i++) {
            if (saved.kind == group_tab(i)->kind && saved.idx == group_tab(i)->idx)
                SV_SELECTED = i;
        }
    }
}

// 357b:23db: loads the three pictures of the system view and sets the orbit
// record of every body.
void PlanetInfo_LoadSystemViewAssets(void)
{
    int16_t sys = ds.Game_CurSystem;
    PString s, tmp;
    uint8_t *pal = Gfx_PalettePtr();
    Gfx_DrawStatusText(P(""));

    System_StrStore(cur_planet() == 0 ? PSTR("hatter1") : PSTR("hatter2"), &s, 0xFF);
    str_cat2(&tmp, 0xFF, PSTR("planets\\"), &s);
    Gfx_LoadPic(PS(&tmp), far_host(SV_BACKDROP), 1, 0x97, 0, 0, 0x140, true, pal, 0,
                0xB0, 0x40, 0, 0xBE);
    System_StrStore(cur_planet() == 0 ? PSTR("hatter3") : PSTR("hatter4"), &s, 0xFF);
    str_cat2(&tmp, 0xFF, PSTR("planets\\"), &s);
    Gfx_LoadPic(PS(&tmp), far_host(SV_ICONS), 1, 0x4B, 0, 0, 0x140, false, pal, 0,
                0xB0, 0x40, 0, 0xBE);
    numbered_name(&tmp, PSTR("planets\\napr"), sys);
    Gfx_LoadPic(PS(&tmp), far_host(SV_SPRITES), 1, 0x44, 0, 0, 0x140, false, pal, 0,
                0x6F, 0x80, 0, 0xBE);

    if (cur_planet() == 0) {
        for (int16_t k = 1; k <= ds.Galaxy_PlanetCount[sys - 1]; k++) {
            const TBodyName *nm = name_at(sys, k);
            OrbitRec *o = orbit(k);
            o->rx = nm->X;
            o->ry = nm->Y;
            o->ang = (int16_t)System_Random(0xE10);
            o->speed = nm->Size;
            o->icon = ds.Galaxy_BodyIcon[sys - 1][k - 1];
        }
    } else {
        int16_t moons = moons_of(sys, cur_planet())->Count;
        for (int16_t k = 1; k <= moons; k++) {
            const TBodyName *nm = name_at(sys, moon_body(sys, cur_planet(), k));
            OrbitRec *o = orbit(k);
            o->rx = nm->X;
            o->ry = nm->Y;
            o->ang = (int16_t)System_Random(0xE10);
            o->speed = nm->Size;
        }
    }
    SV_BLINK = 0;
}

// 357b:235a
void PlanetInfo_InitSystemView(void)
{
    SV_BACKDROP = System_GetMem(0xBCC0);
    SV_ICONS = System_GetMem(24000);
    SV_SPRITES = System_GetMem(0x6900);
    PlanetInfo_LoadSystemViewAssets();
    PlanetInfo_UpdateOrbitPositions();
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
}

// 357b:26ee
void PlanetInfo_ReloadSystemView(void)
{
    PlanetInfo_LoadSystemViewAssets();
    PlanetInfo_UpdateOrbitPositions();
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
}

// Offset into the 320-wide frame of the top left corner of body record o.
static int32_t sprite_origin(const OrbitRec *o)
{
    return (int16_t)((int16_t)(o->y - o->half) * 0x140) + (int16_t)(o->x - o->half);
}

// 357b:270b
void PlanetInfo_DrawOrbitingPlanet(int16_t k)
{
    int16_t sys = ds.Game_CurSystem;
    if (ds.Galaxy_SystemStatus[sys - 1] > 0 && planet_status(sys, k) >= 0) {
        // (The original also computes (ang / 71h + 19h) mod 20h into a local
        // here and never uses it.)
        const OrbitRec *o = orbit(k);
        Scale_ScaleSprite(buf_at(SV_SPRITES, (k - 1) * 0x20 + 0x180), far_host(SV_PLANET), o->size);
        Blit_BlitTransparent(far_host(SV_PLANET), buf_at_wrapped(SV_FRAME, sprite_origin(o)), o->size,
                             o->size, 0x80);
    }
}

// 357b:27e9
void PlanetInfo_DrawOrbitingMoon(int16_t k)
{
    int16_t sys = ds.Game_CurSystem;
    if (planet_status(sys, cur_planet()) > 0) {
        const OrbitRec *o = orbit(k);
        int16_t m = (int16_t)(moon_body(sys, cur_planet(), k) - ds.Galaxy_PlanetCount[sys - 1]);
        Scale_ScaleSprite(buf_at(SV_SPRITES, ((m - 1) % 0xE) * 0x11 + 0x2AC2 + ((m - 1) / 0xE) * 0x1540),
                          far_host(SV_MOON), (int16_t)(o->size << 1));
        int32_t origin = sprite_origin(o);
        Blit_BlitTransparent(far_host(SV_MOON), buf_at_wrapped(SV_FRAME, origin), o->size, o->size,
                             0x80);
        if (o->flag != 0) {
            Blit_BlitTransparent(buf_at(SV_ICONS, 0x2A81 + (o->flag - 1) * 8),
                                 buf_at_wrapped(SV_FRAME,
                                                (int16_t)((int16_t)(o->y - o->half - 3) * 0x140)
                                                    + (int16_t)(o->x - o->half) - 3),
                                 7, 7, 0x40);
        }
        if (k + 1 == SV_MARK_BODY && sys == SV_MARK_SYS && cur_planet() == SV_MARK_PLANET) {
            Blit_BlitTransparent(buf_at(SV_ICONS, 0x2AE9),
                                 buf_at_wrapped(SV_FRAME,
                                                (int16_t)((int16_t)(o->y - o->half + 9) * 0x140)
                                                    + (int16_t)(o->x - o->half) - 3),
                                 7, 7, 0x40);
        }
    }
}

// 357b:29cd: composes one frame in SV_FRAME and copies it to the screen.
void PlanetInfo_RenderSystemView(void)
{
    int16_t sys = ds.Game_CurSystem;
    SV_FRAME = System_GetMem(0xBCC0);
    SV_MOON = System_GetMem(0x2A80);
    SV_PLANET = System_GetMem(0x2A80);
    if (System_MemAvail() < ds.Game_MinMemAvail)
        ds.Game_MinMemAvail = System_MemAvail();
    Blit_CopyBytes(far_host(SV_BACKDROP), far_host(SV_FRAME), 0xBCC0);

    if (SV_HIDDEN == 0) {
        if (cur_planet() == 0) {
            int16_t n = ds.Galaxy_PlanetCount[sys - 1];
            // Far half of the orbits behind the sun, near half in front.
            for (int16_t k = n; k >= 1; k--) {
                if (orbit(k)->y < 0x4B)
                    PlanetInfo_DrawOrbitingPlanet(k);
            }
            Blit_BlitTransparent(buf_at(SV_SPRITES, 0x140), buf_at(SV_FRAME, 0x3640), 0x40, 0x40, 0x80);
            for (int16_t k = 1; k <= n; k++) {
                if (orbit(k)->y > 0x4A)
                    PlanetInfo_DrawOrbitingPlanet(k);
            }
        } else {
            int16_t moons = moons_of(sys, cur_planet())->Count;
            for (int16_t k = moons; k >= 1; k--) {
                if ((uint16_t)(orbit(k)->y + 8) < 0x4B)
                    PlanetInfo_DrawOrbitingMoon(k);
            }
            Blit_BlitTransparent(buf_at(SV_SPRITES, 0x180 + (cur_planet() - 1) * 0x20),
                                 buf_at(SV_FRAME, 0x4A50), 0x20, 0x20, 0x80);
            if (orbit(0)->flag != 0)
                Blit_BlitTransparent(buf_at(SV_ICONS, 0x2A81 + (orbit(0)->flag - 1) * 8),
                                     buf_at(SV_FRAME, 0x4A50), 7, 7, 0x40);
            if (SV_MARK_BODY == 1 && sys == SV_MARK_SYS && cur_planet() == SV_MARK_PLANET)
                Blit_BlitTransparent(buf_at(SV_ICONS, 0x2AE9), buf_at(SV_FRAME, 0x6E90), 7, 7, 0x40);
            for (int16_t k = 1; k <= moons; k++) {
                if ((uint16_t)(orbit(k)->y + 8) > 0x4A)
                    PlanetInfo_DrawOrbitingMoon(k);
            }
        }
    }

    if (cur_planet() > 0) {
        int8_t status = planet_status(sys, cur_planet());
        if (status == 0)
            Blit_BlitTransparent(buf_at(SV_ICONS, 0x1742), buf_at(SV_FRAME, 0x74E), 0x21, 9, 0x40);
        if (status == 1 && SV_HAS_BASE == 0 && SV_HAS_GROUPS == 0)
            Blit_BlitTransparent(buf_at(SV_ICONS, 0x23C2), buf_at(SV_FRAME, 0x74E), 0x21, 9, 0x40);
    }

    // The list of systems down the right side (screen 7).
    if ((ds.Research_Products[0xD].State == 5 || ds.Research_Products[0xE].State == 5) && cur_planet() == 0) {
        for (int16_t i = 0; i <= 0x12; i++)
            Blit_FillBytes(buf_at(SV_FRAME, i * 0x140 + (sys - 1) * 0x17C0 + 0x100), 0x40, 0x48);
        for (int16_t k = 1; k <= 8; k++) {
            if (ds.Galaxy_SystemStatus[k - 1] >= 0)
                Blit_BlitTransparent(buf_at(SV_ICONS, ((k - 1) / 4) * 0x17C0 + ((k - 1) % 4) * 0x40),
                                     buf_at(SV_FRAME, (k - 1) * 0x17C0 + 0x100), 0x40, 0x13, 0x40);
        }
    }

    // The fleets list of the planet view (screen 18).
    if (cur_planet() != 0 && (SV_HAS_BASE != 0 || SV_HAS_GROUPS != 0)) {
        for (int16_t i = 1; i <= 0x12; i++) {
            int16_t kind = group_tab(i)->kind;
            if (kind > 0)
                Blit_BlitTransparent(
                    buf_at(SV_ICONS, ((kind - 1) % 10) * 0x20 + 1 + (((kind - 1) / 10) * 0x10 + 1) * 0x140),
                    buf_at(SV_FRAME, ((i - 1) % 2) * 0x20 + ((i - 1) / 2) * 0x1400 + 0x241), 0x1F, 0xF, 0x40);
        }
        if (SV_SELECTED > 0)
            Blit_BlitTransparent(buf_at(SV_ICONS, 0x15E1),
                                 buf_at(SV_FRAME, ((SV_SELECTED - 1) % 2) * 0x1F + ((SV_SELECTED - 1) / 2) * 0x1400
                                                       + 0x241),
                                 0x1F, 0xF, 0x40);
    }

    if (SV_BLINK < 1)
        SV_BLINK = 0x14;
    else
        SV_BLINK--;
    if (ds.Game_CurrentScreen == 0x12 && SV_BLINK > 7) {
        Blit_BlitTransparent(buf_at(SV_ICONS, 0x2187), buf_at(SV_FRAME, 0xADCA), 0x39, 5, 0x40);
        Blit_BlitTransparent(buf_at(SV_ICONS, 0x2906), buf_at(SV_FRAME, 0xAE03), 0x39, 5, 0x40);
    }

    Blit_CopyRect(far_host(SV_FRAME), screen_at(0x3D40), 0x140, 0x97);
    Mouse_HitTestCursor(&SV_HIT);
    ds.Mouse_SavedCursorShape = ds.Mouse_CursorShape;
    if (SV_HIT < 0x14)
        ds.Mouse_CursorShape = 0;
    else
        ds.Mouse_CursorShape = Mouse_Hotspot((int16_t)(SV_HIT - 0x14))->_1A;
    if (System_MemAvail() < ds.Game_MinMemAvail)
        ds.Game_MinMemAvail = System_MemAvail();
    System_FreeMem(SV_FRAME, 0xBCC0);
    System_FreeMem(SV_MOON, 0x2A80);
    System_FreeMem(SV_PLANET, 0x2A80);
}

// 357b:3010: moves every body along its orbit, then draws the frame.
void PlanetInfo_UpdateOrbitPositions(void)
{
    int16_t sys = ds.Game_CurSystem;
    bool moonView = cur_planet() != 0;
    for (int16_t k = 1; k <= 8; k++) {
        if (!moonView && planet_status(sys, k) < 0)
            continue;
        OrbitRec *o = orbit(k);
        THotspot *h = Mouse_Hotspot((int16_t)(k + 0x13));
        // The inner planets turn at even angles only.
        int16_t ang = k < 4 ? (int16_t)((o->ang / 2) << 1) : o->ang;
        int16_t a = (int16_t)(ang / 10);
        int16_t b = (int16_t)((a + 0x10E) % 0x168);
        o->x = (int16_t)(long_div(long_mul(o->rx, sin_tab(a)), 500) + 0xA0);
        o->y = (uint16_t)(long_div(long_mul((int16_t)o->ry, sin_tab(b)), 500) + 0x4B);
        uint16_t sz = (uint16_t)(long_div(long_mul(sin_tab(b), 0x43), 500) + 0x43);
        o->size = moonView ? (int16_t)((sz >> 4) + 8) : (int16_t)((sz >> 3) + 0x10);
        o->half = (int16_t)((uint16_t)o->size >> 1);
        h->X = (int16_t)(o->x - o->half - 3);
        h->Y = (uint8_t)((uint8_t)o->y - (uint8_t)o->half + 0x2E);
        if (moonView) {
            h->W = 0x10;
            h->H = 0x10;
        } else {
            h->W = o->size;
            h->H = (uint8_t)o->size;
        }
        if (!moonView)
            o->ang = (int16_t)((int16_t)((o->speed >> 1) + o->ang + 0xE10) % 0xE10);
        else
            o->ang = (int16_t)((int16_t)((o->speed >> 2) + o->ang + 0xE10) % 0xE10);
    }
    PlanetInfo_RenderSystemView();
}
