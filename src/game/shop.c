#include "game/shop.h"

#include "game/blit.h"
#include "game/gfx.h"
#include "game/gfxpath.h"
#include "game/message.h"
#include "game/mouse.h"
#include "game/rawtext.h"
#include "game/sound.h"
#include "game/system.h"
#include "game/vecload.h"

#include <string.h>

#define PRODUCT(k) ds.Research_Products[(k) - 1]
#define SEL_PRODUCT_ID ds.Shop_List[ds.Shop_Sel - 1]
#define SEL_PRODUCT (&PRODUCT(SEL_PRODUCT_ID))
#define SPACE_STATION PRODUCT(24) // Research::Products[0x17] in the decompile (index 24)

static const uint8_t *ps(const PString *p)
{
    return (const uint8_t *)p;
}

// OreLines[i + 1] (i = 0..5) as a PString.
static PString *ore_line(int i)
{
    return (PString *)&ds.Shop_OreLines[i];
}

// Each Pascal literal is a const PString; ps(PSTR("...")) gives the pointer
// the drawing routines take.
#define LIT(s) ps(PSTR(s))

// System.LongDivMod's quotient (signed; a zero divisor is run-time error 200).
static int32_t long_div(int32_t a, int32_t b)
{
    if (b == 0)
        System_RunError(200);
    return a / b;
}

// Pascal LongMul: 32-bit product, overflow wraps.
static int32_t long_mul(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a * (uint32_t)b);
}

int32_t Shop_OrderHours(const TProduct *p, int32_t qty)
{
    int32_t rate = ds.Game_ProductionRate;
    int32_t work;
    if (p->_21 == 0)
        work = long_mul((int16_t)p->_1F, qty);
    else
        work = long_mul((int32_t)(qty - 1), (int16_t)p->_1F) + (int16_t)p->_21;
    return long_div(work + rate - 1, rate);
}

int32_t Shop_QueueHours(const TProduct *p)
{
    int32_t rate = ds.Game_ProductionRate;
    int32_t work = long_mul((int16_t)(p->_1D - 1), (int16_t)p->_1F) + (int16_t)p->_21;
    return long_div(work + rate - 1, rate);
}

int32_t Shop_OrderPrice(const TProduct *p, int32_t qty)
{
    return long_mul(qty, p->Price);
}

