// Unit SpWar (20d4): space battle. See spwar.h.
#include "game/spwar.h"

#include <SDL3/SDL.h>
#include <string.h>

#include "game/anim.h"
#include "game/blit.h"
#include "game/gfx.h"
#include "game/gfxpath.h"
#include "game/groups.h"
#include "game/mouse.h"
#include "game/planetinfo.h"
#include "game/sound.h"
#include "game/system.h"
#include "platform/fs.h"

#define NAME(lit) ((const uint8_t *)PSTR(lit))

// Scratch palette for the picture loads (the original used 768 bytes of its
// stack).
static uint8_t scratch_pal[768];

static inline SpShip *Ship(uint8_t *list, int16_t i)
{
    return (SpShip *)list + (i - 1);
}

static inline int32_t lmul(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a * (uint32_t)b);
}

// 20d4:002d
void SpWar_EnterSpaceBattle(void)
{
    Sound_AllocWarSndBuf();
    uint8_t *buf = System_GetMem(0xBCC0);
    Gfx_LoadPicScreen(NAME("war\\spwar"), buf, 0x97, Gfx_PalettePtr());
    Blit_CopyRect(buf, gfx_screen() + 0x3D40, 0x140, 0x97);
    System_FreeMem(buf, 0xBCC0);
    ds.SpWar_View = System_GetMem(0x5E60);
    ds.SpWar_Background = System_GetMem(0x5E60);
    ds.SpWar_Sprites = System_GetMem(0xF00);
    ds.SpWar_Face = System_GetMem(0x1180);
    Gfx_LoadPic(NAME("war\\sphatter"), ds.SpWar_Background, 1, 0x97, 0, 0, 0xA0, false,
                scratch_pal, 0, 0xB0, 0x40, 0, 0xBE);
    Gfx_LoadPic(NAME("war\\spants"), ds.SpWar_Sprites, 1, 0x30, 0, 0, 0x50, false,
                scratch_pal, 0, 0xB0, 0x40, 0, 0xBE);
    // The picture of the hired fighter (Game::StaffHired[2] in the decompile
    // is 0-based: the third of pilot, builder, fighter, developer).
    PString num, name;
    System_StrLong(ds.Game_StaffHired[2], 0, &num, 10);
    System_StrLoad(&name, PSTR("war\\spface"));
    System_StrConcat(&name, &num);
    Gfx_LoadPic((const uint8_t *)&name, ds.SpWar_Face, 1, 0x50, 0, 0, 0x38, false,
                scratch_pal, 0, 0xB0, 0x40, 0, 0xBE);
    SpWar_StepSpaceBattle();
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
    ds.SpWar_Retreated = 0;
    if (ds.SpWar_BattleOver)
        SpWar_EndSpaceBattle();
}

// Appends a ship to a fleet (the 1-based list and its count variable) like
// the original's inline code: no room check here, the callers test < 500.
// mirror: x counts from the right edge. The original does that for every
// alien ship (forces and store ships), whichever side they are on; only the
// player's groups start on the left.
static void add_ship(uint8_t *list, int16_t *count, uint8_t id, uint8_t cls, uint16_t dmg,
                     uint16_t hp, bool mirror)
{
    SpShip *s = Ship(list, *count + 1);
    s->id = id;
    s->cls = cls;
    s->dmg = dmg;
    s->hp = (int16_t)hp;
    uint16_t xr = System_Random(ds.SpWar_StartXRange[cls - 1]);
    uint8_t x = (uint8_t)(ds.SpWar_StartX[cls - 1] + xr);
    s->x = mirror ? (uint8_t)(0xA0 - x) : x;
    uint16_t yr = System_Random(ds.SpWar_StartYRange[cls - 1]);
    s->y = (uint8_t)(ds.SpWar_StartY[cls - 1] + yr);
    s->len = (uint8_t)(System_Random(6) + 2);
    s->path = ds.SpWar_ZonePaths[3][System_Random(8)]; // zone 4, the middle left
    s->pos = 0;
    s->cool = 1;
    (*count)++;
}

