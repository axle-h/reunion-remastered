#include "game/planetinfo.h"

#include <SDL3/SDL.h>
#include <string.h>

#include "game/blit.h"
#include "game/colonystats.h"
#include "game/gfx.h"
#include "game/groups.h"
#include "game/le.h"
#include "game/mouse.h"
#include "game/rawtext.h"
#include "game/scale.h"
#include "game/system.h"

// Routines missing from their units' headers.
int16_t Surface_CountBuildings(int16_t kind, int16_t system, int16_t planet, int16_t moon,
                               bool activeOnly); // 2ef2:2b1a


// ---------------------------------------------------------------------------
// System view state
// ---------------------------------------------------------------------------
// PlanetInfo_Backdrop/Icons/Sprites: the system view's pictures (hatter1/2,
// hatter3/4, planets\napr<system>); Frame: the frame being composed;
// MoonSprite/PlanetSprite: the scaled sprites. PlanetInfo_Orbits[k]: where
// body k is drawn (TOrbit). Launch_SystemViewTable[1..18]: the fleets
// behind hotspots 1..18; Launch_SelectedEntry: the selected one (0 = none).
// Launch_SavedSystem/SavedPlanet/SavedBody: the marked body (1 = the planet,
// k + 1 = moon k).

static TOrbit *orbit(int16_t k)
{
    return &ds.PlanetInfo_Orbits[k];
}

static TViewGroup *group_tab(int16_t i)
{
    return &ds.Launch_SystemViewTable[i - 1];
}

// Launch_SelectedEntry, signed as the original compares it.
static int16_t selected_entry(void)
{
    return (int16_t)ds.Launch_SelectedEntry;
}

// Vec3D's cosine table, read as sines with the angle shifted (scaled by 500).
static int32_t sin_tab(int16_t i)
{
    return ds.Vec3D_CosTable[i];
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


// The system view's pictures.
static uint8_t *icons_at(int32_t delta)
{
    return ds.PlanetInfo_Icons + delta;
}

static uint8_t *sprites_at(int32_t delta)
{
    return ds.PlanetInfo_Sprites + delta;
}

static uint8_t *frame_at(int32_t delta)
{
    return ds.PlanetInfo_Frame + delta;
}

// The original's 16-bit offset wraps inside the frame's segment (a whole
// segment in the port): a sprite at the frame's top edge has a negative
// origin.
static uint8_t *frame_at_wrapped(int32_t delta)
{
    return ds.PlanetInfo_Frame + (uint16_t)delta;
}

static uint8_t *screen_at(uint16_t off)
{
    return gfx_screen() + off;
}

static TBody *body_at(int16_t sys, int16_t idx)
{
    return ds.Galaxy_Bodies[sys - 1] + (idx - 1);
}

static TBodyName *name_at(int16_t sys, int16_t idx)
{
    return ds.Galaxy_Names[sys - 1] + (idx - 1);
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
    return ds.Galaxy_MoonLists[sys - 1] + (planet - 1);
}

// Index in the body array of planet `planet`'s moon number `moon`.
static int16_t moon_body(int16_t sys, int16_t planet, int16_t moon)
{
    return moons_of(sys, planet)->Moons[moon - 1];
}

// Surface::Tileset[SurfaceType], then SurfaceColonisable of that.
static bool colonisable(const TBody *b)
{
    int16_t tileset = ds.Surface_Tileset[(int8_t)b->SurfaceType - 1];
    return ds.Galaxy_SurfaceColonisable[tileset - 1] != 0;
}

// The bytes of Races[owner] (2..12). Owners 0 and 1 index below the array
// (into Surface_BuildingTypes) and owners above 12 past it; the original
// reads them all the same.
static const uint8_t *race_bytes(int16_t owner)
{
    return (const uint8_t *)&ds + offsetof(DSeg, Races_Races) + (owner - 2) * (int)sizeof(TRace);
}

static int8_t race_status(uint8_t owner)
{
    return (int8_t)race_bytes(owner)[offsetof(TRace, Status)];
}

// 32-bit multiply and signed divide as System's LongMul / LongDivMod.
static int32_t long_mul(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a * (uint32_t)b);
}

static int32_t long_div(int32_t a, int32_t b)
{
    if (b == 0)
        System_RunError(200, nullptr);
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
        Mouse_SetHotspot(1, 0, 0x31, 0x39, 0x2F, "SEE OWNER INFO", 1, 0);
        Mouse_SetHotspot(2, 0, 0x61, 0x39, 0x2F, "SHIPS", 1, 0);
        Mouse_SetHotspot(3, 0, 0x91, 0x60, 0x37, "SEE SURFACE", 1, 0);
        // Port addition (game/colonystats.h): the POP row.
        if (ColonyStats_Available()) {
            ds.Mouse_HotspotCount = 4;
            Mouse_SetHotspot(4, 0x3A, 0x43, 0x100, 0x0B, "COLONY STATS", 1, 0);
        }
    } else {
        ds.Mouse_HotspotCount = 1;
        Mouse_SetHotspot(1, 0, 0x31, 0x140, 0x97, "SEE PLANET INFO", 1, 0);
    }
}

// ---------------------------------------------------------------------------
// Planet info screen
// ---------------------------------------------------------------------------
// The 0..69 attitude and resistance scales have ten values per step; other
// values keep the text unchanged.
static int scale_step(int8_t v)
{
    return v >= 0 && v < 70 ? v / 10 : -1;
}

// The info text's string[60] (one buffer: what a section doesn't overwrite
// stays from the one before, as in the original) and its line list's
// string[40]s.
enum { TEXT60 = 61, LINE40 = 41 };

static void set60(char text[TEXT60], const char *s)
{
    SDL_strlcpy(text, s, TEXT60);
}

static void cat60(char text[TEXT60], const char *s)
{
    SDL_strlcat(text, s, TEXT60);
}

