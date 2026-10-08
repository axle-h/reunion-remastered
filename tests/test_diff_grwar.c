// Differential test of unit GrWar (1537): the ground battle, original (under
// Unicorn) against the port. A booted new game has no battle, so the base
// states are built with the port's own routines (as screens 0x13 and 0x20
// do): forces at a target body, CollectAvailableForces, InitBattleUnits,
// EnterGroundBattle, then some BattleTicks; the routines run from those with
// the units, projectiles and selection perturbed. See docs/difftest.md.
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/groups.h"
#include "game/grwar.h"
#include "game/grwarunits.h"
#include "game/planetinfo.h"

#include "diff_grwar.h"

DIFF_PROC(GrWar_EnterGroundBattle, GrWar_EnterGroundBattle());
DIFF_PROC(GrWar_InitBattleUnits, GrWar_InitBattleUnits());
DIFF_PROC(GrWar_SetupBattleHotspots, GrWar_SetupBattleHotspots());
DIFF_PROC(GrWar_SetHotspotFlags, GrWar_SetHotspotFlags((uint8_t)a[0]));
DIFF_PROC(GrWar_DrawSelectedUnitInfo, GrWar_DrawSelectedUnitInfo());
DIFF_PROC(GrWar_DrawUnitSprite, GrWar_DrawUnitSprite((int16_t)a[0], (int16_t)a[1], (int16_t)a[2],
                                                     (int16_t)a[3], (int16_t)a[4]));
DIFF_PROC(GrWar_DrawEffectSprite, GrWar_DrawEffectSprite((int16_t)a[0], (int16_t)a[1], (int16_t)a[2],
                                                         (int16_t)a[3], (int16_t)a[4]));
