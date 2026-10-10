// The whole-game playthrough (docs/playthrough.md, thread F): a new game
// (seed 12345) played by the policy of docs/playthrough-policy.md through
// clicks and keys only, one step per checkpoint, so PLAY_RESUME=<step>
// replays the last recording to that step and plays on from there. The
// recording then replays to the same end in-process and with `reunion
// --replay`. Needs the game data.
//
// The plan is written act by act (tests/play/act1.h, ... act5.h); it ends
// on the victory, the ground win on Earth (exit 10).
#include <SDL3/SDL.h>

#include "play/act1.h"
#include "play/act2.h"
#include "play/act3.h"
#include "play/act4.h"
#include "play/act5.h"
#include "play/play.h"
#include "play/policy.h"
#include "play/run.h"

static const PolStep plan[] = {
    {"start", act1_start, nullptr},
    // Act 1: Economy (E1-E23).
    {"E01_hire_builder", act1_e01_hire_builder, nullptr},
    {"E02_hire_developer", act1_e02_hire_developer, nullptr},
    {"E03_tax", act1_e03_tax, nullptr},
    {"E04_satellite", act1_e04_satellite, nullptr},
    {"E05_mine", act1_e05_mine, act1_goals_e05},
    {"E06_derricks", act1_e06_derricks, act1_goals_e06},
    {"E07_satellites", act1_e07_satellites, act1_goals_s2},
    {"E08_deploy", act1_e08_deploy, nullptr},
    {"E09_vehicle_plant", act1_e09_vehicle_plant, act1_goals_e09},
    {"E10_carrier", act1_e10_carrier, nullptr},
    {"E11_miner_station", act1_e11_miner_station, nullptr},
    {"E12_university", act1_e12_university, act1_goals_e12},
    {"E13_transfer_ship", act1_e13_transfer_ship, nullptr},
    {"E14_pilot", act1_e14_pilot, act1_goals_e14},
    {"E15_station_on_apollo", act1_e15_station_on_apollo, nullptr},
    {"E16_survey", act1_e16_survey, act1_goals_e16},
    {"E17_control_centre", act1_e17_control_centre, nullptr},
    {"E18_apollo", act1_e18_apollo, act1_goals_e18},
    {"E19_mir", act1_e19_mir, act1_goals_e19},
    {"E20_penelope", act1_e20_penelope, act1_goals_e20},
    {"E21_new_earth_mines", act1_e21_new_earth_mines, act1_goals_e21},
    {"E22_jade", act1_e22_jade, nullptr},
    {"E23_standby", act1_e23_standby, nullptr},
    // Act 2: Jaanosians (J1-J13).
    {"J01_contact", act2_j01_contact, act2_goals_j01},
    {"J02_communicator", act2_j02_communicator, nullptr},
    {"J03_developer_home", act2_j03_developer_home, nullptr},
    {"J04_meeting1", act2_j04_meeting1, nullptr},
    {"J05_meeting2", act2_j05_meeting2, nullptr},
    {"J06_sos", act2_j06_sos, nullptr},
    {"J07_commander", act2_j07_commander, nullptr},
    {"J08_army", act2_j08_army, act2_goals_j08},
    {"J09_base_falls", act2_j09_base_falls, act2_goals_j09},
    {"J10_radio", act2_j10_radio, nullptr},
    {"J11_russel", act2_j11_russel, act2_goals_j11},
    {"J12_hyperspace", act2_j12_hyperspace, nullptr},
    {"J13_galleon", act2_j13_galleon, nullptr},
    // Act 3: Morgruls (M1-M18). M6 and M7 come before M5: the tank line
    // they open is the act's gate.
    {"M01_wave1", act3_m01_wave1, nullptr},
    {"M02_twin_lasers", act3_m02_twin_lasers, act3_goals_m02},
    {"M03_phoenix", act3_m03_phoenix, nullptr},
    {"M04_kalls", act3_m04_kalls, nullptr},
    {"M06_phelonians", act3_m06_phelonians, nullptr},
    {"M07_tank_deal", act3_m07_tank_deal, nullptr},
    {"M05_phoenix1", act3_m05_phoenix1, nullptr},
    {"M08_wave2_force", act3_m08_wave2_force, act3_goals_m08},
    {"M09_wave2", act3_m09_wave2, nullptr},
    {"M10_course", act3_m10_course, nullptr},
    {"M11_drifting_ship", act3_m11_drifting_ship, nullptr},
    {"M12_wave3", act3_m12_wave3, nullptr},
    {"M13_spy", act3_m13_spy, nullptr},
    {"M14_spy_satellite", act3_m14_spy_satellite, act3_goals_m14},
    {"M15_explorer", act3_m15_explorer, nullptr},
    {"M16_destroyers", act3_m16_destroyers, act3_goals_m16},
    {"M17_spy_on_mirach", act3_m17_spy_on_mirach, nullptr},
    {"M18_mirach", act3_m18_mirach, nullptr},
    // Act 4: the League (L1-L13).
    {"L01_pilot", act4_l01_pilot, act4_goals_l01},
    {"L02_developer", act4_l02_developer, nullptr},
    {"L03_informer", act4_l03_informer, nullptr},
    {"L04_league_tip", act4_l04_league_tip, act4_goals_l04},
    {"L05_antares", act4_l05_antares, act4_goals_l05},
    {"L06_erans", act4_l06_erans, nullptr},
    {"L07_home_guard", act4_l07_home_guard, nullptr},
    {"L08_spy_satellites", act4_l08_spy_satellites, nullptr},
    {"L09_league_contact", act4_l09_league_contact, nullptr},
    {"L10_eran_job", act4_l10_eran_job, nullptr},
    {"L11_aircraft", act4_l11_aircraft, nullptr},
    {"L12_lisonian_wave", act4_l12_lisonian_wave, nullptr},
    {"L13_league", act4_l13_league, nullptr},
    // Act 5: Earth (X1-X9). The ground win on Earth ends the game.
    {"X01_rigel1", act5_x01_rigel1, nullptr},
    {"X02_research", act5_x02_research, nullptr},
    {"X03_cruisers", act5_x03_cruisers, nullptr},
    {"X04_rigel2", act5_x04_rigel2, nullptr},
    {"X05_sun", act5_x05_sun, act5_goals_x05},
    {"X06_home_guard", act5_x06_home_guard, nullptr},
    {"X07_contact", act5_x07_contact, nullptr},
    {"X08_bounty_hunter", act5_x08_bounty_hunter, nullptr},
    {"X09_earth", act5_x09_earth, nullptr},
};

static void step(void)
{
    pol_run_step(plan, SDL_arraysize(plan));
}

int main(int argc, char *argv[])
{
    static PlayStep steps[SDL_arraysize(plan)];
    for (size_t i = 0; i < SDL_arraysize(plan); i++)
        steps[i] = (PlayStep){plan[i].name, step};
    return play_main(argc, argv,
                     &(PlayPlan){.name = "play_game", .steps = steps,
                                 .count = SDL_arraysize(steps), .expect_exit = 10});
}