// Fleet of a force-owning race (status 2 defends, 6 attacks), or false.
static bool race_side(const TRace *r, uint8_t **list, int16_t **count)
{
    if (r->Status == 2) {
        *list = ds.SpWar_DefenderList;
        *count = &ds.SpWar_DefenderShipCount;
        return true;
    }
    if (r->Status == 6) {
        *list = ds.SpWar_AttackerList;
        *count = &ds.SpWar_AttackerShipCount;
        return true;
    }
    return false;
}

// InitSpaceFleets and ApplySpaceLosses keep the chosen side in SideList / SideCount (status 2
// the defenders, anything else the attackers; only called for status 2 or 6).
static void set_side(const TRace *r)
{
    bool def = r->Status == 2;
    ds.SpWar_SideList = def ? ds.SpWar_DefenderList : ds.SpWar_AttackerList;
    ds.SpWar_SideCount = def ? (uint8_t *)&ds.SpWar_DefenderShipCount
                             : (uint8_t *)&ds.SpWar_AttackerShipCount;
}

// The store ships of the body that is current (CurBody) join the fleet of its
// owner, if the owner is a race at war with the player's fleet.
static void add_store_ships(void)
{
    const TBody *body = ds.Game_CurBody;
    if ((int8_t)body->Owner < 2)
        return;
    uint8_t *list;
    int16_t *count;
    if (!race_side(&ds.Races_Races[body->Owner - 2], &list, &count))
        return;
    set_side(&ds.Races_Races[body->Owner - 2]);
    for (int16_t j = 1; j <= 4; j++) {
        int16_t n = (int16_t)body->Stores[j - 1];
        for (int16_t a = 1; n > 0 && a <= n; a++) {
            if ((uint16_t)*count < 500) {
                // BUG? the id is always C9h; ApplySpaceLosses looks for 200 + moon.
                add_ship(list, count, 0xC9, (uint8_t)j, (uint16_t)ds.SpWar_ClassB[j - 1],
                         (uint16_t)ds.SpWar_ClassA[j - 1], true);
            }
        }
    }
}

static inline uint8_t moon_count(void)
{
    const TMoonList *lists = ds.Galaxy_MoonLists[ds.SpWar_BattleSystem - 1];
    return lists[ds.SpWar_BattlePlanet - 1].Count;
}

static inline void select_moon(int16_t m)
{
    ds.Game_CurMoon[ds.Game_CurSystem - 1] = m;
    PlanetInfo_SelectCurBody();
}

