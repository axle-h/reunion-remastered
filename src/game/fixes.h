// Fixes to the original game (docs/bugs.md, "Fixed in the port"). Each fix is
// a switch so the original behaviour stays reproducible: all are on in the
// game unless REUNION_ORIGINAL=1, and the differential tests turn them off.
#pragma once

typedef struct Fixes {
    // Jade is otherwise found only by New Earth's observatory (1 in 50 a
    // day), so some games never find it before the Jaanosian deadline and
    // can't be won. If it is still hidden on JADE_FOUND_BY, it is found then.
    bool find_jade;
} Fixes;

extern Fixes fixes;

enum : int {
    // 2928-04-01 00:00, game hour 5,449: 551 hours (23 days) before the
    // earliest Jaanosian deadline (6,000 h after the start, 2928-04-23 23:00).
    // A flight to Jade takes at most 19 h with the slowest pilot, and
    // everything after first contact is timed from the contact.
    JADE_FOUND_BY_YEAR = 2928,
    JADE_FOUND_BY_MONTH = 4,
    JADE_FOUND_BY_DAY = 1,
};

// All fixes on, or all off if the environment says REUNION_ORIGINAL=1.
void Fixes_Init(void);
// All fixes off (original behaviour).
void Fixes_Original(void);
