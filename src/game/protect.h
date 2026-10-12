// What is left of the copy protection: the random numbers it drew.
//
// The original has two schemes: "the word at the bottom of page N" of the
// manual (unit Protect, 24eb, with checks in Credits, AlienTalk, PlanetInfo
// and Colonize) and "type the date" from TEXT\VALAMI.TXT (unit Protect2,
// 2693). The port has neither: no questions, no checks, no penalties, none
// of their variables. But both draw from Random, so these routines make
// the draws the original makes for a player who answers correctly, at the
// same moments, as the port did while it still answered for the player:
// the game's random numbers stay what they were. For "the date" that is
// the original's path for a player who types the right date. For "page N"
// the shipped game differs: its prompt is dead code, so it never gets an
// answer and takes the "unanswered" branch instead (it scribbles over
// memory once), so once "page N" arms (FrameCount > 30000, a 1-in-200 draw
// per frame) the port's random numbers are no longer the shipped game's.
// Each routine says which of the original's it stands for.
#pragma once

// At boot: neither scheme armed (the original's initial values).
void Protect_Reset(void);

// "Page N": 357b:3d77 PlanetInfo::RandomizeProtectPage, which the main loop
// calls on a 1-in-200 Random draw once the game has run a while; until the
// first check it picks the page (Random(20)).
void Protect_ArmManualCheck(void);
// "Page N": 264a:034a Credits::EnforceManualCheck and 2e4b:09d3
// AlienTalk::VerifyManualAnswer, each once per game hour. Once armed, a
// correct answer refreshes the lives counter with Random(7).
void Protect_ManualCheckHour(void);

// "The date": 2693:0000 ArmDateQuestion (a group found new planets in
// system 5): picks the question (Random(14)) while unanswered.
void Protect_ArmDateQuestion(void);
// "The date": 2693:0085 AskDateQuestion (a building was destroyed): the
// question is asked and answered; no draw.
void Protect_AskDateQuestion(void);
// "The date": 2693:0342 EnforceDateCheck (the colony step): asks if not
// yet asked, then Random(7) for the lives counter.
void Protect_DateColonyStep(void);
// "The date": 2693:04aa VerifyDateAnswer (the daily step): Random(7) once
// answered.
void Protect_DateDailyCheck(void);