// 32b4:0057
void Shop_ShopScreen(void)
{
    if (ds.Game_ScreenEntered) {
        ds.VecLoad_Angles[0] = 0;
        ds.VecLoad_Angles[1] = 0;
        ds.VecLoad_Angles[2] = 0;
        ds.VecLoad_Spinning = 1;
        ds.Shop_ListMode = false;
        ds.Shop_QtyEdit = 0;
        GfxPath_ShowScreenBackdrop();
        Shop_BuildProductList();
        ds.Shop_PrevSel = ds.Shop_Sel;
        ds.Shop_PrevSel2 = ds.Shop_Sel;
        Shop_InitProductPicture();
        if (SEL_PRODUCT->_1D == 0)
            Shop_DrawNotProducible();
        else
            Shop_DrawQueueSummary();
        ds.Shop_ShowPicture = 1;
        Shop_SetupShopHotspots();
        ds.Gfx_Palette[0xC0][0] = 0;
        ds.Gfx_Palette[0xC0][1] = 0;
        ds.Gfx_Palette[0xC0][2] = 0x3C;
        ds.VecLoad_Buffer = System_GetMem(0xAF00);
        VecLoad_LoadVec(SEL_PRODUCT_ID);
        VecLoad_DrawBlueFrame();
        Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
        RawText_LoadSzTalal();
    }
    if ((ds.Game_MenuCommand == 0x1F || ds.Game_HotspotHit == 0x16) && !ds.Shop_ListMode) {
        Sound_PlayButtonSfx(LIT("select"));
        GfxPath_SetToolbarAndShow(0xE);
        ds.Game_HotspotHit = 0;
        ds.VecLoad_Spinning = 0;
        ds.Shop_ListMode = true;
        Shop_SetupShopListHotspots();
        Shop_ShowSelectPanel();
        ds.Shop_PrevTop = 0;
        if (ds.Shop_Sel < 8)
            ds.Shop_Top = 1;
        else
            ds.Shop_Top = (int16_t)(ds.Shop_Sel - 7);
        if (ds.Shop_Count <= ds.Shop_Top + 0xF)
            ds.Shop_Top = (int16_t)(ds.Shop_Count - 0xE);
        if (ds.Shop_Top < 1)
            ds.Shop_Top = 1;
        Shop_DrawProductList();
    }
    if (ds.Game_MenuCommand == 0x23) {
        GfxPath_SetToolbarAndShow(5);
        ds.Game_HotspotHit = 0;
        ds.Shop_ListMode = false;
        ds.VecLoad_Spinning = 1;
        Shop_ShowInfoPanel();
        VecLoad_LoadVec(SEL_PRODUCT_ID);
        VecLoad_DrawBlueFrame();
        Shop_SetupShopHotspots();
    }
    // Start the quantity editor: take back what is already queued.
    if (ds.Game_MenuCommand == 0x12) {
        TProduct *p = SEL_PRODUCT;
        if (p->Produced != 0 || (p->SpaceBuilt != 0 && SPACE_STATION.State == 5)) {
            ds.Game_ClockRunning = false;
            GfxPath_SetToolbarAndShow(0xF);
            ds.Mouse_HotspotCount = 0;
            ds.Shop_QtyEdit = 1;
            ds.Shop_Qty = (int16_t)p->_1D;
            for (int k = 1; k <= 6; k++)
                ds.Shop_OreStock[k - 1] =
                    long_mul(p->Ores[k - 1], ds.Shop_Qty) + ds.Game_HomeStock[k - 1];
            ds.Game_Credits += long_mul(p->Price, ds.Shop_Qty);
            Gfx_DrawCredits();
            Shop_DrawOrderSummary();
            Shop_DrawProductInfo(false);
            ds.Shop_ShowPicture = 0;
        }
    }
    if (ds.Shop_ListMode && ds.Shop_QtyEdit == 0 && ds.Mouse_ButtonsChanged)
        Shop_HandleListClick();
    ds.Shop_PrevSel = ds.Shop_Sel;
    ds.Shop_PrevTop = ds.Shop_Top;
    if (ds.Game_HotspotHit == 0x15 && ds.Shop_QtyEdit == 0) {
        Sound_PlayButtonSfx(LIT("x"));
        ds.Shop_ShowPicture = ds.Shop_ShowPicture == 0;
        if (ds.Shop_ShowPicture)
            Shop_ShowProductPicture(false);
        else
            Shop_DrawProductInfo(false);
        Shop_SetupShopHotspots();
        if (ds.Shop_ListMode)
            Shop_SetupShopListHotspots();
    }
    if (ds.Game_HotspotHit == 0x17 || ds.Game_HotspotHit == 0x18)
        Shop_ScrollListUp();
    else if (ds.Game_HotspotHit == 0x19 || ds.Game_HotspotHit == 0x1A)
        Shop_ScrollListDown();
    if ((uint16_t)ds.Game_HotspotHit > 0x16 && (uint16_t)ds.Game_HotspotHit < 0x1B)
        Shop_DrawProductList();
    if (ds.Game_MenuCommand == 0x16 && ds.Shop_Sel > 1) {
        ds.Shop_Sel--;
        if (ds.Shop_ListMode) {
            if (ds.Shop_Sel < ds.Shop_Top)
                ds.Shop_Top = ds.Shop_Sel;
            Shop_DrawProductList();
        }
    }
    int16_t next = ds.Shop_Sel;
    if (ds.Game_MenuCommand == 0x15 && ds.Shop_Sel < ds.Shop_Count) {
        next = (int16_t)(ds.Shop_Sel + 1);
        if (ds.Shop_ListMode) {
            if (ds.Shop_Top + 0xD <= next)
                ds.Shop_Top = (int16_t)(ds.Shop_Sel - 0xB);
            ds.Shop_Sel = next;
            Shop_DrawProductList();
            next = ds.Shop_Sel;
        }
    }
    ds.Shop_Sel = next;
    if (ds.Shop_PrevSel != ds.Shop_Sel) {
        VecLoad_LoadVec(SEL_PRODUCT_ID);
        if (ds.Shop_ShowPicture == 0)
            Shop_DrawProductInfo(true);
        else
            Shop_ShowProductPicture(true);
        if (SEL_PRODUCT->_1D == 0)
            Shop_DrawNotProducible();
        else
            Shop_DrawQueueSummary();
    }
    if ((uint16_t)ds.Game_HotspotHit > 0x16 && (uint16_t)ds.Game_HotspotHit < 0x1B
        && ds.Shop_Sel != ds.Shop_PrevSel2 && ds.Mouse_Buttons == 0) {
        VecLoad_LoadVec(SEL_PRODUCT_ID);
        if (ds.Shop_ShowPicture == 0)
            Shop_DrawProductInfo(true);
        else
            Shop_ShowProductPicture(true);
        if (!ds.Shop_ListMode)
            Shop_SetupShopHotspots();
        else
            Shop_SetupShopListHotspots();
        if (SEL_PRODUCT->_1D == 0)
            Shop_DrawNotProducible();
        else
            Shop_DrawQueueSummary();
        ds.Shop_PrevSel2 = ds.Shop_Sel;
    }

    if (ds.Shop_QtyEdit != 0) {
        TProduct *p = SEL_PRODUCT;
        ds.Shop_PrevQty = ds.Shop_Qty;
        switch (ds.Game_MenuCommand) {
        case 0x1A: ds.Shop_Qty += 1; break;
        case 0x1B: ds.Shop_Qty -= 1; break;
        case 0x1C: ds.Shop_Qty += 10; break;
        case 0x1D: ds.Shop_Qty -= 10; break;
        default: break;
        }
        if (ds.Game_MenuCommand == 0x1E && ds.Shop_Qty > 0) {
            if (p->_21 == 0)
                p->_21 = p->_1F;
            p->_1D = (uint16_t)ds.Shop_Qty;
            if (!ds.Shop_ListMode)
                Shop_SetupShopHotspots();
            else
                Shop_SetupShopListHotspots();
        }
        if (ds.Game_MenuCommand == 0x20)
            ds.Shop_Qty = (int16_t)p->_1D;
        if (ds.Game_MenuCommand == 0x1E || ds.Game_MenuCommand == 0x20) {
            // Leave the editor: pay for the order and book the ore it uses.
            ds.Game_ClockRunning = true;
            ds.Game_Credits -= long_mul(p->Price, ds.Shop_Qty);
            for (int k = 1; k <= 6; k++)
                ds.Game_HomeStock[k - 1] =
                    ds.Shop_OreStock[k - 1] - long_mul(p->Ores[k - 1], ds.Shop_Qty);
            Gfx_DrawCredits();
            if (ds.Shop_Qty == 0)
                Shop_DrawNotProducible();
            else
                Shop_DrawQueueSummary();
            ds.Shop_QtyEdit = 0;
            GfxPath_SetToolbarAndShow(ds.Shop_ListMode ? 0xE : 5);
            if (!ds.Shop_ListMode)
                ds.VecLoad_Spinning = 1;
            ds.Shop_ShowPicture = 1;
            Shop_ShowProductPicture(false);
            if (!ds.Shop_ListMode)
                Shop_SetupShopHotspots();
            else
                Shop_SetupShopListHotspots();
        }
        if (ds.Shop_Qty < 0)
            ds.Shop_Qty = 0;
        if ((int16_t)p->_21 > 0 && ds.Shop_Qty == 0)
            ds.Shop_Qty = 1;
        if (ds.Game_Credits < long_mul(p->Price, ds.Shop_Qty))
            Gfx_FlashCredits();
        while (long_mul(p->Price, ds.Shop_Qty) > ds.Game_Credits)
            ds.Shop_Qty--;
        if (p->SpaceBuilt != 0) {
            if ((int32_t)SPACE_STATION.Stock < ds.Shop_Qty)
                Message_ShowMessageNoLog(
                    LIT(" You can build only one Destroyer or Cruiser |             on a space station"));
            while ((int32_t)SPACE_STATION.Stock < ds.Shop_Qty)
                ds.Shop_Qty--;
        }
        if (ds.Shop_QtyEdit != 0 && ds.Shop_Qty != ds.Shop_PrevQty) {
            Shop_UpdateOrderOres();
            Shop_DrawOreTextsMarked();
            Shop_DrawOrderSummary();
        }
    }
}

