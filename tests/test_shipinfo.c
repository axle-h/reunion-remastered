// shipinfo: hotspot layout of the icon grid, selection markers and icon
// placement drawn into the emulated video memory.
#include <stdio.h>
#include <string.h>

#include "game/blit.h"
#include "game/dseg.h"
#include "game/groups.h"
#include "game/mem.h"
#include "game/mouse.h"
#include "game/shipinfo.h"
#include "game/system.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static void reset(void)
{
    mem_init();
    ds.System_SegA000 = 0xA000;
    ds.Gfx_ScreenSeg = 0xA000;
    ds.Mouse_CursorState = 1;  // hidden: Hide/ShowCursor leave the screen alone
    ds.Game_Groups = far_ptr(0x3000, 0);
    ds.ShipInfo_Sprites = far_ptr(0x4000, 0);
    ds.ShipInfo_Backdrop = far_ptr(0x5000, 0);
    ds.ShipInfo_Buffer = far_ptr(0x6000, 0);
}

static void hotspots(void)
{
    reset();
    ds.Game_GroupList = 0;
    ds.Game_ShownGroupCount = 10;
    for (int i = 1; i <= 10; i++) {
        TGroup *g = Groups_At(0, i);
        g->Name.len = 3;
        memcpy(g->Name.s, (char[]){'G', 'r', (char)('a' + i)}, 3);
    }
    ShipInfo_SetupShipInfoHotspots();
    CHECK(ds.Mouse_HotspotCount == 0x2C);
    THotspot *h = Mouse_Hotspot(1);
    CHECK(h->X == 0 && h->Y == 0xBC && h->W == 200 && h->H == 12 && h->Caption.len == 11 && h->Repeat_);
    // Icon 9 is the first of the second column (8 per column), 0x30 apart.
    h = Mouse_Hotspot(9 + 2);
    CHECK(h->X == 0x30 && h->Y == 0x3C && h->W == 0x10 && h->H == 0x11);
    h = Mouse_Hotspot(3 + 2);
    CHECK(h->X == 0 && h->Y == 0x3C + 2 * 16 && h->Caption.len == 3 && h->Caption.s[2] == 'd');
    // Slots beyond the group count have no caption; names exist only for real groups.
    CHECK(Mouse_Hotspot(11 + 2)->Caption.len == 0);
    h = Mouse_Hotspot(10 + 0x22);
    CHECK(h->X == 0x30 + 0x11 + 0 && h->W == 0x20 && h->Caption.s[2] == 'k');
}

static void markers(void)
{
    reset();
    memset((uint8_t *)far_host(ds.ShipInfo_Sprites) + 0x1C55, 0x77, 7 * 6);  // marker "on" pixels
    memset((uint8_t *)far_host(ds.ShipInfo_Sprites) + 0x1C45, 0x11, 16);
    // 6 rows of 7 from the sprite sheet: rows are 320 apart.
    for (int r = 0; r < 6; r++)
        memset((uint8_t *)far_host(ds.ShipInfo_Sprites) + 0x1C55 + r * 320, 0x77, 7);
    ds.Game_SelGroup = 10;  // column 1, row 1
    ShipInfo_DrawSelectionMarker();
    uint8_t *scr = far_host(far_ptr(0xA000, 0));
    CHECK(scr[(1 * 16 + 0x41) * 320 + 5 + 0x30] == 0x77);
    CHECK(scr[(1 * 16 + 0x41 + 5) * 320 + 5 + 0x30 + 6] == 0x77);
    CHECK(scr[(1 * 16 + 0x41) * 320 + 5 + 0x30 + 7] == 0);
    ds.Game_SelGroup = 0;
    ShipInfo_EraseSelectionMarker();  // no group: nothing changes
    CHECK(scr[(1 * 16 + 0x41) * 320 + 5 + 0x30] == 0x77);
    // Buffered marker goes to the off-screen copy, 49 rows down.
    ds.Game_SelGroup = 1;
    ShipInfo_DrawSelectionMarkerBuffered();
    uint8_t *buf = far_host(ds.ShipInfo_Buffer);
    CHECK(buf[0x3D40 + 0x10 * 320 + 5] == 0x77);
}

static void icons(void)
{
    reset();
    uint8_t *spr = far_host(ds.ShipInfo_Sprites);
    uint8_t *bd = far_host(ds.ShipInfo_Backdrop);
    memset(bd, 0x22, 0xBCC0);
    // icon 2: sheet offset (2-1)*32+1+320, a 31x14 sprite with 0x55 pixels (key 0x40 is transparent)
    for (int r = 0; r < 14; r++)
        memset(spr + 32 + 1 + 320 + r * 320, 0x55, 31);
    ds.Game_SelGroup = 0;
    ShipInfo_DrawGroupIcon(2, 9);  // column 1, row 0
    uint8_t *scr = far_host(far_ptr(0xA000, 0));
    CHECK(scr[0x3C * 320 + 0x30 + 0x10] == 0x22);        // backdrop copy
    CHECK(scr[0x3D * 320 + 0x30 + 0x11] == 0x55);        // icon top-left
    CHECK(scr[(0x3D + 13) * 320 + 0x30 + 0x11 + 30] == 0x55);
    CHECK(scr[(0x3D + 14) * 320 + 0x30 + 0x11] == 0);  // below the 15 copied rows
    ShipInfo_DrawGroupIcon(0, 9);  // empty slot: backdrop only
    CHECK(scr[0x3D * 320 + 0x30 + 0x11] == 0x22);
}

int main(void)
{
    hotspots();
    markers();
    icons();
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