DIFF_PROC(GrWar_DrawProjectile, GrWar_DrawProjectile((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_PROC(GrWar_DrawSelectionMarkers, GrWar_DrawSelectionMarkers());
DIFF_PROC(GrWar_DrawBaseSprites, GrWar_DrawBaseSprites());
DIFF_PROC(GrWar_BattleTick, GrWar_BattleTick());
DIFF_PROC(GrWar_RenderBattle, GrWar_RenderBattle());
DIFF_FUNC(GrWar_PickBattleAnim, GrWar_PickBattleAnim());
DIFF_FUNC(GrWar_DistManhattanTiles, GrWar_DistManhattanTiles((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_FUNC(GrWar_DistEuclidTiles, GrWar_DistEuclidTiles((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_FUNC(GrWar_DistSquaredTiles, GrWar_DistSquaredTiles((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_FUNC(GrWar_IsAdjacent, GrWar_IsAdjacent((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_FUNC(GrWar_IsInArtilleryBand, GrWar_IsInArtilleryBand((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_FUNC(GrWar_InWeaponRange,
          GrWar_InWeaponRange((int16_t)a[0], (int16_t)a[1], (int16_t)a[2], (int16_t)a[3]));
DIFF_FUNC(GrWar_DirectionToward,
          GrWar_DirectionToward((int16_t)a[0], (int16_t)a[1], (int16_t)a[2], (int16_t)a[3]));
DIFF_FUNC(GrWar_DirectionToward16,
          GrWar_DirectionToward16((int16_t)a[0], (int16_t)a[1], (int16_t)a[2], (int16_t)a[3]));
DIFF_PROC(GrWar_StartUnitEffect, GrWar_StartUnitEffect((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_FUNC(GrWar_FindUnitNearPoint, GrWar_FindUnitNearPoint((int16_t)a[0], (int16_t)a[1], (int16_t)a[2]));
DIFF_FUNC(GrWar_FindNearestEnemy, GrWar_FindNearestEnemy((int16_t)a[0], (int16_t)a[1]));
DIFF_PROC(GrWar_BattleRound, GrWar_BattleRound());
DIFF_PROC(GrWar_ApplyGroundLosses, GrWar_ApplyGroundLosses());
DIFF_PROC(GrWar_EndGroundBattle, GrWar_EndGroundBattle());
DIFF_PROC(GrWar_AnimateResultScreen, GrWar_AnimateResultScreen());
DIFF_PROC(GrWar_FindPath, GrWar_FindPath((int16_t)a[0], (int16_t)a[1], (int16_t)a[2], (int16_t)a[3],
                                         (int16_t)a[4], (int16_t)a[5], (uint16_t *)far_host((FarPtr)a[6])));
DIFF_PROC(GrWar_Init, GrWar_Init());

// --- Perturbations -------------------------------------------------------------

static void perturb_units(DiffRng *r)
{
    for (int s = 1; s <= 2; s++) {
        int n = gw_slots(s), on = gw_slots(3 - s);
        for (int i = 1; i <= n; i++) {
            if (!diff_chance(r, 40))
                continue;
            GrBattleUnit *u = gw_unit(s, i);
            u->count = (int16_t)diff_pick(r, u->count, 0, 0, 1, 3, 20, 60);
            u->effect = (uint8_t)diff_pick(r, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8);
            u->effTimer = (uint8_t)diff_pick(r, 0, 0, 1, 3, 6, 9);
            u->effDelay = (uint8_t)diff_range(r, 0, 3);
            u->cooldown = (int16_t)diff_pick(r, 0, 0, 0, 1, 4);
            u->moveDelay = (uint8_t)diff_pick(r, 0, 0, 1, 3);
            u->mode = (uint8_t)diff_pick(r, u->mode, 1, 2, 3, 3);
            u->target = (uint8_t)diff_pick(r, u->target, diff_range(r, 0, on), diff_range(r, 1, on), 21);
            if (u->mode == 2) {
                u->target = (uint8_t)diff_range(r, 0, 15);
                u->targetY = (uint8_t)diff_range(r, 0, 8);
            }
            u->moving = (uint8_t)diff_pick(r, u->moving, 0, 1);
            u->step = (uint8_t)diff_pick(r, u->step, 0, 1, 8, 15, 16);
            u->pathLen = (uint8_t)diff_pick(r, 0, 0, 1, 3);
            for (int k = 0; k < u->pathLen; k++)
                u->path[k] = (uint8_t)diff_range(r, 1, 4);
        }
    }
}

static void perturb_projectiles(DiffRng *r)
{
    for (int s = 1; s <= 2; s++) {
        int n = diff_pick(r, gw_proj_count(s), 0, 0, 1, 3);
        gw_set_proj_count(s, n);
        for (int k = 1; k <= n; k++) {
            int16_t *p = gw_proj(s, k);
            p[0] = (int16_t)diff_range(r, 0x40, 0x130);
            p[1] = (int16_t)diff_range(r, 3, 0x83);
            // Aim at a unit's tile of the other side now and then.
            int on = gw_slots(3 - s);
            if (on > 0 && diff_chance(r, 50)) {
                const GrBattleUnit *t = gw_unit(3 - s, diff_range(r, 1, on));
                p[2] = (int16_t)(t->x * 16 + 0x40);
                p[3] = (int16_t)(t->y * 16 + 3);
            } else {
                p[2] = (int16_t)diff_range(r, 0x40, 0x13F);
                p[3] = (int16_t)diff_range(r, 3, 0x92);
            }
            p[5] = (int16_t)diff_pick(r, 1, 8, 16, 40);
            p[4] = (int16_t)diff_pick(r, p[5], p[5], 1, diff_range(r, 1, p[5]));
            p[6] = (int16_t)diff_range(r, 0, 15);
            int32_t dmg = diff_pick(r, 0, 1, 50, 400, 3000);
            memcpy(&p[7], &dmg, 4);
        }
    }
}

// Moves unit i of side s to tile (x, y), keeping the grid in step.
static void move_unit(int s, int i, int x, int y)
{
    uint8_t *g = gw_grid();
    GrBattleUnit *u = gw_unit(s, i);
    uint8_t id = (uint8_t)(i + (s - 1) * 100);
    if (g[0xC + u->x * 11 + u->y] == id)
        g[0xC + u->x * 11 + u->y] = 0;
    u->x = (uint8_t)x;
    u->y = (uint8_t)y;
    u->moving = 0;
    u->step = 0;
    g[0xC + x * 11 + y] = id;
}

// Brings defenders within reach of attackers (melee, rifle and artillery
// range), ready to fire.
static void skirmish(DiffRng *r)
{
    int n1 = gw_slots(1), n2 = gw_slots(2);
    if (n1 == 0 || n2 == 0)
        return;
    for (int k = diff_range(r, 1, 6); k > 0; k--) {
        int i = diff_range(r, 1, n1), j = diff_range(r, 1, n2);
        GrBattleUnit *a = gw_unit(1, i), *d = gw_unit(2, j);
        int dist = diff_pick(r, 1, 1, 2, 3, 4, 5);
        int x = gw_clamp(a->x + dist, 0, 15), y = gw_clamp(a->y + diff_range(r, -1, 1), 0, 8);
        if (gw_grid()[0xC + x * 11 + y] != 0)
            continue;
        move_unit(2, j, x, y);
        for (int s = 1; s <= 2; s++) {
            GrBattleUnit *u = s == 1 ? a : d;
            if (u->count <= 0)
                u->count = (int16_t)diff_range(r, 1, 30);
            u->effect = (uint8_t)diff_pick(r, 0, 0, 0, 3);
            u->cooldown = (int16_t)diff_pick(r, 0, 0, 0, 2);
        }
        if (diff_chance(r, 60)) {
            a->mode = 3;
            a->target = (uint8_t)j;
        }
        if (diff_chance(r, 60)) {
            d->mode = 3;
            d->target = (uint8_t)i;
        }
    }
}

static void perturb_battle(DiffRng *r)
{
    ds.System_RandSeed = (int32_t)diff_rand(r, 0xFFFFFFFFu);
    perturb_units(r);
    if (diff_chance(r, 60))
        skirmish(r);
    if (diff_chance(r, 50))
        perturb_projectiles(r);
    *gw_w(0x7716) = 0;
    if (diff_chance(r, 60)) {
        *gw_w(0x7716) = (int16_t)diff_pick(r, 0, 1, 2, diff_range(r, 1, 20), 21);
        *gw_b(0x771A) = (uint8_t)diff_range(r, 0, 1);
    }
    for (int s = 1; s <= 2; s++) {
        GrBattleUnit *b = gw_unit(s, 21);
        if (diff_chance(r, 30)) {
            b->hp = (uint8_t)diff_pick(r, 100, 99, 50, 1);
            b->effect = (uint8_t)diff_pick(r, 0, 7, 8);
            b->effTimer = (uint8_t)diff_range(r, 0, 6);
            b->effDelay = (uint8_t)diff_range(r, 0, 2);
        }
    }
}

// --- Setups --------------------------------------------------------------------

static void setup_battle(DiffRng *r, int32_t *a)
{
    (void)a;
    perturb_battle(r);
    *gw_b(0x770C) = (uint8_t)diff_pick(r, 0, 0, 0, 2);  // paused
    ds.Game_BattleFlag = (uint8_t)diff_pick(r, 0, 0, 0, 0, 1);
    if (diff_chance(r, 15)) {  // one side wiped out: the battle ends
        int s = diff_range(r, 1, 2);
        for (int i = 1; i <= gw_slots(s); i++) {
            gw_unit(s, i)->count = 0;
            gw_unit(s, i)->effTimer = (uint8_t)diff_pick(r, 0, 0, 0, 2);
        }
    }
}

static void setup_flags(DiffRng *r, int32_t *a)
{
    ds.Mouse_HotspotCount = (int16_t)diff_pick(r, 0, 1, 4, 5, 20, 40);
    a[0] = diff_pick(r, 0, 1, 5, 0xFF);
}

static void setup_selected(DiffRng *r, int32_t *a)
{
    (void)a;
    perturb_battle(r);
    *gw_w(0x7716) = (int16_t)diff_pick(r, 0, 1, diff_range(r, 1, 20), 21, 21);
    *gw_b(0x771A) = (uint8_t)diff_range(r, 0, 1);
    *gw_b(0x778B) = (uint8_t)diff_range(r, 0, 1);
    if (diff_chance(r, 20)) {  // a type without forces: strength 0
        int side = *gw_b(0x771A) ? 1 : 2;
        gw_set_tbl(0, side, diff_range(r, 1, 4), 0);
    }
}

static void setup_sprite(DiffRng *r, int32_t *a)
{
    a[0] = diff_range(r, 1, 20);
    a[1] = diff_range(r, 1, 2);
    a[2] = diff_pick(r, 0x40, diff_range(r, 0x30, 0x130), 0x13F);
    a[3] = diff_pick(r, 3, diff_range(r, 0, 0x90), 0x93);
    a[4] = diff_pick(r, 1, 2, 3, 4, 0, 5);
}

static void setup_effect_sprite(DiffRng *r, int32_t *a)
{
    a[0] = diff_range(r, 1, 20);
    a[1] = diff_range(r, 1, 2);
    a[2] = diff_range(r, 0x30, 0x130);
    a[3] = diff_range(r, 0, 0x90);
    a[4] = diff_range(r, 1, 2);
}

static void setup_projectile(DiffRng *r, int32_t *a)
{
    a[0] = diff_range(r, 0x30, 0x130);
    a[1] = diff_range(r, 0, 0x93);
    a[2] = diff_range(r, 0, 15);
}

static void setup_markers(DiffRng *r, int32_t *a)
{
    setup_selected(r, a);
    int side = *gw_b(0x771A) ? 1 : 2;
    int sel = *gw_w(0x7716);
    if (side == 1 && sel > 0) {
        GrBattleUnit *u = gw_unit(1, sel);
        u->mode = (uint8_t)diff_pick(r, 1, 2, 3, 3);
        if (u->mode == 3)
            u->target = (uint8_t)diff_pick(r, 1, diff_range(r, 1, 20), 21);
    }
}

static void setup_base_style(DiffRng *r, int32_t *a)
{
    (void)a;
    *gw_b(0x778B) = (uint8_t)diff_range(r, 0, 1);
}

static void setup_render(DiffRng *r, int32_t *a)
{
    (void)a;
    perturb_battle(r);
    *gw_b(0x778B) = (uint8_t)diff_pick(r, 0, 0, 1);
}

static void setup_anim(DiffRng *r, int32_t *a)
{
    (void)a;
    ds.System_RandSeed = (int32_t)diff_rand(r, 0xFFFFFFFFu);
    for (int s = 1; s <= 2; s++)
        for (int t = 1; t <= 4; t++)
            gw_set_tbl(0, s, t, diff_pick(r, 0, 0, 1, 50, -3));
    gw_ensure_anim();
}

// Two units: unit i of side 1 and j of side 2 (swapped for side 2).
static void setup_dist(DiffRng *r, int32_t *a)
{
    int n1 = gw_slots(1), n2 = gw_slots(2);
    int side = diff_range(r, 1, 2);
    int i = diff_pick(r, diff_range(r, 1, n1), 21);
    int j = diff_pick(r, diff_range(r, 1, n2), 21);
    for (int s = 1; s <= 2; s++) {
        GrBattleUnit *u = gw_unit(s, s == 1 ? i : j);
        if (diff_chance(r, 70)) {
            u->x = (uint8_t)diff_range(r, 0, 15);
            u->y = (uint8_t)diff_range(r, 0, 8);
        }
        u->step = (uint8_t)diff_pick(r, 0, 0, 1, 8, 15);
        u->dir = (uint8_t)diff_range(r, 1, 4);
    }
    if (diff_chance(r, 30)) {  // close together
        GrBattleUnit *u = gw_unit(1, i), *e = gw_unit(2, j);
        e->x = (uint8_t)gw_clamp(u->x + diff_range(r, -2, 2), 0, 15);
        e->y = (uint8_t)gw_clamp(u->y + diff_range(r, -2, 2), 0, 8);
    }
    a[0] = side == 1 ? i : j;
    a[1] = side == 1 ? j : i;
    a[2] = side;
}

static void setup_weapon(DiffRng *r, int32_t *a)
{
    int32_t b[3];
    setup_dist(r, b);
    a[0] = diff_pick(r, 1, 2, 3, 4, 4, 0, 5);
    a[1] = b[0];
    a[2] = b[1];
    a[3] = b[2];
}

static void setup_direction(DiffRng *r, int32_t *a)
{
    for (int k = 0; k < 4; k++)
        a[k] = diff_pick(r, diff_range(r, 0, 15), diff_range(r, 0, 8), diff_range(r, -40, 40), 5);
    if (diff_chance(r, 20)) {
        a[2] = a[0];
        a[3] = a[1] + diff_range(r, -2, 2);
    }
}

static void setup_direction16(DiffRng *r, int32_t *a)
{
    setup_direction(r, a);
    if (diff_chance(r, 10))  // the same point: division by zero
        a[2] = a[0], a[3] = a[1];
}

static void setup_start_effect(DiffRng *r, int32_t *a)
{
    a[0] = diff_range(r, 1, 2);
    a[1] = diff_pick(r, 1, diff_range(r, 1, gw_slots((int)a[0])), 21);
    a[2] = diff_range(r, 0, 8);
}

static void setup_near_point(DiffRng *r, int32_t *a)
{
    perturb_battle(r);
    *gw_b(0x778B) = (uint8_t)diff_range(r, 0, 1);
    a[2] = diff_pick(r, 1, 2, 3, 3);
    int s = a[2] == 3 ? diff_range(r, 1, 2) : (int)a[2];
    int n = gw_slots(s);
    if (n > 0 && diff_chance(r, 60)) {  // near a unit's sprite
        const GrBattleUnit *u = gw_unit(s, diff_pick(r, diff_range(r, 1, n), 21));
        a[0] = u->x * 16 + 0x42 + diff_range(r, -25, 25);
        a[1] = u->y * 16 + 0x36 + diff_range(r, -25, 25);
    } else {
        a[0] = diff_range(r, 0, 319);
        a[1] = diff_range(r, 0, 199);
    }
}

static void setup_nearest(DiffRng *r, int32_t *a)
{
    perturb_battle(r);
    a[1] = diff_range(r, 1, 2);
    a[0] = diff_pick(r, diff_range(r, 1, gw_slots((int)a[1])), 21);
}

static void setup_losses(DiffRng *r, int32_t *a)
{
    (void)a;
    perturb_units(r);
    for (int s = 1; s <= 2; s++)
        for (int t = 1; t <= 4; t++)
            if (diff_chance(r, 20))
                gw_set_tbl(0, s, t, diff_pick(r, 0, 1, 100, 5000));
    for (int t = 1; t <= 4; t++)
        if (diff_chance(r, 30))
            gw_set_tbl(0x30, 0, t, diff_pick(r, 0, 1, 7, 300));  // the attackers' pool (7676)
    gw_perturb_owners(r);
}

static void setup_end(DiffRng *r, int32_t *a)
{
    setup_losses(r, a);
    ds.GrWar_PlayerWon = (uint8_t)diff_range(r, 0, 1);
    *gw_b(0x77DB) = (uint8_t)diff_pick(r, 0, 0, 1);
}

static void setup_result(DiffRng *r, int32_t *a)
{
    (void)a;
    *gw_b(0x763C) = (uint8_t)diff_pick(r, 0, 1, 1);
    *gw_w(0x7640) = (int16_t)diff_pick(r, 0, 12, 13, diff_range(r, 0, 13));
    *gw_w(0x763E) = (int16_t)diff_range(r, 1, 3);
}

// The grid with units and random blocks, a start and goal(s).
static void setup_path(DiffRng *r, int32_t *a)
{
    uint8_t *g = gw_grid();
    int blocks = diff_pick(r, 0, 5, 20, 40, 70);
    for (int k = 0; k < blocks; k++)
        g[0xC + diff_range(r, 0, 15) * 11 + diff_range(r, 0, 8)] = (uint8_t)diff_pick(r, 1, 0x65, 0x15);
    if (diff_chance(r, 20)) {  // a wall with at most one gap
        int x = diff_range(r, 3, 12), gap = diff_pick(r, -1, diff_range(r, 0, 8));
        for (int y = 0; y <= 8; y++)
            g[0xC + x * 11 + y] = y == gap ? 0 : 1;
    }
    a[0] = diff_range(r, 0, 15);
    a[1] = diff_range(r, 0, 8);
    a[2] = diff_pick(r, diff_range(r, 0, 15), a[0], -1, 16, 20);
    a[3] = diff_pick(r, diff_range(r, 0, 8), a[1], -1, 9);
    a[4] = diff_pick(r, a[2], diff_range(r, 0, 15), -1);
    a[5] = diff_pick(r, a[3], diff_range(r, 0, 8), 9);
    if (diff_chance(r, 50))  // the start cell is the unit's own
        g[0xC + a[0] * 11 + a[1]] = (uint8_t)diff_pick(r, 0, 1, 0x65);
    *(uint16_t *)far_host(DIFF_SCRATCH) = 0x5555;
    a[6] = (int32_t)DIFF_SCRATCH;
}

// A field as BattleRound leaves it: the bases at the edges, the defenders in
// two columns, the goal (a base) occupied, so the search heads for the closest
// free cell.
static void setup_path_field(DiffRng *r, int32_t *a)
{
    uint8_t *g = gw_grid();
    memset(g + 0xC, 0, 16 * 11);
    g[0xC + 0 * 11 + 4] = g[0xC + 1 * 11 + 4] = g[0xC + 0 * 11 + 5] = g[0xC + 1 * 11 + 5] = 0x15;
    g[0xC + 15 * 11 + 4] = 0x79;
    for (int y = 1; y <= 7; y++)
        g[0xC + 12 * 11 + y] = g[0xC + 13 * 11 + y] = (uint8_t)(0x64 + y);
    int n = diff_range(r, 0, 6);
    for (int k = 0; k < n; k++)
        g[0xC + diff_range(r, 2, 4) * 11 + diff_range(r, 0, 8)] = (uint8_t)diff_range(r, 1, 6);
    a[0] = diff_range(r, 2, 4);
    a[1] = diff_range(r, 0, 8);
    g[0xC + a[0] * 11 + a[1]] = 0;
    a[2] = a[4] = diff_pick(r, 15, 15, 13, 14);
    a[3] = a[5] = diff_pick(r, 4, 4, diff_range(r, 0, 8));
    a[6] = (int32_t)DIFF_SCRATCH;
}

static void setup_init(DiffRng *r, int32_t *a)
{
    (void)a;
    gw_perturb_assignment(r);
    *gw_b(0x778B) = (uint8_t)diff_range(r, 0, 1);
}

static void setup_enter(DiffRng *r, int32_t *a)
{
    (void)a;
    ds.System_RandSeed = (int32_t)diff_rand(r, 0xFFFFFFFFu);
    perturb_units(r);
    // Keep both sides alive mostly; sometimes one is already gone (the battle ends at once).
    if (diff_chance(r, 80))
        for (int s = 1; s <= 2; s++)
            if (gw_slots(s) > 0)
                gw_unit(s, 1)->count = (int16_t)diff_range(r, 1, 30);
}

static void setup_none(DiffRng *r, int32_t *a)
{
    (void)r;
    (void)a;
}

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    GwStates st;
    if (!gw_build_states(&st))
        return 1;

    const struct {
        DiffFn *fn;
        DiffState **bases;
        int nbases;
        DiffSetup setup;
        int cases;
    } fns[] = {
        {&diff_GrWar_Init, st.battle, 1, setup_none, 1},
        {&diff_GrWar_InitBattleUnits, st.assign, GW_VARIANTS, setup_init, 40},
        {&diff_GrWar_EnterGroundBattle, st.init, GW_VARIANTS, setup_enter, 12},
        {&diff_GrWar_SetupBattleHotspots, st.battle, 1, setup_none, 2},
        {&diff_GrWar_SetHotspotFlags, st.battle, st.nbattle, setup_flags, 12},
        {&diff_GrWar_DrawSelectedUnitInfo, st.battle, st.nbattle, setup_selected, 40},
        {&diff_GrWar_DrawUnitSprite, st.battle, st.nbattle, setup_sprite, 30},
        {&diff_GrWar_DrawEffectSprite, st.battle, st.nbattle, setup_effect_sprite, 20},
        {&diff_GrWar_DrawProjectile, st.battle, st.nbattle, setup_projectile, 20},
        {&diff_GrWar_DrawSelectionMarkers, st.battle, st.nbattle, setup_markers, 40},
        {&diff_GrWar_DrawBaseSprites, st.battle, st.nbattle, setup_base_style, 8},
        {&diff_GrWar_RenderBattle, st.battle, st.nbattle, setup_render, 40},
        {&diff_GrWar_PickBattleAnim, st.battle, st.nbattle, setup_anim, 30},
        {&diff_GrWar_DistManhattanTiles, st.battle, st.nbattle, setup_dist, 40},
        {&diff_GrWar_DistEuclidTiles, st.battle, st.nbattle, setup_dist, 40},
        {&diff_GrWar_DistSquaredTiles, st.battle, st.nbattle, setup_dist, 40},
        {&diff_GrWar_IsAdjacent, st.battle, st.nbattle, setup_dist, 30},
        {&diff_GrWar_IsInArtilleryBand, st.battle, st.nbattle, setup_dist, 30},
        {&diff_GrWar_InWeaponRange, st.battle, st.nbattle, setup_weapon, 60},
        {&diff_GrWar_DirectionToward, st.battle, 1, setup_direction, 40},
        {&diff_GrWar_DirectionToward16, st.battle, 1, setup_direction16, 60},
        {&diff_GrWar_StartUnitEffect, st.battle, st.nbattle, setup_start_effect, 20},
        {&diff_GrWar_FindUnitNearPoint, st.battle, st.nbattle, setup_near_point, 50},
        {&diff_GrWar_FindNearestEnemy, st.battle, st.nbattle, setup_nearest, 40},
        {&diff_GrWar_BattleRound, st.battle, st.nbattle, setup_battle, 80},
        {&diff_GrWar_BattleTick, st.battle, st.nbattle, setup_battle, 40},
        {&diff_GrWar_FindPath, st.battle, st.nbattle, setup_path, 80},
        {&diff_GrWar_FindPath, st.battle, st.nbattle, setup_path_field, 40},
        {&diff_GrWar_ApplyGroundLosses, st.battle, st.nbattle, setup_losses, 40},
        {&diff_GrWar_EndGroundBattle, st.battle, st.nbattle, setup_end, 16},
        {&diff_GrWar_AnimateResultScreen, st.result, st.nresult, setup_result, 20},
    };
    // The file variable at DS:91EE (StartMainAnim / TickGrAnim read the next
    // .ani): the port keeps the open file on the host.
    static const DiffRange file_rec[] = {DIFF_DS(0x91EE, 0x80), {0, 0}};
    diff_GrWar_EnterGroundBattle.ignore = file_rec;
    diff_GrWar_RenderBattle.ignore = file_rec;
    diff_GrWar_BattleRound.ignore = file_rec;
    diff_GrWar_BattleTick.ignore = file_rec;
    // Near routines (ret, not retf).
    diff_GrWar_DrawUnitSprite.near = true;
    diff_GrWar_DrawEffectSprite.near = true;
    diff_GrWar_DrawProjectile.near = true;
    diff_GrWar_DrawBaseSprites.near = true;
    diff_GrWar_DistManhattanTiles.near = true;
    diff_GrWar_DistEuclidTiles.near = true;
    diff_GrWar_DistSquaredTiles.near = true;
    diff_GrWar_IsAdjacent.near = true;
    diff_GrWar_IsInArtilleryBand.near = true;
    diff_GrWar_InWeaponRange.near = true;
    diff_GrWar_DirectionToward.near = true;
    diff_GrWar_DirectionToward16.near = true;
    diff_GrWar_StartUnitEffect.near = true;
    diff_GrWar_ApplyGroundLosses.near = true;

    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, fns[i].bases, fns[i].nbases, fns[i].setup, diff_cases(fns[i].cases));
    gw_free_states(&st);
    return diff_finish();
}
