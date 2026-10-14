// Improvement (game/improvements.h, autosave): one rolling autosave,
// save/reunion.auto in the port's format (game/saveload.h), outside the
// player's 12 slots. The disk screen lists it as a 13th row that only loads.
//
// - At the start of every game hour, before Sim_AdvanceHour, and after a new
//   game or a load, the game is copied to memory as a save. The copy alone
//   is never written.
// - A battle or an alien talk starting (the switch to unit assignment 19,
//   space battle 29, ground battle 32 or alien talk 34) writes the hour's
//   copy, once per copy. It is the hour's start, not the screen's state:
//   the triggers clear their own flags in the hour they fire, so the file
//   loads into the same event again. The story's scenes (33) don't count.
// - The seventh midnight since the file was last written, by any of these,
//   writes that hour's copy.
// - The quick-save key (game/keyplay.h, quick_save) writes the game as it
//   is, while the main loop waits for a click with the clock running; it is
//   refused elsewhere. The status line says which.
//
// The file's slot name is "Autosave" and the date of the state it holds.
#pragma once

#include <stdint.h>

// A new game or a load: the copy is taken now and the week starts again.
void Autosave_Reset(void);
// The start of a game hour (before Sim_AdvanceHour).
void Autosave_HourStart(void);
// The main loop enters `screen`.
void Autosave_ScreenEntered(int16_t screen);
// The quick-save key, pressed while the main loop waits for a click
// (`waiting`) or not.
void Autosave_QuickSaveKey(bool waiting);
// Each frame the main loop waits for a click: a quick save asked for.
void Autosave_Frame(void);
