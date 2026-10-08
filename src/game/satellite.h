// Unit Satellite (28cd): deploying satellites and colonising bodies, the group
// toolbars (menus 0x10, 0x11, 0x16) and the carrier/trade-ship counters.
#pragma once

#include <stdint.h>

#include "game/dseg.h"

// 28cd:0000 .. 021f: menu commands 0x46, 0x14, 0x43, 0x44, 0x45
void Satellite_DeployStockSatellite(void);
void Satellite_DeploySatellite(void);
void Satellite_DeploySpySat(void);
void Satellite_DeploySpyShip(void);
void Satellite_DeploySolarSat(void);
// 28cd:0254 / 02ec
void Satellite_ColonizeBody(void);
void Satellite_AcquireBody(void);
// 28cd:0367 / 03fb / 0542: toolbars (03fb is Groups_UpdateSatMenu)
void Satellite_SetupGroupScreenMenu(void);
void Satellite_UpdateGroupsMenu(void);
void Satellite_UpdateShipInfoMenu(void);
// 28cd:062b: *any := a menu item was added
void Satellite_AddBodyActionItems(int16_t system, int16_t planet, int16_t moon, bool *any);
// 28cd:0948
void Satellite_HandleBodyActions(void);
// 28cd:0a98 / 0b2c: satType 1..4 = Units[1][1].Weapons[satType] of carrier groups (Kind 4)
int32_t Satellite_CountSatellites(int16_t system, int16_t planet, int16_t moon, int16_t satType);
void Satellite_DeductSatellites(int16_t system, int16_t planet, int16_t moon, int16_t satType,
                                int32_t amount);
// 28cd:0c81 / 0d27: ships (Weapons[4] of the four slots) of trade groups (Kind 2)
int32_t Satellite_CountTradeShips(int16_t system, int16_t planet, int16_t moon);
void Satellite_TakeTradeShips(int16_t system, int16_t planet, int16_t moon, int32_t amount);
// 28cd:0ea4
void Satellite_Init(void);
