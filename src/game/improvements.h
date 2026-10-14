// Changes to the game's design rather than its bugs (game/fixes.h). Each is
// a switch, on unless the port menu's config (platform/config.h) turns it
// off; the Fixes section's "All off (original)" turns them off too.
// Scripted runs (the autopilot, --replay, the tests) play with every
// improvement off, so they play the original's game; a test of one switches
// it on. An improvement keeps its state in fields the original already has
// where it can, so saves stay in the DOS format; trade routes have no room
// there, and a save holding one is in the port's format (game/saveload.h).
#pragma once

#include "game/fixes.h"

typedef struct Improvements {
    // A body with Miner stations and no colony takes up to MAX_STATIONS:
    // the body menu's Miner station action adds one more while a trade
    // group there carries one. Each mines as a droid does, every hour
    // instead of 30% of hours, and stores 10,000 t of each ore, also while
    // a colony bought there waits to be set up. The colony keeps them as
    // its first droids, as the original does with one.
    bool more_stations;
    // Shift+left click on the surface places a building and keeps the build
    // mode with the same kind, for the next one; a plain left click places
    // and ends it, as in the original. It ends too when the next one can't
    // be paid for or the building table is full.
    bool repeat_build;
    // A trade group can be given a route, up to 6 pickup stops and home, and
    // then hauls the ore home on its own (game/routes.h): ROUTE on its
    // control panel opens the editor.
    bool trade_routes;
} Improvements;

extern Improvements improvements;

// Every improvement. A new field of Improvements needs an entry here
// (test_options checks).
extern const SwitchInfo Improvements_List[];
extern const int Improvements_Count;

enum : int {
    MAX_STATIONS = 4,  // on one body (more_stations)
};

// Improvement i (of Improvements_List) keeps state the DOS save has no room
// for: while it is on, saves are in the port's format (game/saveload.h).
bool Improvements_NeedsPortSaves(int i);
// One of those is on.
bool Improvements_PortSaves(void);

// Each improvement as the config says (on by default), or, for a scripted
// run, every one off.
void Improvements_Init(bool scripted);
