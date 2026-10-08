// Unit Intel (29b9): the alien races' AI helpers and story events: first
// contact, race forces (missions, travel timers, landings), ownership changes
// of bodies, group disbanding and the intel reports of the Pub / satellite menus.
//
// A race record (TRace) is at DS:6A06 + race * E4h (races 2..12; the record
// of "race 1" overlaps other data, so it is addressed by offset only). Its
// forces are Forces[1..7], ForceCount of them in use. TRaceForce fields:
//   Mission        0 none, 1 stationed (garrison), >1 on a mission
//   Active         0 idle, 1 present at System/Body/Moon, 2 travelling there
//   _03            travel time, counted down by Sim; Timer: delay before NextMission
//   Units[1..4]    ship classes, Units[5..8] ground units
// Race Status: 0 unknown, 2 / 4 active (IsRaceActive), 6 hostile, -1 eliminated.
// TRace._0F, Speed ... KnownSystems hold flags the intel reports use (KnownSystems[1..4]
// = ship classes, [5..8] = ground units known to the player).
#pragma once

#include <stdint.h>

#include "game/dseg.h"

// The race's record by offset (valid for races 1..12).
static inline TRace *Intel_Race(int16_t race)
{
    return (TRace *)ds_at((uint16_t)(0x6A06 + race * 0xE4));
}
// Forces[force] (1-based) of a race.
static inline TRaceForce *Intel_Force(int16_t race, int16_t force)
{
    return &Intel_Race(race)->Forces[force - 1];
}

// 29b9:0000: first contact with `race`: shows msg (Pascal string) once, sets Status and starts its story timers.
void Intel_FirstContact(int16_t race, const uint8_t *msg);
// 29b9:0217: resets Owner, Colony, Miner, stores (+ population, resistance, tech, attitude when full).
void Intel_ClearBody(int16_t system, int16_t planet, int16_t moon, bool full);
// 29b9:0316: SetForceTarget + arrival timer Random(100 - Speed) + 5 + NextMission.
void Intel_DispatchForce(int16_t race, int16_t force, int16_t system, int16_t planet, int16_t moon,
                         uint8_t nextMission);
// 29b9:0379: DispatchForce, then Mission := NextMission.
void Intel_DispatchForceNow(int16_t race, int16_t force, int16_t system, int16_t planet, int16_t moon,
                            uint8_t mission, int16_t unused);
// 29b9:03b3: arms the launch timer (LaunchTimer hours) for a later DispatchForceNow.
void Intel_ScheduleForceLaunch(int16_t race, int16_t force, int16_t delay, int16_t mission);
// 29b9:03d2 / 0487
void Intel_SpaceBattleWon(int16_t system, int16_t planet);
void Intel_SpaceBattleLost(int16_t system, int16_t planet);
// 29b9:059e / 05b5 / 0666
void Intel_ClearBodyFull(int16_t system, int16_t planet, int16_t moon);
void Intel_PurgeBody(int16_t system, int16_t planet, int16_t moon);
void Intel_DestroyBody(int16_t system, int16_t planet, int16_t moon);
// 29b9:06d4 / 08f5
void Intel_ClaimBodyForRace(int16_t owner, int16_t system, int16_t planet, int16_t moon);
void Intel_ClaimBodyForPlayer(int16_t system, int16_t planet, int16_t moon);
// 29b9:0a60: the force's units join the garrison of its target body; Active := 0.
void Intel_ForceLands(int16_t race, int16_t force);
// 29b9:0b93: removes group `group` of `list` (fleets first merge into the planet force).
void Intel_DisbandGroup(int16_t list, int16_t group);
// 29b9:0d5a / 0d87
void Intel_SendForceHome(int16_t race, int16_t force);
void Intel_SendGroupHome(int16_t group);
// 29b9:0dab / 0e66
void Intel_SetForceTarget(int16_t race, int16_t force, int16_t system, int16_t planet, int16_t moon);
void Intel_SetGroupTarget(int16_t group, int16_t system, int16_t planet, int16_t moon);
// 29b9:0f53 (near): zeroes one force record.
void Intel_ClearForce(int16_t race, int16_t force);
// 29b9:0fe4 / 1179
void Intel_EliminateRace(int16_t race);
void Intel_AnnihilateSystem4(void);
// 29b9:136e / 13ab / 1409
void Intel_ScheduleIdea(int16_t idea, int16_t delayBase, int16_t delayRandom);
void Intel_GrantInvention(int16_t idea);
void Intel_OfferInventionForAnalysis(int16_t idea);
// 29b9:1461: Status is 2 or 4.
bool Intel_IsRaceActive(int16_t race);
// 29b9:1c49: kind 1 / 3 = bodies of the race, 2 = army power, 4 = weapons. (The nested
// ShowRaceBodyInfo / ShowRaceArmyInfo / ShowRaceWeaponInfo at 1501 / 185e / 1aed are static.)
void Intel_ShowRaceIntel(int16_t race, int16_t kind);
// 29b9:1d23: Groups_IntelHook := 29b9:0b93.
void Intel_Init(void);
