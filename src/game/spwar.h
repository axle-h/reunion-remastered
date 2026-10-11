// Unit SpWar (20d4): the space battle screen. Builds the two fleets from the
// groups, buildings and planet stores at the battle's body, steps the
// animated fight (one real step every 4th call), plays the war\anim\anim.def
// scripts around it and scales the survivors back into the game state.
//
// Ghidra splits InitSpaceFleets (076f, 07dd, 093e, 0949) and
// PlayWarAnimScript (0c44, 0dc6) are loop labels of those routines and are
// part of them here.
#pragma once

#include <stdint.h>


// One ship of a fleet list (12-byte records, lists are 1-based, max 500).
typedef struct [[gnu::packed]] SpShip {
    uint8_t id;     // group: list * 100 + group; building: race * 10 + force; store ships: C9h
    uint8_t cls;    // ship class 1..4
    uint16_t dmg;   // damage per hit
    int16_t hp;     // <= 0: destroyed (the explosion plays at the end of the list)
    uint8_t x, y;   // position in the 160 x 150 battle view
    uint8_t len;    // path length 2..7
    uint8_t path;   // route table row 1..9
    uint8_t pos;    // step along the route; after death the explosion frame
    uint8_t cool;   // frames until the next shot
} SpShip;
static_assert(sizeof(SpShip) == 12);

// 20d4:002d: screen entry (loads the pictures and buffers, draws the first step)
void SpWar_EnterSpaceBattle(void);
// 20d4:01d3 (with 076f, 07dd, 093e, 0949)
void SpWar_InitSpaceFleets(void);
// 20d4:0969
void SpWar_ClearHotspots(void);
// 20d4:0985: loads script n of war\anim\anim.def
void SpWar_LoadWarAnimDef(int16_t n);
// 20d4:0a14
void SpWar_FreeWarAnims(void);
// 20d4:0a85 (with 0c44, 0dc6): one tick of the screen
void SpWar_PlayWarAnimScript(void);
// 20d4:0dcc
void SpWar_DrawPilotFace(void);
// 20d4:0df2 / 0e0d / 0e28 / 0e57 / 0e89: sprite from the spants sheet
// (src offset in it) into the battle view buffer (dst offset)
void SpWar_DrawShipDot1(int16_t srcOfs, int16_t dstOfs);
void SpWar_DrawShipDot2(int16_t srcOfs, int16_t dstOfs);
void SpWar_DrawShipDot4(int16_t srcOfs, int16_t dstOfs);
void SpWar_DrawShipDot7(int16_t srcOfs, int16_t dstOfs);
void SpWar_DrawExplosion(int16_t srcOfs, int16_t dstOfs);
// 20d4:0efd
void SpWar_StepSpaceBattle(void);
// 20d4:17f0
void SpWar_ApplySpaceLosses(void);
// 20d4:1de8
void SpWar_EndSpaceBattle(void);