// 357b:0359
void PlanetInfo_DrawPlanetInfoText(void)
{
    const TBody *b = ds.Game_CurBody;
    char text[TEXT60] = "", name[256], desc[TEXT60] = "";
    char lines[9][LINE40];

    uint8_t owner = b->Owner;
    // The original leaves the string undefined for owners above 14h; harmless.
    if (owner == 0) {
        set60(text, "  ");
    } else if (owner == 1) {
        set60(text, "YOUR ");
    } else if ((int8_t)owner > 1 && (int8_t)owner < 0x15) {
        if (b->InfoLevel < 0x1E) {
            set60(text, "");
        } else if (b->InfoLevel < 0x28) {
            set60(text, "ALIEN ");
        } else {
            // Races[owner].Name; owner can exceed the 12 races only in damaged data.
            set60(text, pstr_get(name, sizeof name, race_bytes(owner)));
            cat60(text, " ");
        }
    }
    if (cur_moon() == 0) {
        PlanetInfo_GetBodyName(name, sizeof name, ds.Game_CurSystem, cur_planet(), 0);
        cat60(text, "PLANET'S NAME:  ");
    } else {
        PlanetInfo_GetBodyFullName(name, sizeof name, ds.Game_CurSystem, cur_planet(), cur_moon());
        cat60(text, "MOON'S NAME: ");
    }
    cat60(text, name);
    Gfx_DrawTextF9(text, 0x5E, 0x35, 0x23);

    // Population and attitude.
    if (b->Population == 0)
        set60(text, "No");
    else
        SDL_snprintf(text, TEXT60, "%d", (int)b->Population);
    if (b->Surveyed != 0 && colonisable(b) && b->InfoLevel > 0x1D
        && (b->Owner == 0 || (b->Owner == 1 && b->Colony == 0 && b->Miner == 0)))
        set60(text, "Empty planet, ready for colonysation");
    if ((int8_t)b->Owner > 1) {
        if (b->InfoLevel < 0x1E)
            set60(text, "No");
        if (b->InfoLevel > 0x1D && b->InfoLevel < 0x28)
            set60(text, "Unknown");
        if (b->InfoLevel > 0x27)
            cat60(text, " alien");
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
        if (step >= 0)
            cat60(text, mood[step]);
    }
    Gfx_DrawTextF9(text, 0x61, 0x45, 0x24);

    // Tech level.
    if (b->Owner == 1) {
        static const char *const tech[8] = {"None",   "Very low",       "Low",        "Normal",
                                            "High",   "Difficult",      "Very difficult", "Oppressing"};
        if (b->TechLevel < 8)
            set60(text, tech[b->TechLevel]);
    } else {
        set60(text, "No");
    }
    Gfx_DrawTextF9(text, 0x61, 0x55, 0x23);

    // Resistance. Below 10 it reads "No"; unmatched values keep the tech text.
    if (b->Owner == 1 || b->InfoLevel > 0x27) {
        static const char *const res[7] = {"No",       "Under developed", "Poor",      "Developing",
                                           "Enhanced", "High-tech",       "Super-tech"};
        int step = scale_step(b->Resistance);
        if (step >= 0)
            set60(text, res[step]);
    }
    if ((int8_t)b->Owner > 1 && b->InfoLevel > 0x1D && b->InfoLevel < 0x28)
        set60(text, "Unknown");
    if ((int8_t)b->Owner > 1 && b->InfoLevel < 0x1E)
        set60(text, "No");
    Gfx_DrawTextF9(text, 0x61, 0x65, 0x23);

    // Surface type and what the body is used for.
    if (b->InfoLevel < 6) {
        set60(text, "Unknown");
    } else {
        static const char *const surf[11] = {"No data", "Earth-like", "Gaseous", "Icy",
                                             "Watery",  "Tropical",   "Desert",  "Swampy",
                                             "Rocky",   "Too hot",    "No atmosphere"};
        if (b->SurfaceType < 11)
            set60(text, surf[b->SurfaceType]);
    }
    if (b->InfoLevel > 0x13) {
        if (b->Surveyed == 0 || !colonisable(b))
            set60(desc, "not accomodable");
        else
            set60(desc, "life supporting");
    }
    if (b->InfoLevel > 0x1D && b->InfoLevel < 0x28 && (int8_t)b->Owner > 1)
        set60(desc, "enemy movement");
    if (b->Surveyed != 0 && colonisable(b) && b->InfoLevel > 0x27) {
        if (b->Owner == 1) {
            if (b->Colony != 0)
                set60(desc, "colony");
            if (b->Colony == 0 && b->Miner > 0)
                set60(desc, "under colonization");
        } else if ((int8_t)b->Owner > 1 && (int8_t)b->Owner < 0x15 && b->Colony != 0) {
            set60(desc, "Alien colony");
        }
    }
    if (desc[0] != '\0') {
        cat60(text, ", ");
        cat60(text, desc);
    }
    Gfx_DrawTextF9(text, 0x61, 0x75, 0x24);

    // Ore deposits.
    for (int16_t k = 1; k <= 6; k++) {
        int16_t y = (int16_t)((k - 1) * 8 + 0x95);
        if (b->InfoLevel < 10)
            Gfx_DrawTextF9("???", 0xA0, y, 3);
        else if (b->Deposits[k - 1] < 10 || b->_03 == 0)
            Gfx_DrawTextF9("NO ", 0xA0, y, 3);
        else
            Gfx_DrawTextF9("YES", 0xA0, y, 3);
    }

    SDL_snprintf(text, TEXT60, "%6u", b->Diameter);
    if (b->InfoLevel < 3)
        set60(text, "??????");
    Gfx_DrawTextF9(text, 0x7C, 0x85, 6);
    SDL_snprintf(text, TEXT60, "%4d", b->Temperature);
    if (b->InfoLevel < 5)
        set60(text, "????");
    Gfx_DrawTextF9(text, 0x110, 0x85, 4);

    // The list of things on the body, up to eight lines.
    int16_t n = 0;
#define LINE(lit) SDL_strlcpy(lines[n], lit, LINE40)
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
            SDL_snprintf(lines[n], LINE40, "%d Space Station", ds.Research_Products[0x17].Stock);
        }
        if (b->_0A != 0) {
            n++;
            if (b->Colony == 0)
                LINE("Miner Station");
            else
                SDL_snprintf(lines[n], LINE40, "%d Miner Droid", b->_0A);
        }
        if (b->_0B != 0 && b->Colony != 0) {
            n++;
            SDL_snprintf(lines[n], LINE40, "%d Solar Sat", b->_0B);
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
        Gfx_DrawTextF9(lines[k], 0xC2, (int16_t)((k - 1) * 8 + 0x95), 0x14);
}

