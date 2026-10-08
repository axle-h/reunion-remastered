// Unit Crt (4857): only what the game calls. Text-mode screen routines are
// not used outside the unit's own startup/error output and are not ported.
#pragma once

#include <stdint.h>

// 4857:0308
bool Crt_KeyPressed(void);
// 4857:031a: waits for a key (one retrace at a time, so events keep running).
// Extended keys return 0, then the scan code on the next call. Returns 0 if
// the user asked to quit while waiting.
char Crt_ReadKey(void);
// Delay(ms), done as retrace waits (1000/70.086 ms each, rounded up).
void Crt_Delay(uint16_t ms);
