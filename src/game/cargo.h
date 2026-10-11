// Unit Cargo (2583): cargo transfer screen between a planet's stores and the
// selected group's ship (screen 13).
//
// Screen state: the Cargo_* fields of DS (game/dseg.h): the storable items,
// pointers to the planet's item counts and its store table, and the used and
// total space.
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
