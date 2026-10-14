// The port's own toolbar commands and screens (game/improvements.h), past
// the original's. Its tables are DS arrays sized for commands 0..77
// (Ui_ButtonLabels, GfxPath_CommandIcon, Ui_ButtonSounds) and screens 1..38
// (Ui_Screens); GfxPath and Main look a command or a screen up here, which
// answers from those tables up to there and from the port's beyond.
//
// The port's icons are made at run time from icons of the player's
// ICON.ALL, so no pixels of the original are compiled in: ADD STOP and
// REMOVE STOP are PLANET INFO with its "?" erased and a + in the "?"'s
// greens or a - in END TALK's reds; ROUTE is PLANET INFO's planet and ADD
// MINER STATION's small planet on a loop in those greens. START ROUTE and
// END ROUTE use OK and ABORT, as OK, BUILD IT and ABORT MOVE do.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "game/dseg.h"

enum : int16_t {
    CMD_ORIGINAL_COUNT = 78,  // the original's commands 0..77
    CMD_ROUTE = CMD_ORIGINAL_COUNT,
    CMD_ADD_STOP,
    CMD_REMOVE_STOP,
    CMD_START_ROUTE,
    CMD_END_ROUTE,
    CMD_COUNT,
};

enum : int16_t {
    SCREEN_ORIGINAL_COUNT = 38,  // the original's screens 1..38
    SCREEN_ROUTE = 39,           // game/routes.h
    SCREEN_COUNT = 39,
};

// Ui::Screens[screen], or the port's record.
const TScreenInfo *PortCmd_Screen(int16_t screen);
// The toolbar caption of command `id` (Ui::ButtonLabels[id] or the port's,
// in the game's language).
void PortCmd_Label(int16_t id, char *out, size_t size);
// The sample of command `id` (Ui::ButtonSounds[id] or the port's).
void PortCmd_Sound(int16_t id, char *out, size_t size);
// Draws command `id`'s icon into a toolbar cell as GfxPath_DrawIcon does
// (the cell's 40x24 icon at cell + 504h, rows 320 apart), using
// GfxPath_IconScratch.
void PortCmd_DrawIcon(int16_t id, uint8_t *cell);
