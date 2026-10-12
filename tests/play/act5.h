// Act 5 of the whole-game policy (docs/playthrough.md, "Act 5: Earth",
// steps X1-X9): from the League's surrender to the Earth ground
// win, with Rigel 1 and its research (Plasma gun, Missile launcher, Mental
// radar), the Syonians on Rigel 2, the Sun system and a spy satellite on
// Earth, the Cruisers and launchers (army.c), the contact and the invasion
// of Earth, which ends the game (Halt(10)). Each act5_xNN function is one
// step of the plan in tests/test_play_game.c; act5_goals_xNN registers the
// background goals the step starts (policy.h pol_goal).
#pragma once

void act5_x01_rigel1(void);
void act5_x02_research(void);
void act5_x03_cruisers(void);
void act5_x04_rigel2(void);
void act5_x05_sun(void);
void act5_x06_home_guard(void);
void act5_x07_contact(void);
void act5_x08_bounty_hunter(void);
void act5_x09_earth(void);

void act5_goals_x05(void);
