// spwar: one space-battle step with a fixed RandSeed (hit rolls, damage,
// swap-removal of the dead) and the survivors scaled back into a group.
#include <stdio.h>
#include <string.h>

#include "game/dseg.h"
#include "game/fixes.h"
#include "game/gfx.h"
#include "game/groups.h"
#include "game/planetinfo.h"
#include "game/spwar.h"
#include "game/system.h"
#include "platform/fs.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static bool wait_retrace(void) { return true; }

static void reset(void)
{
    dseg_clear();
    vid_clear();
    dseg_init();
    system_init();
    ds.Gfx_LineStride = 320;
    ds.Gfx_LineStrideCopy = 320;
    gfx_wait_retrace = wait_retrace;
    ds.SpWar_View = game_alloc(0x5E60);
    ds.SpWar_Background = game_alloc(0x5E60);
    ds.SpWar_Sprites = game_alloc(0xF00);
    ds.SpWar_Face = game_alloc(0x1180);
    ds.SpWar_AttackerList = game_alloc(0x177C);
    ds.SpWar_DefenderList = game_alloc(0x177C);
    ds.SpWar_StepNo = 0;
    ds.SpWar_BattleOver = 0;
    ds.SpWar_AttackerShipCount = ds.SpWar_DefenderShipCount = 0;
}

static SpShip *ship(uint8_t *list, int i) { return (SpShip *)list + i - 1; }

static void set_ship(SpShip *s, uint8_t id, uint8_t cls, uint16_t dmg, int16_t hp)
{
    memset(s, 0, sizeof *s);
    s->id = id;
    s->cls = cls;
    s->dmg = dmg;
    s->hp = hp;
    s->x = 20;
    s->y = 30;
    s->len = 2;
    s->path = 1;
    s->pos = 1;
    s->cool = 5; // no movement this step: only the hit rolls use the generator
}

// A step with one attacker (class 4, dmg 30, hp 100) against one defender
// (class 3, dmg 20, hp 25). Each side consumes Random(1) and Random(100).
static void test_step(void)
{
    // Find a seed where both rolls hit (< 11), to check the arithmetic.
    uint32_t seed = 0;
    for (uint32_t s = 1; s < 100000; s++) {
        ds.System_RandSeed = s;
        System_Random(1);
        uint16_t a = System_Random(100);
        System_Random(1);
        uint16_t b = System_Random(100);
        if (a < 11 && b < 11) { seed = s; break; }
    }
    CHECK(seed != 0);

    reset();
    ds.SpWar_AttackerShipCount = 1;
    ds.SpWar_DefenderShipCount = 1;
    ds.SpWar_AttackerStart = 1;
    ds.SpWar_DefenderStart = 1;
    set_ship(ship(ds.SpWar_AttackerList, 1), 1, 4, 30, 100);
    set_ship(ship(ds.SpWar_DefenderList, 1), 2, 3, 20, 25);
    ds.System_RandSeed = seed;
    SpWar_StepSpaceBattle();
    CHECK(ds.SpWar_StepNo == 1);
    // Attacker (4) shoots first: 25 - 30 < 1, the defender dies. It stays in
    // the list tail with the explosion at frame 1 + the one drawn this step.
    CHECK(ds.SpWar_DefenderShipCount == 0);
    CHECK(ship(ds.SpWar_DefenderList, 1)->hp == -5);
    CHECK(ship(ds.SpWar_DefenderList, 1)->pos == 2);
    // The dead defender doesn't shoot back and nobody else is hit.
    CHECK(ship(ds.SpWar_AttackerList, 1)->hp == 100);
    CHECK(ship(ds.SpWar_AttackerList, 1)->cool == 4);
    // The explosion is still playing, so the battle isn't over yet.
    CHECK(ds.SpWar_Exploding == 1);
    CHECK(ds.SpWar_BattleOver == 0);

    // Steps 2..4 of the 4-step cycle do nothing.
    int32_t s2 = ds.System_RandSeed;
    for (int i = 2; i <= 4; i++) {
        SpWar_StepSpaceBattle();
        CHECK(ds.SpWar_StepNo == i);
    }
    CHECK(ds.System_RandSeed == s2);

    // Both sides alive, defender (class 1, dmg 7) against class 4: a class < 3
    // hit on a class > 2 ship does a third of the damage.
    for (uint32_t s = 1; s < 100000; s++) {
        ds.System_RandSeed = s;
        System_Random(1);
        uint16_t a = System_Random(100);
        System_Random(1);
        uint16_t b = System_Random(100);
        if (a >= 11 && b < 11) { seed = s; break; }
    }
    reset();
    ds.SpWar_AttackerShipCount = 1;
    ds.SpWar_DefenderShipCount = 1;
    ds.SpWar_AttackerStart = 1;
    ds.SpWar_DefenderStart = 1;
    set_ship(ship(ds.SpWar_AttackerList, 1), 1, 4, 30, 100);
    set_ship(ship(ds.SpWar_DefenderList, 1), 2, 1, 7 * 3 + 2, 25);
    ds.System_RandSeed = seed;
    SpWar_StepSpaceBattle();
    CHECK(ship(ds.SpWar_AttackerList, 1)->hp == 100 - 23 / 3);
    CHECK(ship(ds.SpWar_DefenderList, 1)->hp == 25);
    CHECK(ds.SpWar_AttackerShipCount == 1 && ds.SpWar_DefenderShipCount == 1);
}

