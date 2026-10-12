// cargo: space arithmetic of the transfer screen (ship capacity, space used by items).
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/cargo.h"
#include "game/dseg.h"
#include "platform/fs.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

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

    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
