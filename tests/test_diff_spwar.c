// Differential test of unit SpWar (20d4): the space battle, original (under
// Unicorn) against the port. Battles are set up at random bodies of the home
// system (fleets, alien forces, store ships of the planet and its moons),
// then entered like Main::ScreenSpaceBattle does, and every routine runs
// from those states with the ships, the animation scripts and the random
// seed perturbed. See docs/difftest.md. Needs the game data.
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/anim.h"
#include "game/dseg.h"
#include "game/groups.h"
#include "game/planetinfo.h"
#include "game/spwar.h"

DIFF_PROC(SpWar_EnterSpaceBattle, SpWar_EnterSpaceBattle());
DIFF_PROC(SpWar_InitSpaceFleets, SpWar_InitSpaceFleets());
DIFF_PROC(SpWar_ClearHotspots, SpWar_ClearHotspots());
DIFF_PROC(SpWar_LoadWarAnimDef, SpWar_LoadWarAnimDef((int16_t)a[0]));
DIFF_PROC(SpWar_FreeWarAnims, SpWar_FreeWarAnims());
DIFF_PROC(SpWar_PlayWarAnimScript, SpWar_PlayWarAnimScript());
DIFF_PROC(SpWar_DrawPilotFace, SpWar_DrawPilotFace());
DIFF_PROC(SpWar_DrawShipDot1, SpWar_DrawShipDot1((int16_t)a[0], (int16_t)a[1]));
DIFF_PROC(SpWar_DrawShipDot2, SpWar_DrawShipDot2((int16_t)a[0], (int16_t)a[1]));
DIFF_PROC(SpWar_DrawShipDot4, SpWar_DrawShipDot4((int16_t)a[0], (int16_t)a[1]));
DIFF_PROC(SpWar_DrawShipDot7, SpWar_DrawShipDot7((int16_t)a[0], (int16_t)a[1]));
DIFF_PROC(SpWar_DrawExplosion, SpWar_DrawExplosion((int16_t)a[0], (int16_t)a[1]));
DIFF_PROC(SpWar_StepSpaceBattle, SpWar_StepSpaceBattle());
DIFF_PROC(SpWar_ApplySpaceLosses, SpWar_ApplySpaceLosses());
DIFF_PROC(SpWar_EndSpaceBattle, SpWar_EndSpaceBattle());

#define W(off) (*(int16_t *)ds_at(off))
#define B(off) (*(uint8_t *)ds_at(off))
#define FP(off) (*(FarPtr *)ds_at(off))

enum : uint16_t {
    BATTLE_END = 0x77DA,
    RETREATED = 0x77DB,
    VIEW = 0x77DC,
    SPRITES = 0x77E4,
    STEP_NO = 0x77EC,
    ATT_LIST = 0x779E,
    DEF_LIST = 0x77A2,
    SYS = 0x7798,
    PLANET = 0x779A,
    MOON = 0x779C,
    ATT_START = 0x77B2,
    DEF_START = 0x77B4,
    LOSS_PTR = 0x77D6,
    ANIM_REPEAT = 0x7790,
    ANIM_SCRIPT = 0x7792,
    ANIM_FRAME = 0x7794,
    ANIM_DELAY = 0x7796,
    ANIM_DEF = 0x7E44,
    CURTAIN = 0x77EE,  // left, right, top, bottom
    USED_SCRIPTS = 0x77F6,
};

static void seed(DiffRng *r)
{
    ds.System_RandSeed = (int32_t)diff_rand(r, 0xFFFFFFFFu);
}

static uint8_t moon_count(int sys, int planet)
{
    return ((const TMoonList *)far_host(ds.Galaxy_MoonLists[sys - 1]))[planet - 1].Count;
}

static TBody *select_body(int moon)
{
    ds.Game_CurMoon[ds.Game_CurSystem - 1] = (int16_t)moon;
    PlanetInfo_SelectCurBody();
    return far_host(ds.Game_CurBody);
}

// --- a battle -------------------------------------------------------------------------------

