// Port addition: the colony stats overlay of the planet info screen (screen
// 8). Clicking the POP row of your own colony shows what the daily colony
// update (Sim_ProcessBodyEvents, sim.c) is about to decide: the housing, food
// and health capacities, the disease timer, the population at which the
// people demand a Stadium or a University, and the mood the people move to.
// Switched by config.colony_stats (port menu, Game).
#pragma once

#include <stdint.h>

enum : int {
    COLONY_STATS_MODE = 4,  // ds.PlanetInfo_AlienMode while the overlay is shown
    COLONY_STATS_HIT = 0x18,  // its hotspot (4) on the planet info screen
};

typedef struct ColonyStats {
    int32_t population;
    int32_t housing, food, health;  // capacities: more people than this is a shortage
    int16_t sick;                   // people count this many times for health (2: diseased)
    int16_t disease_days;           // 0 diseased, 1..99 days to go, -1 never
    bool meteor_world;
    bool radiation;                 // a planet near the sun without an Anti-radiation shield
    int16_t powered[25], unpowered[25];  // finished buildings by kind - 1
    int16_t target;                 // the mood the people move to (without meteorite hits)
    int16_t attitude, attitude_tomorrow;
    int32_t max_pop;                // the population the colony grows (or shrinks) to
    int32_t population_tomorrow;
} ColonyStats;

// The next daily update of your colony at (system, planet, moon), as far as
// it doesn't depend on chance. Changes nothing.
void ColonyStats_Compute(int16_t system, int16_t planet, int16_t moon, ColonyStats *s);
// The POP row is a hotspot: config.colony_stats is on and the current body
// is your colony.
bool ColonyStats_Available(void);
// Draws the overlay for the current body over the planet info screen.
void ColonyStats_Draw(void);
