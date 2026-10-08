// Ground battle states for the differential tests of GrWar and GrWarUnits
// (test_diff_grwar.c, test_diff_grwarunits.c). A new game has no battle, so
// the states are built with the port's routines in the order screens 0x13
// and 0x20 call them: forces at the target body, CollectAvailableForces and
// the assignment screen; InitBattleUnits; EnterGroundBattle and some
// BattleTicks; EndGroundBattle.
#pragma once

#include <stdio.h>
#include <string.h>

#include <SDL3/SDL_stdinc.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/groups.h"
#include "game/grwar.h"
#include "game/grwarunits.h"
#include "game/planetinfo.h"

static inline int16_t *gw_w(uint16_t off) { return (int16_t *)ds_at(off); }
static inline uint8_t *gw_b(uint16_t off) { return (uint8_t *)ds_at(off); }
static inline int gw_slots(int side) { return *gw_w((uint16_t)(0x764C + 2 * side)); }
static inline GrBattleUnit *gw_unit(int side, int i)
{
    FarPtr list = *(FarPtr *)ds_at((uint16_t)(0x7642 + 4 * side));
    return (GrBattleUnit *)((uint8_t *)far_host(list) + (i - 1) * 0x27);
}
static inline int gw_proj_count(int side) { return *gw_w((uint16_t)(0x7654 + 2 * side)); }
static inline void gw_set_proj_count(int side, int n) { *gw_w((uint16_t)(0x7654 + 2 * side)) = (int16_t)n; }
static inline int16_t *gw_proj(int side, int k)
{
    return (int16_t *)((uint8_t *)far_host(ds.GrWar_Projectiles) + (side - 1) * 900 + (k - 1) * 0x12);
}
static inline uint8_t *gw_grid(void) { return (uint8_t *)far_host(ds.GrWar_BattleGrids); }
// Longint tables at 7646 + base + 16*side + 4*type (base 0: totals, 30h: attack sums, ...).
static inline int32_t gw_tbl(uint16_t base, int side, int type)
{
    int32_t v;
    memcpy(&v, ds_at((uint16_t)(0x7646 + base + 16 * side + 4 * type)), 4);
    return v;
}
static inline void gw_set_tbl(uint16_t base, int side, int type, int32_t v)
{
    memcpy(ds_at((uint16_t)(0x7646 + base + 16 * side + 4 * type)), &v, 4);
}
static inline int gw_clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline int gw_cap(int type) { return *gw_w((uint16_t)(0x30 + 2 * type)); }

enum { GW_SYSTEM = 1, GW_PLANET = 2, GW_RACE = 3 /* the planet's owner */, GW_RACE2 = 4 };

static inline TBody *gw_body(void) { return (TBody *)far_host(ds.Game_CurBody); }

// PickBattleAnim loops until some war animation has both its unit types on the
// field: make sure one has.
static inline void gw_ensure_anim(void)
{
    for (int r = 1; r <= 14; r++) {
        uint8_t a = *gw_b((uint16_t)(0x5654 + r * 5)), b = *gw_b((uint16_t)(0x5655 + r * 5));
        if ((gw_tbl(0, 1, a) > 0 || gw_tbl(0, 2, a) > 0) && (gw_tbl(0, 1, b) > 0 || gw_tbl(0, 2, b) > 0))
            return;
    }
    for (int t = 1; t <= 4; t++)
        gw_set_tbl(0, 1, t, 1);
}

