#include "game/gfxpath.h"

#include <SDL3/SDL.h>
#include <string.h>

#include "game/blit.h"
#include "game/dseg.h"
#include "game/gfx.h"
#include "game/mouse.h"
#include "game/sound.h"
#include "game/system.h"

// Routines missing from their units' headers.
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

static inline int16_t slot_id(int n)
{
    return menu_slot(n)->Command;
}

static inline void set_slot_id(int n, int16_t id)
{
    menu_slot(n)->Command = id;
}

// Slot n's caption := button label `id` (DS strings both; the caption is a
// string[18]).
static void set_slot_label(int n, int16_t id)
{
    char label[256];
    pstr_set(&menu_slot(n)->Caption, 18, pstr_get(label, sizeof label, &ds.Ui_ButtonLabels[id]));
}

static uint8_t *block_base(void)
{
    return ds.GfxPath_ToolbarBlock.Data;
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
    int16_t id = slot_id(slot);
    int8_t icon = ds.GfxPath_CommandIcon[id];
    if (!check_icon || icon != 0)
        GfxPath_DrawIcon(icon, cell);
    set_slot_label(slot, id);
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

// 3a1b:0030: the screen record's (Ui::Screens) IconCount and Icons.
void GfxPath_SetToolbar(int16_t screen)
{
    if (screen == 1)
        ds.GfxPath_PageFlagB = ds.GfxPath_SavedPage;
    else
        ds.GfxPath_PageFlagB = 1;
    const TScreenInfo *rec = &ds.Ui_Screens[screen - 1];
    int16_t count = (int8_t)rec->IconCount;
    for (int16_t i = 1; i <= count; i++)
        set_slot_id(i, rec->Icons[i - 1]);
    for (int16_t i = (int16_t)(count + 1); i <= 12; i++)
        set_slot_id(i, 0);
    GfxPath_RedrawToolbar();
    ds.Game_HotspotHit = 0;
}

// 3a1b:00bf
void GfxPath_RedrawToolbar(void)
{
    ds.GfxPath_CellBackgrounds = bar(BG_OFS);
    ds.GfxPath_IconScratch = game_alloc(0x3C0);
    ds.GfxPath_PageFlagA = slot_id(7) != 0;
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
    game_free(ds.GfxPath_IconScratch);
}

// 3a1b:0366: puts id in the first free slot.
void GfxPath_AddToolbarIcon(int16_t id)
{
    ds.GfxPath_CellBackgrounds = bar(BG_OFS);
    ds.GfxPath_IconScratch = game_alloc(0x3C0);
    uint16_t slot = 1;
    while (slot_id(slot) != 0)
        slot++;
    if (slot < 13) {
        // Clears slot[slot] 13 - slot times instead of the slots after it, as the
        // original does; toolbars fill without gaps, so those are empty anyway.
        for (uint16_t j = slot; j <= 12; j++)
            set_slot_id(slot, 0);
    }
    set_slot_id(slot, id);
    ds.GfxPath_PageFlagA = (ds.GfxPath_PageFlagA == 0 && slot != 7) ? 0 : 1;

    uint16_t k = (uint16_t)(slot - 1);
    uint8_t *cell = bar(cell_ofs(k));
    clear_cell(cell);
    GfxPath_DrawIcon(ds.GfxPath_CommandIcon[id], cell);
    set_slot_label(slot, id);
    if (slot == 7) {
        draw_arrow(0x30, ARROW_NEXT);
        draw_arrow(0x50, ARROW_PREV);
        for (uint16_t s = 8; s <= 12; s++) {
            clear_cell(bar(cell_ofs((uint16_t)(s - 1))));
            pstr_set(&menu_slot(s)->Caption, 18, "");
        }
    }
    game_free(ds.GfxPath_IconScratch);
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
    uint16_t ofs = 0x140;
    for (int i = 0; i <= 20; i++) {
        scroll_tick();
        copy_to_screen(bar((uint16_t)(ofs + 1)), 0);
        ofs = (uint16_t)(ds.Ui_RowOffset[i] * 0x140 + ofs);
        scroll_extras();
    }
    toggle_pages();
}

// 3a1b:0643
void GfxPath_ScrollToolbarPrev(void)
{
    uint16_t ofs = PAGE_SIZE;
    for (int i = 0; i <= 20; i++) {
        scroll_tick();
        ofs = (uint16_t)(ofs - ds.Ui_RowOffset[i] * 0x140);
        copy_to_screen(bar((uint16_t)(ofs + 1)), 0);
        scroll_extras();
    }
    toggle_pages();
}

// 3a1b:0704
void GfxPath_ShowToolbar(void)
{
    Mouse_HideCursor();
    uint16_t page = ds.GfxPath_PageFlagB == 0 ? (uint16_t)(PAGE_SIZE + 1) : 1;
    Blit_InterlaceReveal(bar(page), gfx_screen(), 0x140, 0x20);
    Mouse_ShowCursor();
    Sound_FreeSampleIfIdle();
}

// 3a1b:0780
void GfxPath_ShowToolbarInstant(void)
{
    Mouse_HideCursor();
    copy_to_screen(bar(1), 0);
    Mouse_ShowCursor();
}

// 3a1b:07ca: scans the slots up to the first empty one (or the end of the
// table, as the original does).
bool GfxPath_ToolbarHasIcon(int16_t id)
{
    bool found = false;
    int16_t i = 1;
    while (slot_id(i) != 0) {
        found = found || slot_id(i) == id;
        i++;
    }
    return found;
}

// 3a1b:0810
void GfxPath_IconAllIndex(void)
{
    Gfx_IconAllOpen();
    uint16_t len;
    for (uint16_t i = 0; i <= 0x43; i++) {
        ds.GfxPath_IconIndex[i] = ds.Gfx_ReadPos;
        Gfx_IconAllReadChunk(ds.GfxPath_ToolbarBlock.Data, &len);
    }
    Gfx_IconAllClose();
}

// 3a1b:0864: RLE-decodes the icon (+2 on every pixel) into the scratch buffer.
void GfxPath_DrawIcon(int16_t icon, uint8_t *dest)
{
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
}

// 3a1b:0932
void GfxPath_ShowScreenBackdrop(void)
{
    uint8_t *buf = game_alloc(0xBCC0);
    char name[256], path[256];
    pstr_get(name, sizeof name, &ds.Ui_Screens[ds.Game_CurrentScreen - 1].Name);
    SDL_snprintf(path, sizeof path, "grafika\\%s", name);
    Gfx_LoadPicScreen(path, buf, 0x97, Gfx_PalettePtr());
    Blit_CopyRect(buf, gfx_screen() + 0x3D40, 0x140, 0x97);
    game_free(buf);
}

// 3a1b:09a3: 4719:0253 EmsRelease on the toolbar block.
void GfxPath_FreeToolbarBuffer(void)
{
    if (ds.GfxPath_ToolbarBlock.State == 1)
        game_free(ds.GfxPath_ToolbarBlock.Data);
    ds.GfxPath_ToolbarBlock.State = 0;
    ds.GfxPath_ToolbarBlock.Size = 0;
}

// 3a1b:09b2
void GfxPath_Init(void)
{
    Gfx_IconAllOpenSize(&ds.GfxPath_IconAllSize);
    uint16_t size = (uint16_t)(ds.GfxPath_IconAllSize + 0x5500 + ds.GfxPath_CellsSize);
    ds.GfxPath_InitBlock = game_alloc(size);
    memset(ds.GfxPath_InitBlock, 2, size);
    // 4719:0188 EmsMoveToEms. The original moved the block into EMS when an
    // EMM was installed (state 2, mapped before every use); the port keeps
    // the heap copy (state 1), as on a DOS machine without an EMM.
    ds.GfxPath_ToolbarBlock.State = 1;
    ds.GfxPath_ToolbarBlock.Size = size;
    ds.GfxPath_ToolbarBlock.Data = ds.GfxPath_InitBlock;
    ds.Game_GfxPathProc = GfxPath_FreeToolbarBuffer;
}
