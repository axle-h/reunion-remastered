// Differential test of unit Shop (32b4): the production orders screen,
// original (under Unicorn) against the port, from the shop screen of a new
// game (picture and list modes) with the finished products, their orders and
// the screen's state perturbed. See docs/difftest.md. Needs the game data.
#include <stdio.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/shop.h"

DIFF_PROC(Shop_ShopScreen, Shop_ShopScreen());
DIFF_PROC(Shop_ShowSelectPanel, Shop_ShowSelectPanel());
DIFF_PROC(Shop_ShowInfoPanel, Shop_ShowInfoPanel());
DIFF_PROC(Shop_ShowProductPicture, Shop_ShowProductPicture(a[0] != 0));
DIFF_PROC(Shop_InitProductPicture, Shop_InitProductPicture());
DIFF_PROC(Shop_DrawProductList, Shop_DrawProductList());
DIFF_PROC(Shop_DrawProductInfo, Shop_DrawProductInfo(a[0] != 0));
DIFF_PROC(Shop_BuildProductList, Shop_BuildProductList());
DIFF_PROC(Shop_UpdateOrderOres, Shop_UpdateOrderOres());
DIFF_PROC(Shop_SetupOnePieceOreText, Shop_SetupOnePieceOreText());
DIFF_PROC(Shop_DrawOreTexts, Shop_DrawOreTexts());
DIFF_PROC(Shop_DrawOreTextsMarked, Shop_DrawOreTextsMarked());
DIFF_PROC(Shop_ShowOreStock, Shop_ShowOreStock());
DIFF_PROC(Shop_DrawOrderSummary, Shop_DrawOrderSummary());
DIFF_PROC(Shop_DrawQueueSummary, Shop_DrawQueueSummary());
DIFF_PROC(Shop_DrawNotProducible, Shop_DrawNotProducible());
DIFF_PROC(Shop_DrawQueueNumbers, Shop_DrawQueueNumbers());
DIFF_PROC(Shop_DrawQueueTime, Shop_DrawQueueTime());
DIFF_PROC(Shop_SetupShopHotspots, Shop_SetupShopHotspots());
DIFF_PROC(Shop_SetupShopListHotspots, Shop_SetupShopListHotspots());
DIFF_PROC(Shop_ScrollListUp, Shop_ScrollListUp());
DIFF_PROC(Shop_ScrollListDown, Shop_ScrollListDown());
DIFF_PROC(Shop_HandleListClick, Shop_HandleListClick());

// Unnamed globals (shop.h).
#define PREV_SEL (*(int16_t *)ds_at(0x79F0))
#define PREV_TOP (*(int16_t *)ds_at(0x79F2))
#define PREV_QTY (*(int32_t *)ds_at(0x79F4))
#define PREV_SEL2 (*(int16_t *)ds_at(0x79F8))
#define QTY_EDIT (*(uint8_t *)ds_at(0x79FB))
#define SHOW_PICTURE (*(uint8_t *)ds_at(0x79FC))
#define ORE_FLAG(k) (*(uint8_t *)ds_at((uint16_t)(0x7A2D + (k))))
#define LIST ((uint8_t *)ds_at(0x7AB6))