// Shared by both panels: LoadPic into a temporary buffer, blit 126x126 to
// screen offset 3D40h, free it.
static void show_panel(const PString *name, bool keep_far)
{
    FarPtr buf = System_GetMem(0x9EC0);
    if (keep_far)
        ds.Shop_PanelBuf = buf;
    Gfx_LoadPic(ps(name), far_host(buf), 1, 0x7F, 0, 0, 0x140, false, Gfx_PalettePtr(), 0, 0x1F, 0x40, 0, 0x1F);
    Blit_CopyRect(far_host(buf), gfx_screen() + 0x3D40, 0x7E, 0x7E);
    System_FreeMem(buf, 0x9EC0);
}

// 32b4:074d
void Shop_ShowSelectPanel(void)
{
    show_panel(PSTR("grafika\\select"), true);
}

// 32b4:07c8
void Shop_ShowInfoPanel(void)
{
    show_panel(PSTR("grafika\\info"), false);
}

// Pictures of the product use palette entries 60h..F6h; F9h..FEh are copied to F1h..F6h.
static void load_product_picture(bool fade)
{
    PString num, name;
    System_StrLong(SEL_PRODUCT_ID, 0, &num, 255);
    FarPtr buf = System_GetMem(24000);
    System_StrLoad(&name, PSTR("info\\info"));
    System_StrConcat(&name, &num);
    Gfx_LoadPic(ps(&name), far_host(buf), 1, 0x79, 0, 0, 0x140, fade, Gfx_PalettePtr(), 0, 0x5F, 0x60, 0, 0x96);
    Mouse_HideCursor();
    Blit_CopyRectStrided((uint8_t *)far_host(buf) + 0x486, gfx_screen() + 0x4546, 0xB4, 0x73,
                         0xC0, 0x140);
    Mouse_ShowCursor();
    System_FreeMem(buf, 24000);
    for (int i = 0; i < 6; i++)
        System_MoveFwd(ds.Gfx_Palette[0xF9 + i], ds.Gfx_Palette[0xF1 + i], 3);
}