// A battle at a random body of system 1: the player's fleets and planet
// forces (some elsewhere or not fit to fight), alien forces of races at war
// (status 2 defends, 6 attacks) and store ships of the bodies' owners.
// `fight`: both sides surely have ships.
static void make_battle(DiffRng *r, bool fight)
{
    int sys = 1, planet = diff_range(r, 1, ds.Galaxy_PlanetCount[sys - 1]);
    int moons = moon_count(sys, planet);
    int moon = diff_pick(r, 0, 0, diff_range(r, 0, moons));
    W(SYS) = (int16_t)sys;
    W(PLANET) = (int16_t)planet;
    W(MOON) = (int16_t)moon;
    ds.Game_CurSystem = (int16_t)sys;
    ds.Game_CurPlanet[sys - 1] = (int16_t)planet;

    int counts[2] = {diff_range(r, 0, 5), diff_range(r, 0, 3)};
    if (fight && counts[0] == 0)
        counts[0] = 1;
    ds.Game_GroupCount = (int16_t)counts[0];
    ds.Game_PlanetForceCount = (int16_t)counts[1];
    for (int li = 0; li < 2; li++)
        for (int g = 1; g <= counts[li]; g++) {
            TGroup *grp = Groups_At((int16_t)li, (int16_t)g);
            bool forced = fight && li == 0 && g == 1;
            bool here = forced || diff_chance(r, 75);
            grp->Kind = (uint8_t)(forced ? 1 : diff_pick(r, 1, 1, 3, 5, 2, 4));
            grp->System = (uint8_t)(here ? sys : diff_pick(r, sys, 2));
            grp->Planet = (uint8_t)(here ? planet : diff_range(r, 1, 3));
            grp->Moon = (uint8_t)diff_range(r, 0, 1);
            grp->Status = (uint8_t)(forced ? 1 : diff_pick(r, 1, 2, 7, 3, 4, 1));
            for (int j = 0; j < 4; j++) {
                TGroupSlot *sl = &grp->Units[0][j];
                sl->ShipType = (int16_t)diff_pick(r, 0, diff_range(r, 0, 8), diff_range(r, 0, 40),
                                                  diff_range(r, 0, 40), 200);  // > 500 in all: full
                if (fight && li == 0 && g == 1 && j == 0 && sl->ShipType == 0)
                    sl->ShipType = 3;
                for (int w = 0; w < 4; w++)
                    sl->Weapons[w] = (int16_t)diff_pick(r, 0, diff_range(r, 0, 50), diff_range(r, 0, 500));
            }
        }
    ds.Game_StaffExp[2] = (uint16_t)diff_pick(r, 0, diff_range(r, 0, 70));
    ds.Game_StaffHired[2] = (uint16_t)diff_range(r, 0, 3);

    bool defender = false;
    for (int k = 0; k < 11; k++) {
        TRace *rc = &ds.Races_Races[k];
        if (!diff_chance(r, 35) && !(fight && k == 10 && !defender))
            continue;
        rc->Status = (int8_t)(fight && k == 10 && !defender ? 2 : diff_pick(r, 2, 2, 6, 0, 1));
        rc->ForceCount = (int8_t)diff_pick(r, 0, diff_range(r, 1, 3), diff_range(r, 1, 7));
        if (fight && k == 10 && !defender && rc->ForceCount == 0)
            rc->ForceCount = 1;
        for (int f = 0; f < rc->ForceCount; f++) {
            TRaceForce *fc = &rc->Forces[f];
            bool here = diff_chance(r, 70) || (fight && k == 10 && !defender && f == 0);
            fc->System = (uint8_t)(here ? sys : 2);
            fc->Body = (uint8_t)(here ? planet : diff_pick(r, planet, diff_range(r, 1, 3)));
            fc->Moon = (uint8_t)diff_range(r, 0, 1);
            fc->Active = (uint8_t)(here && fight && k == 10 && !defender ? 1 : diff_pick(r, 1, 1, 0, 2));
            for (int j = 0; j < 4; j++)
                fc->Units[j] = (int16_t)diff_pick(r, 0, diff_range(r, 0, 10), diff_range(r, 0, 60),
                                                  diff_range(r, 0, 60), 300);
            if (fight && k == 10 && !defender && f == 0 && fc->Units[0] == 0)
                fc->Units[0] = 2;
            if (rc->Status == 2 && fc->Active == 1 && here)
                defender = true;
        }
    }

    for (int m = 0; m <= moons; m++) {
        TBody *b = select_body(m);
        if (!diff_chance(r, 40))
            continue;
        b->Owner = (uint8_t)diff_pick(r, 0, 1, diff_range(r, 2, 12));
        for (int j = 0; j < 4; j++)
            b->Stores[j] = diff_pick(r, 0, diff_range(r, 0, 10), diff_range(r, 0, 150));
    }
    select_body(moon);
}

