// grwar: FindPath on a small synthetic map, the distance and direction
// helpers, one BattleRound's damage arithmetic and ApplyGroundLosses.
#include <stdio.h>
#include <string.h>

#include "game/dseg.h"
#include "game/grwar.h"
#include "game/gfx.h"
#include "game/system.h"
#include "platform/fs.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

#define W(off) (*(int16_t *)ds_at(off))
#define B(off) (*(uint8_t *)ds_at(off))
#define FPV(off) (*(FarPtr *)ds_at(off))

static bool wait_retrace(void) { return true; }

static uint8_t *grid(void) { return (uint8_t *)far_host(ds.GrWar_BattleGrids); }
static void clear_grid(void) { memset(grid(), 0, 0xC6); }
static void block(int x, int y) { grid()[x * 11 + y + 0xC] = 1; }

static uint16_t path(int sx, int sy, int gx, int gy)
{
    uint16_t dir = 99;
    GrWar_FindPath((int16_t)sx, (int16_t)sy, (int16_t)gx, (int16_t)gy, (int16_t)gx, (int16_t)gy, &dir);
    return dir;
}

static void find_path(void)
{
    ds.GrWar_BattleGrids = System_GetMem(0xC6);
    int32_t free_before = system_heap_free();

    clear_grid();
    CHECK(path(0, 4, 5, 4) == 3);  // east
    CHECK(path(5, 4, 0, 4) == 1);  // west
    CHECK(path(3, 8, 3, 0) == 2);  // north
    CHECK(path(3, 0, 3, 8) == 4);  // south
    CHECK(path(3, 3, 3, 3) == 0);  // already there

    // A wall at x = 2 with a gap at y = 8: from (0,0) to (4,0) the way leads south first.
    clear_grid();
    for (int y = 0; y <= 7; y++)
        block(2, y);
    uint16_t d = path(1, 0, 4, 0);
    CHECK(d == 4);
    // Past the gap the way is east again.
    CHECK(path(2, 8, 4, 0) == 3 || path(2, 8, 4, 0) == 2);

    // An unreachable goal (walled in): head for the closest reachable cell.
    clear_grid();
    block(8, 3); block(8, 5); block(7, 4); block(9, 4);
    block(8, 4); // the goal itself is occupied too
    d = path(0, 4, 8, 4);
    CHECK(d == 3);

    // Enclosed start: nothing to do.
    clear_grid();
    block(4, 3); block(4, 5); block(3, 4); block(5, 4);
    block(4, 4);
    CHECK(path(4, 4, 10, 4) == 0);

    // FindPath gives back everything it took from the heap.
    CHECK(system_heap_free() == free_before);
}

// Unit lists as the unit assignment screen leaves them.
static GrBattleUnit *list(int side) { return (GrBattleUnit *)far_host(FPV(0x7642 + 4 * side)); }

static void setup_field(void)
{
    for (int s = 1; s <= 2; s++) {
        FPV(0x7642 + 4 * s) = System_GetMem(21 * 0x27);
        System_FillChar(far_host(FPV(0x7642 + 4 * s)), 21 * 0x27, 0);
    }
    ds.GrWarUnits_AttackerSlotCount = 1;
    ds.GrWarUnits_DefenderSlotCount = 1;
    ds.GrWar_BattleGrids = System_GetMem(0xC6);
    ds.GrWar_Projectiles = System_GetMem(0x708);
    System_FillChar(far_host(ds.GrWar_Projectiles), 0x708, 0);
    W(0x7654 + 2) = 0;
    W(0x7654 + 4) = 0;
}

