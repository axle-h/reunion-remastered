// cargo: space arithmetic of the transfer screen (ship capacity, space used by
// items), and loading ore against the planet's storage.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/cargo.h"
#include "game/dseg.h"
#include "game/fixes.h"
#include "game/groups.h"
#include "platform/fs.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

// A ship with room for 4500 t, docked at a colony with 2000 t of storage and
// 1500 t of ore 2, already carrying 1000 t of it; the "Transfer to ship"
// button of ore 2 is clicked (right button: all there is).
static int32_t load_ore(void)
{
    static TGroup groups[2][32];
    static TBody body;
    static int32_t stores[6];
    static uint8_t charset[0x2880];
    SDL_memset(groups, 0, sizeof groups);
    SDL_memset(&body, 0, sizeof body);
    ds.Gfx_Charset = charset;
    ds.Game_Groups = groups;
    ds.Game_GroupList = 0;
    ds.Game_SelGroup = 1;
    TGroup *g = Groups_At(0, 1);
    g->Status = 1;
    int32_t carried = 1000;
    SDL_memcpy(&g->Cargo[1], &carried, 4);
    body.Owner = 1;
    body._0A = 1;
    ds.Game_CurBody = &body;
    stores[1] = 1500;
    ds.Cargo_Stores = stores;
    ds.Cargo_PlanetCap = 2000;
    ds.Cargo_ShipCap = 4500;
    ds.Cargo_ShipUsed = 1000;
    ds.Cargo_ItemCount = 0;
    ds.Mouse_HeldLeft = false;
    ds.Game_ScreenEntered = false;
    ds.Game_MenuCommand = 0;
    ds.Game_HotspotHit = 0x27 + 2;
    Cargo_CargoScreen();
    SDL_memcpy(&carried, &g->Cargo[1], 4);
    CHECK(carried + stores[1] == 2500 && ds.Cargo_ShipUsed == carried);
    return carried;
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    dseg_clear();
    vid_clear();
    dseg_init();

    // Storage units per ship type, from the typed constants.
    CHECK(ds.Cargo_UnitSpace[0] == 500 && ds.Cargo_UnitSpace[1] == 2000);
    CHECK(ds.Cargo_UnitSpace[2] == 1000 && ds.Cargo_UnitSpace[3] == 8000);

    TGroup g;
    SDL_memset(&g, 0, sizeof g);
    g.Units[0][0].ShipType = 3;
    g.Units[0][1].ShipType = 2;
    g.Units[0][3].ShipType = 1;
    CHECK(Cargo_ShipCapacity(&g) == 3 * 500 + 2 * 2000 + 8000);

    // Capacity wraps like the original's 32-bit LongMul sum.
    g.Units[0][3].ShipType = 32000;
    CHECK(Cargo_ShipCapacity(&g) == (int32_t)(3u * 500 + 2u * 2000 + 32000u * 8000));

    g.Items[0] = 4;
    g.Items[2] = 1;
    int32_t expect = 4 * ds.Cargo_ItemSpace[0] + ds.Cargo_ItemSpace[2];
    CHECK(Cargo_ItemsSpace(&g) == expect);
    CHECK(expect > 0);

    // The original stops the ship's 2000 t of ore at the planet's storage;
    // the fix takes all 1500 t, as the ship has room.
    fixes.cargo_planet_cap = false;
    CHECK(load_ore() == 2000);
    fixes.cargo_planet_cap = true;
    CHECK(load_ore() == 2500);

    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
