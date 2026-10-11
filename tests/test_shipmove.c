// shipmove: the viewport blit (loop port of the unrolled rep-movs), the star
// field and the group-screen hotspots.
#include <stdio.h>
#include <string.h>

#include "game/dseg.h"
#include "game/groups.h"
#include "game/mouse.h"
#include "game/shipmove.h"
#include "game/system.h"
#include "platform/fs.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static void viewport(void)
{
    dseg_clear();
    vid_clear();
    static uint8_t src[0x10000];
    for (uint32_t i = 0; i < 0xBCC0; i++)
        src[i] = (uint8_t)(i % 251 + 1);
    ShipMove_BlitViewport(src);

    // Runs stay inside the 151-row image and do not overlap.
    uint32_t total = 0, prev_end = 0;
    for (int i = 0; i < ShipMove_ViewRunCount; i++) {
        uint32_t o = ShipMove_ViewRuns[i].offset, n = ShipMove_ViewRuns[i].length;
        CHECK(o + n <= 0xBCC0 && o >= prev_end); // adjacent rows merge into one run
        prev_end = o + n;
        total += n;
    }
    CHECK(total == 19423);

    const uint8_t *scr = &vid_screen[0][0] + 0x3D40;
    uint32_t nonzero = 0;
    bool same = true;
    for (uint32_t i = 0; i < 0xBCC0; i++) {
        if (scr[i]) {
            nonzero++;
            same &= scr[i] == src[i];
        }
    }
    CHECK(nonzero == total && same);
    // First row: bytes 30..288 are copied (81h words + 1 byte), 29 and 289 are not.
    CHECK(scr[29] == 0 && scr[30] != 0 && scr[288] != 0 && scr[289] == 0);
    CHECK((&vid_screen[0][0])[0x3D40 - 1] == 0);
}

static void starfield(void)
{
    ds.Mouse_CursorState = 1;
    ds.ShipMove_Backdrop = System_GetSegment() + 8;  // as alloc_anim_buffers

    ds.System_RandSeed = 1234;
    ShipMove_InitStarfield();
    const uint8_t *a = ds.ShipMove_Stars;
    uint8_t copy[0x1E1B];
    memcpy(copy, a, sizeof copy);

    // The first star, from the same random sequence.
    ds.System_RandSeed = 1234;
    int16_t x = (int16_t)(System_Random(0x28E) - 0x147), y = (int16_t)(System_Random(0xD2) - 0x69);
    int16_t d = (int16_t)(System_Random(900) + 0x73);
    uint8_t sp = (uint8_t)(System_Random(10) + 5);
    if (x > -0x32 && x < 0x32 && y > -0x32 && y < 0x32) {
        x *= 2;
        y *= 2;
    }
    if (x > -0x1E && x < 0x1E && y > -0x1E && y < 0x1E) {
        x = (int16_t)((x < 0 ? -x : x) + 0x1E);
        y = (int16_t)((y < 0 ? -y : y) + 0x1E);
    }
    int16_t got[3];
    memcpy(got, copy, 6);
    CHECK(got[0] == x && got[1] == y && got[2] == d && copy[6] == sp);

    // Same seed, same field.
    ds.System_RandSeed = 1234;
    ShipMove_InitStarfield();
    CHECK(memcmp(copy, ds.ShipMove_Stars, sizeof copy) == 0);

    // Animate a travelling group: stars appear in the backdrop and on screen.
    ds.Game_Groups = System_GetMem(0x2840);
    memset(ds.Game_Groups, 0, 0x2840);
    ds.Game_GroupList = 0;
    ds.Game_SelGroup = 1;
    Groups_Selected()->Status = 4;
    memset(&vid_screen[0][0], 0, 64000);
    ShipMove_AnimateStarfield();
    const uint8_t *bd = ds.ShipMove_Backdrop;
    int stars = 0;
    for (uint32_t i = 0; i < 0x8980; i++)
        stars += bd[i] >= 0x70 && bd[i] < 0x7F + 1 && bd[i] != 0x7F;
    CHECK(stars > 500);
    int on_screen = 0;
    for (uint32_t i = 0; i < 0xBCC0; i++)
        on_screen += (&vid_screen[0][0])[0x3D40 + i] != 0;
    CHECK(on_screen == 19423);
    // Status 3 (docked/orbit): nothing is drawn.
    Groups_Selected()->Status = 3;
    memset(&vid_screen[0][0], 0, 64000);
    ShipMove_AnimateStarfield();
    CHECK((&vid_screen[0][0])[0x3D40 + 0x1E] == 0);
}

static void hotspots(void)
{
    TGroup *g = Groups_Selected();
    g->Kind = 2;
    g->Status = 1;
    ShipMove_SetupGroupScreenHotspots();
    CHECK(ds.Mouse_HotspotCount == 6);
    CHECK(Mouse_Hotspot(1)->W == 0x140 && Mouse_Hotspot(1)->Caption.len == 11);
    CHECK(Mouse_Hotspot(2)->Caption.len == 6 && memcmp(Mouse_Hotspot(2)->Caption.s, "Launch", 6) == 0);
    CHECK(Mouse_Hotspot(3)->W == 0x2A);
    CHECK(Mouse_Hotspot(4)->Caption.len == 9);
    g->Kind = 1;
    g->Status = 4;
    ShipMove_SetupGroupScreenHotspots();
    CHECK(Mouse_Hotspot(1)->W == 0 && Mouse_Hotspot(3)->W == 0);
    CHECK(memcmp(Mouse_Hotspot(4)->Caption.s, "Change destination", 18) == 0);
    CHECK(Mouse_Hotspot(6)->X == 0xFF && Mouse_Hotspot(6)->Repeat_ == 1);
}

int main(int argc, char *argv[])
{
    viewport();
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping the data tests\n");
        return ok ? 77 : 1;
    }
    dseg_clear();
    vid_clear();
    if (!dseg_init()) {
        printf("no REUNION.PRG, skipping the data tests\n");
        return ok ? 77 : 1;
    }
    system_init();
    starfield();
    hotspots();
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
