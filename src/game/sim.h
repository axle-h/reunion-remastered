// Unit Sim (1b8a): the hourly simulation. Main calls, once per game hour and
// in this order: AdvanceHour, ProcessStoryTimers, ProgressResearch,
// ProgressProduction, ProcessAllBodyEvents (only when Hour = 0, i.e. daily),
// GrowStaffExperience, UpdateBuildings, UpdateColonySetups, MineAllBodies,
// ProcessTravellingGroups, CheckEnemyAttacks, ProcessPubTimers.
//
// Every routine draws its random numbers from System_Random in the original's
// order, so a game seeded the same way evolves identically.
#pragma once

#include <stdint.h>

// 1b8a:0000: Hour+1 with carry into Day (24 h), Month (30 days) and Year
// (12 months); runs the copy-protection token check; redraws the date.
void Sim_AdvanceHour(void);
// 1b8a:006e: pub people's wait timers (deal results, deliveries, pirates) and
// the pirate raids scheduled in Pub_Deals.
void Sim_ProcessPubTimers(void);
// 1b8a:04d8: development progress of the inventions in state 2 or 4.
void Sim_ProgressResearch(void);
// 1b8a:0705: production queues (TProduct Queued, WorkPerPiece, WorkLeft).
void Sim_ProgressProduction(void);
// 1b8a:206e: daily events of every body in the open systems (colony growth and
// taxes, disease, meteorites, revolts, satellite/observatory discoveries).
void Sim_ProcessAllBodyEvents(void);
// 1b8a:2178: building counts, random damage to buildings under construction
// and mining by kind-5 buildings.
void Sim_UpdateBuildings(void);
// 1b8a:25b5: counts down Game_ColonySetups and places the starter buildings.
void Sim_UpdateColonySetups(void);
// 1b8a:2837: mining on every colony (and droid-mined body) of the open systems.
void Sim_MineAllBodies(void);
// 1b8a:2a0e: fleets in flight: legs, arrival, exploration, first contacts.
void Sim_ProcessTravellingGroups(void);
// 1b8a:3546: alien forces arriving at the player's colonies and fleets.
void Sim_CheckEnemyAttacks(void);
// 1b8a:3b83: staff experience and the developer's return from university.
void Sim_GrowStaffExperience(void);
// 1b8a:3d07: idea timers and the story's event timers.
void Sim_ProcessStoryTimers(void);