// 32b4:083a
void Shop_ShowProductPicture(bool itemChanged)
{
    load_product_picture(true);
    if (!ds.Shop_ListMode && itemChanged)
        VecLoad_DrawBlueFrame();
    Gfx_FadeIn(0x60, 0xF6, Gfx_PalettePtr());
}

// 32b4:098f
void Shop_InitProductPicture(void)
{
    VecLoad_AllocVecBuffers();
    load_product_picture(false);
}

// 32b4:0aac
void Shop_DrawProductList(void)
{
    int16_t n = ds.Shop_Count;
    if (n > 0xD)
        n = 0xD;
    if ((ds.Shop_PrevSel != ds.Shop_Sel || ds.Shop_PrevTop != ds.Shop_Top) && n > 0) {
        for (int16_t i = 1; i <= n; i++) {
            const TProduct *p = &PRODUCT(ds.Shop_List[ds.Shop_Top + i - 2]);
            const uint8_t *name = ps((const PString *)&p->Name);
            if (ds.Shop_Top + i - 1 == ds.Shop_Sel)
                Gfx_DrawTextFC(name, 0x10, (int16_t)(i * 9 + 0x2E), 0x10);
            else
                Gfx_DrawTextF9(name, 0x10, (int16_t)(i * 9 + 0x2E), 0x10);
        }
    }
}

// Line `line` (1..7) of product `id`'s description in sz_talal.raw (7 x string[30]).
static const uint8_t *talal_line(uint8_t id, int line)
{
    FarPtr base = ds.RawText_SzTalal;
    return far_host(far_ptr((uint16_t)(base >> 16),
                            (uint16_t)((uint16_t)base + (id - 1) * 0xD9 + (line - 1) * 0x1F)));
}

