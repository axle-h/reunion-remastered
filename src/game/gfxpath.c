#include "game/gfxpath.h"


#include "game/blit.h"
#include "game/dseg.h"
#include "game/ems.h"
#include "game/gfx.h"
#include "game/mouse.h"
#include "game/sound.h"
#include "game/system.h"

// Not ported yet: see stubs.c.
void Command_TickAmbientLights(void);   // 3abd:14c5
void Command_TickPanelDoorAnim(void);   // 3abd:1395

// Unnamed globals.
#define ICON_SIZE (*(uint16_t *)ds_at(0x54CA))     // size of ICON.ALL (from Gfx_IconAllOpenSize)
#define EXTRA_SIZE (*(uint16_t *)ds_at(0x54CC))    // further bytes of the block
#define PAGE2_FLAG (*(uint8_t *)ds_at(0x7C62))     // second page of icons in use
#define CUR_PAGE (*(uint8_t *)ds_at(0x7C63))       // page shown
#define BLOCK_INIT (*(uint32_t *)ds_at(0x7D62))    // GetMem block handed to EmsMoveToEms
#define BG_PTR (*(uint32_t *)ds_at(0x7D66))        // cell backgrounds
#define TMP_PTR (*(uint32_t *)ds_at(0x7D6A))       // 3C0h-byte icon scratch buffer
#define ICON_INDEX(n) (*(uint16_t *)ds_at((uint16_t)(0x7D72 + (n) * 2)))

enum : uint16_t {
    PAGE_SIZE = 0x2940,   // 33 rows of 320
    BG_OFS = 0x54FF,      // from the end of the icons: background cells
    ARROW_NEXT = 0x121 + 0x2940,
    ARROW_PREV = 0x121,
};

// Slot records (1..12).
static inline int16_t *slot_id(int slot)
{
    return (int16_t *)ds_at((uint16_t)(0x7C64 + slot * 21));
}

static inline uint8_t *slot_label(int slot)
{
    return (uint8_t *)ds_at((uint16_t)(0x7C51 + slot * 21));
}

static FarPtr block_base(void)
{
    return Ems_ToolbarBlock()->Data;
}

// block base + ICON_SIZE + extra, the offset wrapping in 16 bits.
static FarPtr bar(uint16_t extra)
{
    FarPtr b = block_base();
    return far_ptr((uint16_t)(b >> 16), (uint16_t)((uint16_t)b + ICON_SIZE + extra));
}

static FarPtr near_add(FarPtr p, uint16_t n)
{
    return far_ptr((uint16_t)(p >> 16), (uint16_t)((uint16_t)p + n));
}

static void copy_to_screen(FarPtr src, uint16_t ofs)
{
    Blit_CopyRect(src, far_ptr(ds.Gfx_ScreenSeg, ofs), 0x140, 0x20);
}

// Cell k (0..11) of the bar: page k / 6, column k % 6.
static uint16_t cell_ofs(uint16_t k)
{
    return (uint16_t)((k % 6) * 0x30 + 1 + (k / 6) * PAGE_SIZE);
}

static void draw_slot(uint16_t k, bool check_icon)
{
    int slot = k + 1;
    FarPtr cell = bar(cell_ofs(k));
    Blit_CopyRectStrided(BG_PTR, cell, 0x30, 0x20, 0xF0, 0x140);
    int16_t id = *slot_id(slot);
    int8_t icon = (int8_t)*(uint8_t *)ds_at((uint16_t)(0x4E06 + id));
    if (!check_icon || icon != 0)
        GfxPath_DrawIcon(icon, cell);
    System_StrStore((const PString *)ds_at((uint16_t)(0x488A + id * 18)),
                    (PString *)slot_label(slot), 18);
}

// 3a1b:0000
void GfxPath_WaitVRetrace(void)
{
    Gfx_WaitVRetrace();
}

// 3a1b:0019
void GfxPath_SetToolbarAndShow(int16_t screen)
{
    GfxPath_SetToolbar(screen);
    GfxPath_ShowToolbar();
    ds.Game_HotspotHit = 0;
}

// 3a1b:0030: the screen record (25 bytes at DS:50FB + 25 * screen) has the
// slot count at +0Ch and the ids after it.
void GfxPath_SetToolbar(int16_t screen)
{
    if (screen == 1)
        CUR_PAGE = *(uint8_t *)ds_at(0x7C64);  // low byte of slot 0's word
    else
        CUR_PAGE = 1;
    const int8_t *rec = (const int8_t *)ds_at((uint16_t)(0x50FB + screen * 25));
    int16_t count = rec[0x0C];
    for (int16_t i = 1; i <= count; i++)
        *slot_id(i) = rec[0x0C + i];
    for (int16_t i = (int16_t)(count + 1); i <= 12; i++)
        *slot_id(i) = 0;
    GfxPath_RedrawToolbar();
    ds.Game_HotspotHit = 0;
}

