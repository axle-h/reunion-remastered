// Unit Title (321d): the start-up menu ("options" picture), the hero choice,
// the charset/cursor setup and the main-menu palette.
//
// The Ghidra decompile of the first two routines is cut short (it shows
// dead-end loops); the ports follow the disassembly.
#pragma once

#include <stdint.h>

// 321d:000d (HeroSelect): with the
// "choise" picture up, waits for a click; the left half picks hero 2, the
// right half hero 1. Plays the select jingles and fades to the hero picture.
void Title_HeroSelect(void);
// 321d:0155: shows grafika\options and waits for a click on one of its three
// buttons; DS:5DAA becomes 1 (new game), 2 (load) or 3 (quit).
void Title_StartMenu(void);
// 321d:026f
void Title_InitTitleAndMainMenu(void);
// 321d:0866
void Title_SetMainMenuPalette(void);
