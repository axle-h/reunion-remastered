// Unit Protect (24eb): copy protection scheme A, "the word at the bottom of
// page N of the manual". The port removes the player-facing check: it always
// passes, and the state a passed check leaves behind (Key1 / Key2 / PageIndex)
// is kept so the rest of the game, which still compares those values, sees a
// player who answered correctly. See docs/protection.md.
#pragma once

#include <stdint.h>

#include "game/dseg.h"

// PageIndex value meaning "scheme A not armed".
enum : int32_t { PROTECT_PAGE_UNARMED = 0x62EC };
// Key2 value meaning "not answered" (hash 0).
enum : int32_t { PROTECT_KEY2_UNANSWERED = 0x1F5F };

// 24eb:00a5: in the shipped EXE the prompt body is dead code (NOPs and a JMP
// to the epilogue), so the original does nothing; so does the port.
void Protect_CheckToken(void);

// Not in the original. What 264a:034a and 2e4b:09d3 would see after the
// player typed the right word for page PageIndex: Key2 := PageCodes[PageIndex].
// Does nothing while PageIndex says "not armed" or when Key2 already holds an
// answer.
void Protect_PassManualCheck(void);
