// The cut-scene programs CREDITS.PRG, INTRO.PRG and VICTORY.PRG, which
// start.exe ran around the game:
//
//   Exec('grwar\credits.prg'); Exec('grwar\intro.prg');
//   repeat Exec('grwar\reunion.prg', 'sxca') until DosExitCode <> 15;
//   if DosExitCode = 10 then Exec('grwar\victory.prg');
//
// The cut-scenes' own exit codes are ignored. src/main.c does the same.
#pragma once

typedef enum : int {
    CUTSCENE_CREDITS,
    CUTSCENE_INTRO,
    CUTSCENE_VICTORY,
} Cutscene;

// Runs one program from start to its Halt, in VGA mode 13h/12h on the port's
// video layer, with its own music. Opens the audio device for the program and
// closes it again (the game's Sound_Init opens its own). Leaves mode 13h with
// a black screen. Needs fs_init. Returns false if the user closed the window
// (the caller should quit), true otherwise (normal end, ESC, or missing data).
bool cutscene_play(Cutscene which);

// The port menu changed the music: a running cut-scene plays its modules or
// plays them silently (they have no AdLib music). Returns false if no
// cut-scene is running.
bool cutscene_music_changed(void);

// "credits", "intro" or "victory" (any case). Returns false if unknown.
bool cutscene_from_name(const char *name, Cutscene *out);
