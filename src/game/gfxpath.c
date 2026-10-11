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

enum : uint16_t {
    PAGE_SIZE = 0x2940,   // 33 rows of 320
    BG_OFS = 0x54FF,      // from the end of the icons: background cells
    ARROW_NEXT = 0x121 + 0x2940,
    ARROW_PREV = 0x121,
};

// Slot records (1..12). The scans for a free or a given slot have no bound:
// past slot 12 they read on into the globals after MenuBar, as the original.
static inline TMenuButton *menu_slot(int n)
{
    return ds.GfxPath_MenuBar + (n - 1);
}

static inline int16_t *slot_id(int n)
{
    return &menu_slot(n)->Command;
}

static inline PString *slot_label(int n)
{
    return (PString *)&menu_slot(n)->Caption;
}

static uint8_t *block_base(void)
{
    return Ems_ToolbarBlock()->Data;
}

// block base + IconAllSize + extra (the original adds in 16 bits; the
// offsets stay inside the block).
static uint8_t *bar(uint16_t extra)
{
    return block_base() + (uint16_t)(ds.GfxPath_IconAllSize + extra);
}

static void copy_to_screen(const uint8_t *src, uint16_t ofs)
{
    Blit_CopyRect(src, gfx_screen() + ofs, 0x140, 0x20);
}

// The empty 48x32 cell, from the backgrounds (240 wide).
static void clear_cell(uint8_t *cell)
{
    Blit_CopyRectStrided(ds.GfxPath_CellBackgrounds, cell, 0x30, 0x20, 0xF0, 0x140);
}

// A 32x32 page arrow from the backgrounds at bg.
static void draw_arrow(uint16_t bg, uint16_t at)
{
    Blit_CopyRectStrided(ds.GfxPath_CellBackgrounds + bg, bar(at), 0x20, 0x20, 0xF0, 0x140);
}

// Cell k (0..11) of the bar: page k / 6, column k % 6.
static uint16_t cell_ofs(uint16_t k)
{
    return (uint16_t)((k % 6) * 0x30 + 1 + (k / 6) * PAGE_SIZE);
}

