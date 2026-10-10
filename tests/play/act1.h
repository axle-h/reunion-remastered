// Act 1 of the whole-game policy (docs/playthrough-policy.md §1 "Act 1:
// Economy", steps E1-E23): from a new game to Jade revealed and Trade 1
// standing by in orbit at New Earth. Each act1_eNN function is one step of
// the plan in tests/test_play_game.c, run after the checkpoint of the same
// name. Steps marked ‖ in the policy start background goals (policy.h
// pol_goal) that the later waits advance: act1_goals_eNN registers them
// (pol_run_step calls them, also for the steps before a resumed one).
#pragma once

void act1_start(void);
void act1_e01_hire_builder(void);
void act1_e02_hire_developer(void);
void act1_e03_tax(void);
void act1_e04_satellite(void);
void act1_e05_mine(void);
void act1_e06_derricks(void);
void act1_e07_satellites(void);
void act1_e08_deploy(void);
void act1_e09_vehicle_plant(void);
void act1_e10_carrier(void);
void act1_e11_miner_station(void);
void act1_e12_university(void);
void act1_e13_transfer_ship(void);
void act1_e14_pilot(void);
void act1_e15_station_on_apollo(void);
void act1_e16_survey(void);
void act1_e17_control_centre(void);
void act1_e18_apollo(void);
void act1_e19_mir(void);
void act1_e20_penelope(void);
void act1_e21_new_earth_mines(void);
void act1_e22_jade(void);
void act1_e23_standby(void);

void act1_goals_s2(void);
void act1_goals_e05(void);
void act1_goals_e06(void);
void act1_goals_e09(void);
void act1_goals_e12(void);
void act1_goals_e14(void);
void act1_goals_e16(void);
void act1_goals_e18(void);
void act1_goals_e19(void);
void act1_goals_e20(void);
void act1_goals_e21(void);
// The colony standing orders (E18's build-out, S2-S4) for Russel, the
// colony act 2 founds (J11).
void act1_goals_russel(void);
