// Unit Cargo (2583): cargo transfer screen between a planet's stores and the
// selected group's ship (screen 13).
//
// Screen state lives in unnamed globals (see cargo.c): the list of storable
// items (DS:7816), pointers to the planet's item counts (DS:782E), the planet
// store table (DS:7866) and the used/total space longints (DS:786A..7874).
#pragma once
#include <stdint.h>
#include "game/dseg.h"

// 2583:0029: main handler; transfers on hotspot hits 15h.. (to planet) and 28h.. (to ship).
void Cargo_CargoScreen(void);
// 2583:0494: current body from the group, store list, capacities.
void Cargo_InitCargoScreen(void);
// 2583:07a6
void Cargo_SetupCargoHotspots(void);
// 2583:0894: row 1..6 resources, 7.. items; 0 = none highlighted.
void Cargo_FlashCargoRow(int16_t row);
// 2583:08b1
void Cargo_DrawCargoNames(int16_t row);
// 2583:09c6
void Cargo_DrawCargoAmounts(void);

// Pure space arithmetic, split out of InitCargoScreen for tests: capacity of
// a ship (sum of unit count * Cargo_UnitSpace) and the space its items use.
int32_t Cargo_ShipCapacity(const TGroup *g);
int32_t Cargo_ItemsSpace(const TGroup *g);