// The forces at the battle site that CollectAvailableForces and
// ApplyGroundLosses read: the player's fleet 1, the planet owner's force and
// stores, another race's force.
static inline void gw_perturb_owners(DiffRng *r)
{
    TGroup *g = Groups_At(0, 1);
    if (diff_chance(r, 30)) {
        g->Status = (uint8_t)diff_pick(r, 1, 2, 7, 3, 0);
        g->Kind = (uint8_t)diff_pick(r, 1, 1, 5, 2);
        g->Moon = (uint8_t)diff_pick(r, 0, 0, 0, 1);
    }
    for (int t = 0; t < 4; t++)
        if (diff_chance(r, 40)) {
            g->Units[1][t].ShipType = (int16_t)diff_pick(r, 0, 1, 7, 40, 300, 3000);
            for (int w = 0; w < 3; w++)
                g->Units[1][t].Weapons[w] = (int16_t)diff_pick(r, 0, 1, 5, 200, 2000);
        }
    if (diff_chance(r, 20))
        ds.Game_GroupCount = (int16_t)diff_pick(r, 0, 1, 2);
    TRace *own = &ds.Races_Races[GW_RACE - 2], *other = &ds.Races_Races[GW_RACE2 - 2];
    if (diff_chance(r, 30))
        own->Status = (int8_t)diff_pick(r, 2, 2, 6, 0, 3);
    if (diff_chance(r, 30))
        other->Status = (int8_t)diff_pick(r, 6, 6, 2, 0);
    for (int k = 0; k < 2; k++) {
        TRace *rc = k == 0 ? own : other;
        TRaceForce *f = &rc->Forces[0];
        if (diff_chance(r, 20))
            f->Active = (uint8_t)diff_pick(r, 1, 1, 0, 2);
        if (diff_chance(r, 15))
            f->Body = (uint8_t)diff_pick(r, GW_PLANET, GW_PLANET + 1);
        for (int t = 4; t < 8; t++)
            if (diff_chance(r, 40))
                f->Units[t] = (int16_t)diff_pick(r, 0, 1, 12, 150, 2500, -2);
    }
    TBody *b = gw_body();
    if (diff_chance(r, 25))
        b->Owner = (uint8_t)diff_pick(r, GW_RACE, GW_RACE, GW_RACE2, 1, 0);
    for (int t = 4; t < 8; t++)
        if (diff_chance(r, 40))
            b->Stores[t] = diff_pick(r, 0, 1, 9, 300, 70000);
}

// Random slot lists, as the assignment screen may leave them (empty slots
// included).
static inline void gw_perturb_assignment(DiffRng *r)
{
    for (int s = 1; s <= 2; s++) {
        int n = diff_pick(r, gw_slots(s), gw_slots(s), 1, diff_range(r, 1, 20), 20);
        *gw_w((uint16_t)(0x764C + 2 * s)) = (int16_t)n;
        for (int i = 1; i <= n; i++) {
            GrBattleUnit *u = gw_unit(s, i);
            if (u->type < 1 || u->type > 4 || diff_chance(r, 30))
                u->type = (uint8_t)diff_range(r, 1, 4);
            if (u->count <= 0 || diff_chance(r, 30))
                u->count = (int16_t)diff_pick(r, 0, 1, diff_range(r, 1, gw_cap(u->type)), gw_cap(u->type));
        }
    }
    if (diff_chance(r, 10))
        for (int s = 1; s <= 2; s++)
            gw_unit(s, diff_range(r, 1, gw_slots(s)))->count = 0;
}

enum { GW_VARIANTS = 3, GW_MAX_BATTLE = GW_VARIANTS * 4 };

typedef struct GwStates {
    DiffState *pre[GW_VARIANTS];     // forces placed, before CollectAvailableForces
    DiffState *assign[GW_VARIANTS];  // the assignment screen (0x13)
    DiffState *init[GW_VARIANTS];    // after InitBattleUnits
    DiffState *battle[GW_MAX_BATTLE];
    int nbattle;
    DiffState *result[GW_VARIANTS];  // the grvict/grlost screen
    int nresult;
} GwStates;

