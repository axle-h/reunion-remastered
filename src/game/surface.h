// Unit Surface (2ef2): the planet surface screen (screen 0x14). It loads the
// tile sheets and the .MAP of the current body, draws the 14x9 tile window
// with the building footprints, the radar minimap and the info panel, and
// places, counts and deletes the TBuilding records of a body.
//
// The map layers are 60x60 grids stored x-major: cell (x, y), both 1-based,
// is at [(x-1)*60 + (y-1)] (bytes for ds.Surface_TileLayer / OccupyLayer,
// words for BuildingLayer). Occupy values: 0 free, FFh blocked terrain,
// DDh..DFh construction stage markers, otherwise sheet tile + 1 of BufC.
#pragma once

#include <stdint.h>

#include "game/dseg.h"

// 2ef2:004a: loads the sheets into BufA..BufD, allocates the layers, LoadMap
// and works out the radar geometry.
void Surface_EnterSurface(void);
// 2ef2:03e7: MAP\MAP<SurfaceType>_<MapVariant>.MAP into the three layers.
void Surface_LoadMap(void);
// 2ef2:062f
void Surface_RegisterBuildingAnims(int16_t system, int16_t planet, int16_t moon);
// 2ef2:0791: 11 hotspots (the count is set to 12 like the original).
void Surface_SetupSurfaceHotspots(void);
// 2ef2:0a9d (+ the Ghidra fragment 0e00): info panel for Mode 3 / 4.
void Surface_DrawBuildingInfo(void);
// 2ef2:112c / 1204
void Surface_ShowBuildingInfoView(void);
void Surface_ShowBuildingPicture(void);
// 2ef2:12af (+ fragment 1404): the ten border blits from BufA.
void Surface_DrawSurfaceFrame(void);
// 2ef2:1441 (+ fragment 1472) / 151b
void Surface_SetupInfoHotspots(void);
void Surface_SetupSimpleInfoHotspots(void);
// 2ef2:1589 / 15de / 16c3
void Surface_DrawModeButtons(void);
void Surface_BlinkModeButton(void);
void Surface_DrawKindArrows(void);
// 2ef2:1924 / 198a: 16x16 (14x14 inner) tile `tile` (0-based, 20 per sheet
// row) of the sheet at src to map-window cell (x, y), both 1-based.
void Surface_BlitTile(const uint8_t *src, uint16_t tile, int16_t x, int16_t y);
void Surface_BlitTileInner(const uint8_t *src, uint16_t tile, int16_t x, int16_t y);
// 2ef2:19f7
void Surface_DrawBuildingPreview(int16_t kind);
// 2ef2:1b98: all footprint tiles of `kind` with BufC at screen (x, y).
void Surface_DrawBuildingTiles(int16_t kind, int16_t x, int16_t y);
// 2ef2:1c3c: redraws the footprint of `kind` at map cell (x, y) (a near
// procedure in the original, with the arguments in this order).
void Surface_DrawFootprint(int16_t kind, int16_t x, int16_t y);
// 2ef2:1da3
void Surface_DrawMapView(void);
// 2ef2:22df
void Surface_UpdateModeForCell(int16_t x, int16_t y);
// 2ef2:2463: x = y = FFh means not placed yet.
void Surface_AddBuilding(uint8_t kind, uint8_t system, uint8_t planet, uint8_t moon, uint8_t x,
                         uint8_t y);
// 2ef2:2581
void Surface_BuildOnCurrentPlanet(int16_t kind, int16_t x, int16_t y);
// 2ef2:25fd: mode 0 writes the tile pattern, mode 1..3 a stage marker.
void Surface_MarkFootprint(int16_t index, int16_t kind, int16_t x, int16_t y, int16_t mode);
// 2ef2:27a5: spiral search from the map centre (also sets ds.Surface_TileSet).
void Surface_FindBuildSpot(int16_t kind, int16_t system, int16_t planet, int16_t moon, int16_t *x,
                           int16_t *y, bool *found);
// 2ef2:29b1 / 2a64 / 2ac5
void Surface_AutoPlaceBuilding(int16_t kind, int16_t system, int16_t planet, int16_t moon);
void Surface_PlaceBuilding(int16_t index);
void Surface_DeleteBuilding(int16_t index);
// 2ef2:2b1a (= Game_FindBuildingProc): completed buildings of `kind` on the body.
int16_t Surface_CountBuildings(int16_t kind, int16_t system, int16_t planet, int16_t moon,
                               bool activeOnly);
// 2ef2:2b9e: fills the longint table DS:791C + flag * 100 + kind * 4.
void Surface_CountBuildingsByKind(int16_t system, int16_t planet, int16_t moon);
// 2ef2:2c52 / 312f
void Surface_RebuildRadar(void);
void Surface_DrawRadar(void);
// 2ef2:329e: sets Game_FindBuildingProc.
void Surface_Init(void);