// Finished products (State 5) and their orders; the list rebuilt to match.
static void setup_products(DiffRng *r)
{
    for (int k = 0; k < 35; k++) {
        TProduct *p = &ds.Research_Products[k];
        if (diff_chance(r, 60))
            p->State = (int16_t)diff_pick(r, 1, 3, 5, 5, 5, 5, 5);
        if (diff_chance(r, 50)) {
            p->_1D = (uint16_t)diff_pick(r, 0, 0, 1, 2, 10, diff_range(r, 0, 300));
            p->_1F = (uint16_t)diff_pick(r, 1, 5, 24, diff_range(r, 1, 500));
            p->_21 = (uint16_t)diff_pick(r, 0, 0, 1, diff_range(r, 0, 500));
            p->Price = diff_pick(r, 0, 100, 2500, 40000, diff_range(r, 0, 200000));
            p->Stock = (int16_t)diff_pick(r, 0, 0, 1, 5, diff_range(r, 0, 3000));
            p->Produced = (uint8_t)diff_pick(r, 0, 1, 1);
            p->SpaceBuilt = (uint8_t)diff_pick(r, 0, 0, 1);
            for (int j = 0; j < 6; j++)
                p->Ores[j] = (uint16_t)diff_pick(r, 0, 0, 0, 1, 10, 250, diff_range(r, 0, 5000));
        }
    }
    if (diff_chance(r, 30))
        ds.Research_Products[23].State = 5;  // the space station
    ds.Shop_Count = 0;
    for (int i = 1; i <= 35; i++)
        if (ds.Research_Products[i - 1].State == 5)
            LIST[ds.Shop_Count++] = (uint8_t)i;
    if (ds.Shop_Count == 0) {
        ds.Research_Products[0].State = 5;
        LIST[ds.Shop_Count++] = 1;
    }
    ds.Shop_Sel = (int16_t)diff_pick(r, 1, ds.Shop_Count, diff_range(r, 1, ds.Shop_Count));
    int maxtop = ds.Shop_Count > 13 ? ds.Shop_Count - 12 : 1;
    ds.Shop_Top = (int16_t)diff_pick(r, 1, maxtop, diff_range(r, 1, maxtop));
    if (diff_chance(r, 60)) {  // the selection visible, as the screen keeps it
        if (ds.Shop_Sel < ds.Shop_Top)
            ds.Shop_Top = ds.Shop_Sel;
        if (ds.Shop_Sel > ds.Shop_Top + 12)
            ds.Shop_Top = (int16_t)(ds.Shop_Sel - 12);
    }
    ds.Game_ProductionRate = (uint16_t)diff_pick(r, 1, 10, 10, 7, diff_range(r, 1, 40));
    ds.Game_Credits = diff_pick(r, 0, 1000, 50000, 1000000, diff_range(r, 0, 3000000));
    for (int j = 0; j < 6; j++) {
        ds.Game_HomeStock[j] = diff_pick(r, 0, 10, 1000, 2000000, diff_range(r, 0, 300000));
        ds.Shop_OreStock[j] = diff_pick(r, 0, 10, 1000, 2000000, 2000000, diff_range(r, 0, 300000));
    }
}

// The screen's state on top.
static void setup_shop(DiffRng *r, int32_t *a)
{
    setup_products(r);
    PREV_SEL = (int16_t)diff_pick(r, ds.Shop_Sel, ds.Shop_Sel, 0, diff_range(r, 1, ds.Shop_Count));
    PREV_TOP = (int16_t)diff_pick(r, ds.Shop_Top, ds.Shop_Top, 0);
    PREV_SEL2 = (int16_t)diff_pick(r, ds.Shop_Sel, 0);
    ds.Shop_Qty = diff_pick(r, 0, 1, 2, 10, 99, diff_range(r, 0, 500));
    PREV_QTY = diff_pick(r, ds.Shop_Qty, 0, 1);
    SHOW_PICTURE = (uint8_t)diff_range(r, 0, 1);
    QTY_EDIT = (uint8_t)diff_pick(r, 0, 0, 1);
    for (int k = 1; k <= 6; k++)
        ORE_FLAG(k) = (uint8_t)diff_pick(r, 0, 0, 1);
    ds.Mouse_ClickX = (int16_t)diff_pick(r, 5, 13, 14, 15, 16, 60, 0x70, 0x71, 200, diff_range(r, 0, 319));
    ds.Mouse_ClickY = (int16_t)diff_pick(r, 0x30, 0x38, 0x39, 0x80, 0xB6, 0xBF, diff_range(r, 0, 199));
    ds.Mouse_Buttons = (uint8_t)diff_pick(r, 0, 0, 1);
    a[0] = diff_range(r, 0, 1);
}

