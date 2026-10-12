// Unit PlanetInfo (357b): the planet info screens (screens 7 and 8), the
// animated system view with its orbiting bodies (screens 18 and 7), the body
// name helpers, the building power balancing and the protection page picker.
//
// A "body" is a planet (moon = 0) or one of its moons. Game_CurBody points at
// the selected one (SelectCurBody). The bodies of system s are the TBody
// array ds.Galaxy_Bodies[s - 1]; planets come first, then the moons. The
// planets' moons are listed in the TMoonList array ds.Galaxy_MoonLists, whose
// Moons[] hold indices into the body array. ds.Galaxy_Names has a TBodyName
// per body.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "game/dseg.h"

// 357b:0031: 3 hotspots (4 with the colony stats), or 1 when
// ds.PlanetInfo_AlienMode is set.
void PlanetInfo_SetupPlanetInfoHotspots(void);
// 357b:0359
void PlanetInfo_DrawPlanetInfoText(void);
// 357b:10d9
void PlanetInfo_DrawPlanetInfoScreen(void);
// 357b:134b / 1429: only when AlienMode is 0.
void PlanetInfo_LoadPlanetFajPic(int16_t n, bool setPalette);
void PlanetInfo_LoadPlanetPlanetPic(int16_t n, bool setPalette);
// 357b:1515
void PlanetInfo_DrawAlienInfoScreen(void);
// 357b:1671
void PlanetInfo_DrawStoresList(void);
// 357b:1825
void PlanetInfo_DrawPlanetSurfaceBackdrop(void);
// 357b:18cc
void PlanetInfo_SetupSystemViewHotspots(void);
// 357b:235a
void PlanetInfo_InitSystemView(void);
// 357b:23db
void PlanetInfo_LoadSystemViewAssets(void);
// 357b:26ee
void PlanetInfo_ReloadSystemView(void);
// 357b:270b / 27e9: nested in RenderSystemView; k is its loop variable.
void PlanetInfo_DrawOrbitingPlanet(int16_t k);
void PlanetInfo_DrawOrbitingMoon(int16_t k);
// 357b:29cd
void PlanetInfo_RenderSystemView(void);
// 357b:3010
void PlanetInfo_UpdateOrbitPositions(void);

// 357b:3253
void PlanetInfo_SelectCurBody(void);
// 357b:32f1: the first parameter is compared with CurSystem.
bool PlanetInfo_IsCurrentBody(int16_t sys, int16_t planet, int16_t moon);
// The body names, into dst (cap bytes).
// 357b:332c: name of the planet (moon = 0) or the moon.
void PlanetInfo_GetBodyName(char *dst, size_t cap, int16_t sys, int16_t planet, int16_t moon);
// 357b:33b3: "<planet> <moon>"
void PlanetInfo_GetBodyFullName(char *dst, size_t cap, int16_t sys, int16_t planet, int16_t moon);
// 357b:346d: "<System> system,planet <p>,moon <m>"
void PlanetInfo_GetBodyLongName(char *dst, size_t cap, int16_t sys, int16_t planet, int16_t moon);
// 357b:355e: "<System> system <p> <m>"
void PlanetInfo_GetBodyLocationName(char *dst, size_t cap, int16_t sys, int16_t planet,
                                    int16_t moon);

// 357b:3cc0: balances the power of a body's buildings. Switches buildings
// off (lowest priority first) while twice the output is below the energy
// demand, stores each building's W9/WB/BD scaled by the supply ratio, and
// leaves the planet force's energy supply in its Storage field and the
// total of kind 16h's output in the body's bytes 4..5.
void PlanetInfo_BalanceBuildingPower(int16_t sys, int16_t planet, int16_t moon);
