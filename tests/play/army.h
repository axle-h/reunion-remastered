// The army of the whole-game policy (docs/playthrough.md: M2, M8, M12,
// M16, M18, L7, L11, L13; S9, S10): one background goal, "M8_army", keeps
// every production line the target force needs busy (each product queued
// about three days deep) and fills the army groups, docked at New Earth,
// from the shop's stores, units first, then their weapons. Act 3 has one
// group, "Home Guard", renamed "Strike" for Mirach (M18); act 4 keeps
// "Strike" for the invasions and a new "Home Guard" with the ground reserve
// (S9); act 5 adds Cruisers, Missile launchers and the Destroyers' Plasma
// guns (X3, X6). The targets grow with the step running.
#pragma once

#include <stdint.h>

// The units of an army group's grid that the policy builds.
enum : int {
    ARMY_HUNTER, ARMY_SF, ARMY_DESTROYER, ARMY_TANK, ARMY_AIRCRAFT, ARMY_CRUISER, ARMY_LAUNCHER,
    ARMY_UNITS
};

typedef struct {
    int n[ARMY_UNITS];
} ArmyForce;

// Registers the goal (M2 on: Twin laser gun, the first refit).
void army_goals(void);
// The group the act-3 steps mean: "Strike" once Home Guard was renamed for
// Mirach, else "Home Guard" (goes to ship info).
const char *army_group(void);
// The grid count of `ship` (and `weapon` on its row) in group `group`
// (ui_group_units; goes to ship info).
int army_count(const char *group, const char *ship, const char *weapon);
// What the goal builds group `group` up to at the step running (nullptr:
// act 3's group).
ArmyForce army_target(const char *group);
// Group `group` (nullptr: army_group(), after the research screen is read)
// holds `f` with each unit's weapons (those researched; the tanks' twins
// only once the step gives them one). With `tanks` false the tanks are not
// counted (M8 counts its ground reserve itself).
bool army_holds(const char *group, const ArmyForce *f, bool tanks);
