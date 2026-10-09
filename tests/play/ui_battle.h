// UI drivers and observers for the battle screens (docs/playthrough-battles.md):
// space battle (29), unit assignment (19), ground battle (32), and the
// GROUND WAR order on the star map that starts an own invasion.
//
// Observers read only what the battle screens draw; each one names the
// on-screen element it stands for.
#pragma once

#include <stdint.h>

// Ground unit types, in the order of the slot lists and the gricon sprites.
enum : int { UI_TROOPER = 1, UI_TANK = 2, UI_AIRCRAFT = 3, UI_LAUNCHER = 4 };

// One stack on the ground-battle field or the assignment screen.
typedef struct {
    int slot;   // 1..20 in its side's list
    int type;   // UI_TROOPER .. UI_LAUNCHER: the sprite drawn
    int count;  // units in it: "Pcs:" in the left panel when it is selected
                // (screen 32), the number in its box (screen 19)
    int x, y;   // screen 32: top-left pixel of its 16x16 sprite
} UiStack;

typedef enum {
    // No orders: every stack stands and fires at whatever comes into range
    // (the enemy always advances, grwar.c:189-196).
    UI_TACTIC_HOLD,
    // Hold, except that a tank or aircraft stack that an enemy stack can hit
    // while it cannot hit back (an aircraft on the diagonal, a launcher 3..4
    // tiles away) is sent against that stack.
    UI_TACTIC_COUNTER,
    // Send every stack against the enemy stack nearest to it, from the start
    // (for comparison; it loses more than holding).
    UI_TACTIC_CHARGE,
} UiTactic;

// --- Observers ---

// A battle screen (29 or 32) shows END BATTLE: the fight is over and the
// result picture (spvict/splost, grvict/grlost) is drawn.
bool ui_battle_over(void);
// The result picture of the battle just shown is the victory one.
bool ui_battle_won(void);
// Screen 29: ships of one side still drawn as dots in the battle view (own
// ships fly on the left).
int ui_space_ships(bool own);
// Screen 32: the stacks of one side drawn on the field (up to 20).
int ui_ground_stacks(bool own, UiStack out[20]);
// The result picture's table: units lost per class (space: Hunter,
// Fighter, Destroyer, Cruiser; ground: Trooper, Tank, Aircraft, Launcher)
// on the player's side ("Your loss") and the enemy's ("Enemy loss").
typedef struct {
    int own[4], enemy[4];
} UiLosses;
void ui_battle_losses(UiLosses *out);
// Screen 19: the filled slots (icon and count in each box), up to 20.
int ui_assign_slots(UiStack out[20]);
// Screen 19: units of a type not in any slot ("Battle tank   12" under the
// boxes).
int ui_assign_pool(int type);

// --- Drivers ---

// Star map (screen 7): opens `system`, then `planet`, selects the Army group
// `group` there and clicks GROUND WAR (0x47). False if a step is not offered.
bool ui_invade(const char *system, const char *planet, const char *group);
// Screen 29: the fight runs by itself; waits for END BATTLE (never RETREAT),
// clicks it and returns whether it was won; `losses` (may be nullptr) gets
// the result table. Fails the run on a timeout.
bool ui_space_battle(UiLosses *losses);
// Screen 19: reorders the pre-filled slots by unit type, `order` listing
// the four types from the rear column (slot 1) to the front (nullptr keeps
// the game's pre-fill: troopers, tanks, aircraft, launchers), then OK, ATTACK.
void ui_assign_units(const int order[4]);
// Screen 32: fights with `tactic` until END BATTLE (never RETREAT), clicks
// it and returns whether it was won; `losses` (may be nullptr) gets the
// result table. Fails the run on a timeout.
bool ui_ground_battle(UiTactic tactic, UiLosses *losses);