// Main::ScreenSpaceBattle's entry, through the port.
static void enter_battle(DiffRng *r)
{
    SpWar_InitSpaceFleets();
    SpWar_EnterSpaceBattle();
    SpWar_ClearHotspots();
    SpWar_LoadWarAnimDef((int16_t)diff_range(r, 1, 25));
}

static SpShip *ship(uint16_t list, int i) { return (SpShip *)far_host(FP(list)) + i - 1; }

// --- setups -------------------------------------------------------------------------------------

static void setup_init(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    make_battle(r, diff_chance(r, 50));
}

static void setup_enter(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    if (diff_chance(r, 25)) {  // a side without ships: over at once
        bool att = diff_chance(r, 50);
        *(att ? &ds.SpWar_AttackerShipCount : &ds.SpWar_DefenderShipCount) = 0;
        W(att ? ATT_START : DEF_START) = 0;
    }
    ds.Game_StaffHired[2] = (uint16_t)diff_range(r, 0, 3);
}

static void setup_none(DiffRng *r, int32_t *a)
{
    (void)r;
    (void)a;
}

// Ships moved along their routes, damaged, cooling down; some of each side
// dead (swapped behind the live count, exploding).
static void perturb_ships(DiffRng *r)
{
    static const uint16_t lists[2] = {ATT_LIST, DEF_LIST};
    int16_t *counts[2] = {&ds.SpWar_AttackerShipCount, &ds.SpWar_DefenderShipCount};
    for (int side = 0; side < 2; side++) {
        int n = *counts[side];
        for (int i = 1; i <= n; i++) {
            SpShip *s = ship(lists[side], i);
            if (diff_chance(r, 30))
                s->cool = (uint8_t)diff_pick(r, 0, 0, 1, 2, 10);
            if (diff_chance(r, 30))  // near the end of the route
                s->pos = (uint8_t)diff_range(r, 0, s->len);
            if (diff_chance(r, 20))
                s->hp = (int16_t)diff_pick(r, 1, 5, diff_range(r, 1, 20000));
            if (diff_chance(r, 10))
                s->dmg = (uint16_t)diff_pick(r, 0, 1, 3, diff_range(r, 0, 3000));
        }
        if (n > 0 && diff_chance(r, 40)) {  // kill some
            int dead = diff_pick(r, 1, diff_range(r, 1, n), n);
            for (int k = 0; k < dead; k++) {
                SpShip *s = ship(lists[side], n - k);
                s->hp = (int16_t)diff_pick(r, 0, -5, -300);
                s->pos = (uint8_t)diff_pick(r, 1, 1, 2, 5, 9);
            }
            *counts[side] = (int16_t)(n - dead);
        }
    }
}

static void setup_step(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    W(STEP_NO) = (int16_t)diff_pick(r, 0, 4, 4, 4, 1, 2);
    B(BATTLE_END) = (uint8_t)diff_pick(r, 0, 0, 0, 1);
    ds.Game_BattleFlag = (uint8_t)diff_pick(r, 0, 0, 0, 1);
    perturb_ships(r);
}

