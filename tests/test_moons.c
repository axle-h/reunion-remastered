// moons: which planets' moons the system view shows (port bugs 3 and 4).
// The original gates moons on Galaxy::PlanetStatus (DS:47CD + 8s + p), which
// Ghidra prints as "BodyIcon[6] + 7 + ..."; the port read Galaxy::BodyIcon
// instead, so New Earth's moon never showed and Zeus's five showed at once.
#include <stdio.h>

#include "game/dseg.h"
#include "game/ems.h"
#include "game/gfx.h"
#include "game/groups.h"
#include "game/mem.h"
#include "game/message.h"
#include "game/mouse.h"
#include "game/planetinfo.h"
#include "game/sim.h"
#include "game/system.h"
#include "game/textfile.h"
#include "platform/fs.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static int messages;
static bool wait_retrace(void) { return true; }
// Message boxes wait for a key: answer at once.
static bool key_pressed(void) { messages++; return true; }
static char read_key(void) { return '\r'; }

// Hotspots 14h.. are the moons (or, with planet 0, the planets), so the count
// is 13h + the visible moons.
static int16_t moon_hotspots(int16_t sys, int16_t planet)
{
    ds.Game_CurSystem = sys;
    ds.Game_CurPlanet[sys - 1] = planet;
    ds.Game_CurMoon[sys - 1] = 0;
    PlanetInfo_SetupSystemViewHotspots();
    return (int16_t)(ds.Mouse_HotspotCount - 0x13);
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    mem_init();
    CHECK(dseg_init());
    system_init();
    Ems_Init();
    Message_Init();
    ds.Gfx_LineStride = 320;
    ds.Gfx_LineStrideCopy = 320;
    gfx_wait_retrace = wait_retrace;
    TextFile_KeyPressed = key_pressed;
    TextFile_ReadKey = read_key;

    // New game, Amnesty system map: Klatoo, Zeus and New Earth are known
    // (status >= 0, as the manual describes them); the other four are not.
    ds.Game_CurrentScreen = 7;
    CHECK(moon_hotspots(1, 0) == 7);
    for (int16_t k = 1; k <= 7; k++) {
        bool known = k == 2 || k == 4 || k == 5;
        CHECK((Mouse_Hotspot((int16_t)(k + 0x13))->W != 0) == known);
    }

    // Only New Earth (planet 5) has its moons explored.
    CHECK(ds.Galaxy_PlanetStatus[0][4] == 1);
    CHECK(ds.Galaxy_PlanetStatus[0][3] == 0);
    CHECK(moon_hotspots(1, 5) == 1);   // Apollo
    CHECK(moon_hotspots(1, 4) == 0);   // Zeus: 5 moons, none known yet
    CHECK(moon_hotspots(1, 2) == 0);   // Klatoo
    CHECK(moon_hotspots(1, 7) == 0);   // Jade

    // A satellite carrier reaching Zeus explores its moons (1b8a:2e4e).
    ds.Game_Groups = System_GetMem(2 * 32 * sizeof(TGroup));
    System_FillChar(ds.Game_Groups, 2 * 32 * sizeof(TGroup), 0);
    ds.Game_GroupCount = 1;
    TGroup *g = Groups_At(0, 1);
    g->Kind = 4;
    g->System = 1;
    g->Planet = 4;
    g->Status = 6;
    g->Leg2 = 1;
    Sim_ProcessTravellingGroups();
    CHECK(g->Status == 2 && g->Leg2 == 0);
    CHECK(ds.Galaxy_PlanetStatus[0][3] == 1);
    CHECK(messages > 0);
    CHECK(moon_hotspots(1, 4) == 5);

    fs_shutdown();
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