// 20d4:01d3
void SpWar_InitSpaceFleets(void)
{
    ds.Game_BattleFlag = 0;
    ds.SpWar_BattleOver = 0;
    ds.SpWar_StepNo = 0;
    ds.SpWar_AttackerList = System_GetMem(0x177C);
    ds.SpWar_DefenderList = System_GetMem(0x177C);
    ds.Anim_Record = System_GetMem(0x349);
    ds.Anim_Script = System_GetMem(0x230);
    ds.SpWar_Script = 0;
    ds.SpWar_ScriptStep = 0;
    ds.SpWar_RepeatsLeft = 0;
    *ds.Anim_Record = 0;
    // Scripts 2..25 are available to be picked (set constant at 20d4:01b3).
    ds.SpWar_ScriptsLeft[0] = 0xFC;
    ds.SpWar_ScriptsLeft[1] = 0xFF;
    ds.SpWar_ScriptsLeft[2] = 0xFF;
    ds.SpWar_ScriptsLeft[3] = 0x03;

    // Attackers: the player's groups and planet forces at this planet.
    ds.SpWar_AttackerShipCount = 0;
    int16_t lists[2] = {ds.Game_GroupCount, ds.Game_PlanetForceCount};
    for (int16_t li = 0; li < 2; li++) {
        for (int16_t g = 1; g <= lists[li]; g++) {
            TGroup *grp = Groups_At(li, g);
            if (!(grp->System == ds.SpWar_BattleSystem && grp->Planet == ds.SpWar_BattlePlanet
                  && (grp->Status == 1 || grp->Status == 2 || grp->Status == 7)
                  && (grp->Kind == 1 || grp->Kind == 3 || grp->Kind == 5)))
                continue;
            for (int16_t j = 1; j <= 4; j++) {
                TGroupSlot *sl = &grp->Units[0][j - 1];
                int32_t dmg = lmul(sl->Weapons[3], ds.SpWar_WeaponPower[3])
                              + lmul(sl->Weapons[2], ds.SpWar_WeaponPower[2])
                              + lmul(sl->Weapons[1], ds.SpWar_WeaponPower[1])
                              + lmul(sl->Weapons[0], ds.SpWar_WeaponPower[0]);
                dmg = (int32_t)((uint32_t)dmg);
                if (ds.Game_StaffExp[2] == 0)
                    dmg = lmul(dmg, 2) / 3;
                else
                    dmg = lmul((uint16_t)(ds.Game_StaffExp[2] + 200), dmg) / 0xDC;
                int32_t per = sl->ShipType < 1 ? 0 : dmg / sl->ShipType;
                if ((uint16_t)ds.SpWar_AttackerShipCount < 500) {
                    for (int16_t a = 1; a <= sl->ShipType; a++)
                        add_ship(ds.SpWar_AttackerList, &ds.SpWar_AttackerShipCount,
                                 (uint8_t)(li * 100 + g), (uint8_t)j, (uint16_t)per,
                                 (uint16_t)ds.SpWar_ClassA[j - 1], false);
                }
            }
        }
    }

    // Buildings (race forces) at this planet.
    ds.SpWar_DefenderShipCount = 0;
    for (int16_t r = 2; r <= 12; r++) {
        TRace *race = &ds.Races_Races[r - 2];
        uint8_t *list;
        int16_t *count;
        if (!((race->Status == 2 || race->Status == 6) && race->ForceCount > 0))
            continue;
        for (int16_t k = 1; k <= race->ForceCount; k++) {
            TRaceForce *f = &race->Forces[k - 1];
            race_side(race, &list, &count);
            set_side(race);
            if (f->System == ds.SpWar_BattleSystem && f->Body == ds.SpWar_BattlePlanet
                && f->Active == 1) {
                for (int16_t j = 1; j <= 4; j++) {
                    int16_t n = f->Units[j - 1];
                    for (int16_t a = 1; n > 0 && a <= n; a++) {
                        if ((uint16_t)*count < 500)
                            add_ship(list, count, (uint8_t)(r * 10 + k), (uint8_t)j,
                                     (uint16_t)ds.SpWar_ClassB[j - 1],
                                     (uint16_t)ds.SpWar_ClassA[j - 1], true);
                    }
                }
            }
        }
    }

    // Planet and moons: the store ships of their owners.
    uint8_t moons = moon_count();
    for (int16_t m = 0;; m++) {
        select_moon(m);
        add_store_ships();
        if (m == moons)
            break;
    }
    ds.Game_CurMoon[ds.Game_CurSystem - 1] = ds.SpWar_BattleMoon;
    PlanetInfo_SelectCurBody();
    ds.SpWar_AttackerStart = ds.SpWar_AttackerShipCount;
    ds.SpWar_DefenderStart = ds.SpWar_DefenderShipCount;
}

// 20d4:0969
void SpWar_ClearHotspots(void)
{
    ds.Mouse_HotspotCount = 0;
}

// 20d4:0985
void SpWar_LoadWarAnimDef(int16_t n)
{
    SpWar_FreeWarAnims();
    ds.SpWar_Script = n;
    ds.SpWar_ScriptStep = 1;
    ds.SpWar_ScriptDelay = 0;
    ds.SpWar_RepeatsLeft = ds.SpWar_ScriptPlays[n - 1];
    size_t size;
    uint8_t *file = fs_load("war\\anim\\anim.def", &size);
    if (file == nullptr)
        return;
    // Seek to script n, BlockRead 0x230 bytes (a short file leaves the rest).
    size_t from = (size_t)(n - 1) * 0x230;
    if (from < size)
        memcpy(ds.Anim_Script, file + from, size - from < 0x230 ? size - from : 0x230);
    SDL_free(file);
}