// Mid-script: a random entry, delay, repeat count and curtain, with the
// entry's animation loaded or not.
static void setup_anim(DiffRng *r, int32_t *a)
{
    (void)a;
    seed(r);
    W(STEP_NO) = (int16_t)diff_pick(r, 1, 2, 3, 4);
    if (diff_chance(r, 15))
        perturb_ships(r);
    if (diff_chance(r, 8))  // over: the battle ends this tick
        B(BATTLE_END) = 1;
    const uint8_t *def = far_host(FP(ANIM_DEF));
    int len = 1;
    while (len < 140 && def[len * 4 - 1] != 0xFF)
        len++;
    int frame = diff_pick(r, len - 1, len - 1, diff_range(r, 1, len - 1), 1);
    if (frame < 1)
        frame = 1;
    W(ANIM_FRAME) = (int16_t)frame;
    W(ANIM_DELAY) = (int16_t)diff_pick(r, 0, 0, 0, 1, 7);
    W(ANIM_REPEAT) = (int16_t)diff_pick(r, 0, 0, 1, 2);
    if (diff_chance(r, 10))
        W(ANIM_SCRIPT) = 0;
    if (diff_chance(r, 50) && def[frame * 4 - 4] != 0)
        Anim_LoadWarAnim(def[frame * 4 - 4]);
    for (int i = 0; i < 4; i++)
        B(USED_SCRIPTS + i) = (uint8_t)diff_pick(r, 0, B(USED_SCRIPTS + i), (int32_t)diff_rand(r, 256));
    B(USED_SCRIPTS) &= 0xFC;
    B(USED_SCRIPTS + 3) &= 0x03;
    // The curtain after k ticks (it closes at k = 7, when it is less than
    // 15h high) or done (2000).
    int k = diff_pick(r, 0, diff_range(r, 0, 7), 6, 7, 100);
    int16_t *c = (int16_t *)ds_at(CURTAIN);
    if (k == 100) {
        c[0] = 2000;
    } else {
        c[0] = (int16_t)(0xA0 + 10 * k);
        c[1] = (int16_t)(0x13F - 10 * k);
        c[2] = (int16_t)(0x31 + 10 * k);
        c[3] = (int16_t)(199 - 10 * k);
    }
}

static void setup_loaddef(DiffRng *r, int32_t *a)
{
    if (diff_chance(r, 30))
        W(ANIM_SCRIPT) = 0;
    a[0] = diff_range(r, 1, 25);
}

static void setup_free(DiffRng *r, int32_t *a)
{
    setup_anim(r, a);
}

static void setup_dots(DiffRng *r, int32_t *a)
{
    a[0] = diff_pick(r, 0, 0x280, diff_range(r, 0, 0xF00 - 0x150));
    a[1] = diff_pick(r, 0, diff_range(r, 0, 0x5E60 - 0x150), diff_range(r, 0, 0x5E60 - 0x150));
}

static void setup_explosion(DiffRng *r, int32_t *a)
{
    int cls = diff_range(r, 1, 4), pos = diff_range(r, 1, 9);
    a[0] = diff_pick(r, pos * 8 + 0x278 + cls * 0x280, diff_range(r, 0, 0xF00 - 0x2B0));
    a[1] = diff_range(r, 0, 0x5E60 - 0x480);
}

static void setup_losses(DiffRng *r, int32_t *a)
{
    (void)a;
    perturb_ships(r);
    // Shuffle the live ships (count_alive looks at ids and classes).
    static const uint16_t lists[2] = {ATT_LIST, DEF_LIST};
    int16_t counts[2] = {ds.SpWar_AttackerShipCount, ds.SpWar_DefenderShipCount};
    for (int side = 0; side < 2; side++)
        for (int i = counts[side]; i > 1; i--) {
            int j = diff_range(r, 1, i);
            SpShip t = *ship(lists[side], i);
            *ship(lists[side], i) = *ship(lists[side], j);
            *ship(lists[side], j) = t;
        }
    // Bodies taken over by a race at war since the fleets were built.
    int moons = moon_count(W(SYS), W(PLANET));
    for (int m = 0; m <= moons; m++) {
        TBody *b = select_body(m);
        if (!diff_chance(r, 30))
            continue;
        int k = diff_range(r, 0, 10);
        b->Owner = (uint8_t)(k + 2);
        ds.Races_Races[k].Status = (int8_t)diff_pick(r, 2, 6, ds.Races_Races[k].Status);
        for (int j = 0; j < 4; j++)
            b->Stores[j] = diff_pick(r, 0, diff_range(r, 0, 10), diff_range(r, 0, 150));
    }
    select_body(W(MOON));
    FP(LOSS_PTR) = (FarPtr)diff_pick(r, (int32_t)FP(LOSS_PTR), (int32_t)far_ptr(MEM_DS_SEG, 0x77B6),
                                     (int32_t)far_ptr(MEM_DS_SEG, 0x77C6));
    B(RETREATED) = (uint8_t)diff_pick(r, 0, 0, 1);
}

