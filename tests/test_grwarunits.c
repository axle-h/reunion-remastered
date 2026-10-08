// grwarunits: CollectAvailableForces on a synthetic group and the unit hotspots.
#include <stdio.h>
#include <string.h>

#include "game/dseg.h"
#include "game/grwarunits.h"
#include "game/groups.h"
#include "game/mouse.h"
#include "game/system.h"
#include "platform/fs.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)
#define L(off) (*(int32_t *)ds_at(off))

int main(int argc, char *argv[])
{
    mem_init();
    if (argc < 2 || !fs_init(argv[1]) || !dseg_init()) {
        printf("no game data\n");
        return 77;
    }
    system_init();
    ds.Game_Groups = System_GetMem(2 * 32 * sizeof(TGroup));
    System_FillChar(far_host(ds.Game_Groups), 2 * 32 * sizeof(TGroup), 0);
    ds.Game_GroupCount = 1;
    ds.Game_PlanetForceCount = 0;
    *(int16_t *)ds_at(0x7798) = 3;
    *(int16_t *)ds_at(0x779A) = 2;
    *(int16_t *)ds_at(0x779C) = 0;
    TGroup *g = Groups_At(0, 1);
    g->Kind = 1; g->System = 3; g->Planet = 2; g->Moon = 0; g->Status = 1;
    g->Units[1][0] = (TGroupSlot){.ShipType = 100, .Weapons = {10, 20, 30, 0}};
    TBody body = {0}; // owner 0: stores ignored
    ds.Game_CurBody = System_GetMem(sizeof(TBody));
    memcpy(far_host(ds.Game_CurBody), &body, sizeof body);

    GrWarUnits_CollectAvailableForces();
    CHECK(L(0x765A) == 100);
    CHECK(L(0x768A) == 10 * 2 + 20 * 7);   // attack: weapons 1 and 2 weighted 2 and 7
    CHECK(L(0x76AA) == 30 * 9);            // defence: weapon 3 weighted 9
    int cap = *(int16_t *)ds_at(0x32);     // most units per slot of type 1
    int slots = (100 + cap - 1) / cap;
    CHECK(ds.GrWarUnits_AttackerSlotCount == slots);
    CHECK(L(0x767A) == 0);                 // all of it fits in slots
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
