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
void Ems_EmsMoveToEms(TEmsBlock *blk, uint16_t size, uint8_t *src)
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
    // 4719:02d6 MapGameData. The original maps the block again after every
    // use of the toolbar block, which shared the EMS page frame; a heap
    // copy stays where it is.
    uint8_t *base = ds.Ems_Buffer;
    ds.Game_Groups = (TGroup (*)[32])base;
    ds.Game_Buildings = (TBuilding *)(base + BUILDINGS_OFS);
    ds.Gfx_Charset = base + CHARSET_OFS;
    ds.Ems_RestoreProc = Ems_EmsReleaseGameData;
}
