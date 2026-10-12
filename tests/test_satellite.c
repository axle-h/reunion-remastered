// satellite: carrier/trade-ship counting and deduction on synthetic groups, and the
// stock satellite (menu 0x46) taken from Products[3] = Satellite (port bug 2).
#include <stdio.h>

#include "game/dseg.h"
#include "game/groups.h"
#include "game/satellite.h"
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
    ds.Game_GroupCount = 3;
    ds.Game_CurrentScreen = 8;

    TGroup *a = Groups_At(0, 1), *b = Groups_At(0, 2), *c = Groups_At(0, 3);
    a->Kind = 4; a->System = 2; a->Planet = 3; a->Status = 1; a->Units[0][0].Weapons[0] = 5;
    b->Kind = 4; b->System = 2; b->Planet = 3; b->Status = 2; b->Units[0][0].Weapons[0] = 4;
    c->Kind = 4; c->System = 2; c->Planet = 3; c->Status = 4; c->Units[0][0].Weapons[0] = 100;
    CHECK(Satellite_CountSatellites(2, 3, 0, 1) == 9);
    CHECK(Satellite_CountSatellites(2, 3, 0, 2) == 0);
    CHECK(Satellite_CountSatellites(2, 4, 0, 1) == 0);

    // Deducting takes from the groups in order and leaves the rest.
    Satellite_DeductSatellites(2, 3, 0, 1, 7);
    CHECK(a->Units[0][0].Weapons[0] == 0 && b->Units[0][0].Weapons[0] == 2);
    CHECK(c->Units[0][0].Weapons[0] == 100);
    // On screen 0x11 the selected group (SelGroup0) goes first.
    ds.Game_CurrentScreen = 0x11;
    ds.Game_SelGroup0 = 2;
    a->Units[0][0].Weapons[0] = 3;
    Satellite_DeductSatellites(2, 3, 0, 1, 1);
    CHECK(b->Units[0][0].Weapons[0] == 1 && a->Units[0][0].Weapons[0] == 3);
    ds.Game_CurrentScreen = 8;

    // Trade ships: Weapons[4] of the four slots of trade groups (Kind 2).
    a->Kind = 2; b->Kind = 2;
    for (int k = 0; k < 4; k++) {
        a->Units[0][k].Weapons[3] = 1;
        b->Units[0][k].Weapons[3] = 2;
    }
    CHECK(Satellite_CountTradeShips(2, 3, 0) == 12);
    Satellite_TakeTradeShips(2, 3, 0, 6);
    CHECK(Satellite_CountTradeShips(2, 3, 0) == 6);
    CHECK(a->Units[0][0].Weapons[3] == 0 && a->Units[0][3].Weapons[3] == 0 && b->Units[0][0].Weapons[3] == 0 && b->Units[0][1].Weapons[3] == 2);
    Satellite_TakeTradeShips(2, 3, 0, 100);
    CHECK(Satellite_CountTradeShips(2, 3, 0) == 0);

    // Port bug 2: the stock satellite comes from Research_Products[2] (Satellite, DS:5E31),
    // not [1] (Miner droid). 28cd:0000 decrements DS:5E31; 28cd:06cd tests it for menu 0x46.
    ds.Research_Products[1].Stock = 7;
    ds.Research_Products[2].Stock = 2;
    ds.Research_Products[6].State = 0; // no colony item (menu 5)
    ds.Game_GroupCount = 0;
    ds.Game_CurSystem = 1;
    ds.Game_CurPlanet[0] = 3;
    ds.Game_CurMoon[0] = 0;
    Satellite_DeployStockSatellite();
    const TBody *body = ds.Game_CurBody;
    CHECK(body->Satellite == 0xE2);
    CHECK(ds.Research_Products[2].Stock == 1 && ds.Research_Products[1].Stock == 7);
    ds.Game_CurPlanet[0] = 5;
    Satellite_DeployStockSatellite();
    body = ds.Game_CurBody;
    CHECK(body->Satellite == 1);
    CHECK(ds.Research_Products[2].Stock == 0 && ds.Research_Products[1].Stock == 7);
    // No satellites left: no 0x46 item even with droids in stock.
    TBody *other = ds.Galaxy_Bodies[0] + 1; // planet 2
    other->Satellite = 0; other->Colony = 0; other->_0A = 0; other->Owner = 0; other->InfoLevel = 0;
    bool any = true;
    Satellite_AddBodyActionItems(1, 2, 0, &any);
    CHECK(!any);

    Satellite_Init();
    CHECK(ds.Groups_UpdateSatMenu == Satellite_UpdateGroupsMenu);
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