// 10 ships in a group, 4 survive: 4 + 6/5 = 5 remain, weapons scale with
// them and the 5 lost ships are counted per class.
static void test_losses(void)
{
    reset();
    ds.Game_Groups = game_alloc(0x2840);
    memset(ds.Game_Groups, 0, 0x2840);
    ds.Game_GroupCount = 1;
    ds.Game_PlanetForceCount = 0;
    ds.Game_CurSystem = 1;
    ds.SpWar_BattleSystem = 1;
    ds.SpWar_BattlePlanet = 1;
    ds.SpWar_BattleMoon = 0;
    TGroup *g = Groups_At(0, 1);
    g->Kind = 1;
    g->System = 1;
    g->Planet = 1;
    g->Status = 1;
    g->Units[0][0].Count = 10;
    g->Units[0][0].Weapons[0] = 5;
    g->Units[0][0].Weapons[1] = 7;
    ds.SpWar_AttackerShipCount = 4;
    for (int i = 1; i <= 4; i++)
        set_ship(ship(ds.SpWar_AttackerList, i), 1, 1, 1, 10);
    SpWar_ApplySpaceLosses();
    CHECK(g->Units[0][0].Count == 5);
    CHECK(g->Units[0][0].Weapons[0] == 2);  // 5 * 5 / 10
    CHECK(g->Units[0][0].Weapons[1] == 3);  // 7 * 5 / 10
    CHECK(ds.SpWar_AttackerLosses[0] == 5);
    CHECK(ds.SpWar_AttackerLosses[1] == 0);
    CHECK(ds.Game_CurMoon[0] == 0);
}

static TBody *zeus_body(int16_t moon)
{
    ds.Game_CurMoon[0] = moon;
    PlanetInfo_SelectCurBody();
    return ds.Game_CurBody;
}

// Fix (game/fixes.h): a battle at Zeus (Amnesty 4), whose planet and first
// two moons the Morgruls (race 3, at war: status 2) hold with 10 fighters
// each. The fight destroys moon 2's ten and nothing else. An ally (race 4,
// status 6) has a force elsewhere, which leaves LossPtr on the player's side.
static void test_garrison_losses(void)
{
    for (int fix = 0; fix <= 1; fix++) {
        reset();
        fixes.garrison_losses = fix;
        ds.Game_GroupCount = 0;
        ds.Game_PlanetForceCount = 0;
        for (int r = 0; r < 11; r++) {
            ds.Races_Races[r].Status = 0;
            ds.Races_Races[r].ForceCount = 0;
        }
        ds.Races_Races[3 - 2].Status = 2;
        TRace *ally = &ds.Races_Races[4 - 2];
        ally->Status = 6;
        ally->ForceCount = 1;
        ally->Forces[0].System = 2;
        ally->Forces[0].Active = 1;
        ds.Game_CurSystem = 1;
        ds.Game_CurPlanet[0] = 4;
        ds.SpWar_BattleSystem = 1;
        ds.SpWar_BattlePlanet = 4;
        ds.SpWar_BattleMoon = 0;
        CHECK(ds.Galaxy_MoonLists[0][3].Count >= 2);
        for (int16_t m = 0; m <= 2; m++) {
            TBody *b = zeus_body(m);
            b->Owner = 3;
            memset(b->Stores, 0, sizeof b->Stores);
            b->Stores[0] = 10;
        }
        SpWar_InitSpaceFleets();
        CHECK(ds.SpWar_DefenderShipCount == 30);
        CHECK(ship(ds.SpWar_DefenderList, 25)->id == (fix ? 202 : 0xC9));
        ds.SpWar_DefenderShipCount = 20;  // moon 2's ten, added last, are gone
        SpWar_ApplySpaceLosses();
        if (fix) {
            CHECK(zeus_body(0)->Stores[0] == 10);
            CHECK(zeus_body(1)->Stores[0] == 10);
            CHECK(zeus_body(2)->Stores[0] == 2);
            CHECK(ds.SpWar_DefenderLosses[0] == 8 && ds.SpWar_AttackerLosses[0] == 0);
        } else {
            // The planet and moon 2 keep a fifth, moon 1 gains the planet's
            // survivors; the net loss of 8 goes to the player's side.
            CHECK(zeus_body(0)->Stores[0] == 2);
            CHECK(zeus_body(1)->Stores[0] == 18);
            CHECK(zeus_body(2)->Stores[0] == 2);
            CHECK(ds.SpWar_DefenderLosses[0] == 0 && ds.SpWar_AttackerLosses[0] == 8);
        }
    }
    fixes.garrison_losses = true;
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    dseg_clear();
    vid_clear();
    dseg_init();
    test_step();
    test_losses();
    test_garrison_losses();
    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
