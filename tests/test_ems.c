// ems: the emulated EMM (INT 67h) and unit Ems on top of it: with a driver
// the game data block moves into EMS and leaves the heap; blocks sharing the
// page frame keep their contents across remapping; without a driver the
// block stays a heap copy (state 1) and every call is a no-op.
#include <stdio.h>

#include "game/dseg.h"
#include "game/emm.h"
#include "game/ems.h"
#include "game/system.h"
#include "platform/fs.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static uint8_t emm(uint8_t fn, Registers *r)
{
    r->AX = (uint16_t)(fn << 8 | (r->AX & 0xFF));
    emm_int67(r);
    return (uint8_t)(r->AX >> 8);
}

static void driver(void)
{
    Registers r = {};
    CHECK(emm(0x40, &r) == 0);
    CHECK(emm(0x41, &r) == 0 && r.BX == EMM_FRAME_SEG);
    CHECK(emm(0x42, &r) == 0 && r.BX == EMM_DEFAULT_PAGES && r.DX == EMM_DEFAULT_PAGES);
    CHECK(emm(0x46, &r) == 0 && (r.AX & 0xFF) == 0x40);
    r.BX = 0;
    CHECK(emm(0x43, &r) == 0x89);
    r.BX = EMM_DEFAULT_PAGES + 1;
    CHECK(emm(0x43, &r) == 0x87);

    Registers a = {.BX = 2}, b = {.BX = EMM_DEFAULT_PAGES - 1};
    CHECK(emm(0x43, &a) == 0 && a.DX != 0);
    CHECK(emm(0x43, &b) == 0x88);
    b.BX = 1;
    CHECK(emm(0x43, &b) == 0 && b.DX != a.DX);
    CHECK(emm(0x4C, &a) == 0 && a.BX == 2);

    // Bank switching: a's two pages and b's page through physical page 0.
    uint8_t *f = far_host(far_ptr(EMM_FRAME_SEG, 0));
    uint8_t *f3 = far_host(far_ptr(EMM_FRAME_SEG, 0xC000));
    Registers m = {.AX = 0, .BX = 0, .DX = a.DX};
    CHECK(emm(0x44, &m) == 0);
    f[5] = 0xA0;
    m = (Registers){.AX = 0, .BX = 1, .DX = a.DX};
    CHECK(emm(0x44, &m) == 0);
    f[5] = 0xA1;
    m = (Registers){.AX = 0, .BX = 0, .DX = b.DX};
    CHECK(emm(0x44, &m) == 0);
    f[5] = 0xB0;
    m = (Registers){.AX = 3, .BX = 0, .DX = a.DX};
    CHECK(emm(0x44, &m) == 0 && f3[5] == 0xA0);
    m = (Registers){.AX = 0, .BX = 1, .DX = a.DX};
    CHECK(emm(0x44, &m) == 0 && f[5] == 0xA1);
    m = (Registers){.AX = 0, .BX = 0, .DX = b.DX};
    CHECK(emm(0x44, &m) == 0 && f[5] == 0xB0);
    // A page mapped twice moves: a's page 0 goes from physical 3 to 1.
    f3[6] = 0xA2;
    m = (Registers){.AX = 1, .BX = 0, .DX = a.DX};
    CHECK(emm(0x44, &m) == 0 && f[0x4006] == 0xA2);

    m = (Registers){.AX = 4, .BX = 0, .DX = a.DX};
    CHECK(emm(0x44, &m) == 0x8B);
    m = (Registers){.AX = 0, .BX = 2, .DX = a.DX};
    CHECK(emm(0x44, &m) == 0x8A);
    m = (Registers){.AX = 0, .BX = 0, .DX = 0x1F};
    CHECK(emm(0x44, &m) == 0x83);

    CHECK(emm(0x45, &a) == 0 && emm(0x45, &b) == 0);
    CHECK(emm(0x45, &a) == 0x83);
    CHECK(emm(0x42, &r) == 0 && r.BX == EMM_DEFAULT_PAGES);
    CHECK(emm(0x47, &r) == 0x84);
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    mem_init();
    CHECK(dseg_init());
    system_init();
    driver();

    // With EMS: the block's 3 pages are mapped at the frame, the heap copy
    // is freed.
    int32_t avail = System_MemAvail();
    TEmsBlock *blk = Ems_GameDataBlock();
    CHECK(Ems_DetectEms(blk));
    CHECK(Ems_EmsFreePages(blk) == EMM_DEFAULT_PAGES && ds.Ems_Status == 0);

    Ems_Init();
    FarPtr frame = far_ptr(EMM_FRAME_SEG, 0);
    CHECK(ds.Ems_Buffer != 0);
    CHECK(blk->State == 2 && blk->Size == 0x8770 && blk->Data == frame);
    CHECK(blk->PageCount == 3 && blk->FrameSeg == EMM_FRAME_SEG && blk->Handle != 0);
    CHECK(ds.Ems_Status == 0);
    CHECK(Ems_EmsFreePages(blk) == EMM_DEFAULT_PAGES - 3);
    CHECK(ds.Game_Groups == frame);
    CHECK(ds.Game_Buildings == far_ptr(EMM_FRAME_SEG, 0x2840));
    CHECK(ds.Gfx_Charset == far_ptr(EMM_FRAME_SEG, 0x5EF0));
    CHECK(ds.Ems_RestoreProc == far_ptr(0x4719, 0x0317));
    CHECK(System_MemAvail() == avail);

    // The moved contents are there; a second block takes over the frame and
    // remapping brings the game data back.
    uint8_t *p = far_host(frame);
    p[0] = 1;
    p[0x876F] = 2;
    TEmsBlock *tb = Ems_ToolbarBlock();
    FarPtr src = System_GetMem(0xC000);
    System_FillChar(far_host(src), 0xC000, 7);
    Ems_EmsMoveToEms(tb, 0xC000, src);
    CHECK(tb->State == 2 && tb->PageCount == 3 && tb->Handle != blk->Handle);
    CHECK(p[0] == 7 && p[0xBFFF] == 7);
    p[1] = 9;
    Ems_EmsMapGameData();
    CHECK(p[0] == 1 && p[1] == 0 && p[0x876F] == 2);
    Ems_EmsMapAll(tb);
    CHECK(p[0] == 7 && p[1] == 9);
    Ems_EmsRelease(tb);
    CHECK(tb->State == 0 && System_MemAvail() == avail);

    Ems_EmsReleaseGameData();
    CHECK(blk->State == 0 && blk->Size == 0);
    CHECK(Ems_EmsFreePages(blk) == EMM_DEFAULT_PAGES);
    CHECK(System_MemAvail() == avail);

    // Without EMS: a heap copy.
    mem_init();
    emm_install(0);
    CHECK(dseg_init());
    system_init();
    avail = System_MemAvail();
    CHECK(!Ems_DetectEms(blk));
    Ems_Init();
    CHECK(blk->State == 1 && blk->Size == 0x8770 && blk->Data == ds.Ems_Buffer);
    CHECK(ds.Game_Groups == ds.Ems_Buffer);
    CHECK(System_MemAvail() == avail - 0x8770);
    FarPtr groups = ds.Game_Groups;
    Ems_EmsMapAll(blk);
    Ems_EmsMapGameData();
    CHECK(ds.Game_Groups == groups && blk->State == 1);
    Ems_EmsReleaseGameData();
    CHECK(blk->State == 0 && System_MemAvail() == avail);
    return ok ? 0 : 1;
}