// 3a1b:00bf
void GfxPath_RedrawToolbar(void)
{
    Ems_EmsMapAll(Ems_ToolbarBlock());
    BG_PTR = bar(BG_OFS);
    TMP_PTR = System_GetMem(0x3C0);
    PAGE2_FLAG = *slot_id(7) != 0;
    for (uint16_t k = 0; k <= 5; k++)
        draw_slot(k, true);
    if (PAGE2_FLAG == 0) {
        Blit_CopyRectStrided(near_add(BG_PTR, 0x70), bar(ARROW_PREV), 0x20, 0x20, 0xF0, 0x140);
    } else {
        for (uint16_t k = 6; k <= 11; k++)
            draw_slot(k, true);
        Blit_CopyRectStrided(near_add(BG_PTR, 0x30), bar(ARROW_NEXT), 0x20, 0x20, 0xF0, 0x140);
        Blit_CopyRectStrided(near_add(BG_PTR, 0x50), bar(ARROW_PREV), 0x20, 0x20, 0xF0, 0x140);
    }
    System_FreeMem(TMP_PTR, 0x3C0);
    Ems_EmsNop(Ems_ToolbarBlock());
    Ems_EmsMapGameData();
}

// 3a1b:0366: puts id in the first free slot.
void GfxPath_AddToolbarIcon(int16_t id)
{
    Ems_EmsMapAll(Ems_ToolbarBlock());
    BG_PTR = bar(BG_OFS);
    TMP_PTR = System_GetMem(0x3C0);
    uint16_t slot = 1;
    while (*slot_id(slot) != 0)
        slot++;
    if (slot < 13) {
        // BUG? clears slot[slot] 13 - slot times instead of the slots after it.
        for (uint16_t j = slot; j <= 12; j++)
            *slot_id(slot) = 0;
    }
    *slot_id(slot) = id;
    PAGE2_FLAG = (PAGE2_FLAG == 0 && slot != 7) ? 0 : 1;

    uint16_t k = (uint16_t)(slot - 1);
    FarPtr cell = bar(cell_ofs(k));
    Blit_CopyRectStrided(BG_PTR, cell, 0x30, 0x20, 0xF0, 0x140);
    GfxPath_DrawIcon((int8_t)*(uint8_t *)ds_at((uint16_t)(0x4E06 + id)), cell);
    System_StrStore((const PString *)ds_at((uint16_t)(0x488A + id * 18)),
                    (PString *)slot_label(slot), 18);
    if (slot == 7) {
        Blit_CopyRectStrided(near_add(BG_PTR, 0x30), bar(ARROW_NEXT), 0x20, 0x20, 0xF0, 0x140);
        Blit_CopyRectStrided(near_add(BG_PTR, 0x50), bar(ARROW_PREV), 0x20, 0x20, 0xF0, 0x140);
        for (uint16_t s = 8; s <= 12; s++) {
            Blit_CopyRectStrided(BG_PTR, bar(cell_ofs((uint16_t)(s - 1))), 0x30, 0x20, 0xF0, 0x140);
            slot_label(s)[0] = 0;
        }
    }
    System_FreeMem(TMP_PTR, 0x3C0);
    Ems_EmsNop(Ems_ToolbarBlock());
    Ems_EmsMapGameData();
}

static void scroll_tick(void)
{
    Gfx_WaitScanlines(40);
}

static void scroll_extras(void)
{
    GfxPath_WaitVRetrace();
    if (ds.Game_CurrentScreen == 1) {
        Command_TickAmbientLights();
        Command_TickPanelDoorAnim();
    }
}

// Ui_RowOffset[-1 + i]: the table starts one byte before DS:4875.
static uint8_t row_offset(int i)
{
    return *(uint8_t *)ds_at((uint16_t)(0x4874 + i));
}

static void toggle_pages(void)
{
    CUR_PAGE = CUR_PAGE == 0;
    // BUG? toggles the low byte of slot 0's word (DS:7C64, not a used slot:
    // they are 1..12), probably meant DS:7C62. SetToolbar(1) reads it back.
    *(uint8_t *)ds_at(0x7C64) = *(uint8_t *)ds_at(0x7C64) == 0;
}

// 3a1b:0586
void GfxPath_ScrollToolbarNext(void)
{
    Ems_EmsMapAll(Ems_ToolbarBlock());
    uint16_t ofs = 0x140;
    for (int i = 0; i <= 20; i++) {
        scroll_tick();
        copy_to_screen(bar((uint16_t)(ofs + 1)), 0);
        ofs = (uint16_t)(row_offset(i) * 0x140 + ofs);
        scroll_extras();
    }
    toggle_pages();
    Ems_EmsNop(Ems_ToolbarBlock());
    Ems_EmsMapGameData();
}

// 3a1b:0643
void GfxPath_ScrollToolbarPrev(void)
{
    Ems_EmsMapAll(Ems_ToolbarBlock());
    uint16_t ofs = PAGE_SIZE;
    for (int i = 0; i <= 20; i++) {
        scroll_tick();
        ofs = (uint16_t)(ofs - row_offset(i) * 0x140);
        copy_to_screen(bar((uint16_t)(ofs + 1)), 0);
        scroll_extras();
    }
    toggle_pages();
    Ems_EmsNop(Ems_ToolbarBlock());
    Ems_EmsMapGameData();
}