static void geometry(void)
{
    setup_field();
    clear_grid();
    GrBattleUnit *a = &list(1)[0], *d = &list(2)[0];
    a->type = 1; a->count = 10; a->x = 4; a->y = 4; a->dir = 3;
    d->type = 1; d->count = 10; d->x = 5; d->y = 4; d->dir = 1;
    // Pixel positions differ by 16 in x only.
    CHECK(GrWar_DistManhattanTiles(1, 1, 1) == (int16_t)(((16 - 5) >> 4) + 1));
    CHECK(GrWar_DistManhattanTiles(1, 1, 2) == GrWar_DistManhattanTiles(1, 1, 1));
    CHECK((int16_t)GrWar_DistEuclidTiles(1, 1, 1) == 1);
    CHECK(GrWar_IsAdjacent(1, 1, 1));
    CHECK(GrWar_InWeaponRange(1, 1, 1, 1));
    CHECK(GrWar_InWeaponRange(2, 1, 1, 1));
    CHECK(!GrWar_IsInArtilleryBand(1, 1, 1));
    d->x = 8; // 64 px away: 4 tiles in squared distance 4096 >> 8 = 16
    CHECK(GrWar_DistSquaredTiles(1, 1, 1) == ((4096 - 1) >> 8) + 1);
    CHECK(GrWar_IsInArtilleryBand(1, 1, 1));
    CHECK(!GrWar_InWeaponRange(1, 1, 1, 1));
    CHECK(GrWar_InWeaponRange(4, 1, 1, 1));
    CHECK(GrWar_FindNearestEnemy(1, 1) == 1);
    d->count = 0;
    CHECK(GrWar_FindNearestEnemy(1, 1) == 0);
    d->count = 10;

    CHECK(GrWar_DirectionToward(5, 5, 9, 5) == 3);
    CHECK(GrWar_DirectionToward(5, 5, 1, 5) == 1);
    CHECK(GrWar_DirectionToward(5, 5, 5, 9) == 4);
    CHECK(GrWar_DirectionToward(5, 5, 5, 1) == 2);
    CHECK(GrWar_DirectionToward(5, 5, 9, 5) == 3);
    // 16-way: straight east is sprite 6+? (a = 4 + 4*... ); check it is stable and in 0..15.
    for (int i = -8; i <= 8; i++) {
        int16_t r = GrWar_DirectionToward16(0, 0, 40, (int16_t)(i * 4));
        CHECK(r >= 0 && r <= 15);
    }
    CHECK(GrWar_DirectionToward16(0, 0, 40, 0) == GrWar_DirectionToward16(0, 0, 80, 0));

    GrWar_StartUnitEffect(1, 1, 5);
    CHECK(a->effect == 5 && a->effTimer == B(5 * 6 + 0x86) && a->effDelay == 3);
}

// One round: two adjacent type-1 slots hit each other; damage = count * attack / total * strength / 500.
static void round_damage(void)
{
    setup_field();
    clear_grid();
    GrBattleUnit *a = &list(1)[0], *d = &list(2)[0];
    a->type = 1; a->count = 10; a->x = 4; a->y = 4; a->dir = 3; a->mode = 1; a->moveDelay = 1;
    d->type = 1; d->count = 10; d->x = 5; d->y = 4; d->dir = 1; d->mode = 3; d->target = 1; d->moveDelay = 1;
    for (int s = 1; s <= 2; s++) {
        GrBattleUnit *b = &list(s)[20];
        b->type = 5; b->count = 1; b->hp = 100; b->y = 4; b->x = s == 1 ? 0 : 15;
    }
    for (int s = 1; s <= 2; s++) {
        memcpy(ds_at(0x7646 + 16 * s + 4 * 1), &(int32_t){10}, 4);        // totals
        memcpy(ds_at(0x7676 + 16 * s + 4 * 1), &(int32_t){100}, 4);       // attack sums
    }
    // unit type 1 strength (word table at DS:28) and cooldown (DS:20)
    int32_t strength = W(0x28 + 2 * 1);
    int32_t expect = 10 * 100 / 10 * strength / 500;
    if (expect < 1) expect = 1;
    if (expect > 15) expect = 15;
    ds.System_RandSeed = 1234;
    B(0x770B) = 0;
    ds.Game_BattleFlag = false;
    ds.Gfx_Charset = 0;
    GrWar_BattleRound();
    // Whoever the random side order lets act first hits; the victim is busy with
    // its hit effect and cannot strike back this round.
    bool a_hit = d->count == 10 - expect;
    GrBattleUnit *att = a_hit ? a : d, *vic = a_hit ? d : a;
    CHECK((a->count == 10) != (d->count == 10));
    CHECK(vic->count == 10 - expect);
    CHECK(att->count == 10);
    CHECK(att->cooldown == W(0x20 + 2 * 1) - 1);
    CHECK(vic->effect == 8 && att->effect == 1);
    CHECK(vic->effTimer == B(8 * 6 + 0x86) || vic->effTimer == B(8 * 6 + 0x86) - 1);

    // The victim strikes back in the next round once its effect has run out
    a->effect = d->effect = 0;
    GrWar_BattleRound();
    // ... with the damage its few survivors can still deal (2 * 100 / 10 * 40 / 500 = 1, at least 1).
    CHECK(vic->count == 10 - expect);
    CHECK(att->count == 9);
}

