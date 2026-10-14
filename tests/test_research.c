// research: skill shortfall / progress rule and the invention hotspot grid.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/dseg.h"
#include "game/mouse.h"
#include "game/research.h"
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

    // Only the skills the developer lacks count.
    TProduct *p = &ds.Research_Products[4];
    p->Skills[0] = 10;
    p->Skills[1] = 30;
    p->Skills[2] = 0;
    p->Skills[3] = 50;
    ds.Game_DevSkill[0] = 20;
    ds.Game_DevSkill[1] = 25;
    ds.Game_DevSkill[2] = 0;
    ds.Game_DevSkill[3] = 40;
    CHECK(Research_SkillShortfall(5) == 5 + 10);

    // Progress shows the working frames once it passes 10000 - 10000 / (short + 1).
    p->Progress = 9374;   // 10000 - 10000 / 16 = 9375
    CHECK(!Research_ProgressVisible(5));
    p->Progress = 9376;
    CHECK(Research_ProgressVisible(5));
    ds.Game_DevSkill[1] = 30;
    ds.Game_DevSkill[3] = 50;
    CHECK(Research_SkillShortfall(5) == 0);
    p->Progress = 1;      // nothing missing: 10000 - 10000 = 0
    CHECK(Research_ProgressVisible(5));

    // Hotspot grid: 5 columns of 32x16, known inventions named, unknown ones blank.
    for (int i = 0; i < 35; i++)
        ds.Research_Products[i].State = 0;
    ds.Research_Products[0].State = 1;
    ds.Research_Products[13].State = 3;   // invention 14: "Unknown" until revealed
    ds.Intel_Invention14Revealed = 0;
    Research_SetupResearchHotspots();
    CHECK(ds.Mouse_HotspotCount == 35);
    THotspot *h1 = Mouse_Hotspot(1), *h6 = Mouse_Hotspot(6), *h14 = Mouse_Hotspot(14);
    CHECK(h1->X == 0 && h1->Y == 0x34 && h1->W == 0x20 && h1->H == 0x10);   // first column is clamped to 0
    CHECK(h6->X == 0 && h6->Y == 0x44);
    CHECK(Mouse_Hotspot(2)->X == 30 && Mouse_Hotspot(5)->X == 126);
    CHECK(h1->Caption.len == ds.Research_Products[0].Name.len && h1->Caption.len > 0);
    CHECK(Mouse_Hotspot(2)->Caption.len == 0);
    CHECK(h14->Caption.len == 7 && SDL_memcmp(h14->Caption.s, "Unknown", 7) == 0);
    CHECK(h14->Repeat_ == 1 && h14->Cursor == 0);

    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