static void draw_slot(uint16_t k, bool check_icon)
{
    int slot = k + 1;
    uint8_t *cell = bar(cell_ofs(k));
    clear_cell(cell);
    int16_t id = *slot_id(slot);
    int8_t icon = ds.GfxPath_CommandIcon[id];
    if (!check_icon || icon != 0)
        GfxPath_DrawIcon(icon, cell);
    System_StrStore((const PString *)&ds.Ui_ButtonLabels[id], slot_label(slot), 18);
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

// 3a1b:0030: the screen record (Ui::Screens) has the slot count at +0Ch and
// the ids after it.
void GfxPath_SetToolbar(int16_t screen)
{
    if (screen == 1)
        ds.GfxPath_PageFlagB = ds.GfxPath_SavedPage;
    else
        ds.GfxPath_PageFlagB = 1;
    const int8_t *rec = (const int8_t *)&ds.Ui_Screens[screen - 1];
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
    ds.GfxPath_CellBackgrounds = bar(BG_OFS);
    ds.GfxPath_IconScratch = System_GetMem(0x3C0);
    ds.GfxPath_PageFlagA = *slot_id(7) != 0;
    for (uint16_t k = 0; k <= 5; k++)
        draw_slot(k, true);
    if (ds.GfxPath_PageFlagA == 0) {
        draw_arrow(0x70, ARROW_PREV);
    } else {
        for (uint16_t k = 6; k <= 11; k++)
            draw_slot(k, true);
        draw_arrow(0x30, ARROW_NEXT);
        draw_arrow(0x50, ARROW_PREV);
    }
    System_FreeMem(ds.GfxPath_IconScratch, 0x3C0);
    Ems_EmsNop(Ems_ToolbarBlock());
    Ems_EmsMapGameData();
}

// 3a1b:0366: puts id in the first free slot.
void GfxPath_AddToolbarIcon(int16_t id)
{
    Ems_EmsMapAll(Ems_ToolbarBlock());
    ds.GfxPath_CellBackgrounds = bar(BG_OFS);
    ds.GfxPath_IconScratch = System_GetMem(0x3C0);
    uint16_t slot = 1;
    while (*slot_id(slot) != 0)
        slot++;
    if (slot < 13) {
        // BUG? clears slot[slot] 13 - slot times instead of the slots after it.
        for (uint16_t j = slot; j <= 12; j++)
            *slot_id(slot) = 0;
    }
    *slot_id(slot) = id;
    ds.GfxPath_PageFlagA = (ds.GfxPath_PageFlagA == 0 && slot != 7) ? 0 : 1;

    uint16_t k = (uint16_t)(slot - 1);
    uint8_t *cell = bar(cell_ofs(k));
    clear_cell(cell);
    GfxPath_DrawIcon(ds.GfxPath_CommandIcon[id], cell);
    System_StrStore((const PString *)&ds.Ui_ButtonLabels[id], slot_label(slot), 18);
    if (slot == 7) {
        draw_arrow(0x30, ARROW_NEXT);
        draw_arrow(0x50, ARROW_PREV);
        for (uint16_t s = 8; s <= 12; s++) {
            clear_cell(bar(cell_ofs((uint16_t)(s - 1))));
            slot_label(s)->len = 0;
        }
    }
    System_FreeMem(ds.GfxPath_IconScratch, 0x3C0);
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

static void toggle_pages(void)
{
    ds.GfxPath_PageFlagB = ds.GfxPath_PageFlagB == 0;
    // Where MenuBar[0].Command would be; SetToolbar(1) reads it back.
    ds.GfxPath_SavedPage = ds.GfxPath_SavedPage == 0;
}

// 3a1b:0586
void GfxPath_ScrollToolbarNext(void)
{
    Ems_EmsMapAll(Ems_ToolbarBlock());
    uint16_t ofs = 0x140;
    for (int i = 0; i <= 20; i++) {
        scroll_tick();
        copy_to_screen(bar((uint16_t)(ofs + 1)), 0);
        ofs = (uint16_t)(ds.Ui_RowOffset[i] * 0x140 + ofs);
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
        ofs = (uint16_t)(ofs - ds.Ui_RowOffset[i] * 0x140);
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
    uint16_t page = ds.GfxPath_PageFlagB == 0 ? (uint16_t)(PAGE_SIZE + 1) : 1;
    Blit_InterlaceReveal(bar(page), gfx_screen(), 0x140, 0x20);
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
        ds.GfxPath_IconIndex[i] = ds.Gfx_ReadPos;
        Gfx_IconAllReadChunk(&ds.GfxPath_ToolbarBlock.Data, &len);
    }
    Gfx_IconAllClose();
    Ems_EmsNop(Ems_ToolbarBlock());
    Ems_EmsMapGameData();
}

// 3a1b:0864: RLE-decodes the icon (+2 on every pixel) into the scratch buffer.
void GfxPath_DrawIcon(int16_t icon, uint8_t *dest)
{
    Ems_EmsMapAll(Ems_ToolbarBlock());
    const uint8_t *src = block_base() + ds.GfxPath_IconIndex[icon];
    uint8_t *out = ds.GfxPath_IconScratch;
    int16_t left = 0x3C0;
    while (left > 0) {
        uint8_t b = *src++;
        if (b < 0xC0) {
            *out++ = (uint8_t)(b + 2);
            left--;
        } else {
            uint16_t n = b & 0x3F;
            uint8_t v = *src++;
            left = (int16_t)(left - n);
            for (; n != 0; n--)
                *out++ = (uint8_t)(v + 2);
        }
    }  // the original tests != 0 after each run; a damaged icon would run on
    Blit_CopyRectStrided(ds.GfxPath_IconScratch, dest + 0x504, 0x27, 0x18, 0x28, 0x140);
    if (System_MemAvail() < ds.Game_MinMemAvail)
        ds.Game_MinMemAvail = (int32_t)System_MemAvail();
    Ems_EmsNop(Ems_ToolbarBlock());
}

// 3a1b:0932
void GfxPath_ShowScreenBackdrop(void)
{
    uint8_t *buf = System_GetMem(0xBCC0);
    uint8_t path[256] = {8, 'g', 'r', 'a', 'f', 'i', 'k', 'a', '\\'};
    const uint8_t *name = (const uint8_t *)&ds.Ui_Screens[ds.Game_CurrentScreen - 1].Name;
    for (uint8_t i = 1; i <= name[0] && path[0] < 255; i++)
        path[++path[0]] = name[i];
    Gfx_LoadPicScreen(path, buf, 0x97, Gfx_PalettePtr());
    Blit_CopyRect(buf, gfx_screen() + 0x3D40, 0x140, 0x97);
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
    Gfx_IconAllOpenSize(&ds.GfxPath_IconAllSize);
    uint16_t size = (uint16_t)(ds.GfxPath_IconAllSize + 0x5500 + ds.GfxPath_CellsSize);
    ds.GfxPath_InitBlock = System_GetMem(size);
    System_FillChar(ds.GfxPath_InitBlock, size, 2);
    Ems_EmsMoveToEms(Ems_ToolbarBlock(), size, ds.GfxPath_InitBlock);
    ds.Game_GfxPathProc = far_ptr(0x3A1B, 0x09A3);
}