// 357b:10d9
void PlanetInfo_DrawPlanetInfoScreen(void)
{
    const TBody *b = ds.Game_CurBody;
    uint8_t *buf = game_alloc(0xBCC0);
    char screen[256], name[256];
    pstr_get(screen, sizeof screen, &ds.Ui_Screens[ds.Game_CurrentScreen - 1].Name);
    SDL_snprintf(name, sizeof name, "grafika\\%s", screen);
    Gfx_LoadPicScreen(name, buf, 0x97, Gfx_PalettePtr());
    Blit_CopyRect(buf, screen_at(0x3D40), 0x140, 0x97);
    int16_t n = b->InfoLevel < 6 ? 0 : (int8_t)b->SurfaceType;
    PlanetInfo_LoadPlanetPlanetPic(n, false);
    if ((int8_t)b->Owner < 2)
        n = (int8_t)b->Owner;
    else if (b->InfoLevel < 0x28)
        n = b->InfoLevel < 0x1E ? 0 : 0xD;
    else
        n = (int8_t)b->Owner;
    PlanetInfo_LoadPlanetFajPic(n, false);
    game_free(buf);

    // The sprite sheet is only needed here; the global pointer is reused as in
    // the original (InitSystemView allocates it again).
    ds.PlanetInfo_Sprites = game_alloc(0x5500);
    SDL_snprintf(name, sizeof name, "planets\\napr%d", ds.Game_CurSystem);
    Gfx_LoadPic(name, ds.PlanetInfo_Sprites, 1, 0x44, 0, 0, 0x140, false,
                Gfx_PalettePtr(), 0, 0x6F, 0x80, 0, 0xBE);
    Mouse_HideCursor();
    if (cur_moon() == 0) {
        Blit_BlitTransparent(sprites_at((cur_planet() - 1) * 0x20 + 0x180), screen_at(0x834D), 0x20,
                             0x20, 0x80);
    } else {
        int16_t m = (int16_t)(moon_body(ds.Game_CurSystem, cur_planet(), cur_moon())
                              - ds.Galaxy_PlanetCount[ds.Game_CurSystem - 1]);
        Blit_BlitTransparent(sprites_at(((m - 1) / 0xE) * 0x1540 + ((m - 1) % 0xE) * 0x11 + 0x2AC2),
                             screen_at(0x8D55), 0x10, 0x10, 0x80);
    }
    game_free(ds.PlanetInfo_Sprites);
    Mouse_ShowCursor();
    PlanetInfo_DrawPlanetInfoText();
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
}

// 357b:134b
void PlanetInfo_LoadPlanetFajPic(int16_t n, bool setPalette)
{
    if (ds.PlanetInfo_AlienMode != 0)
        return;
    uint8_t *buf = game_alloc(0xAA6);
    char name[32];
    SDL_snprintf(name, sizeof name, "planets\\faj%d", n);
    Gfx_LoadPic(name, buf, 1, 0x2F, 0, 0, 0x3A, false, Gfx_PalettePtr(), 0, 0xF, 0x70, 0, 0xBE);
    Mouse_HideCursor();
    Blit_CopyRectStrided(buf + 0x76, screen_at(0x3FC2), 0x35, 0x2B, 0x3A, 0x140);
    Mouse_ShowCursor();
    game_free(buf);
    if (setPalette)
        RawText_SetPaletteRange(0x70, 0x7F, Gfx_PalettePtr());
}

// 357b:1429
void PlanetInfo_LoadPlanetPlanetPic(int16_t n, bool setPalette)
{
    if (ds.PlanetInfo_AlienMode != 0)
        return;
    uint8_t *buf = game_alloc(0x14A0);
    char name[32];
    SDL_snprintf(name, sizeof name, "planets\\planet%d", n);
    Gfx_LoadPic(name, buf, 1, 0x37, 0, 0, 0x60, false, Gfx_PalettePtr(), 0, 0x1F, 0x50, 0, 0xBE);
    Mouse_HideCursor();
    Blit_CopyRectStrided(buf + 0xC2, screen_at(0xB7C2), 0x5C, 0x33, 0x60, 0x140);
    Mouse_ShowCursor();
    game_free(buf);
    if (setPalette)
        RawText_SetPaletteRange(0x50, 0x6F, Gfx_PalettePtr());
}

// 357b:1515
void PlanetInfo_DrawAlienInfoScreen(void)
{
    const TBody *b = ds.Game_CurBody;
    uint8_t *buf = game_alloc(0xBCC0);
    char name[256];
    Gfx_LoadPicScreen("grafika\\aliennfo", buf, 0x97, Gfx_PalettePtr());
    Blit_CopyRect(buf, screen_at(0x3D40), 0x140, 0x97);
    SDL_snprintf(name, sizeof name, "alien\\alien%d", (int8_t)b->Owner);
    Gfx_LoadPic(name, buf, 1, 0x97, 0, 0, 0x62, false, Gfx_PalettePtr(), 0, 0x8F, 0x60, 0, 0xBE);
    Mouse_HideCursor();
    Blit_CopyRectStrided(buf + 0x63, screen_at(0x4382), 0x5E, 0x8F, 0x62, 0x140);
    Mouse_ShowCursor();
    game_free(buf);
    // The race's text: 12 strings of 36 bytes, 1B0h bytes per race.
    const uint8_t *text = ds.RawText_SzFaj + ((int8_t)b->Owner - 1) * 0x1B0;
    Gfx_DrawTextFC(pstr_get(name, sizeof name, text), 0x69, 0x36, 0x23);
    for (int16_t k = 2; k <= 12; k++)
        Gfx_DrawTextF9(pstr_get(name, sizeof name, text + (k - 1) * 0x24), 0x69,
                       (int16_t)(k * 9 + 0x36), 0x23);
    PlanetInfo_DrawStoresList();
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
}