// 20d4:0a14
void SpWar_FreeWarAnims(void)
{
    if (ds.SpWar_Script > 0) {
        for (int16_t i = 1; i <= ds.SpWar_ScriptStep; i++) {
            uint8_t anim = ds.Anim_Script[i * 4 - 4];
            if (anim != 0)
                Anim_FreeWarAnim(anim);
        }
        ds.SpWar_Script = 0;
    }
}

// Entry of a script: {anim, frame, sound, delay}.
static inline uint8_t *script_entry(int16_t i)
{
    return ds.Anim_Script + i * 4 - 4;
}

// Shows the script entry at ScriptStep and sets the delay to its time.
static void play_entry(void)
{
    uint8_t *e = script_entry(ds.SpWar_ScriptStep);
    if (e[0] != 0) {
        const TSaAnim *r = &ds.SpWar_SpriteRects[e[0] - 1];
        Anim_LoadWarAnim(e[0]);
        Anim_DrawAnimKeyed(e[1], ds.SpWar_View);
        Mouse_HideCursor();
        Blit_CopyRectStrided(ds.SpWar_View,
                             gfx_screen() + (uint16_t)(r->Y * 0x140 + r->X), r->Width, r->Height,
                             r->Width, 0x140);
        Mouse_ShowCursor();
        if (e[2] != 0)
            Sound_PlayWarSnd(e[2]);
    }
    ds.SpWar_ScriptDelay = (int8_t)e[3];
}

// Starts script n: curtain rectangle reset, pause of 10..19 ticks.
static void restart_script(int16_t n)
{
    ds.SpWar_CurtainLeft = 0xA0;
    ds.SpWar_CurtainRight = 0x13F;
    ds.SpWar_CurtainTop = 0x31;
    ds.SpWar_CurtainBottom = 199;
    SpWar_FreeWarAnims();
    SpWar_LoadWarAnimDef(n);
    ds.SpWar_ScriptDelay = (int16_t)(System_Random(10) + 10);
}

// 20d4:0a85
void SpWar_PlayWarAnimScript(void)
{
    SpWar_StepSpaceBattle();
    ds.SpWar_Retreated = 0;
    if (ds.SpWar_BattleOver)
        SpWar_EndSpaceBattle();
    if (ds.SpWar_BattleOver)
        return;

    // Curtain wipe: the new animation area is revealed from the outside in
    // by 10 pixels per tick.
    if (ds.SpWar_CurtainLeft < 1000) {
        int16_t l = ds.SpWar_CurtainLeft, r = ds.SpWar_CurtainRight;
        int16_t t = ds.SpWar_CurtainTop, b = ds.SpWar_CurtainBottom;
        if (r - l < 0x15 || b - t < 0x15) {
            Blit_FillRect(l, t, r - l + 1, b - t + 1, 0x40);
            ds.SpWar_CurtainLeft = 2000;
        } else {
            Mouse_HideCursor();
            Blit_FillRect(l, t, r - l + 1, 10, 0x40);
            Blit_FillRect(l, b - 10, r - l + 1, 0xB, 0x40);
            Blit_FillRect(l, t, 10, b - t + 1, 0x40);
            Blit_FillRect(r - 10, t, 0xB, b - t + 1, 0x40);
            Mouse_ShowCursor();
            ds.SpWar_CurtainLeft += 10;
            ds.SpWar_CurtainRight -= 10;
            ds.SpWar_CurtainTop += 10;
            ds.SpWar_CurtainBottom -= 10;
        }
    }
    if (ds.SpWar_Script == 0)
        return;
    if (ds.SpWar_ScriptDelay != 0) {
        ds.SpWar_ScriptDelay--; // 20d4:0dc6
        return;
    }
    int16_t next = ds.SpWar_Script;
    ds.SpWar_ScriptStep++;
    if (script_entry(ds.SpWar_ScriptStep)[3] == 0xFF) {
        // End of the script: pick the next one from the unused set (refilled
        // when it is empty), unless the script has repeats left.
        static const uint8_t all[4] = {0xFC, 0xFF, 0xFF, 0x03};
        bool empty = true;
        for (int i = 0; i < 4; i++)
            if (ds.SpWar_ScriptsLeft[i] != 0)
                empty = false;
        if (empty)
            memcpy(ds.SpWar_ScriptsLeft, all, 4);
        if (ds.SpWar_RepeatsLeft == 0) {
            next = (int16_t)(System_Random(0x19) + 1);
            while (!(ds.SpWar_ScriptsLeft[next >> 3] & (1 << (next & 7))))
                next = (int16_t)(System_Random(0x18) + 2);
            ds.SpWar_ScriptsLeft[next >> 3] &= (uint8_t) ~(1 << (next & 7));
        } else {
            ds.SpWar_RepeatsLeft--;
        }
        // 20d4:0c44
        if (next != ds.SpWar_Script) {
            restart_script(next);
            return;
        }
        ds.SpWar_ScriptStep = 1;
    }
    play_entry();
}

