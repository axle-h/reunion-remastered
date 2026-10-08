// The cut-scene programs CREDITS.PRG, INTRO.PRG and VICTORY.PRG, which
// start.exe ran around the game (docs/cutscenes.md): credits and intro before
// REUNION.PRG, victory when it exits with code 10.
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

// "credits", "intro" or "victory" (any case). Returns false if unknown.
bool cutscene_from_name(const char *name, Cutscene *out);
