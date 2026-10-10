// Act 4 of the whole-game policy (docs/playthrough-policy.md §1 "Act 4: The
// League", steps L1-L13): from Mirach 3 taken to the League's surrender,
// with the new staff, the Antares colony and the Erans, the League tip and
// the Eran's job, the Lisonian wave at New Earth, Aircraft, and the
// invasions of Orionis 3, Lyrae 5 and Lyrae 7. Each act4_lNN function is one
// step of the plan in tests/test_play_game.c; act4_goals_lNN registers the
// background goals the step starts (policy.h pol_goal). The army is
// army.c's goal.
#pragma once

void act4_l01_pilot(void);
void act4_l02_developer(void);
void act4_l03_informer(void);
void act4_l04_league_tip(void);
void act4_l05_antares(void);
void act4_l06_erans(void);
void act4_l07_home_guard(void);
void act4_l08_spy_satellites(void);
void act4_l09_league_contact(void);
void act4_l10_eran_job(void);
void act4_l11_aircraft(void);
void act4_l12_lisonian_wave(void);
void act4_l13_league(void);

void act4_goals_l01(void);
void act4_goals_l04(void);
void act4_goals_l05(void);