// 32b4:0b57
void Shop_DrawProductInfo(bool itemChanged)
{
    int16_t extra = itemChanged ? 1 : 0;
    Gfx_FadeOut(0x60, (int16_t)(0xF6 + extra), Gfx_PalettePtr());
    Mouse_HideCursor();
    Blit_FillRect(0x86, 0x37, 0xB4, 0x73, 0x40);
    Mouse_ShowCursor();
    Gfx_DrawTextF4(talal_line(SEL_PRODUCT_ID, 1), 0x88, 0x3B, 0x1D);
    if (ds.Shop_QtyEdit == 0) {
        for (int k = 2; k <= 7; k++)
            Gfx_DrawTextF1(talal_line(SEL_PRODUCT_ID, k), 0x88, (int16_t)(k * 10 + 0x33), 0x1D);
    } else {
        Shop_ShowOreStock();
    }
    if (ds.Shop_QtyEdit == 0)
        Shop_SetupOnePieceOreText();
    else
        Shop_UpdateOrderOres();
    Shop_DrawOreTexts();
    if (!ds.Shop_ListMode && itemChanged)
        VecLoad_DrawBlueFrame();
    Gfx_FadeIn(0x60, (int16_t)(0xF6 + extra), Gfx_PalettePtr());
}

// 32b4:0c50
void Shop_BuildProductList(void)
{
    ds.Shop_Sel = 1;
    ds.Shop_Count = 0;
    for (int16_t i = 1; i <= 0x23; i++) {
        if (PRODUCT(i).State == 5) {
            ds.Shop_List[ds.Shop_Count] = (uint8_t)i;
            ds.Shop_Count++;
            if (ds.Game_PrevScreen == 3 && ds.Game_SelInvention == i)
                ds.Shop_Sel = ds.Shop_Count;
        }
    }
}

// Ore name + value, right aligned in 6 columns (a leading blank becomes ':').
static void format_ore(PString *result, int ore, int32_t value)
{
    PString num, t;
    System_StrLong(value, 6, &num, 255);
    if (num.s[0] == ' ')
        num.s[0] = ':';
    System_StrLoad(&t, (const PString *)&ds.Names_Ores[ore - 1]);
    System_StrConcat(&t, &num);
    System_StrStore(&t, result, 255);
}

// The six ore lines are stored in the order 1, 3, 2, 6, 5, 4 (two columns).
static const uint8_t ORE_ORDER[6] = {1, 3, 2, 6, 5, 4};

// 32b4:0d21
void Shop_UpdateOrderOres(void)
{
    const TProduct *p = SEL_PRODUCT;
    for (int k = 1; k <= 6; k++)
        ds.Shop_OreShort[k - 1] = 0;
    // Reduce the quantity until every ore is in stock.
    for (int k = 1; k <= 6; k++) {
        while (long_mul(p->Ores[k - 1], ds.Shop_Qty) > ds.Shop_OreStock[k - 1])
            ds.Shop_Qty--;
    }
    for (int k = 1; k <= 6; k++) {
        ds.Shop_OreNeeded[k - 1] = long_mul(p->Ores[k - 1], ds.Shop_Qty);
        if (long_mul(ds.Shop_Qty + 1, p->Ores[k - 1]) > ds.Shop_OreStock[k - 1])
            ds.Shop_OreShort[k - 1] = 1;
    }
    PString num, t;
    System_StrLong(ds.Shop_Qty, 0, &num, 255);
    System_StrLoad(&t, PSTR("Ore needed: ("));
    System_StrConcat(&t, &num);
    System_StrConcat(&t, PSTR(" piece"));
    System_StrStore(&t, (PString *)&ds.Shop_OreHeader, 30);
    System_StrLoad(&t, (const PString *)&ds.Shop_OreHeader);
    System_StrConcat(&t, ds.Shop_Qty < 2 ? PSTR(" )") : PSTR("s )"));
    System_StrStore(&t, (PString *)&ds.Shop_OreHeader, 30);
    for (int i = 0; i < 6; i++) {
        format_ore(&t, ORE_ORDER[i], ds.Shop_OreNeeded[ORE_ORDER[i] - 1]);
        System_StrStore(&t, ore_line(i), 15);
    }
}