// 20d4:0dcc
void SpWar_DrawPilotFace(void)
{
    Blit_BlitTransparentStrided(ds.SpWar_Face, gfx_screen() + 0xB42A, 0x50, 0x38, 0x50,
                                0x140, 0x40);
}

static inline uint8_t *view_at(int16_t ofs)
{
    return ds.SpWar_View + (uint16_t)ofs;
}

static inline uint8_t *sprite_at(int16_t ofs)
{
    return ds.SpWar_Sprites + (uint16_t)ofs;
}

// 20d4:0df2
void SpWar_DrawShipDot1(int16_t srcOfs, int16_t dstOfs)
{
    *view_at(dstOfs) = sprite_at(srcOfs)[0x51];
}

// 20d4:0e0d
void SpWar_DrawShipDot2(int16_t srcOfs, int16_t dstOfs)
{
    memcpy(view_at(dstOfs), sprite_at(srcOfs) + 0x59, 2);
}

// 20d4:0e28
void SpWar_DrawShipDot4(int16_t srcOfs, int16_t dstOfs)
{
    uint8_t *d = view_at(dstOfs), *s = sprite_at(srcOfs);
    memcpy(d + 1, s + 0x62, 2);
    memcpy(d + 0xA0, s + 0xB1, 2);
    memcpy(d + 0xA2, s + 0xB3, 2);
    memcpy(d + 0x141, s + 0x102, 2);
}

// 20d4:0e57
void SpWar_DrawShipDot7(int16_t srcOfs, int16_t dstOfs)
{
    uint8_t *d = view_at(dstOfs), *s = sprite_at(srcOfs);
    memcpy(d + 1, s + 0x6A, 2);
    memcpy(d + 3, s + 0x6C, 2);
    memcpy(d + 0xA0, s + 0xB9, 2);
    memcpy(d + 0xA2, s + 0xBB, 2);
    memcpy(d + 0xA4, s + 0xBD, 2);
    memcpy(d + 0x141, s + 0x10A, 2);
    memcpy(d + 0x143, s + 0x10C, 2);
}

// 20d4:0e89: 7x7, colour 40h ('@') is transparent
void SpWar_DrawExplosion(int16_t srcOfs, int16_t dstOfs)
{
    uint8_t *d = view_at(dstOfs) + 1, *s = sprite_at(srcOfs) + 0x51;
    for (int row = 0; row < 7; row++) {
        for (int i = 0; i < 7; i++)
            if (s[i] != '@')
                d[i] = s[i];
        s += 0x50;
        d += 0xA0;
    }
}

static void draw_ship(const SpShip *s, int16_t srcOfs)
{
    int16_t dst = (int16_t)(s->x + s->y * 0xA0);
    switch (s->cls) {
    case 1: SpWar_DrawShipDot1(srcOfs, dst); break;
    case 2: SpWar_DrawShipDot2(srcOfs, dst); break;
    case 3: SpWar_DrawShipDot4(srcOfs, dst); break;
    case 4: SpWar_DrawShipDot7(srcOfs, dst); break;
    }
}

