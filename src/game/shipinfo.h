// Unit ShipInfo (26fe): screen 0x10, the grid of up to 32 group icons with the
// info panel on the right, and the group-flight panel used on screen 0x11.
#pragma once

#include <stdint.h>

#include "game/dseg.h"

// 26fe:001b: screen 0x10 handler
void ShipInfo_ShipInfoScreen(void);
// 26fe:02e6
void ShipInfo_InitShipInfoScreen(void);
// 26fe:03c3 / 056d: slot 1..32 (column = (slot-1) / 8, row = (slot-1) % 8)
void ShipInfo_DrawGroupIcon(int16_t icon, int16_t slot);
void ShipInfo_DrawGroupIconBuffered(int16_t icon, int16_t slot);
// 26fe:04d4
void ShipInfo_DrawAllGroupIcons(void);
// 26fe:0693
void ShipInfo_SwitchGroupList(void);
// 26fe:093b / 0f2a
void ShipInfo_DrawGroupInfo(void);
void ShipInfo_DrawGroupFlightInfo(void);
// 26fe:10e8 / 1147 / 11a6 / 1209: markers for Game_SelGroup
void ShipInfo_DrawSelectionMarker(void);
void ShipInfo_EraseSelectionMarker(void);
void ShipInfo_DrawSelectionMarkerBuffered(void);
void ShipInfo_EraseSelectionMarkerBuffered(void);
// 26fe:1278
void ShipInfo_SetupShipInfoHotspots(void);