// 357b:1671
void PlanetInfo_DrawStoresList(void)
{
    const TBody *b = ds.Game_CurBody;
    if (b->_09 != 0 || ((int8_t)b->Owner > 1 && ds.Races_Hostile[b->Owner - 1] == 2)) {
        // Bytes 13h..1Ah of the race record are its weapon flags (ship
        // classes, then ground units), read as the spy reports read them. A
        // body whose race was eliminated keeps its spy ship (_09) with owner
        // 0, and then the bytes below Races_Races may add lines of 0; kept as
        // in the original.
        const uint8_t *race = race_bytes((int8_t)b->Owner);
        char s[16], name[256];
        for (int16_t k = 1; k <= 4; k++) {
            int16_t y = (int16_t)((k - 1) * 9 + 0x9C);
            if (race[k + 0x12] != 0) {
                const TProduct *p = &ds.Research_Products[ds.Research_ShipProduct[k - 1] - 1];
                SDL_snprintf(s, sizeof s, "%d", (int)b->Stores[k - 1]);
                Gfx_DrawTextF9(pstr_get(name, sizeof name, &p->Name), 0x68, y, 0x10);
                Gfx_DrawTextF9(":", 0xAA, y, 1);
                Gfx_DrawTextFC(s, 0xB0, y, 3);
            }
        }
        for (int16_t k = 1; k <= 4; k++) {
            int16_t y = (int16_t)((k - 1) * 9 + 0x9C);
            if (race[k + 0x16] != 0) {
                const TProduct *p = &ds.Research_Products[ds.Research_GroundProduct[k - 1] - 1];
                SDL_snprintf(s, sizeof s, "%d", (int)b->Stores[k + 3]);
                Gfx_DrawTextF9(pstr_get(name, sizeof name, &p->Name), 0xC5, y, 0x10);
                Gfx_DrawTextF9(":", 0x125, y, 1);
                Gfx_DrawTextFC(s, 299, y, 3);
            }
        }
    }
}

// 357b:1825
void PlanetInfo_DrawPlanetSurfaceBackdrop(void)
{
    const TBody *b = ds.Game_CurBody;
    uint8_t *buf = game_alloc(0xBCC0);
    char name[32];
    SDL_snprintf(name, sizeof name, "planets\\nagy%d", (int8_t)b->SurfaceType);
    Gfx_LoadPicScreen(name, buf, 0x97, Gfx_PalettePtr());
    Blit_CopyRect(buf, screen_at(0x3D40), 0x140, 0x97);
    game_free(buf);
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
    ds.Game_CurBody = ds.Galaxy_Bodies[sys - 1] + (idx - 1);
    const TBody *b = ds.Game_CurBody;
    // Only the low byte of the word is written.
    *(uint8_t *)&ds.Game_CurBodyIsColony = (b->Colony == 0 || b->Owner != 1) ? 0 : 1;
}

// 357b:32f1
bool PlanetInfo_IsCurrentBody(int16_t sys, int16_t planet, int16_t moon)
{
    return ds.Game_CurSystem == sys && cur_planet() == planet && cur_moon() == moon;
}

static const PStr9 *planet_name(int16_t sys, int16_t planet)
{
    return &name_at(sys, planet)->Name;
}

static const PStr9 *moon_name(int16_t sys, int16_t planet, int16_t moon)
{
    return &name_at(sys, moon_body(sys, planet, moon))->Name;
}

// The original builds these names in 255-character temporaries, one piece at
// a time; cutting each step at 255 characters is the same as cutting the
// whole name once, as a 256-byte buffer does.

// 357b:332c
void PlanetInfo_GetBodyName(char *dst, size_t cap, int16_t sys, int16_t planet, int16_t moon)
{
    pstr_get(dst, cap, moon == 0 ? planet_name(sys, planet) : moon_name(sys, planet, moon));
}

// "<prefix><planet>", then "<before_moon><moon>" for a moon.
static void body_name(char *dst, size_t cap, const char *prefix, int16_t sys, int16_t planet,
                      int16_t moon, const char *before_moon)
{
    char t[256], p[256], m[256] = "";
    pstr_get(p, sizeof p, planet_name(sys, planet));
    if (moon > 0)
        pstr_get(m, sizeof m, moon_name(sys, planet, moon));
    SDL_snprintf(t, sizeof t, "%s%s%s%s", prefix, p, moon > 0 ? before_moon : "", m);
    SDL_strlcpy(dst, t, cap);
}

// "<system><between>", the prefix of the long and location names.
static char *system_prefix(char prefix[256], int16_t sys, const char *between)
{
    char name[256];
    pstr_get(name, sizeof name, &ds.Names_Systems[sys - 1]);
    SDL_snprintf(prefix, 256, "%s%s", name, between);
    return prefix;
}

// 357b:33b3: "<planet>" or "<planet> <moon>".
void PlanetInfo_GetBodyFullName(char *dst, size_t cap, int16_t sys, int16_t planet, int16_t moon)
{
    body_name(dst, cap, "", sys, planet, moon, " ");
}

