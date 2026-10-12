// grwarunits: CollectAvailableForces on a synthetic group and the unit hotspots.
#include <stdio.h>
#include <string.h>

#include "game/dseg.h"
#include "game/grwarunits.h"
#include "game/groups.h"
#include "game/mouse.h"
#include "game/system.h"
#include "platform/fs.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

int main(int argc, char *argv[])
{
    dseg_clear();
    vid_clear();
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data\n");
        return 77;
    }
    dseg_init();
    system_init();
    ds.Game_Groups = game_alloc(2 * 32 * sizeof(TGroup));
    SDL_memset(ds.Game_Groups, 0, 2 * 32 * sizeof(TGroup));
    ds.Game_GroupCount = 1;
    ds.Game_PlanetForceCount = 0;
    ds.SpWar_BattleSystem = 3;
    ds.SpWar_BattlePlanet = 2;
    ds.SpWar_BattleMoon = 0;
    TGroup *g = Groups_At(0, 1);
    g->Kind = 1; g->System = 3; g->Planet = 2; g->Moon = 0; g->Status = 1;
    g->Units[1][0] = (TGroupSlot){.ShipType = 100, .Weapons = {10, 20, 30, 0}};
    TBody body = {0}; // owner 0: stores ignored
    ds.Game_CurBody = game_alloc(sizeof(TBody));
    memcpy(ds.Game_CurBody, &body, sizeof body);

    GrWarUnits_CollectAvailableForces();
    CHECK(ds.GrWarUnits_Totals[0][0] == 100);
    CHECK(ds.GrWarUnits_Attack[0][0] == 10 * 2 + 20 * 7);   // weapons 1 and 2 weighted 2 and 7
    CHECK(ds.GrWarUnits_Defence[0][0] == 30 * 9);           // weapon 3 weighted 9
    int cap = ds.GrWarUnits_SlotCap[0];                     // most units per slot of type 1
    int slots = (100 + cap - 1) / cap;
    CHECK(ds.GrWarUnits_AttackerSlotCount == slots);
    CHECK(ds.GrWarUnits_Pool[0] == 0);                      // all of it fits in slots
    CHECK(ds.GrWarUnits_DefenderSlotCount == 0);

    GrWarUnits_SetupUnitHotspots();
    CHECK(ds.Mouse_HotspotCount == 40);
    CHECK(Mouse_Hotspot(1)->X == 10 && Mouse_Hotspot(1)->Y == 0x38 && Mouse_Hotspot(1)->Repeat_ == 1);
    CHECK(Mouse_Hotspot(6)->X == 0x4D + 10 && Mouse_Hotspot(6)->Y == 0x38 + 0x17);
    CHECK(Mouse_Hotspot(21)->X == 0x33 && Mouse_Hotspot(21)->W == 0x1E && Mouse_Hotspot(21)->Repeat_ == 0);
    CHECK(Mouse_Hotspot(1)->Caption.len == 11);
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
