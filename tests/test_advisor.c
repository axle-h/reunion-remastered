// advisor: which hint is offered for an advisor kind, and the talking hotspot.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/advisor.h"
#include "game/dseg.h"
#include "game/mouse.h"
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

    // Command::StaffPicked = advisor kind. Hints are (who, product) pairs.
    int16_t *kind = &ds.Command_StaffPicked;
    for (int i = 0; i < 9; i++)
        ds.Advisor_Hints[i].Who = 0;
    ds.Advisor_Hints[1] = (TAdvisorHint){2, 7};
    ds.Advisor_Hints[4] = (TAdvisorHint){2, 9};
    ds.Advisor_Hints[6] = (TAdvisorHint){3, 11};
    *kind = 2;
    for (int i = 0; i < 35; i++)
        ds.Research_Products[i].State = 0;
    CHECK(Advisor_FindAdvisorItem(false) == 0 && Advisor_FindAdvisorItem(true) == 0);

    ds.Research_Products[6].State = 5;        // product 7 is done
    CHECK(Advisor_FindAdvisorItem(true) == 2);
    CHECK(Advisor_FindAdvisorItem(false) == 0);
    ds.Research_Products[8].State = 2;        // product 9 is being developed
    CHECK(Advisor_FindAdvisorItem(false) == 5);
    CHECK(Advisor_FindAdvisorItem(true) == 2);
    ds.Research_Products[8].State = 5;        // the last matching hint wins
    CHECK(Advisor_FindAdvisorItem(true) == 5);
    *kind = 3;
    CHECK(Advisor_FindAdvisorItem(true) == 0);
    ds.Research_Products[10].State = 1;
    CHECK(Advisor_FindAdvisorItem(false) == 7);

    Advisor_SetupAdvisorHotspots();
    THotspot *h = Mouse_Hotspot(1);
    CHECK(ds.Mouse_HotspotCount == 1 && h->X == 0x70 && h->Y == 0x5C && h->W == 0x60 && h->H == 0x2D);
    CHECK(h->Caption.len == 7 && SDL_memcmp(h->Caption.s, "Talking", 7) == 0);

    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
