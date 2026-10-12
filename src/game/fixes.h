// Fixes to the original game's bugs (the open ones are in PLAN.md). Each fix is
// a switch so the original behaviour stays reproducible: all are on unless
// the port menu's config (platform/config.h) turns one off.
#pragma once

typedef struct Fixes {
    // Jade is otherwise found only by New Earth's observatory (1 in 50 a
    // day), so some games never find it before the Jaanosian deadline and
    // can't be won. If it is still hidden on JADE_FOUND_BY, it is found then.
    bool find_jade;
    // Mining reads every body from the selected system's table (the one the
    // map shows), so only that system's colonies and droids mine, some of
    // them more than once a turn. The fix reads each system's own table.
    bool mine_all_systems;
    // A meteorite that destroys a building lowers the droids of the selected
    // body, not of the colony it hit. The fix lowers the hit colony's.
    bool meteorite_droids;
    // Construction speed reads the body's kind-16h output from the planet
    // instead of the moon a building is on whenever that system's selected
    // body is a planet. The fix always reads the building's own body.
    bool build_on_moons;
    // A Derrick on a moon puts its ore into the planet's store whenever the
    // selected system's selected body is a planet. The fix uses the moon's.
    bool derricks_on_moons;
    // The drifting Morgrul ship is found when any group arrives anywhere
    // while the map shows Phoenix 1. The fix needs a group arriving there.
    bool drifting_ship;
    // Disbanding a group shifts the list up to the count of the list the
    // ship info screen last showed, so a disband from the simulation (revolt,
    // lost battle, story) can wipe or duplicate the other groups of its list.
    // The fix shifts up to the disbanded list's own count.
    bool disband_list_count;
    // System 4's annihilation (a story timer) deletes buildings and groups
    // while a screen may still hold their indices or the selected group.
    // The fix leaves those screens first, as a destroyed colony does.
    bool annihilate_leaves_screen;
    // A building the simulation places (a new colony's command centre and
    // starter buildings) is dropped if the player is watching that colony's
    // surface and no spot is free. The fix adds it unplaced, as elsewhere.
    bool autoplace_when_full;
    // The command centre's door and window animations redraw the builder
    // and the kind-3 staff member even while they are at the university.
    // The fix leaves them off, as the rest of the room does.
    bool away_staff_hidden;
    // Loading ore onto a ship caps the ship's load of that ore at the
    // colony's storage space, even when the ship has more room. The fix
    // caps it at the ship's room only.
    bool cargo_planet_cap;
    // A body with only a Miner station has no planet force, and its storage
    // is read from Groups^[1][0], which is the fleet's group 32. A power
    // balance of any body without a planet force overwrites it (a conquest
    // sets it to 0), and then no such body mines. The fix gives each one
    // a Miner station's 10,000 t.
    bool station_storage;
    // After a space battle each body's garrison counts its survivors by
    // id 200 + moon, but every garrison ship was given id C9h: the planet
    // and moons 2.. keep a fifth whatever survived, moon 1 gains the others'
    // survivors. The losses are booked on the side of the last race force
    // handled, or nowhere. The fix gives each body's ships its own id and
    // books the losses on the side of the body's owner.
    bool garrison_losses;
    // When a new colony's starter buildings are all placed, its entry is
    // removed and the next one moves into its slot, which the loop then
    // skips for that hour. The fix counts that one down in the same hour.
    bool colony_setups;
    // Demolishing a Mine or Miner station leaves its droid working. The
    // fix takes a droid that has lost its place off the body, back into the
    // colony's stock, as a meteorite's hit takes one.
    bool demolish_droids;
    // With 1,000 buildings in the game a new one is refused with a sound,
    // but still paid for (and a Building still adds its people). The fix
    // refuses it before anything is paid.
    bool building_limit;
    // Spy reports on a race's armies, garrisons and weapons build the line
    // "Information on: <race>" and then overwrite it, so they never say
    // which race they are about. The fix keeps it (a garrison report then
    // has 7 lines, and message boxes take up to 7).
    bool spy_reports;
} Fixes;

extern Fixes fixes;

// Every fix, for the port menu and the config file ([fixes] key = on|off).
// A new field of Fixes needs an entry here (test_options checks).
typedef struct FixInfo {
    const char *key;    // the field's name
    const char *label;  // at most 20 characters
    const char *help;   // at most 72 characters
    bool *value;
} FixInfo;

extern const FixInfo Fixes_List[];
extern const int Fixes_Count;

enum : int {
    // 2928-04-01 00:00, game hour 5,449: 551 hours (23 days) before the
    // earliest Jaanosian deadline (6,000 h after the start, 2928-04-23 23:00).
    // A flight to Jade takes at most 19 h with the slowest pilot, and
    // everything after first contact is timed from the contact.
    JADE_FOUND_BY_YEAR = 2928,
    JADE_FOUND_BY_MONTH = 4,
    JADE_FOUND_BY_DAY = 1,
};

// Each fix as the config says (on by default).
void Fixes_Init(void);