static void setup_screen(DiffRng *r, int32_t *a)
{
    setup_shop(r, a);
    ds.Game_ScreenEntered = (uint8_t)diff_chance(r, 8);
    ds.Game_PrevScreen = (int16_t)diff_pick(r, 1, 3);
    ds.Game_SelInvention = (int16_t)diff_range(r, 1, 35);
    // Menu: 12h order, 15h/16h next/previous, 1Ah..1Dh quantity, 1Eh/20h leave the
    // editor, 1Fh list, 23h back to the picture.
    ds.Game_MenuCommand = (int16_t)diff_pick(r, 0, 0, 0x12, 0x12, 0x15, 0x16, 0x1A, 0x1B, 0x1C, 0x1D,
                                             0x1E, 0x1E, 0x1F, 0x20, 0x23);
    // Hotspots 15h info/picture, 16h select, 17h..1Ah scroll.
    ds.Game_HotspotHit = (int16_t)diff_pick(r, 0, 0, 0, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B);
    ds.Mouse_ButtonsChanged = (uint8_t)diff_pick(r, 0, 0, 1);
    ds.Mouse_Buttons = 0;
    if (diff_chance(r, 20))
        ds.Shop_ListMode = (uint8_t)diff_range(r, 0, 1);
    if (diff_chance(r, 40)) {  // the selection at the top or bottom of the visible list
        ds.Shop_Top = (int16_t)(ds.Shop_Sel - diff_pick(r, 0, 12));
        if (ds.Shop_Top < 1)
            ds.Shop_Top = 1;
    }
}

// DrawNotProducible: each product with its own hint.
static void setup_hint(DiffRng *r, int32_t *a)
{
    setup_shop(r, a);
    TProduct *p = &ds.Research_Products[LIST[ds.Shop_Sel - 1] - 1];
    p->Produced = 0;
    p->SpaceBuilt = (uint8_t)diff_pick(r, 0, 0, 1);
    LIST[ds.Shop_Sel - 1] = (uint8_t)diff_pick(r, 8, 12, 0x22, 14, 18, 0x1C, 0x17, 1, 7, 0x1D, 0x1E,
                                               0x23, diff_range(r, 1, 35));
}

static const struct {
    const char *name;
    const char *script;
} states[] = {
    {"shop (5)", "900:click:24,16;1300:quit"},
    {"shop (5), list", "900:click:24,16;1300:click:60,100;1500:quit"},
    {"shop (5), info", "900:click:24,16;1300:click:200,100;1500:quit"},
};
enum { STATES = sizeof states / sizeof states[0] };

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    DiffState *bases[STATES];
    for (int i = 0; i < STATES; i++) {
        if (!diff_boot(states[i].script) || ds.Game_CurrentScreen != 5) {
            printf("FAILED to reach %s\n", states[i].name);
            return 1;
        }
        bases[i] = diff_save();
    }

    const struct {
        DiffFn *fn;
        DiffSetup setup;
        int cases;
    } fns[] = {
        {&diff_Shop_BuildProductList, setup_screen, 20},
        {&diff_Shop_SetupShopHotspots, setup_shop, 6},
        {&diff_Shop_SetupShopListHotspots, setup_shop, 6},
        {&diff_Shop_ScrollListUp, setup_shop, 30},
        {&diff_Shop_ScrollListDown, setup_shop, 20},
        {&diff_Shop_DrawProductList, setup_shop, 30},
        {&diff_Shop_UpdateOrderOres, setup_shop, 40},
        {&diff_Shop_SetupOnePieceOreText, setup_shop, 12},
        {&diff_Shop_DrawOreTexts, setup_shop, 6},
        {&diff_Shop_DrawOreTextsMarked, setup_shop, 12},
        {&diff_Shop_ShowOreStock, setup_shop, 12},
        {&diff_Shop_DrawOrderSummary, setup_shop, 30},
        {&diff_Shop_DrawQueueSummary, setup_shop, 30},
        {&diff_Shop_DrawNotProducible, setup_shop, 40},
        {&diff_Shop_DrawNotProducible, setup_hint, 40},
        {&diff_Shop_DrawQueueNumbers, setup_shop, 12},
        {&diff_Shop_DrawQueueTime, setup_shop, 12},
        {&diff_Shop_ShowSelectPanel, setup_shop, 3},
        {&diff_Shop_ShowInfoPanel, setup_shop, 3},
        {&diff_Shop_ShowProductPicture, setup_shop, 8},
        {&diff_Shop_InitProductPicture, setup_shop, 4},
        {&diff_Shop_DrawProductInfo, setup_shop, 16},
        {&diff_Shop_HandleListClick, setup_shop, 60},
        {&diff_Shop_ShopScreen, setup_screen, 160},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, bases, STATES, fns[i].setup, diff_cases(fns[i].cases));
    for (int i = 0; i < STATES; i++)
        diff_free(bases[i]);
    return diff_finish();
}