// 3a1b:0704
void GfxPath_ShowToolbar(void)
{
    Ems_EmsMapAll(Ems_ToolbarBlock());
    Mouse_HideCursor();
    uint16_t page = CUR_PAGE == 0 ? (uint16_t)(PAGE_SIZE + 1) : 1;
    Blit_InterlaceReveal(bar(page), far_ptr(ds.Gfx_ScreenSeg, 0), 0x140, 0x20);
    Mouse_ShowCursor();
    Sound_FreeSampleIfIdle();
    Ems_EmsNop(Ems_ToolbarBlock());
    Ems_EmsMapGameData();
}

// 3a1b:0780
void GfxPath_ShowToolbarInstant(void)
{
    Ems_EmsMapAll(Ems_ToolbarBlock());
    Mouse_HideCursor();
    copy_to_screen(bar(1), 0);
    Mouse_ShowCursor();
    Ems_EmsNop(Ems_ToolbarBlock());
    Ems_EmsMapGameData();
}

// 3a1b:07ca: scans the slots up to the first empty one (or the end of the
// table, as the original does).
bool GfxPath_ToolbarHasIcon(int16_t id)
{
    bool found = false;
    int16_t i = 1;
    while (*slot_id(i) != 0) {
        found = found || *slot_id(i) == id;
        i++;
    }
    return found;
}

// 3a1b:0810
void GfxPath_IconAllIndex(void)
{
    Ems_EmsMapAll(Ems_ToolbarBlock());
    Gfx_IconAllOpen();
    uint16_t len;
    for (uint16_t i = 0; i <= 0x43; i++) {
        ICON_INDEX(i) = ds.Gfx_ReadPos;
        Gfx_IconAllReadChunk((const FarPtr *)ds_at(0x7DFB), &len);
    }
    Gfx_IconAllClose();
    Ems_EmsNop(Ems_ToolbarBlock());
    Ems_EmsMapGameData();
}

// 3a1b:0864: RLE-decodes the icon (+2 on every pixel) into the scratch buffer.
void GfxPath_DrawIcon(int16_t icon, FarPtr dest)
{
    Ems_EmsMapAll(Ems_ToolbarBlock());
    FarPtr base = block_base();
    uint16_t seg = (uint16_t)(base >> 16);
    uint16_t src = (uint16_t)((uint16_t)base + ICON_INDEX(icon));
    uint16_t out = (uint16_t)TMP_PTR;
    uint16_t tseg = (uint16_t)(TMP_PTR >> 16);
    int16_t left = 0x3C0;
    while (left > 0) {
        uint8_t b = *(uint8_t *)far_host(far_ptr(seg, src++));
        if (b < 0xC0) {
            *(uint8_t *)far_host(far_ptr(tseg, out++)) = (uint8_t)(b + 2);
            left--;
        } else {
            uint16_t n = b & 0x3F;
            uint8_t v = *(uint8_t *)far_host(far_ptr(seg, src++));
            left = (int16_t)(left - n);
            for (; n != 0; n--)
                *(uint8_t *)far_host(far_ptr(tseg, out++)) = (uint8_t)(v + 2);
        }
    }  // the original tests != 0 after each run; a damaged icon would run on
    Blit_CopyRectStrided(TMP_PTR, near_add(dest, 0x504), 0x27, 0x18, 0x28, 0x140);
    if (System_MemAvail() < ds.Game_MinMemAvail)
        ds.Game_MinMemAvail = (int32_t)System_MemAvail();
    Ems_EmsNop(Ems_ToolbarBlock());
}

// 3a1b:0932
void GfxPath_ShowScreenBackdrop(void)
{
    FarPtr buf = System_GetMem(0xBCC0);
    uint8_t path[256] = {8, 'g', 'r', 'a', 'f', 'i', 'k', 'a', '\\'};
    const uint8_t *name = ds_at((uint16_t)(0x50FB + ds.Game_CurrentScreen * 25));
    for (uint8_t i = 1; i <= name[0] && path[0] < 255; i++)
        path[++path[0]] = name[i];
    Gfx_LoadPicScreen(path, buf, 0x97, Gfx_PalettePtr());
    Blit_CopyRect(buf, far_ptr(ds.Gfx_ScreenSeg, 0x3D40), 0x140, 0x97);
    System_FreeMem(buf, 0xBCC0);
}

// 3a1b:09a3
void GfxPath_FreeToolbarBuffer(void)
{
    Ems_EmsRelease(Ems_ToolbarBlock());
}

// 3a1b:09b2
void GfxPath_Init(void)
{
    Gfx_IconAllOpenSize(&ICON_SIZE);
    uint16_t size = (uint16_t)(ICON_SIZE + 0x5500 + EXTRA_SIZE);
    BLOCK_INIT = System_GetMem(size);
    System_FillChar(far_host(BLOCK_INIT), size, 2);
    Ems_EmsMoveToEms(Ems_ToolbarBlock(), size, BLOCK_INIT);
    ds.Game_GfxPathProc = far_ptr(0x3A1B, 0x09A3);
}
