// Act 3 of the whole-game policy (docs/playthrough.md, "Act 3:
// Morgruls", steps M1-M18): from the Galleon to the Morgrul home world
// taken, with Morgrul waves 2 and 3, the Kalls, Phelonians and the drifting
// ship in Phoenix, the Spy and Explorer 1 in Mirach. Each act3_mNN function
// is one step of the plan in tests/test_play_game.c; act3_goals_mNN
// registers the background goals the step starts (policy.h pol_goal).
#pragma once

void act3_m01_wave1(void);
void act3_m02_twin_lasers(void);
void act3_m03_phoenix(void);
void act3_m04_kalls(void);
void act3_m05_phoenix1(void);
void act3_m06_phelonians(void);
void act3_m07_tank_deal(void);
void act3_m08_wave2_force(void);
void act3_m09_wave2(void);
void act3_m10_course(void);
void act3_m11_drifting_ship(void);
void act3_m12_wave3(void);
void act3_m13_spy(void);
void act3_m14_spy_satellite(void);
void act3_m15_explorer(void);
void act3_m16_destroyers(void);
void act3_m17_spy_on_mirach(void);
void act3_m18_mirach(void);

void act3_goals_m02(void);
void act3_goals_m05(void);
void act3_goals_m08(void);
void act3_goals_m14(void);
void act3_goals_m16(void);