// Route code (1..9, 0 = end) at the ship's position. Routes[set][len][pos] in
// the original's flat addressing: pos 0, a new ship's first move, reads the
// byte before the row (the previous row's last, or SpWar::ClassB's for the
// first row).
static uint8_t route_code(const SpShip *s)
{
    const uint8_t *routes = &ds.SpWar_Routes[0][0][0];
    uint8_t set = ds.SpWar_RouteSet[s->path - 1];
    return routes[(set - 1) * 0x78 + (s->len - 2) * 0x14 + s->pos - 1];
}

// Direction of a route code on a path, PathDirs[path][code] in the same flat
// addressing: code 0 reads the previous path's last entry (RouteSet[8] for
// path 1).
static uint8_t path_dir(uint8_t path, uint8_t code)
{
    const uint8_t *dirs = &ds.SpWar_PathDirs[0][0];
    return dirs[(path - 1) * 9 + code - 1];
}

// One side's ships fly their routes, shoot at random enemies and the dead
// are swapped to the end of their list. Defenders mirror the x movement.
static void step_side(uint8_t *own, int16_t ownCount, uint8_t *foe, int16_t *foeCount,
                      bool defender)
{
    for (int16_t i = 1; i <= ownCount; i++) {
        SpShip *s = Ship(own, i);
        if (!(s->hp > 0))
            continue;
        draw_ship(s, defender ? 0x280 : 0);
        if (s->cool == 0) {
            uint8_t dir = path_dir(s->path, route_code(s));
            int8_t dx = defender ? ds.SpWar_DxDefender[dir - 1] : ds.SpWar_DxAttacker[dir - 1];
            s->x = (uint8_t)(s->x + dx);
            s->y = (uint8_t)(s->y + ds.SpWar_Dy[dir - 1]);
            s->pos++;
            if (route_code(s) == 0) {
                // End of the route: choose a new one from where the ship is.
                int zx = s->x < 0x32 ? 0 : s->x < 0x6F ? 1 : 2;
                if (defender)
                    zx = 2 - zx;
                int zy = s->y < 0x28 ? 0 : s->y < 0x6F ? 1 : 2;
                s->len = (uint8_t)(System_Random(6) + 2);
                s->path = ds.SpWar_ZonePaths[zx + zy * 3][System_Random(8)];
                s->pos = 1;
            }
            uint16_t r = System_Random(ds.SpWar_CoolRange[s->cls - 1]);
            s->cool = (uint8_t)(ds.SpWar_CoolBase[s->cls - 1] + r);
        } else {
            s->cool--;
        }
        if (*foeCount == 0) {
            ds.SpWar_BattleOver = 1;
            continue;
        }
        int16_t t = (int16_t)(System_Random((uint16_t)*foeCount) + 1);
        SpShip *tg = Ship(foe, t);
        if (System_Random(100) < 11) {
            if (s->cls < 3 && tg->cls > 2)
                tg->hp = (int16_t)(tg->hp - s->dmg / 3);
            else
                tg->hp = (int16_t)(tg->hp - s->dmg);
            if (s->cls < 3 && tg->cls < 3) {
                uint16_t r = System_Random(100);
                int16_t chance = ds.SpWar_KillChance[s->cls - 1][tg->cls - 1];
                if (chance >= 0 && r <= (uint16_t)chance)
                    tg->hp = 0;
            }
            if (!defender && ds.Game_BattleFlag)
                tg->hp = 0;
            if (tg->hp < 1) {
                tg->pos = 1;
                SpShip tmp = *Ship(foe, *foeCount);
                *Ship(foe, *foeCount) = *tg;
                *tg = tmp;
                (*foeCount)--;
            }
        }
    }
}

// Explosion frames of the ships lost (the end of the list from the live count).
static void draw_explosions(uint8_t *list, int16_t live, int16_t start)
{
    for (int16_t i = live + 1; i <= start; i++) {
        SpShip *s = Ship(list, i);
        if (s->pos <= ds.SpWar_ExplosionFrames[s->cls - 1]) {
            SpWar_DrawExplosion((int16_t)(s->pos * 8 + 0x278 + s->cls * 0x280),
                                (int16_t)(s->x + s->y * 0xA0));
            s->pos++;
            ds.SpWar_Exploding = 1;
        }
    }
}