static void losses(void)
{
    setup_field();
    ds.System_RandSeed = 7;
    // Totals 10 / 20 of type 1; survivors 4 attackers (slot) + 2 unassigned, 5 defenders.
    list(1)[0].type = 1; list(1)[0].count = 4;
    list(2)[0].type = 1; list(2)[0].count = 5;
    memcpy(ds_at(0x7646 + 16 + 4), &(int32_t){10}, 4);
    memcpy(ds_at(0x7646 + 32 + 4), &(int32_t){20}, 4);
    memcpy(ds_at(0x7676 + 4), &(int32_t){2}, 4);
    W(0x7798) = 3; W(0x779A) = 2; W(0x779C) = 0;
    ds.Game_GroupCount = 1;
    ds.Game_PlanetForceCount = 0;
    ds.Game_Groups = System_GetMem(2 * 32 * sizeof(TGroup));
    System_FillChar(far_host(ds.Game_Groups), 2 * 32 * sizeof(TGroup), 0);
    TGroup *g = (TGroup *)far_host(ds.Game_Groups);
    g->Kind = 1; g->System = 3; g->Planet = 2; g->Moon = 0; g->Status = 1;
    g->Units[1][0].ShipType = 10;
    g->Units[1][0].Weapons[0] = 30;
    g->Units[1][0].Weapons[3] = 40; // weapon 4 is not scaled
    ds.Game_CurBody = System_GetMem(sizeof(TBody));
    System_FillChar(far_host(ds.Game_CurBody), sizeof(TBody), 0);
    for (int r = 0; r < 11; r++)
        ds.Races_Races[r].ForceCount = 0;

    GrWar_ApplyGroundLosses();
    int32_t l1, l2, s1;
    memcpy(&s1, ds_at(0x76C6 + 4), 4);
    memcpy(&l1, ds_at(0x76E6 + 4), 4);
    memcpy(&l2, ds_at(0x76F6 + 4), 4);
    CHECK(s1 == 6);   // 4 in the slot + 2 unassigned
    CHECK(l1 == 4);   // 10 - 6
    CHECK(l2 == 15);  // 20 - 5
    CHECK(g->Units[1][0].ShipType == 10 * 6 / 10);
    CHECK(g->Units[1][0].Weapons[0] == 30 * 6 / 10);
    CHECK(g->Units[1][0].Weapons[3] == 40);
    // Types that were absent on the field are zeroed.
    g->Units[1][1].ShipType = 7;
    GrWar_ApplyGroundLosses();
    CHECK(g->Units[1][1].ShipType == 0);
}

static void pick_anim(void)
{
    // Types present on either side make the animation eligible.
    for (int s = 1; s <= 2; s++)
        for (int t = 1; t <= 4; t++)
            memcpy(ds_at(0x7646 + 16 * s + 4 * t), &(int32_t){s == 1 ? 3 : 0}, 4);
    for (int i = 0; i < 20; i++) {
        int16_t id = GrWar_PickBattleAnim();
        CHECK(id >= 1 && id <= 14);
    }
    GrWar_Init();
    CHECK(ds.Game_GrWarProc == far_ptr(0x1537, 0x11F2));
}

int main(int argc, char *argv[])
{
    mem_init();
    if (argc < 2 || !fs_init(argv[1]) || !dseg_init()) {
        printf("no game data\n");
        return 77;
    }
    system_init();
    ds.Gfx_LineStride = 320;
    ds.Gfx_LineStrideCopy = 320;
    gfx_wait_retrace = wait_retrace;

    find_path();
    geometry();
    round_damage();
    losses();
    pick_anim();
    printf(ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
}
