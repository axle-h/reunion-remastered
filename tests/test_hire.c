// hire: which candidate can be hired (rank and price checks of the hire button).
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/dseg.h"
#include "game/hire.h"
#include "platform/fs.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    mem_init();
    CHECK(dseg_init());

    // Pilots cost 4500 / 25000 / 120000.
    CHECK(ds.Staff_Price[0][0] == 4500 && ds.Staff_Price[0][1] == 25000 && ds.Staff_Price[0][2] == 120000);

    ds.Game_Credits = 30000;
    ds.Game_StaffHired[0] = 0;
    CHECK(Hire_CheckHire(1, 1) == HIRE_OK);
    CHECK(Hire_CheckHire(1, 2) == HIRE_OK);
    CHECK(Hire_CheckHire(1, 3) == HIRE_NO_CREDITS);
    ds.Game_Credits = 25000;  // exactly the price is enough
    CHECK(Hire_CheckHire(1, 2) == HIRE_OK);
    ds.Game_Credits = 24999;
    CHECK(Hire_CheckHire(1, 2) == HIRE_NO_CREDITS);

    // With candidate 2 hired: 2 is "still working", 1 is worse, 3 is an upgrade.
    ds.Game_Credits = 500000;
    ds.Game_StaffHired[0] = 2;
    CHECK(Hire_CheckHire(1, 2) == HIRE_ALREADY_HIRED);
    CHECK(Hire_CheckHire(1, 1) == HIRE_NOT_SKILLED);
    CHECK(Hire_CheckHire(1, 3) == HIRE_OK);
    // Other kinds are independent.
    CHECK(Hire_CheckHire(2, 1) == HIRE_OK);

    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