// 32b4:1003
void Shop_SetupOnePieceOreText(void)
{
    PString t;
    System_StrStore(PSTR("Ore needed: (One piece)"), (PString *)&ds.Shop_OreHeader, 30);
    for (int i = 0; i < 6; i++) {
        format_ore(&t, ORE_ORDER[i], SEL_PRODUCT->Ores[ORE_ORDER[i] - 1]);
        System_StrStore(&t, ore_line(i), 15);
    }
}

// Ore line positions: two columns, three rows.
static const struct { int16_t x, y, w; } ORE_POS[6] = {
    {0x8E, 0x8E, 0xD}, {0xE1, 0x8E, 0xE}, {0x8E, 0x97, 0xD},
    {0xE1, 0x97, 0xE}, {0x8E, 0xA0, 0xD}, {0xE1, 0xA0, 0xE},
};

// 32b4:10bd
void Shop_DrawOreTexts(void)
{
    Gfx_DrawTextF4(ps((const PString *)&ds.Shop_OreHeader), 0x88, 0x85, 0x1D);
    for (int i = 0; i < 6; i++)
        Gfx_DrawTextF1(ps(ore_line(i)), ORE_POS[i].x, ORE_POS[i].y, ORE_POS[i].w);
}

// 32b4:1141
void Shop_DrawOreTextsMarked(void)
{
    Gfx_DrawTextF4(ps((const PString *)&ds.Shop_OreHeader), 0x88, 0x85, 0x1D);
    bool any = false;
    for (int i = 0; i < 6; i++) {
        bool flagged = ds.Shop_OreShort[ORE_ORDER[i] - 1] != 0;
        if (flagged)
            Gfx_DrawTextF4(ps(ore_line(i)), ORE_POS[i].x, ORE_POS[i].y, ORE_POS[i].w);
        else
            Gfx_DrawTextF1(ps(ore_line(i)), ORE_POS[i].x, ORE_POS[i].y, ORE_POS[i].w);
        any = any || flagged;
    }
    if (any) {
        // The warning blink: the flagged lines stay highlighted for 8 frames.
        Gfx_WaitFrames(8);
        Shop_DrawOreTexts();
    }
}

// 32b4:1314
void Shop_ShowOreStock(void)
{
    static const int16_t ys[6] = {0x52, 0x52, 0x5B, 0x5B, 0x64, 0x64};
    PString t;
    System_StrStore(PSTR("Ores in stock"), (PString *)&ds.Shop_OreHeader, 30);
    for (int i = 0; i < 6; i++) {
        format_ore(&t, ORE_ORDER[i], ds.Shop_OreStock[ORE_ORDER[i] - 1]);
        System_StrStore(&t, ore_line(i), 15);
    }
    Gfx_DrawTextF4(ps((const PString *)&ds.Shop_OreHeader), 0x88, 0x49, 0x1D);
    for (int i = 0; i < 6; i++)
        Gfx_DrawTextF1(ps(ore_line(i)), ORE_POS[i].x, ys[i], ORE_POS[i].w);
}

// Label / value pair of the summary.
static void field(const char *label, int16_t lx, int16_t ly, int16_t lw, int32_t value,
                  int16_t vx, int16_t vw, bool labels)
{
    PString num, lab;
    System_StrLong(value, 0, &num, 255);
    if (labels) {
        lab.len = (uint8_t)strlen(label);
        memcpy(lab.s, label, lab.len);
        Gfx_DrawTextF9(ps(&lab), lx, ly, lw);
    }
    Gfx_DrawTextFC(ps(&num), vx, ly, vw);
}

// 32b4:1474
void Shop_DrawOrderSummary(void)
{
    const TProduct *p = SEL_PRODUCT;
    field("Bought items:", 0xE, 0xB4, 0xF, ds.Shop_Qty, 100, 5, true);
    field("Time to go:", 0xA0, 0xB4, 0x11, Shop_OrderHours(p, ds.Shop_Qty), 0x108, 6, true);
    field("Stores:", 0xE, 0xBD, 0x13, p->Stock, 100, 10, true);
    field("Total price:", 0xA0, 0xBD, 0x11, Shop_OrderPrice(p, ds.Shop_Qty), 0x108, 8, true);
}