// 357b:346d: "<system> system,planet <planet>[,moon <moon>]".
void PlanetInfo_GetBodyLongName(char *dst, size_t cap, int16_t sys, int16_t planet, int16_t moon)
{
    char prefix[256];
    body_name(dst, cap, system_prefix(prefix, sys, " system,planet "), sys, planet, moon, ",moon ");
}

// 357b:355e: "<system> system <planet>[ <moon>]".
void PlanetInfo_GetBodyLocationName(char *dst, size_t cap, int16_t sys, int16_t planet,
                                    int16_t moon)
{
    char prefix[256];
    body_name(dst, cap, system_prefix(prefix, sys, " system "), sys, planet, moon, " ");
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
    return ds.Game_Buildings + (i - 1);
}

// TBuildingType of a building kind; kind 0 is not a real type but the
// original indexes it all the same (the 63 bytes before the table).
static const TBuildingType *kind_type(uint8_t kind)
{
    return (const TBuildingType *)((const uint8_t *)&ds + offsetof(DSeg, Surface_BuildingTypes)
                                   + (kind - 1) * (int)sizeof(TBuildingType));
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
    TGroup *pf = Groups_At(1, force);
    pf->Storage = 0;
    for (int16_t i = 1; i <= ds.Game_BuildingCount; i++) {
        TBuilding *b = building_at(i);
        if (!on_body(c, b))
            continue;
        b->Flag = 1;
        if (b->Kind == 0x0B)
            pf->Storage = (int16_t)(pf->Storage + 0x14);
        if (b->Kind == 0x04)
            pf->Storage = (int16_t)(pf->Storage + 10);
        if (b->Kind == 0x05)
            pf->Storage = (int16_t)(pf->Storage + 5);
        if (b->Kind == 0x19)
            pf->Storage = (int16_t)(pf->Storage + 10);
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
    wr16le(c.body->_04, (int16_t)c.shields);
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

// Sets PlanetInfo_HasBase when the body has something the player can see.
static void note_base(const TBody *b)
{
    if (b->Owner > 1 && b->Colony != 0 && race_status(b->Owner) == 6)
        ds.PlanetInfo_HasBase = 1;
    if (b->Owner == 1 && b->Colony != 0)
        ds.PlanetInfo_HasBase = 1;
    if (b->Owner == 1 && b->_0A != 0)
        ds.PlanetInfo_HasBase = 1;
    if (b->_09 != 0)
        ds.PlanetInfo_HasBase = 1;
}

static void body_hotspot(int16_t j, const TBodyName *nm, int16_t size, int16_t sys, int16_t planet,
                         int16_t moon)
{
    THotspot *h = Mouse_Hotspot(j);
    char name[256];
    h->X = nm->X;
    h->Y = (uint8_t)(nm->Y + 0x31);
    h->W = size;
    h->H = (uint8_t)size;
    PlanetInfo_GetBodyName(name, sizeof name, sys, planet, moon);
    pstr_set(&h->Caption, 0x12, name);
    h->Repeat_ = 0;
    h->_1A = 5;
}

// 357b:18cc
void PlanetInfo_SetupSystemViewHotspots(void)
{
    int16_t sys = ds.Game_CurSystem;
    ds.PlanetInfo_BodiesHidden = (sys == 4 && ds.Intel_System4Destroyed != 0) ? 1 : 0;

    // Hotspots 1..8: the system list down the right side, once product 14 or
    // 15 is developed and the system is known.
    char name[256];
    for (int16_t k = 1; k <= 8; k++) {
        THotspot *h = Mouse_Hotspot(k);
        if (cur_planet() == 0 && ds.Galaxy_SystemStatus[k - 1] >= 0
            && (ds.Research_Products[0xD].State == 5 || ds.Research_Products[0xE].State == 5)) {
            h->X = 0x100;
            h->Y = (uint8_t)((k - 1) * 0x13 + 0x31);
            h->W = 0x40;
            h->H = 0x13;
            pstr_set(&h->Caption, 0x12, pstr_get(name, sizeof name, &ds.Names_Systems[k - 1]));
            h->Repeat_ = 1;
            h->_1A = 0;
        } else {
            h->X = 1000;
            h->Y = 0xDE;
            h->W = 0;
            h->H = 0;
            pstr_set(&h->Caption, 0x12, "");
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
        if (ds.PlanetInfo_BodiesHidden != 0)
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
            pstr_set(&h->Caption, 0x12, pstr_get(name, sizeof name, &ds.Names_Systems[sys - 1]));
            h->Repeat_ = 0;
            h->_1A = 5;
            // k is 8 here, so this writes record 8's flag, and the body is
            // Bodies[sys][CurPlanet - 1] = Bodies[sys][-1] as CurPlanet is 0 in
            // this branch. As in the original: the flag is drawn only for an
            // eighth moon, and the planet view sets its moons' flags first.
            const TBody *b = body_at(sys, cur_planet());
            orbit(8)->Flag = 0;
            if (b->Owner > 1 && b->InfoLevel > 0x1D)
                orbit(8)->Flag = 1;
            if (b->Owner > 1 && b->InfoLevel > 0x27)
                orbit(8)->Flag = (uint8_t)(b->Owner + 1);
            if (b->Owner == 1)
                orbit(8)->Flag = 2;
        }
        return;
    }

    // One planet and its moons.
    int16_t planet = cur_planet();
    ds.PlanetInfo_HasBase = 0;
    THotspot *h = Mouse_Hotspot(0x13);
    h->X = 0x90;
    h->Y = 0x6C;
    h->W = 0x20;
    h->H = 0x20;
    PlanetInfo_GetBodyName(name, sizeof name, sys, planet, 0);
    pstr_set(&h->Caption, 0x12, name);
    h->Repeat_ = 0;
    h->_1A = 5;
    const TBody *pb = body_at(sys, planet);
    note_base(pb);
    orbit(0)->Flag = body_flag(pb);

    int16_t moons = moons_of(sys, planet)->Count;
    for (int16_t k = 1; moons != 0 && k <= moons; k++) {
        int16_t bodyIdx = moon_body(sys, planet, k);
        body_hotspot((int16_t)(k + 0x13), name_at(sys, bodyIdx), 0x10, sys, planet, k);
        const TBody *mb = body_at(sys, bodyIdx);
        note_base(mb);
        orbit(k)->Flag = body_flag(mb);
    }
    if (planet_status(sys, planet) < 1)
        ds.Mouse_HotspotCount = 0x13;
    else
        ds.Mouse_HotspotCount = (int16_t)(moons_of(sys, planet)->Count + 0x13);
    Mouse_ClearHotspots(1, 0x12);
    if (ds.PlanetInfo_BodiesHidden != 0)
        ds.Mouse_HotspotCount = 0x12;

    // Remember which fleet was selected, to find it again after the rebuild.
    TViewGroup saved = {0, 0, 0};
    if (selected_entry() > 0)
        saved = *group_tab(selected_entry());

    // Hotspots 1..18: the player's fleets here, then the aliens' forces.
    int16_t j = 1;
    for (int16_t g = 1; g <= ds.Game_GroupCount; g++) {
        const TGroup *grp = Groups_At(0, g);
        if (grp->System == ds.Game_CurSystem && grp->Planet == cur_planet()
            && (grp->Status == 1 || grp->Status == 2 || grp->Status == 3) && j < 0x13) {
            Mouse_SetHotspot(j, (int16_t)(((j - 1) % 2) * 0x20 + 0x101),
                             (uint8_t)(((j - 1) / 2) * 0x10 + 0x32), 0x20, 0x10,
                             pstr_get(name, sizeof name, &grp->Name), 0, 0);
            TViewGroup *t = group_tab(j);
            t->Kind = grp->Kind;
            t->Index = g;
            t->Moon = grp->Moon;
            j++;
        }
    }
    ds.PlanetInfo_HasGroups = j > 1;
    for (int16_t r = 2; r <= 12; r++) {
        const TRace *race = &ds.Races_Races[r - 2];
        if (race->Status == 6 && (int8_t)race->ForceCount > 0) {
            for (int16_t f = 1; f <= (int8_t)race->ForceCount; f++) {
                const TRaceForce *rf = &race->Forces[f - 1];
                if (rf->System == ds.Game_CurSystem && rf->Body == cur_planet() && rf->Active == 1)
                    ds.PlanetInfo_HasGroups = 1;
            }
        }
    }
    if (ds.PlanetInfo_HasBase != 0 || ds.PlanetInfo_HasGroups != 0) {
        for (int16_t r = 2; r <= 12; r++) {
            const TRace *race = &ds.Races_Races[r - 2];
            // Each race starts at a new row (an odd slot).
            if (!(j & 1))
                group_tab(j++)->Kind = 0;
            // j can reach 19 (entry 19 would be Launch_SelectedEntry), but
            // only even j are cleared here.
            int16_t nf = (int8_t)race->ForceCount;
            for (int16_t f = 1; nf > 0 && f <= nf; f++) {
                const TRaceForce *rf = &race->Forces[f - 1];
                if (rf->System == ds.Game_CurSystem && rf->Body == cur_planet() && rf->Active == 1
                    && rf->Mission > 1 && j < 0x13) {
                    char fleet[256];
                    SDL_snprintf(fleet, sizeof fleet, "%s Fleet %d",
                                 pstr_get(name, sizeof name, &ds.Names_Races[r - 1]), f);
                    Mouse_SetHotspot(j, (int16_t)(((j - 1) % 2) * 0x20 + 0x101),
                                     (uint8_t)(((j - 1) / 2) * 0x10 + 0x32), 0x20, 0x10, fleet, 0,
                                     0);
                    TViewGroup *t = group_tab(j);
                    t->Kind = r + 3;
                    t->Index = f;
                    t->Moon = rf->Moon;
                    j++;
                }
            }
        }
    }
    if (j < 0x13) {
        for (int16_t i = j; i <= 0x12; i++)
            group_tab(i)->Kind = 0;
    }
    if (selected_entry() > 0) {
        ds.Launch_SelectedEntry = 0;
        for (int16_t i = 1; i <= 0x12; i++) {
            if (saved.Kind == group_tab(i)->Kind && saved.Index == group_tab(i)->Index)
                ds.Launch_SelectedEntry = (uint16_t)i;
        }
    }
}

// 357b:23db: loads the three pictures of the system view and sets the orbit
// record of every body.
void PlanetInfo_LoadSystemViewAssets(void)
{
    int16_t sys = ds.Game_CurSystem;
    char name[32];
    uint8_t *pal = Gfx_PalettePtr();
    Gfx_DrawStatusText("");

    Gfx_LoadPic(cur_planet() == 0 ? "planets\\hatter1" : "planets\\hatter2",
                ds.PlanetInfo_Backdrop, 1, 0x97, 0, 0, 0x140, true, pal, 0, 0xB0, 0x40, 0, 0xBE);
    Gfx_LoadPic(cur_planet() == 0 ? "planets\\hatter3" : "planets\\hatter4",
                ds.PlanetInfo_Icons, 1, 0x4B, 0, 0, 0x140, false, pal, 0, 0xB0, 0x40, 0, 0xBE);
    SDL_snprintf(name, sizeof name, "planets\\napr%d", sys);
    Gfx_LoadPic(name, ds.PlanetInfo_Sprites, 1, 0x44, 0, 0, 0x140, false, pal, 0,
                0x6F, 0x80, 0, 0xBE);

    if (cur_planet() == 0) {
        for (int16_t k = 1; k <= ds.Galaxy_PlanetCount[sys - 1]; k++) {
            const TBodyName *nm = name_at(sys, k);
            TOrbit *o = orbit(k);
            o->RX = nm->X;
            o->RY = nm->Y;
            o->Angle = (int16_t)System_Random(0xE10);
            o->Speed = nm->Size;
            o->Icon = ds.Galaxy_BodyIcon[sys - 1][k - 1];
        }
    } else {
        int16_t moons = moons_of(sys, cur_planet())->Count;
        for (int16_t k = 1; k <= moons; k++) {
            const TBodyName *nm = name_at(sys, moon_body(sys, cur_planet(), k));
            TOrbit *o = orbit(k);
            o->RX = nm->X;
            o->RY = nm->Y;
            o->Angle = (int16_t)System_Random(0xE10);
            o->Speed = nm->Size;
        }
    }
    ds.PlanetInfo_BlinkTimer = 0;
}

// 357b:235a
void PlanetInfo_InitSystemView(void)
{
    ds.PlanetInfo_Backdrop = game_alloc(0xBCC0);
    ds.PlanetInfo_Icons = game_alloc(24000);
    ds.PlanetInfo_Sprites = game_alloc(0x6900);
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

// Body `body` of the planet view (1 = the planet, k + 1 = moon k) is the
// marked one.
static bool is_marked(int16_t body)
{
    return body == ds.Launch_SavedBody && ds.Game_CurSystem == ds.Launch_SavedSystem
           && cur_planet() == ds.Launch_SavedPlanet;
}

// Offset into the 320-wide frame of the top left corner of body record o.
static int32_t sprite_origin(const TOrbit *o)
{
    return (int16_t)((int16_t)(o->Y - o->Half) * 0x140) + (int16_t)(o->X - o->Half);
}

// 357b:270b
void PlanetInfo_DrawOrbitingPlanet(int16_t k)
{
    int16_t sys = ds.Game_CurSystem;
    if (ds.Galaxy_SystemStatus[sys - 1] > 0 && planet_status(sys, k) >= 0) {
        // (The original also computes (ang / 71h + 19h) mod 20h into a local
        // here and never uses it.)
        const TOrbit *o = orbit(k);
        uint8_t *sprite = ds.PlanetInfo_PlanetSprite;
        Scale_ScaleSprite(sprites_at((k - 1) * 0x20 + 0x180), sprite, o->Size);
        Blit_BlitTransparent(sprite, frame_at_wrapped(sprite_origin(o)), o->Size, o->Size, 0x80);
    }
}

// 357b:27e9
void PlanetInfo_DrawOrbitingMoon(int16_t k)
{
    int16_t sys = ds.Game_CurSystem;
    if (planet_status(sys, cur_planet()) > 0) {
        const TOrbit *o = orbit(k);
        int16_t m = (int16_t)(moon_body(sys, cur_planet(), k) - ds.Galaxy_PlanetCount[sys - 1]);
        Scale_ScaleSprite(sprites_at(((m - 1) % 0xE) * 0x11 + 0x2AC2 + ((m - 1) / 0xE) * 0x1540),
                          ds.PlanetInfo_MoonSprite, (int16_t)(o->Size << 1));
        int32_t origin = sprite_origin(o);
        Blit_BlitTransparent(ds.PlanetInfo_MoonSprite, frame_at_wrapped(origin), o->Size,
                             o->Size, 0x80);
        if (o->Flag != 0) {
            Blit_BlitTransparent(icons_at(0x2A81 + (o->Flag - 1) * 8),
                                 frame_at_wrapped(
                                                (int16_t)((int16_t)(o->Y - o->Half - 3) * 0x140)
                                                    + (int16_t)(o->X - o->Half) - 3),
                                 7, 7, 0x40);
        }
        if (is_marked(k + 1)) {
            Blit_BlitTransparent(icons_at(0x2AE9),
                                 frame_at_wrapped(
                                                (int16_t)((int16_t)(o->Y - o->Half + 9) * 0x140)
                                                    + (int16_t)(o->X - o->Half) - 3),
                                 7, 7, 0x40);
        }
    }
}

// 357b:29cd: composes one frame in ds.PlanetInfo_Frame and copies it to the screen.
void PlanetInfo_RenderSystemView(void)
{
    int16_t sys = ds.Game_CurSystem;
    // GetMem(0xBCC0). A whole segment: a sprite at the frame's top edge has
    // a negative origin that wraps (frame_at_wrapped).
    ds.PlanetInfo_Frame = game_alloc(GAME_SEGMENT);
    ds.PlanetInfo_MoonSprite = game_alloc(0x2A80);
    ds.PlanetInfo_PlanetSprite = game_alloc(0x2A80);
    Blit_CopyBytes(ds.PlanetInfo_Backdrop, ds.PlanetInfo_Frame, 0xBCC0);

    if (ds.PlanetInfo_BodiesHidden == 0) {
        if (cur_planet() == 0) {
            int16_t n = ds.Galaxy_PlanetCount[sys - 1];
            // Far half of the orbits behind the sun, near half in front.
            for (int16_t k = n; k >= 1; k--) {
                if (orbit(k)->Y < 0x4B)
                    PlanetInfo_DrawOrbitingPlanet(k);
            }
            Blit_BlitTransparent(sprites_at(0x140), frame_at(0x3640), 0x40, 0x40, 0x80);
            for (int16_t k = 1; k <= n; k++) {
                if (orbit(k)->Y > 0x4A)
                    PlanetInfo_DrawOrbitingPlanet(k);
            }
        } else {
            int16_t moons = moons_of(sys, cur_planet())->Count;
            for (int16_t k = moons; k >= 1; k--) {
                if ((uint16_t)(orbit(k)->Y + 8) < 0x4B)
                    PlanetInfo_DrawOrbitingMoon(k);
            }
            Blit_BlitTransparent(sprites_at(0x180 + (cur_planet() - 1) * 0x20),
                                 frame_at(0x4A50), 0x20, 0x20, 0x80);
            if (orbit(0)->Flag != 0)
                Blit_BlitTransparent(icons_at(0x2A81 + (orbit(0)->Flag - 1) * 8),
                                     frame_at(0x4A50), 7, 7, 0x40);
            if (is_marked(1))
                Blit_BlitTransparent(icons_at(0x2AE9), frame_at(0x6E90), 7, 7, 0x40);
            for (int16_t k = 1; k <= moons; k++) {
                if ((uint16_t)(orbit(k)->Y + 8) > 0x4A)
                    PlanetInfo_DrawOrbitingMoon(k);
            }
        }
    }

    if (cur_planet() > 0) {
        int8_t status = planet_status(sys, cur_planet());
        if (status == 0)
            Blit_BlitTransparent(icons_at(0x1742), frame_at(0x74E), 0x21, 9, 0x40);
        if (status == 1 && ds.PlanetInfo_HasBase == 0 && ds.PlanetInfo_HasGroups == 0)
            Blit_BlitTransparent(icons_at(0x23C2), frame_at(0x74E), 0x21, 9, 0x40);
    }

    // The list of systems down the right side (screen 7).
    if ((ds.Research_Products[0xD].State == 5 || ds.Research_Products[0xE].State == 5) && cur_planet() == 0) {
        for (int16_t i = 0; i <= 0x12; i++)
            Blit_FillBytes(frame_at(i * 0x140 + (sys - 1) * 0x17C0 + 0x100), 0x40, 0x48);
        for (int16_t k = 1; k <= 8; k++) {
            if (ds.Galaxy_SystemStatus[k - 1] >= 0)
                Blit_BlitTransparent(icons_at(((k - 1) / 4) * 0x17C0 + ((k - 1) % 4) * 0x40),
                                     frame_at((k - 1) * 0x17C0 + 0x100), 0x40, 0x13, 0x40);
        }
    }

    // The fleets list of the planet view (screen 18).
    if (cur_planet() != 0 && (ds.PlanetInfo_HasBase != 0 || ds.PlanetInfo_HasGroups != 0)) {
        for (int16_t i = 1; i <= 0x12; i++) {
            int16_t kind = group_tab(i)->Kind;
            if (kind > 0)
                Blit_BlitTransparent(
                    icons_at(((kind - 1) % 10) * 0x20 + 1 + (((kind - 1) / 10) * 0x10 + 1) * 0x140),
                    frame_at(((i - 1) % 2) * 0x20 + ((i - 1) / 2) * 0x1400 + 0x241), 0x1F, 0xF,
                    0x40);
        }
        int16_t sel = selected_entry();
        if (sel > 0)
            Blit_BlitTransparent(icons_at(0x15E1),
                                 frame_at(((sel - 1) % 2) * 0x1F + ((sel - 1) / 2) * 0x1400
                                                       + 0x241),
                                 0x1F, 0xF, 0x40);
    }

    if (ds.PlanetInfo_BlinkTimer < 1)
        ds.PlanetInfo_BlinkTimer = 0x14;
    else
        ds.PlanetInfo_BlinkTimer--;
    if (ds.Game_CurrentScreen == 0x12 && ds.PlanetInfo_BlinkTimer > 7) {
        Blit_BlitTransparent(icons_at(0x2187), frame_at(0xADCA), 0x39, 5, 0x40);
        Blit_BlitTransparent(icons_at(0x2906), frame_at(0xAE03), 0x39, 5, 0x40);
    }

    Blit_CopyRect(ds.PlanetInfo_Frame, screen_at(0x3D40), 0x140, 0x97);
    int16_t hit;
    Mouse_HitTestCursor(&hit);
    ds.Mouse_ViewHotspot = hit;
    ds.Mouse_SavedCursorShape = ds.Mouse_CursorShape;
    if (hit < 0x14)
        ds.Mouse_CursorShape = 0;
    else
        ds.Mouse_CursorShape = Mouse_Hotspot((int16_t)(hit - 0x14))->_1A;
    game_free(ds.PlanetInfo_Frame);
    game_free(ds.PlanetInfo_MoonSprite);
    game_free(ds.PlanetInfo_PlanetSprite);
}

// 357b:3010: moves every body along its orbit, then draws the frame.
void PlanetInfo_UpdateOrbitPositions(void)
{
    int16_t sys = ds.Game_CurSystem;
    bool moonView = cur_planet() != 0;
    for (int16_t k = 1; k <= 8; k++) {
        if (!moonView && planet_status(sys, k) < 0)
            continue;
        TOrbit *o = orbit(k);
        THotspot *h = Mouse_Hotspot((int16_t)(k + 0x13));
        // The inner planets turn at even angles only.
        int16_t ang = k < 4 ? (int16_t)((o->Angle / 2) << 1) : o->Angle;
        int16_t a = (int16_t)(ang / 10);
        int16_t b = (int16_t)((a + 0x10E) % 0x168);
        o->X = (int16_t)(long_div(long_mul(o->RX, sin_tab(a)), 500) + 0xA0);
        o->Y = (uint16_t)(long_div(long_mul((int16_t)o->RY, sin_tab(b)), 500) + 0x4B);
        uint16_t sz = (uint16_t)(long_div(long_mul(sin_tab(b), 0x43), 500) + 0x43);
        o->Size = moonView ? (int16_t)((sz >> 4) + 8) : (int16_t)((sz >> 3) + 0x10);
        o->Half = (int16_t)((uint16_t)o->Size >> 1);
        h->X = (int16_t)(o->X - o->Half - 3);
        h->Y = (uint8_t)((uint8_t)o->Y - (uint8_t)o->Half + 0x2E);
        if (moonView) {
            h->W = 0x10;
            h->H = 0x10;
        } else {
            h->W = o->Size;
            h->H = (uint8_t)o->Size;
        }
        if (!moonView)
            o->Angle = (int16_t)((int16_t)((o->Speed >> 1) + o->Angle + 0xE10) % 0xE10);
        else
            o->Angle = (int16_t)((int16_t)((o->Speed >> 2) + o->Angle + 0xE10) % 0xE10);
    }
    PlanetInfo_RenderSystemView();
}
