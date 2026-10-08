// Unit Credits (264a): the credits screen (CurrentScreen 10) and the scheme A
// verifier (see protect.h).
#pragma once

#include <stdint.h>

// 264a:01b8
void Credits_ShowCredits(void);
// 264a:034a: called hourly. For an armed, answered page check it sets
// Key1 := Random(7) + 479h.
void Credits_EnforceManualCheck(void);