// 20d4:0efd
void SpWar_StepSpaceBattle(void)
{
    uint8_t wasOver = ds.SpWar_BattleOver;
    ds.SpWar_StepNo++;
    if (ds.SpWar_StepNo > 4)
        ds.SpWar_StepNo = 1;
    if (ds.SpWar_StepNo != 1)
        return;
    ds.SpWar_Exploding = 0;
    Blit_CopyBytes(ds.SpWar_Background, ds.SpWar_View, 0x5E60);
    step_side(ds.SpWar_AttackerList, ds.SpWar_AttackerShipCount, ds.SpWar_DefenderList,
              &ds.SpWar_DefenderShipCount, false);
    step_side(ds.SpWar_DefenderList, ds.SpWar_DefenderShipCount, ds.SpWar_AttackerList,
              &ds.SpWar_AttackerShipCount, true);
    draw_explosions(ds.SpWar_AttackerList, ds.SpWar_AttackerShipCount, ds.SpWar_AttackerStart);
    draw_explosions(ds.SpWar_DefenderList, ds.SpWar_DefenderShipCount, ds.SpWar_DefenderStart);
    Mouse_HideCursor();
    Anim_BlitWedgeToScreen(ds.SpWar_View);
    SpWar_DrawPilotFace();
    Mouse_ShowCursor();
    if (ds.SpWar_AttackerShipCount == 0)
        ds.SpWar_BattleOver = 1;
    if (ds.SpWar_DefenderShipCount == 0)
        ds.SpWar_BattleOver = 1;
    if (ds.SpWar_Exploding) // wait for the last explosion
        ds.SpWar_BattleOver = 0;
    if (wasOver == 0 && ds.SpWar_BattleOver != 0) {
        SpWar_FreeWarAnims();
        GfxPath_SetToolbarAndShow(0x1E);
    }
}

// Ships of one id and class that are still alive in a list.
static int32_t count_alive(uint8_t *list, int16_t n, int id, int cls)
{
    int32_t c = 0;
    for (int16_t i = 1; i <= n; i++)
        if (Ship(list, i)->id == id && Ship(list, i)->cls == cls)
            c++;
    return c;
}

