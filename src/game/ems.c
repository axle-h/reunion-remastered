#include "game/ems.h"

#include "game/system.h"

enum : uint16_t {
    GAME_DATA_SIZE = 0x8770,
    BUILDINGS_OFS = 0x2840,
    CHARSET_OFS = 0x5EF0,
};

// 4719:0188: the original moves src into EMS pages when an EMM has enough
// of them. The port has no EMS, so the block stays the heap copy src
// (state 1), as on a DOS machine without an EMM.
void Ems_EmsMoveToEms(TEmsBlock *blk, uint16_t size, FarPtr src)
{
    blk->State = 1;
    blk->Size = size;
    blk->Data = src;
}

// 4719:0253 (state 2, a block in EMS, never happens in the port)
void Ems_EmsRelease(TEmsBlock *blk)
{
    if (blk->State == 1)
        System_FreeMem(blk->Data, blk->Size);
    blk->State = 0;
    blk->Size = 0;
}

// 4719:0290: maps a block in EMS into the page frame; a heap copy needs
// nothing.
void Ems_EmsMapAll(TEmsBlock *blk)
{
    (void)blk;
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
