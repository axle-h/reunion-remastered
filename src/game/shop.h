// Unit Shop (32b4): the production orders screen (screen 5, INFO-BUY). A list
// of the inventions that are done (Research State 5), each with a picture or
// an info text, the ore it needs per piece, and a quantity editor that buys
// pieces with Credits and queues them in TProduct._1D.
//
// Product fields as used here (TProduct): _1D = queued pieces, _1F = hours
// per piece, _21 = hours left on the piece in progress (0 = none started),
// Produced / SpaceBuilt = where it can be built, Ores[k] = ore k per piece.
//
// Unnamed state (DS): 79F0 selection and 79F2 top at the last redraw, 79F4
// quantity before the last change, 79F8 selection the info was drawn for,
// 79FA ListMode, 79FB quantity editor active, 79FC picture (1) or info (0),
// 79FE..7A15 OreNeeded[1..6], 7A2E..7A33 flags "one more piece runs out
// of ore k", 7A54.. six string[15] ore lines (ores 1,3,2,6,5,4).
#pragma once
#include <stdint.h>
#include "game/dseg.h"

// 32b4:0057
void Shop_ShopScreen(void);
// 32b4:074d / 07c8: grafika\select and grafika\info background panels.
void Shop_ShowSelectPanel(void);
void Shop_ShowInfoPanel(void);
// 32b4:083a / 098f
void Shop_ShowProductPicture(bool itemChanged);
void Shop_InitProductPicture(void);
// 32b4:0aac / 0b57
void Shop_DrawProductList(void);
void Shop_DrawProductInfo(bool itemChanged);
// 32b4:0c50
void Shop_BuildProductList(void);
// 32b4:0d21
void Shop_UpdateOrderOres(void);
// 32b4:1003 / 10bd / 1141
void Shop_SetupOnePieceOreText(void);
void Shop_DrawOreTexts(void);
void Shop_DrawOreTextsMarked(void);
// 32b4:1314
void Shop_ShowOreStock(void);
// 32b4:1474 / 1666 / 18a1 / 1a29 / 1b4a
void Shop_DrawOrderSummary(void);
void Shop_DrawQueueSummary(void);
void Shop_DrawNotProducible(void);
void Shop_DrawQueueNumbers(void);
void Shop_DrawQueueTime(void);
// 32b4:1bf2 / 1ca3
void Shop_SetupShopHotspots(void);
void Shop_SetupShopListHotspots(void);
// 32b4:1de0 / 1e00 / 1e23
void Shop_ScrollListUp(void);
void Shop_ScrollListDown(void);
void Shop_HandleListClick(void);

// Hours needed to build `qty` pieces from scratch or from the piece in
// progress: (qty * time + rate - 1) div ProductionRate (DrawOrderSummary).
int32_t Shop_OrderHours(const TProduct *p, int32_t qty);
// The same for what is already queued (_1D pieces; DrawQueueSummary).
int32_t Shop_QueueHours(const TProduct *p);
// Total price of qty pieces.
int32_t Shop_OrderPrice(const TProduct *p, int32_t qty);