// 20d4:17f0
void SpWar_ApplySpaceLosses(void)
{
    int16_t lists[2] = {ds.Game_GroupCount, ds.Game_PlanetForceCount};
    for (int c = 1; c <= 4; c++) {
        ds.SpWar_AttackerLosses[c - 1] = 0;
        ds.SpWar_DefenderLosses[c - 1] = 0;
    }
    // (the first loop in the original zeroes AttackerLosses, then DefenderLosses)
    for (int16_t li = 0; li < 2; li++) {
        for (int16_t g = 1; g <= lists[li]; g++) {
            TGroup *grp = Groups_At(li, g);
            if (!(grp->System == ds.SpWar_BattleSystem && grp->Planet == ds.SpWar_BattlePlanet
                  && (grp->Status == 1 || grp->Status == 2 || grp->Status == 7)
                  && (grp->Kind == 1 || grp->Kind == 3 || grp->Kind == 5)))
                continue;
            for (int16_t j = 1; j <= 4; j++) {
                TGroupSlot *sl = &grp->Units[0][j - 1];
                int32_t alive =
                    count_alive(ds.SpWar_AttackerList, ds.SpWar_AttackerShipCount, li * 100 + g, j);
                int32_t orig = sl->ShipType;
                // A fifth of the destroyed ships survive.
                int32_t now = alive + (orig - alive) / 5;
                if (sl->ShipType < 1) {
                    for (int w = 0; w < 4; w++)
                        sl->Weapons[w] = 0;
                } else {
                    for (int w = 0; w < 4; w++)
                        sl->Weapons[w] = (int16_t)(lmul(sl->Weapons[w], now) / orig);
                }
                ds.SpWar_AttackerLosses[j - 1] += orig - now;
                sl->ShipType = (int16_t)now;
            }
        }
    }

    for (int16_t r = 2; r <= 12; r++) {
        TRace *race = &ds.Races_Races[r - 2];
        for (int16_t k = 1; k <= race->ForceCount; k++) {
            if (!(race->Status == 2 || race->Status == 6))
                continue;
            TRaceForce *f = &race->Forces[k - 1];
            uint8_t *list;
            int16_t *count;
            race_side(race, &list, &count);
            set_side(race);
            ds.SpWar_LossPtr = race->Status == 2 ? (uint8_t *)ds.SpWar_DefenderLosses
                                                 : (uint8_t *)ds.SpWar_AttackerLosses;
            if (f->System == ds.SpWar_BattleSystem && f->Body == ds.SpWar_BattlePlanet
                && f->Active == 1) {
                for (int16_t j = 1; j <= 4; j++) {
                    int32_t alive = count_alive(list, *count, r * 10 + k, j);
                    uint16_t orig = (uint16_t)f->Units[j - 1];
                    int32_t now = alive + ((int32_t)orig - alive) / 5;
                    int32_t *loss = (int32_t *)ds.SpWar_LossPtr + (j - 1);
                    *loss += (int32_t)orig - now;
                    f->Units[j - 1] = (int16_t)now;
                }
            }
        }
    }

    uint8_t moons = moon_count();
    for (int16_t m = 0; m <= moons; m++) {
        select_moon(m);
        TBody *body = ds.Game_CurBody;
        if (!((int8_t)body->Owner > 1))
            continue;
        TRace *race = &ds.Races_Races[body->Owner - 2];
        uint8_t *list;
        int16_t *count;
        if (!race_side(race, &list, &count))
            continue;
        set_side(race);
        for (int16_t j = 1; j <= 4; j++) {
            int32_t alive = count_alive(list, *count, m + 200, j);
            int32_t orig = body->Stores[j - 1];
            int32_t now = alive + (orig - alive) / 5;
            // BUG? LossPtr still points where the last force loop left it
            // (or at nothing), not at the side of this owner. Nil made the
            // original add to the interrupt table at 0000:0000; the port drops it.
            if (ds.SpWar_LossPtr != nullptr) {
                int32_t *loss = (int32_t *)ds.SpWar_LossPtr + (j - 1);
                *loss += orig - now;
            }
            body->Stores[j - 1] = now;
        }
    }
    ds.Game_CurMoon[ds.Game_CurSystem - 1] = ds.SpWar_BattleMoon;
    PlanetInfo_SelectCurBody();
}

// 20d4:1de8
void SpWar_EndSpaceBattle(void)
{
    SpWar_ApplySpaceLosses();
    SpWar_FreeWarAnims();
    ds.SpWar_PlayerWon = !(ds.SpWar_AttackerShipCount == 0 || ds.SpWar_Retreated != 0);
    uint8_t *buf = System_GetMem(0xBCC0);
    PString name;
    System_StrLoad(&name, PSTR("war\\"));
    System_StrConcat(&name, ds.SpWar_PlayerWon ? PSTR("spvict") : PSTR("splost"));
    Gfx_LoadPic((const uint8_t *)&name, buf, 1, 0x97, 0, 0, 0x140, true, Gfx_PalettePtr(),
                0, 0xB0, 0x40, 0, 0xBE);
    Blit_CopyRect(buf, gfx_screen() + 0x3D40, 0x140, 0x97);
    System_FreeMem(buf, 0xBCC0);
    PString num;
    for (int c = 1; c <= 4; c++) {
        System_StrLong(ds.SpWar_AttackerLosses[c - 1], 4, &num, 255);
        Gfx_DrawTextF9((const uint8_t *)&num, 0x57, c * 9 + 0x4F, 4);
    }
    for (int c = 1; c <= 4; c++) {
        System_StrLong(ds.SpWar_DefenderLosses[c - 1], 4, &num, 255);
        Gfx_DrawTextF9((const uint8_t *)&num, 0x57, c * 9 + 0x86, 4);
    }
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
}
