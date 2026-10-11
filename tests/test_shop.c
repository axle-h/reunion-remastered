// shop: the product list, build-time and price rules, and the ore clamping of an order.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/dseg.h"
#include "game/mouse.h"
#include "game/shop.h"
#include "platform/fs.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static bool pstr_is(const void *pstr, const char *want)
{
    const uint8_t *p = pstr;
    return p[0] == SDL_strlen(want) && SDL_memcmp(p + 1, want, p[0]) == 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    dseg_clear();
    vid_clear();
    CHECK(dseg_init());

    // Product list: the inventions that are done, in order; coming from the research screen preselects.
    for (int i = 0; i < 35; i++)
        ds.Research_Products[i].State = 0;
    ds.Research_Products[2].State = 5;
    ds.Research_Products[6].State = 5;
    ds.Research_Products[9].State = 4;
    ds.Research_Products[20].State = 5;
    ds.Game_PrevScreen = 3;
    ds.Game_SelInvention = 7;
    Shop_BuildProductList();
    uint8_t *list = ds.Shop_List;
    CHECK(ds.Shop_Count == 3 && list[0] == 3 && list[1] == 7 && list[2] == 21);
    CHECK(ds.Shop_Sel == 2);
    ds.Game_PrevScreen = 4;
    Shop_BuildProductList();
    CHECK(ds.Shop_Sel == 1);

    // Times: (qty * hours + rate - 1) div rate; with a piece in progress the first one costs _21.
    TProduct p;
    SDL_memset(&p, 0, sizeof p);
    p._1F = 10;
    p.Price = 250;
    ds.Game_ProductionRate = 4;
    CHECK(Shop_OrderHours(&p, 1) == 3);      // (10 + 3) / 4
    CHECK(Shop_OrderHours(&p, 3) == 8);      // (30 + 3) / 4
    CHECK(Shop_OrderHours(&p, 0) == 0);
    p._21 = 6;                               // first piece has 6 hours left
    CHECK(Shop_OrderHours(&p, 1) == 2);      // (6 + 3) / 4
    CHECK(Shop_OrderHours(&p, 3) == 7);      // (2*10 + 6 + 3) / 4
    p._1D = 3;
    CHECK(Shop_QueueHours(&p) == 7);
    CHECK(Shop_OrderPrice(&p, 12) == 3000);

    // An order is cut down to what the ore stock allows; ore lines are in the order 1,3,2,6,5,4.
    ds.Research_Products[6].Ores[0] = 10;
    ds.Research_Products[6].Ores[1] = 0;
    ds.Research_Products[6].Ores[2] = 5;
    ds.Research_Products[6].Ores[3] = 0;
    ds.Research_Products[6].Ores[4] = 1;
    ds.Research_Products[6].Ores[5] = 0;
    ds.Shop_Sel = 2;                          // list[1] = product 7
    int32_t *stock = ds.Shop_OreStock;
    for (int k = 0; k < 6; k++)
        stock[k] = 1000;
    stock[2] = 22;                            // ore 3: 22 in stock, 5 per piece -> 4 pieces
    ds.Shop_Qty = 9;
    Shop_UpdateOrderOres();
    CHECK(ds.Shop_Qty == 4);
    int32_t *needed = ds.Shop_OreNeeded;
    CHECK(needed[0] == 40 && needed[2] == 20 && needed[4] == 4 && needed[1] == 0);
    uint8_t *flags = ds.Shop_OreShort;        // would one more piece run out?
    CHECK(flags[2] == 1 && flags[0] == 0 && flags[4] == 0);   // 25 > 22
    CHECK(pstr_is(&ds.Shop_OreHeader, "Ore needed: (4 pieces )"));
    PString *line1 = (PString *)&ds.Shop_OreLines[0];
    CHECK(line1->len == ds.Names_Ores[0].len + 6);
    CHECK(SDL_memcmp(line1->s + line1->len - 2, "40", 2) == 0);
    ds.Shop_Qty = 1;
    Shop_UpdateOrderOres();
    CHECK(pstr_is(&ds.Shop_OreHeader, "Ore needed: (1 piece )"));
    Shop_SetupOnePieceOreText();
    CHECK(pstr_is(&ds.Shop_OreHeader, "Ore needed: (One piece)"));

    // Hotspots of the list mode: info toggle, list, scroll arrows.
    Shop_SetupShopListHotspots();
    CHECK(ds.Mouse_HotspotCount == 6);
    CHECK(Mouse_Hotspot(2)->W == 0x68 && Mouse_Hotspot(2)->Caption.len == 0);
    CHECK(pstr_is(&Mouse_Hotspot(5)->Caption, "Scroll down") && Mouse_Hotspot(5)->Y == 0x9E);

    // Scrolling keeps the selection inside the window of 13 rows.
    ds.Shop_Count = 30;
    ds.Shop_Top = 5;
    ds.Shop_Sel = 17;
    Shop_ScrollListUp();
    CHECK(ds.Shop_Top == 4 && ds.Shop_Sel == 16);
    ds.Shop_Top = 1;
    ds.Shop_Sel = 3;
    Shop_ScrollListUp();
    CHECK(ds.Shop_Top == 1 && ds.Shop_Sel == 3);
    ds.Shop_Top = 18;
    Shop_ScrollListDown();
    CHECK(ds.Shop_Top == 18);                 // 18 + 12 = 30: already at the end
    ds.Shop_Top = 5;
    ds.Shop_Sel = 5;
    Shop_ScrollListDown();
    CHECK(ds.Shop_Top == 6 && ds.Shop_Sel == 6);

    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
