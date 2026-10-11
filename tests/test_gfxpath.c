// gfxpath: ICON.ALL index and DrawIcon against assets/icons.h, the toolbar
// slot bookkeeping.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "assets/icons.h"
#include "game/dseg.h"
#include "game/ems.h"
#include "game/gfx.h"
#include "game/gfxpath.h"
#include "game/system.h"
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
    system_init();
    ds.Gfx_ScreenSeg = 0xA000;

    GfxPath_Init();
    TEmsBlock *blk = Ems_ToolbarBlock();
    CHECK(blk->State == 1 && blk->Data != 0);  // a heap copy
    CHECK(ds.Game_GfxPathProc == far_ptr(0x3A1B, 0x09A3));
    GfxPath_IconAllIndex();

    Icons icons;
    CHECK(icons_load("icon\\icon.all", &icons));
    FarPtr canvas = System_GetMem(0x4000);
    uint8_t *img = far_host(canvas);
    const int picks[] = {0, 1, 10, 33, 67};
    for (size_t k = 0; k < SDL_arraysize(picks); k++) {
        SDL_memset(img, 0, 0x4000);
        GfxPath_DrawIcon((int16_t)picks[k], canvas);
        const uint8_t *want = icons_pixels(&icons, picks[k]);
        bool same = true;
        // 39 of the 40 columns, 24 rows, at +504h with a 320 stride.
        for (int y = 0; y < ICON_HEIGHT; y++)
            for (int x = 0; x < 39; x++)
                same = same && img[0x504 + y * 320 + x] == want[y * ICON_WIDTH + x];
        CHECK(same);
    }
    icons_free(&icons);

    // Slots: ids at DS:7C64 + 21 * slot.
    for (int i = 1; i <= 12; i++)
        *(int16_t *)ds_at((uint16_t)(0x7C64 + i * 21)) = 0;
    CHECK(!GfxPath_ToolbarHasIcon(5));
    *(int16_t *)ds_at(0x7C64 + 21) = 5;
    *(int16_t *)ds_at(0x7C64 + 2 * 21) = 9;
    CHECK(GfxPath_ToolbarHasIcon(5) && GfxPath_ToolbarHasIcon(9) && !GfxPath_ToolbarHasIcon(7));

    GfxPath_FreeToolbarBuffer();
    CHECK(blk->State == 0);
    return ok ? 0 : 1;
}
