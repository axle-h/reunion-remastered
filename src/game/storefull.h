// Improvement (game/improvements.h, storage_full_message): production stops
// without a word when a store is full; with the switch on, one log entry
// "Storage is full on <body>" says so (sample and flash, no box).
//
// Each body (system, planet, moon) has a full bit per ore and an armed flag,
// in memory only. The hourly production checks each ore the body produces
// where it skips a full store: ores 2..6 in Sim_MineBody (a body with
// droids and a deposit, every hour, whatever the roll of a body without a
// colony), ore 1 in Sim_UpdateBuildings' Derrick step (a finished, powered
// Derrick there); New Earth's store is Game_HomeStock. At or over the cap,
// an ore's clear bit is set, and an armed body logs the entry and is
// disarmed. Under the cap, a set bit is cleared and the body armed. So the
// first ore to fill warns, nothing more is said while it or others stay
// full, and bringing any full ore below its cap lets the next one to fill
// warn again. The checks draw no random numbers.
//
// A new game and every load seed the state from the stores: a bit for each
// produced ore at its cap, and the body armed only if none is, so a load
// repeats no warning the player already had.
#pragma once

#include <stdint.h>

// The bits from the stores as they are (a new game or a load).
void StoreFull_Reset(void);
// Ore `ore` (1..6) of the body, produced there, has `stock` t of `cap`.
void StoreFull_Check(int16_t system, int16_t planet, int16_t moon, int16_t ore, int32_t stock,
                     int32_t cap);
