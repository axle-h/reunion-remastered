#include "game/ems.h"

#include <SDL3/SDL.h>

#include "game/emm.h"
#include "game/system.h"

enum : uint16_t {
    GAME_DATA_SIZE = 0x8770,
    BUILDINGS_OFS = 0x2840,
    CHARSET_OFS = 0x5EF0,
};

// INT 67h with AH = function; the status (AH) goes to DS:8A4C as the
// original does after every EMM call.
static void emm_call(Registers *r, uint8_t fn)
{
    r->AX = (uint16_t)(fn << 8 | (r->AX & 0xFF));
    emm_int67(r);
    ds.Ems_Status = r->AX >> 8;
}

// 4719:0009: INT 21h AH=35h gets INT 67h's vector; an EMM's device header
// there has the name "EMMXXXX0" at offset 0Ah.
bool Ems_DetectEms(TEmsBlock *blk)
{
    (void)blk;
    FarPtr vec = *(const uint32_t *)(mem + 0x67 * 4);
    const uint8_t *name = far_host(far_ptr((uint16_t)(vec >> 16), 0x000A));
    return SDL_memcmp(name, "EMMXXXX0", 8) == 0;
}

// 4719:00a5: AH=42h, unallocated pages.
uint16_t Ems_EmsFreePages(TEmsBlock *blk)
{
    (void)blk;
    Registers r = {};
    emm_call(&r, 0x42);
    return r.BX;
}

// 4719:00ce: AH=41h page frame -> FrameSeg; AH=43h allocate -> Handle.
void Ems_EmsAlloc(TEmsBlock *blk, uint16_t pages)
{
    Registers r = {};
    emm_call(&r, 0x41);
    blk->FrameSeg = r.BX;
    blk->PageCount = pages;
    r.BX = blk->PageCount;
    emm_call(&r, 0x43);
    blk->Handle = r.DX;
}

// 4719:0128: AH=44h.
void Ems_EmsMapPage(TEmsBlock *blk, uint8_t physPage, uint16_t logPage)
{
    Registers r = {.AX = physPage, .BX = logPage, .DX = blk->Handle};
    emm_call(&r, 0x44);
}

// 4719:015e: AH=45h.
void Ems_EmsFree(TEmsBlock *blk)
{
    Registers r = {.DX = blk->Handle};
    emm_call(&r, 0x45);
}

// 4719:0188: a heap copy (state 1) of src unless an EMM has enough pages:
// then src moves into newly allocated pages mapped at the frame (state 2)
// and is freed. Allocation errors aren't checked, as in the original.
void Ems_EmsMoveToEms(TEmsBlock *blk, uint16_t size, FarPtr src)
{
    blk->State = 1;
    blk->Size = size;
    blk->Data = src;
    if (!Ems_DetectEms(blk))
        return;
    uint16_t pages = (uint16_t)(((uint16_t)(size - 1) >> 14) + 1);
    if (Ems_EmsFreePages(blk) < pages)
        return;
    blk->PageCount = pages;
    Ems_EmsAlloc(blk, blk->PageCount);
    uint16_t last = (uint16_t)(blk->PageCount - 1);
    for (uint16_t i = 0;; i++) {
        Ems_EmsMapPage(blk, (uint8_t)i, i);
        if (i == last)
            break;
    }
    blk->State = 2;
    blk->Data = far_ptr(blk->FrameSeg, 0);
    System_Move(far_host(src), far_host(blk->Data), size);
    System_FreeMem(src, size);
}

// 4719:0253
void Ems_EmsRelease(TEmsBlock *blk)
{
    if (blk->State == 1)
        System_FreeMem(blk->Data, blk->Size);
    if (blk->State == 2)
        Ems_EmsFree(blk);
    blk->State = 0;
    blk->Size = 0;
}

// 4719:0290: maps logical pages 0..n-1 of a block in EMS (state 2) to
// physical pages 0..n-1. Blocks share the frame, so this displaces others.
void Ems_EmsMapAll(TEmsBlock *blk)
{
    if (blk->State != 2)
        return;
    int16_t last = (int16_t)(blk->PageCount - 1);
    for (int16_t i = 0; i <= last; i++)
        Ems_EmsMapPage(blk, (uint8_t)i, (uint16_t)i);
}

// 4719:02cf
void Ems_EmsNop(TEmsBlock *blk)
{
    (void)blk;
}

// 4719:02d6
void Ems_EmsMapGameData(void)
{
    TEmsBlock *blk = Ems_GameDataBlock();
    Ems_EmsMapAll(blk);
    FarPtr base = blk->Data;
    ds.Game_Groups = base;
    ds.Game_Buildings = far_ptr((uint16_t)(base >> 16), (uint16_t)((uint16_t)base + BUILDINGS_OFS));
    ds.Gfx_Charset = far_ptr((uint16_t)(base >> 16), (uint16_t)((uint16_t)base + CHARSET_OFS));
}

// 4719:0317
void Ems_EmsReleaseGameData(void)
{
    Ems_EmsRelease(Ems_GameDataBlock());
}

// 4719:0325
void Ems_Init(void)
{
    ds.Ems_Buffer = System_GetMem(GAME_DATA_SIZE);
    Ems_EmsMoveToEms(Ems_GameDataBlock(), GAME_DATA_SIZE, ds.Ems_Buffer);
    Ems_EmsMapGameData();
    ds.Ems_RestoreProc = far_ptr(0x4719, 0x0317);
}
