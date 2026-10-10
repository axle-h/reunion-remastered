// Act 2 of the whole-game policy (docs/playthrough-policy.md §1 "Act 2:
// Jaanosians", steps J1-J13): from Jade revealed to the Galleon, with the
// Jaanosian meetings, the army for Morgrul wave 1 and the standing orders
// act 2 adds (S6 ore hauling, S7 droid delivery, S8 wave evacuation in
// policy.c, S3 on the colonies in act1.c). Each act2_jNN function is one
// step of the plan in tests/test_play_game.c; act2_goals_jNN registers the
// background goals the step starts (policy.h pol_goal).
#pragma once

void act2_j01_contact(void);
void act2_j02_communicator(void);
void act2_j03_developer_home(void);
void act2_j04_meeting1(void);
void act2_j05_meeting2(void);
void act2_j06_sos(void);
void act2_j07_commander(void);
void act2_j08_army(void);
void act2_j09_base_falls(void);
void act2_j10_radio(void);
void act2_j11_russel(void);
void act2_j12_hyperspace(void);
void act2_j13_galleon(void);

void act2_goals_j01(void);
void act2_goals_j08(void);
void act2_goals_j09(void);
void act2_goals_j11(void);