// 32b4:1666
void Shop_DrawQueueSummary(void)
{
    const TProduct *p = SEL_PRODUCT;
    field("Bought items:", 0xE, 0xB4, 0xF, (int16_t)p->_1D, 100, 5, true);
    field("Time to go:", 0xA0, 0xB4, 0x11, Shop_QueueHours(p), 0x108, 6, true);
    field("Stores:", 0xE, 0xBD, 0x13, p->Stock, 100, 10, true);
    field("Total price:", 0xA0, 0xBD, 0x11, Shop_OrderPrice(p, (int16_t)p->_1D), 0x108, 8, true);
}

// 32b4:18a1
void Shop_DrawNotProducible(void)
{
    uint8_t id = SEL_PRODUCT_ID;
    const TProduct *p = SEL_PRODUCT;
    Gfx_DrawTextF9(LIT(""), 0xE, 0xB4, 0x1A);
    Gfx_DrawTextF9(LIT(""), 0xA0, 0xB4, 0x1A);
    Gfx_DrawTextF9(LIT(""), 0xA0, 0xBD, 0x1A);
    bool stores = p->Produced != 0 || (p->SpaceBuilt != 0 && SPACE_STATION.State == 5);
    if (!stores) {
        // One expression: a LIT() literal inside an if branch would end with
        // the branch (docs/port-bugs.md #32).
        const uint8_t *msg =
            id == 8 || id == 12 || id == 0x22 ? LIT("You only need one of this item.")
            : id == 14 || id == 18            ? LIT("This item will be fitted on the ships")
            : id == 0x1C                      ? LIT("This is fitted on the colony centres")
            : id == 0x17                      ? LIT("This cannot be produced on New Earth")
            : id == 1 || id == 7 || id == 0x1D || id == 0x1E || id == 0x23
                ? LIT("This can be built on your colonies")
                : LIT("This cannot be produced");
        Gfx_DrawTextFC(msg, 0xE, 0xBD, 0x28);
        return;
    }
    PString num;
    System_StrLong(p->Stock, 0, &num, 255);
    Gfx_DrawTextF9(LIT("Stores:"), 0xE, 0xBD, 0x13);
    Gfx_DrawTextFC(ps(&num), 100, 0xBD, 0xA);
}

// 32b4:1a29: the numbers of DrawQueueSummary without the labels.
void Shop_DrawQueueNumbers(void)
{
    const TProduct *p = SEL_PRODUCT;
    field("", 0, 0xB4, 0, (int16_t)p->_1D, 100, 5, false);
    field("", 0, 0xB4, 0, Shop_QueueHours(p), 0x108, 6, false);
    field("", 0, 0xBD, 0, p->Stock, 100, 10, false);
    field("", 0, 0xBD, 0, Shop_OrderPrice(p, (int16_t)p->_1D), 0x108, 8, false);
}

// 32b4:1b4a
void Shop_DrawQueueTime(void)
{
    field("", 0, 0xB4, 0, Shop_QueueHours(SEL_PRODUCT), 0x108, 6, false);
}

// Fills hotspot j (1-based) like the original's inline code.
static void set_hot(int16_t j, int16_t x, uint8_t y, int16_t w, uint8_t h, const PString *caption,
                    uint8_t repeat)
{
    THotspot *s = Mouse_Hotspot(j);
    s->X = x;
    s->Y = y;
    s->W = w;
    s->H = h;
    if (caption)
        System_StrStore(caption, (PString *)&s->Caption, 0x12);
    else
        s->Caption.len = 0;
    s->Repeat_ = repeat;
    s->_1A = 0;
}

// 32b4:1bf2
void Shop_SetupShopHotspots(void)
{
    ds.Mouse_HotspotCount = 2;
    set_hot(1, 0x81, 0x31, 0xBE, 0x7F,
            ds.Shop_ShowPicture == 0 ? PSTR("See picture") : PSTR("See info"), 1);
    set_hot(2, 0, 0x31, 0x80, 0x7F, PSTR("Select"), 1);
}