// Variant v: small, mixed and full (20 slots a side, the 32x32 base) armies.
static inline void gw_place_forces(int v)
{
    static const int16_t att[GW_VARIANTS][4] = {{12, 5, 0, 0}, {60, 20, 10, 8}, {600, 200, 150, 120}};
    static const int16_t def[GW_VARIANTS][4] = {{8, 0, 3, 0}, {40, 10, 12, 4}, {500, 150, 100, 90}};
    static const int32_t stores[GW_VARIANTS][4] = {{0, 2, 0, 0}, {10, 0, 0, 3}, {200, 50, 40, 30}};

    *gw_w(0x7798) = GW_SYSTEM;
    *gw_w(0x779A) = GW_PLANET;
    *gw_w(0x779C) = 0;
    *gw_b(0x778B) = v == 2;
    ds.Game_CurSystem = GW_SYSTEM;
    ds.Game_CurPlanet[GW_SYSTEM - 1] = GW_PLANET;
    ds.Game_CurMoon[GW_SYSTEM - 1] = 0;
    PlanetInfo_SelectCurBody();

    if (ds.Game_GroupCount < 1)
        ds.Game_GroupCount = 1;
    TGroup *g = Groups_At(0, 1);
    g->System = GW_SYSTEM;
    g->Planet = GW_PLANET;
    g->Moon = 0;
    g->Status = 1;
    g->Kind = 1;
    for (int t = 0; t < 4; t++) {
        g->Units[1][t].ShipType = att[v][t];
        g->Units[1][t].Weapons[0] = (int16_t)(att[v][t] * 2);
        g->Units[1][t].Weapons[1] = att[v][t];
        g->Units[1][t].Weapons[2] = (int16_t)(att[v][t] * 3);
    }
    TRace *own = &ds.Races_Races[GW_RACE - 2];
    own->Status = 2;
    own->ForceCount = 1;
    TRaceForce *f = &own->Forces[0];
    f->Active = 1;
    f->System = GW_SYSTEM;
    f->Body = GW_PLANET;
    f->Moon = 0;
    for (int t = 0; t < 4; t++)
        f->Units[4 + t] = def[v][t];
    TRace *other = &ds.Races_Races[GW_RACE2 - 2];
    other->Status = 0;
    other->ForceCount = 1;
    other->Forces[0] = *f;
    TBody *b = gw_body();
    b->Owner = GW_RACE;
    for (int t = 0; t < 4; t++)
        b->Stores[4 + t] = stores[v][t];
}

static inline bool gw_build_states(GwStates *st)
{
    memset(st, 0, sizeof *st);
    if (!diff_boot_new_game(1500)) {
        printf("FAILED to boot a new game\n");
        return false;
    }
    DiffState *boot = diff_save();
    for (int v = 0; v < GW_VARIANTS; v++) {
        diff_load(boot);
        gw_place_forces(v);
        diff_quiesce();
        st->pre[v] = diff_save();
        GrWarUnits_CollectAvailableForces();
        char label[32];
        for (int s = 1; s <= 2; s++) {  // offsets print as +27h*(slot-1)+field
            SDL_snprintf(label, sizeof label, "GrWar list%d (v%d)", s, v);
            diff_name_region(label, *(FarPtr *)ds_at((uint16_t)(0x7642 + 4 * s)), 21 * 0x27, nullptr);
        }
        GrWarUnits_UnitAssignDrawScreen();
        GrWarUnits_SetupUnitHotspots();
        diff_quiesce();
        st->assign[v] = diff_save();
        GrWar_InitBattleUnits();
        SDL_snprintf(label, sizeof label, "GrWar grid (v%d)", v);
        diff_name_region(label, ds.GrWar_BattleGrids, 0xC6, nullptr);
        SDL_snprintf(label, sizeof label, "GrWar projectiles (v%d)", v);
        diff_name_region(label, ds.GrWar_Projectiles, 0x708, nullptr);
        diff_quiesce();
        st->init[v] = diff_save();
        GrWar_EnterGroundBattle();
        GrWar_SetupBattleHotspots();
        diff_quiesce();
        st->battle[st->nbattle++] = diff_save();
        for (int t = 1; t <= 400 && *gw_b(0x770B) == 0; t++) {
            GrWar_BattleTick();
            if ((t == 12 || t == 150 || t == 300) && *gw_b(0x770B) == 0) {
                diff_quiesce();
                st->battle[st->nbattle++] = diff_save();
            }
        }
        if (*gw_b(0x770B) == 0)
            GrWar_EndGroundBattle();
        diff_quiesce();
        st->result[st->nresult++] = diff_save();
    }
    diff_free(boot);
    return true;
}

static inline void gw_free_states(GwStates *st)
{
    for (int v = 0; v < GW_VARIANTS; v++) {
        diff_free(st->pre[v]);
        diff_free(st->assign[v]);
        diff_free(st->init[v]);
    }
    for (int i = 0; i < st->nbattle; i++)
        diff_free(st->battle[i]);
    for (int i = 0; i < st->nresult; i++)
        diff_free(st->result[i]);
}
