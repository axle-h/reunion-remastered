// The port's settings (port menu, F1): reunion.ini in the player's folder
// (platform/playerdir.h), read at start-up and rewritten on every change. Nothing is
// read or written until config_set_path, so tests and scripted runs keep the
// built-in defaults, which are the behaviour without a config file.
#pragma once

#include <stdint.h>

typedef enum : int {
    CONFIG_MUSIC_SETUP,  // as SAVE\SETUP.SPD says (the default without a file)
    CONFIG_MUSIC_NONE,
    CONFIG_MUSIC_SOUND_BLASTER,  // the AdLib (FM) tunes; cut-scenes play the modules
    CONFIG_MUSIC_GUS,    // Gravis UltraSound: the ProTracker modules
} ConfigMusic;

typedef enum : int {
    CONFIG_WINDOWED,
    CONFIG_FULLSCREEN,  // exclusive, at fullscreen_width x _height
    CONFIG_DESKTOP,     // borderless at the desktop resolution
} ConfigDisplay;

enum : int {
    CONFIG_MAX_FIXES = 32,
    CONFIG_KEY_SIZE = 40,
};

typedef struct Config {
    // [cheats]
    bool freeze_money;
    bool freeze_ore;
    // [sound]
    ConfigMusic music;
    bool effects;
    int music_volume;    // percent
    int effects_volume;  // percent
    // [video]
    ConfigDisplay display;
    int window_width, window_height;
    int fullscreen_width, fullscreen_height;  // 0: the desktop's mode
    float fullscreen_refresh;                 // Hz, 0: any
    // [game]
    bool skip_intro;
    bool colony_stats;  // planet info: the POP row opens the colony's stats
    bool detoxin_rate;  // droids screen and Derrick info: the Derricks' Detoxin rate
    // [fixes]: switched off ones; a fix not listed is on.
    int fix_count;
    char fix_keys[CONFIG_MAX_FIXES][CONFIG_KEY_SIZE];
    bool fix_on[CONFIG_MAX_FIXES];
} Config;

extern Config config;

// Sets the file and loads it (a missing or unreadable file leaves the
// defaults). From then on config_save writes there. Settings that only the
// file can change from SETUP.SPD's choice (the music) get the app's default.
void config_load(const char *path);
// The default path: reunion.ini in the player's folder.
// Free with SDL_free; nullptr if SDL has no pref path.
char *config_default_path(void);
// Writes the file if config_load set one. False on a write error.
bool config_save(void);
// Back to the built-in defaults; forgets the path (tests).
void config_reset(void);

// A fix's switch: on unless the file says off.
bool config_fix(const char *key);
void config_set_fix(const char *key, bool on);

// Names as written in the file.
const char *config_music_name(ConfigMusic music);