// 32b4:1ca3
void Shop_SetupShopListHotspots(void)
{
    ds.Mouse_HotspotCount = 6;
    set_hot(1, 0x81, 0x31, 0xBE, 0x7F,
            ds.Shop_ShowPicture == 0 ? PSTR("See picture") : PSTR("See info"), 1);
    set_hot(2, 0xC, 0x31, 0x68, 0x80, nullptr, 0);
    set_hot(3, 0, 0x31, 0xB, 0x12, PSTR("Scroll up"), 0);
    set_hot(4, 0x72, 0x31, 0xC, 0x12, PSTR("Scroll up"), 0);
    set_hot(5, 0, 0x9E, 0xB, 0x12, PSTR("Scroll down"), 0);
    set_hot(6, 0x72, 0x9E, 0xC, 0x12, PSTR("Scroll down"), 0);
}

// 32b4:1de0
void Shop_ScrollListUp(void)
{
    if (ds.Shop_Top > 1) {
        int16_t old_top = ds.Shop_Top;
        ds.Shop_Top = (int16_t)(old_top - 1);
        if (old_top + 0xC <= ds.Shop_Sel)
            ds.Shop_Sel--;
    }
}

// 32b4:1e00
void Shop_ScrollListDown(void)
{
    if (ds.Shop_Top + 0xC < ds.Shop_Count) {
        ds.Shop_Top++;
        if (ds.Shop_Sel < ds.Shop_Top)
            ds.Shop_Sel++;
    }
}

// Row 0..14 of the list column under the click, clamped.
static int16_t click_row(void)
{
    int16_t row = (int16_t)((ds.Mouse_ClickY - 0x30) / 9);
    if (row < 0)
        row = 0;
    if (row > 0xE)
        row = 0xE;
    return row;
}

// 32b4:1e23: a click in the list column selects a row; holding the button on
// the first/last row scrolls while redrawing.
void Shop_HandleListClick(void)
{
    int16_t old_sel = ds.Shop_Sel;
    int16_t row = click_row();
    int16_t col = ds.Mouse_ClickX >= 0xE;
    if (ds.Mouse_ClickX > 0x70)
        col = 2;
    if (col == 1 && row > 0 && row < 0xE && ds.Mouse_Buttons != 0) {
        do {
            if (row == 0) {
                ds.Shop_PrevSel = ds.Shop_Sel;
                ds.Shop_PrevTop = ds.Shop_Top;
                Shop_ScrollListUp();
                ds.Shop_Sel = ds.Shop_Top;
                Shop_DrawProductList();
            } else if (row == 0xE) {
                ds.Shop_PrevSel = ds.Shop_Sel;
                ds.Shop_PrevTop = ds.Shop_Top;
                Shop_ScrollListDown();
                ds.Shop_Sel = (int16_t)(ds.Shop_Top + 0xC);
                if (ds.Shop_Count < ds.Shop_Sel)
                    ds.Shop_Sel = ds.Shop_Count;
                Shop_DrawProductList();
            } else {
                ds.Shop_PrevSel = ds.Shop_Sel;
                ds.Shop_PrevTop = ds.Shop_Top;
                ds.Shop_Sel = (int16_t)(ds.Shop_Top + row - 1);
                if (ds.Shop_Count < ds.Shop_Sel)
                    ds.Shop_Sel = ds.Shop_Count;
                Shop_DrawProductList();
            }
            Mouse_Poll();
            row = click_row();
            col = ds.Mouse_ClickX >= 0x10;
            if (ds.Mouse_ClickX > 0x70)
                col = 2;
        } while (ds.Mouse_Buttons != 0);
    }
    if (ds.Shop_Sel != old_sel) {
        if (SEL_PRODUCT->_1D == 0)
            Shop_DrawNotProducible();
        else
            Shop_DrawQueueSummary();
        if (ds.Shop_ShowPicture == 0)
            Shop_DrawProductInfo(true);
        else
            Shop_ShowProductPicture(true);
        Shop_SetupShopHotspots();
        if (ds.Shop_ListMode)
            Shop_SetupShopListHotspots();
        ds.Shop_PrevSel = ds.Shop_Sel;
        ds.Shop_PrevSel2 = ds.Shop_Sel;
    }
}
