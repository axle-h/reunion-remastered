// Unit Launch (18e0): the galaxy/system map screen (7) and the destination
// chooser (0x12) used to send the selected group somewhere.
#pragma once

#include <stdint.h>

#include "game/dseg.h"

// 18e0:00e8: screen 7 handler
void Launch_MapScreen(void);
// 18e0:07ec: screen 0x12 handler
void Launch_DestinationScreen(void);
