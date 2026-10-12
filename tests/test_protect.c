// protect: the copy protection is gone, but its random draws stay, at the
// moments the original made them for a player who answers correctly.
#include <stdio.h>

#include "game/dseg.h"
#include "game/protect.h"
#include "game/system.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

// How many Random draws fn made (every draw is one step of the generator).
static int draws(void (*fn)(void))
{
    ds.System_RandSeed = 0x1234567;
    fn();
    int32_t after = ds.System_RandSeed;
    ds.System_RandSeed = 0x1234567;
    int n = 0;
    while (ds.System_RandSeed != after && n < 10) {
        System_Random(2);
        n++;
    }
    return n;
}

int main(void)
{
    dseg_init();

    // "Page N": nothing before it is armed; picking the page draws until the
    // first hourly check has answered it, then every check draws.
    Protect_Reset();
    CHECK(draws(Protect_ManualCheckHour) == 0);
    CHECK(draws(Protect_ArmManualCheck) == 1);
    CHECK(draws(Protect_ArmManualCheck) == 1);  // picked again
    CHECK(draws(Protect_ManualCheckHour) == 1);
    CHECK(draws(Protect_ManualCheckHour) == 1);
    CHECK(draws(Protect_ArmManualCheck) == 0);

    // "The date": the same, asked when a building is destroyed or at the
    // colony step; the daily check draws only once answered.
    CHECK(draws(Protect_DateDailyCheck) == 0);
    CHECK(draws(Protect_DateColonyStep) == 0);
    CHECK(draws(Protect_AskDateQuestion) == 0);
    CHECK(draws(Protect_ArmDateQuestion) == 1);
    CHECK(draws(Protect_DateDailyCheck) == 0);
    CHECK(draws(Protect_ArmDateQuestion) == 1);  // a new question
    CHECK(draws(Protect_AskDateQuestion) == 0);
    CHECK(draws(Protect_ArmDateQuestion) == 0);
    CHECK(draws(Protect_DateDailyCheck) == 1);
    CHECK(draws(Protect_DateColonyStep) == 1);

    // Answered at the colony step instead.
    Protect_Reset();
    CHECK(draws(Protect_ArmDateQuestion) == 1);
    CHECK(draws(Protect_DateColonyStep) == 1);
    CHECK(draws(Protect_DateDailyCheck) == 1);

    // A boot starts over.
    Protect_Reset();
    CHECK(draws(Protect_ManualCheckHour) == 0 && draws(Protect_DateDailyCheck) == 0);

    printf(ok ? "protect OK\n" : "protect FAILED\n");
    return ok ? 0 : 1;
}
