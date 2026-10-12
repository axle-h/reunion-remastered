// groups: planet-force lookup, group clearing/initialisation, slot category cycling and the
// emptiness tests, on synthetic groups in the Groups array.
#include <stdio.h>
#include <string.h>

#include "game/dseg.h"
#include "game/groups.h"
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
    ds.Game_Groups = System_GetMem(2 * 32 * sizeof(TGroup));
    System_FillChar(ds.Game_Groups, 2 * 32 * sizeof(TGroup), 0);

    // Addressing: list 1 starts 32 groups after list 0, groups are 1-based.
    CHECK((uint8_t *)Groups_At(1, 1) - (uint8_t *)Groups_At(0, 1) == 32 * 161);
    CHECK((uint8_t *)Groups_At(0, 2) - (uint8_t *)Groups_At(0, 1) == 161);

    // FindPlanetForce: the last matching planet force wins; 0 when none.
    ds.Game_PlanetForceCount = 3;
    for (int i = 1; i <= 3; i++) {
        TGroup *g = Groups_At(1, i);
        g->System = (uint8_t)(i == 3 ? 2 : 1);
        g->Planet = (uint8_t)(i == 2 ? 7 : 5);
        g->Moon = 0;
    }
    CHECK(Groups_FindPlanetForce(1, 5, 0) == 1);
    CHECK(Groups_FindPlanetForce(1, 7, 0) == 2);
    CHECK(Groups_FindPlanetForce(2, 5, 0) == 3);
    CHECK(Groups_FindPlanetForce(2, 5, 1) == 0);
    Groups_At(1, 3)->System = 1;
    CHECK(Groups_FindPlanetForce(1, 5, 0) == 3);
    ds.Game_PlanetForceCount = 2;  // only the counted groups are searched
    CHECK(Groups_FindPlanetForce(1, 5, 0) == 1);

    // InitPlanetForce: one planet force of Kind 5 in the dock of 1:5:0, status 7.
    Groups_InitPlanetForce();
    TGroup *pf = Groups_At(1, 1);
    CHECK(ds.Game_PlanetForceCount == 1 && ds.Game_GroupCount == 0 && ds.Game_GroupList == 1);
    CHECK(pf->Kind == 5 && pf->Status == 7 && pf->System == 1 && pf->Planet == 5 && pf->Moon == 0);
    // Named from DefaultNames ("New Earth forces"), not TypeNames (port bug 13).
    CHECK(pf->Name.len == ds.Groups_DefaultNames[4].len
          && memcmp(pf->Name.s, ds.Groups_DefaultNames[4].s, pf->Name.len) == 0);

    // InitGroupOfKind on a fleet: the kind follows the slot number, 3 becomes 4.
    ds.Game_GroupList = 0;
    static const uint8_t kinds[] = {1, 2, 4, 4, 1};
    for (int i = 1; i <= 5; i++) {
        ds.Game_SelGroup = (int16_t)i;
        Groups_Selected()->Units[0][0].ShipType = 9;  // cleared again by InitGroupOfKind
        Groups_InitGroupOfKind();
        CHECK(Groups_Selected()->Kind == kinds[i - 1]);
        CHECK(Groups_Selected()->Units[0][0].ShipType == 0 && Groups_Selected()->Status == 1);
    }

    // ClearGroup
    TGroup *g = Groups_Selected();
    g->Kind = 3; g->Cargo[2].B = 4; g->Items[12] = 7; g->Leg1 = 5; g->Storage = 3;
    Groups_ClearGroup();
    CHECK(g->Kind == 1 && g->Name.len == 9 && memcmp(g->Name.s, "New Group", 9) == 0);
    CHECK(g->Cargo[2].B == 0 && g->Items[12] == 0 && g->Leg1 == 0 && g->Storage == 3);

    // Slot categories cycle through the ones the kind uses.
    g->Kind = 1;
    int used = 0;
    for (int c = 1; c <= 4; c++)
        used += Groups_SlotUsed(1, c);
    CHECK(used > 0);
    Groups_FirstSlotCategory();
    CHECK(Groups_SlotUsed(1, ds.Groups_SlotCategory));
    int16_t first = ds.Groups_SlotCategory;
    for (int n = 0; n < used; n++)
        Groups_NextSlotCategory();
    CHECK(ds.Groups_SlotCategory == first);
    Groups_NextSlotCategory();
    Groups_PrevSlotCategory();
    CHECK(ds.Groups_SlotCategory == first);

    // Emptiness: only the two ship categories of the kind count.
    g->Kind = 1;
    CHECK(Groups_IsGroupEmpty(0, ds.Game_SelGroup));
    g->Units[0][2].ShipType = 1;
    CHECK(!Groups_IsGroupEmpty(0, ds.Game_SelGroup));
    g->Units[0][2].ShipType = 0;
    CHECK(Groups_IsGroupEmptyOfCargo(0, ds.Game_SelGroup));

    Groups_Init();
    CHECK(ds.Game_FindPlanetForceProc == Groups_FindPlanetForce);

    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
