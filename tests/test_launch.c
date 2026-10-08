// launch: screen handlers with the toolbar/system-view routines stubbed.
#include <stdio.h>

#include "game/dseg.h"
#include "game/groups.h"
#include "game/launch.h"
#include "game/system.h"
#include "platform/fs.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

int main(int argc, char **argv)
{
    mem_init();
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data\n");
        return 77;
    }
    if (!dseg_init()) {
        printf("dseg_init failed\n");
        return 77;
    }
    system_init();
    ds.Game_Groups = far_ptr(0x3000, 0);
    ds.Game_CurSystem = 2;
    ds.Game_CurPlanet[1] = 0;

    // Clicking system 3 in the galaxy view selects it.
    ds.Game_ScreenEntered = 0;
    ds.Game_HotspotHit = 0x14 + 3;
    ds.Game_MenuCommand = 0;
    Launch_MapScreen();
    CHECK(ds.Game_CurSystem == 3 && ds.Game_HotspotHit == 0);

    // Menu 0x38 goes through Message for failures, which blocks; only the success path is run.
    ds.Game_SelGroup = 1;
    ds.Game_GroupList = 0;
    ds.Game_StaffExp[0] = 1;
    Groups_At(0, 1)->Kind = 1;
    ds.Game_MenuCommand = 0x38;
    // Non-empty docked-in-orbit group goes to the destination screen.
    Groups_At(0, 1)->Units[0][0].ShipType = 1;
    Groups_At(0, 1)->Status = 2;
    Launch_MapScreen();
    CHECK(ds.Game_ScreenSwitch && ds.Game_CurrentScreen == 0x12);

    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
