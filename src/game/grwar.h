// Unit GrWar (1537): the ground battle. Two armies of up to 20 slots plus a
// base each fight on a 16x9 tile field; BattleRound is one AI/combat turn
// and RenderBattle redraws the field.
//
// Data segment state without names in dseg.h:
//   7628 far  off-screen copy of the field (0BCD0h bytes, 16-byte lead-in)
//   762C far  background (64000)      7630 far  gricon unit sprites
//   7634 far  gricon2 effect sprites  7638 far  granim frames (lost screen)
//   763C byte lost screen shown       763E int  granim frame (1..3)
//   7640 int  granim tick (1..13)     7642 far  grid (0C6h bytes, cell (x,y)
//   at +0Ch + x*11 + y: 0 free, 1..20 attacker slot, 101..120 defender slot)
//   7646/764A far unit lists (21 records of 27h bytes, the 21st is the base)
//   764E/7650 int slot counts         7652 far  projectiles (2 x 50 x 12h)
//   7656/7658 int projectile counts of side 1/2
//   770B byte battle over             770C byte 2 = paused
//   770E int  FindPath direction      7710 int  (cleared at start)
//   7716 int  selected unit           771A byte selected unit is attacker's
//   778B byte 1 = the defender's base is the 32x32 sprite
//   77DB byte player retreated        7798/779A/779C battle system/planet/moon
//   7E36 far  war animation buffer
// Longint tables [side 1..2][type 1..4] at 7646 + 16*side + 4*type: totals
// (7656/7666), attack sums (7686/7696), defence sums (76A6/76B6), survivors
// (76C6/76D6), losses (76E6/76F6) -- see grwarunits.h.
#pragma once

#include <stdint.h>

#include "game/dseg.h"

// One slot of a unit list (39 bytes, 1-based in lists of 21).
typedef struct [[gnu::packed]] GrBattleUnit {
    uint8_t type;       // +00 1..4 (5 = base)
    int16_t count;      // +01 units left; 0 = dead
    uint8_t hp;         // +03 (the base's 100)
    uint8_t x, y;       // +04 tile
    int16_t cooldown;   // +06 turns until it may attack again
    uint8_t dir;        // +08 1..4 (index into the dx/dy tables at DS:44/4E)
    uint8_t moving;     // +09
    uint8_t step;       // +0A 0..16 pixels moved towards the next tile
    uint8_t moveDelay;  // +0B
    uint8_t effect;     // +0C effect id (6-byte records at DS:82)
    uint8_t effTimer;   // +0D
    uint8_t effDelay;   // +0E
    uint8_t mode;       // +0F 1 hold, 2 move to (target,targetY), 3 attack unit target
    uint8_t target;     // +10
    uint8_t targetY;    // +11
    uint8_t pathLen;    // +12
    uint8_t path[20];   // +13 queued directions
} GrBattleUnit;
static_assert(sizeof(GrBattleUnit) == 0x27);
static_assert(offsetof(GrBattleUnit, pathLen) == 0x12);

// 1537:0029
void GrWar_EnterGroundBattle(void);
// 1537:0200
void GrWar_InitBattleUnits(void);
// 1537:053a
void GrWar_SetupBattleHotspots(void);
// 1537:05ee
void GrWar_SetHotspotFlags(uint8_t flag);
// 1537:0642
void GrWar_DrawSelectedUnitInfo(void);
// 1537:08a4 / 0961 / 09c2
void GrWar_DrawUnitSprite(int16_t frame, int16_t sheet, int16_t x, int16_t y, int16_t mode);
void GrWar_DrawEffectSprite(int16_t frame, int16_t sheet, int16_t x, int16_t y, int16_t variant);
void GrWar_DrawProjectile(int16_t x, int16_t y, int16_t kind);
// 1537:0a0c / 0ca0
void GrWar_DrawSelectionMarkers(void);
void GrWar_DrawBaseSprites(void);
// 1537:0d85
void GrWar_BattleTick(void);
// 1537:0da8
void GrWar_RenderBattle(void);
// 1537:11f2: Game::GrWarProc
int16_t GrWar_PickBattleAnim(void);
// 1537:12aa / 13c7 / 14ff: distances between unit `unit` of list 1 and `enemy`
// of list 2 (swapped for side 2). Euclid and Squared return longints whose
// low word is what the game uses.
int16_t GrWar_DistManhattanTiles(int16_t unit, int16_t enemy, int16_t side);
int32_t GrWar_DistEuclidTiles(int16_t unit, int16_t enemy, int16_t side);
int32_t GrWar_DistSquaredTiles(int16_t unit, int16_t enemy, int16_t side);
// 1537:1652 / 1674 / 16a3
bool GrWar_IsAdjacent(int16_t unit, int16_t enemy, int16_t side);
bool GrWar_IsInArtilleryBand(int16_t unit, int16_t enemy, int16_t side);
bool GrWar_InWeaponRange(int16_t weapon, int16_t unit, int16_t enemy, int16_t side);
// 1537:1725 / 177e
int16_t GrWar_DirectionToward(int16_t selfX, int16_t selfY, int16_t targetX, int16_t targetY);
int16_t GrWar_DirectionToward16(int16_t selfX, int16_t selfY, int16_t targetX, int16_t targetY);
// 1537:188f
void GrWar_StartUnitEffect(int16_t side, int16_t unit, int16_t effect);
// 1537:18d3
int16_t GrWar_FindUnitNearPoint(int16_t x, int16_t y, int16_t side);
// 1537:1b80
int16_t GrWar_FindNearestEnemy(int16_t unit, int16_t side);
// 1537:1c05
void GrWar_BattleRound(void);
// 1537:2a19
void GrWar_ApplyGroundLosses(void);
// 1537:312c
void GrWar_EndGroundBattle(void);
// 1537:3369
void GrWar_AnimateResultScreen(void);
// 1537:34d4 (with the nested FloodFillStep 33da): flood-fill search on the
// grid at DS:7642 from (sx,sy) towards (a,b) (and, if that cell is free,
// (c,d)); *dir = 1..4 (west, north, east, south) or 0 when no step brings
// it closer.
void GrWar_FindPath(int16_t sx, int16_t sy, int16_t a, int16_t b, int16_t c, int16_t d,
                    uint16_t *dir);
// 1537:3a71
void GrWar_Init(void);
