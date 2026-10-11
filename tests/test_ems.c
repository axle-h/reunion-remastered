// ems: unit Ems without EMS: the game data block is a heap copy (state 1),
// the map calls change nothing and releasing frees the copy.
#include <stdio.h>

#include "game/dseg.h"
#include "game/ems.h"
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

    int32_t avail = system_heap_free();
    TEmsBlock *blk = Ems_GameDataBlock();
    Ems_Init();
    CHECK(blk->State == 1 && blk->Size == 0x8770 && blk->Data == ds.Ems_Buffer);
    CHECK((uint8_t *)ds.Game_Groups == ds.Ems_Buffer);
    CHECK((uint8_t *)ds.Game_Buildings == ds.Ems_Buffer + 0x2840);
    CHECK(ds.Gfx_Charset == ds.Ems_Buffer + 0x5EF0);
    CHECK(ds.Ems_RestoreProc == far_ptr(0x4719, 0x0317));
    CHECK(system_heap_free() == avail - 0x8770);

    // Another block keeps its own copy; mapping changes nothing.
    TEmsBlock *tb = Ems_ToolbarBlock();
    uint8_t *src = System_GetMem(0xC000);
    Ems_EmsMoveToEms(tb, 0xC000, src);
    CHECK(tb->State == 1 && tb->Data == src && tb->Size == 0xC000);
    TGroup (*groups)[32] = ds.Game_Groups;
    Ems_EmsMapAll(tb);
    Ems_EmsMapGameData();
    CHECK(ds.Game_Groups == groups && blk->State == 1);
    Ems_EmsRelease(tb);
    CHECK(tb->State == 0 && tb->Size == 0 && system_heap_free() == avail - 0x8770);

    Ems_EmsReleaseGameData();
    CHECK(blk->State == 0 && blk->Size == 0 && system_heap_free() == avail);
    return ok ? 0 : 1;
}