// --- states ---------------------------------------------------------------------------------------

// Loading a war animation goes through the data-file FileRec at DS:91EE (128
// bytes: handle, mode, ..., name at +30h), which the port keeps on the host
// (src/game/gfx.c data_file) and never writes.
static const DiffRange data_filerec[] = {DIFF_DS(0x91EE, 0x80), {0, 0}};

enum { BATTLES = 6 };

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    if (!diff_boot_new_game(1500)) {
        printf("FAILED to boot\n");
        return 1;
    }
    DiffState *plain = diff_save();
    // Per battle: before InitSpaceFleets, after it, and on the battle screen.
    DiffState *before[BATTLES], *fleets[BATTLES], *battle[BATTLES];
    for (int i = 0; i < BATTLES; i++) {
        DiffRng r = {0x5157A2ull + (uint64_t)i * 0x9E3779B97F4A7C15ull};
        diff_load(plain);
        make_battle(&r, true);
        before[i] = diff_save();
        SpWar_InitSpaceFleets();
        fleets[i] = diff_save();
        diff_load(before[i]);
        enter_battle(&r);
        battle[i] = diff_save();
        printf("battle %d: system %d planet %d moon %d, %d attackers, %d defenders%s\n", i, W(SYS),
               W(PLANET), W(MOON), ds.SpWar_AttackerShipCount, ds.SpWar_DefenderShipCount,
               B(BATTLE_END) ? " (over)" : "");
    }

    diff_SpWar_PlayWarAnimScript.ignore = data_filerec;
    diff_SpWar_EnterSpaceBattle.ignore = data_filerec;
    diff_SpWar_EndSpaceBattle.ignore = data_filerec;
    diff_run(&diff_SpWar_ClearHotspots, battle, BATTLES, setup_none, diff_cases(2));
    diff_run(&diff_SpWar_InitSpaceFleets, &plain, 1, setup_init, diff_cases(60));
    diff_run(&diff_SpWar_EnterSpaceBattle, fleets, BATTLES, setup_enter, diff_cases(12));
    diff_run(&diff_SpWar_DrawShipDot1, battle, BATTLES, setup_dots, diff_cases(6));
    diff_run(&diff_SpWar_DrawShipDot2, battle, BATTLES, setup_dots, diff_cases(6));
    diff_run(&diff_SpWar_DrawShipDot4, battle, BATTLES, setup_dots, diff_cases(6));
    diff_run(&diff_SpWar_DrawShipDot7, battle, BATTLES, setup_dots, diff_cases(6));
    diff_run(&diff_SpWar_DrawExplosion, battle, BATTLES, setup_explosion, diff_cases(12));
    diff_run(&diff_SpWar_DrawPilotFace, battle, BATTLES, setup_none, diff_cases(6));
    diff_run(&diff_SpWar_LoadWarAnimDef, battle, BATTLES, setup_loaddef, diff_cases(24));
    diff_run(&diff_SpWar_FreeWarAnims, battle, BATTLES, setup_free, diff_cases(24));
    diff_run(&diff_SpWar_StepSpaceBattle, battle, BATTLES, setup_step, diff_cases(60));
    diff_run(&diff_SpWar_PlayWarAnimScript, battle, BATTLES, setup_anim, diff_cases(90));
    diff_run(&diff_SpWar_ApplySpaceLosses, battle, BATTLES, setup_losses, diff_cases(36));
    diff_run(&diff_SpWar_EndSpaceBattle, battle, BATTLES, setup_losses, diff_cases(12));

    diff_free(plain);
    for (int i = 0; i < BATTLES; i++) {
        diff_free(before[i]);
        diff_free(fleets[i]);
        diff_free(battle[i]);
    }
    return diff_finish();
}
