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

// Unnamed globals.
#define W(off) (*(int16_t *)ds_at(off))
#define U16(off) (*(uint16_t *)ds_at(off))
#define B(off) (*(uint8_t *)ds_at(off))
#define L(off) (*(int32_t *)ds_at(off))
#define FP(off) (*(FarPtr *)ds_at(off))

#define BATTLE_END B(0x77DA)     // the fight is over (a side has no ships left)
#define RETREATED B(0x77DB)      // set by the screen code when the player leaves: counts as a loss
#define VIEW FP(0x77DC)          // 160 x 150 battle view (0x5E60 bytes)
#define BACKGROUND FP(0x77E0)    // sphatter picture, copied into VIEW every step
#define SPRITES FP(0x77E4)       // spants: ship dots and explosion frames, 80 wide
#define FACE FP(0x77E8)          // spface<n>: 80 x 56 pilot portrait
#define STEP_FLAG B(0x778E)      // an explosion was drawn this step
#define STEP_NO W(0x77EC)        // 1..4, the battle only advances on 1
#define ATT_LIST FP(0x779E)
#define DEF_LIST FP(0x77A2)
#define SYS W(0x7798)            // body the battle is at
#define PLANET W(0x779A)
#define MOON W(0x779C)
#define ATT_START W(0x77B2)      // ship counts when the battle began
#define DEF_START W(0x77B4)
#define ATT_LOSS(c) L(0x77B2 + 4 * (c))  // [1..4] ships lost per class (77B6..)
#define DEF_LOSS(c) L(0x77C2 + 4 * (c))  // (77C6..)
#define LOSS_PTR FP(0x77D6)      // ApplySpaceLosses' pointer to the loss array last chosen
#define SIDE_LIST FP(0x77A6)     // InitSpaceFleets: the list a race's ships go to (ATT/DEF_LIST)
#define SIDE_COUNT FP(0x77AE)    // and a far pointer to its count (DS:77AA or DS:77AC)
#define ANIM_SCRIPT W(0x7792)    // current anim.def script, 0 = none
#define ANIM_FRAME W(0x7794)     // entry in it (1..140)
#define ANIM_DELAY W(0x7796)
#define ANIM_REPEAT W(0x7790)    // plays of this script still to skip a new pick
#define ANIM_DEF FP(0x7E44)      // 0x230 bytes: 140 entries of {anim, frame, sound, delay}
#define ANIM_REC FP(0x7E40)      // Anim's 841-byte record, flag cleared by InitSpaceFleets
#define CURTAIN_L W(0x77EE)
#define CURTAIN_R W(0x77F0)
#define CURTAIN_T W(0x77F2)
#define CURTAIN_B W(0x77F4)
#define USED_SCRIPTS(i) B(0x77F6 + (i)) // set of anim.def scripts not played yet (bits 2..25)

// Scratch palette for the picture loads (the original used 768 bytes of its
// stack); the port's stack segment is otherwise unused.
#define NAME(lit) ((const uint8_t *)PSTR(lit))
#define SCRATCH_PAL far_ptr(MEM_SS_SEG, 0x0400)

static inline SpShip *Ship(FarPtr list, int16_t i)
{
    return (SpShip *)far_host(list) + (i - 1);
}

static inline int32_t lmul(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a * (uint32_t)b);
}

// 20d4:002d
void SpWar_EnterSpaceBattle(void)
{
    Sound_AllocWarSndBuf();
    FarPtr buf = System_GetMem(0xBCC0);
    Gfx_LoadPicScreen(NAME("war\\spwar"), buf, 0x97, Gfx_PalettePtr());
    Blit_CopyRect(buf, far_ptr(ds.Gfx_ScreenSeg, 0x3D40), 0x140, 0x97);
    System_FreeMem(buf, 0xBCC0);
    VIEW = System_GetMem(0x5E60);
    BACKGROUND = System_GetMem(0x5E60);
    SPRITES = System_GetMem(0xF00);
    FACE = System_GetMem(0x1180);
    Gfx_LoadPic(NAME("war\\sphatter"), BACKGROUND, 1, 0x97, 0, 0, 0xA0, false, SCRATCH_PAL, 0, 0xB0,
                0x40, 0, 0xBE);
    Gfx_LoadPic(NAME("war\\spants"), SPRITES, 1, 0x30, 0, 0, 0x50, false, SCRATCH_PAL, 0, 0xB0, 0x40,
                0, 0xBE);
    // The picture of the hired fighter (Game::StaffHired[2] in the decompile
    // is 0-based: the third of pilot, builder, fighter, developer).
    PString num, name;
    System_StrLong(ds.Game_StaffHired[2], 0, &num, 10);
    System_StrLoad(&name, PSTR("war\\spface"));
    System_StrConcat(&name, &num);
    Gfx_LoadPic((const uint8_t *)&name, FACE, 1, 0x50, 0, 0, 0x38, false, SCRATCH_PAL, 0, 0xB0, 0x40,
                0, 0xBE);
    SpWar_StepSpaceBattle();
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
    RETREATED = 0;
    if (BATTLE_END)
        SpWar_EndSpaceBattle();
}

// Appends a ship to a fleet (the 1-based list and its count variable) like
// the original's inline code: no room check here, the callers test < 500.
// mirror: x counts from the right edge. The original does that for every
// alien ship (forces and store ships), whichever side they are on; only the
// player's groups start on the left.
static void add_ship(FarPtr list, int16_t *count, uint8_t id, uint8_t cls, uint16_t dmg, uint16_t hp,
                     bool mirror)
{
    SpShip *s = Ship(list, *count + 1);
    s->id = id;
    s->cls = cls;
    s->dmg = dmg;
    s->hp = (int16_t)hp;
    uint16_t xr = System_Random(B(0x7BB + cls));
    uint8_t x = (uint8_t)(B(0x7B7 + cls) + xr);
    s->x = mirror ? (uint8_t)(0xA0 - x) : x;
    uint16_t yr = System_Random(B(0x7C3 + cls));
    s->y = (uint8_t)(B(0x7BF + cls) + yr);
    s->len = (uint8_t)(System_Random(6) + 2);
    s->path = B(0x753 + System_Random(8));
    s->pos = 0;
    s->cool = 1;
    (*count)++;
}

// Fleet of a force-owning race (status 2 defends, 6 attacks), or false.
static bool race_side(const TRace *r, FarPtr *list, int16_t **count)
{
    if (r->Status == 2) {
        *list = DEF_LIST;
        *count = &ds.SpWar_DefenderShipCount;
        return true;
    }
    if (r->Status == 6) {
        *list = ATT_LIST;
        *count = &ds.SpWar_AttackerShipCount;
        return true;
    }
    return false;
}

// InitSpaceFleets and ApplySpaceLosses keep the chosen side in DS:77A6 / DS:77AE (status 2 the
// defenders, anything else the attackers; only called for status 2 or 6).
static void set_side(const TRace *r)
{
    bool def = r->Status == 2;
    SIDE_LIST = def ? DEF_LIST : ATT_LIST;
    SIDE_COUNT = far_ptr(MEM_DS_SEG, def ? 0x77AC : 0x77AA);
}

// The store ships of the body that is current (CurBody) join the fleet of its
// owner, if the owner is a race at war with the player's fleet.
static void add_store_ships(void)
{
    const TBody *body = (const TBody *)far_host(ds.Game_CurBody);
    if ((int8_t)body->Owner < 2)
        return;
    FarPtr list;
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
    return ((const TMoonList *)far_host(ds.Galaxy_MoonLists[SYS - 1]))[PLANET - 1].Count;
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
    BATTLE_END = 0;
    STEP_NO = 0;
    ATT_LIST = System_GetMem(0x177C);
    DEF_LIST = System_GetMem(0x177C);
    ANIM_REC = System_GetMem(0x349);
    ANIM_DEF = System_GetMem(0x230);
    ANIM_SCRIPT = 0;
    ANIM_FRAME = 0;
    ANIM_REPEAT = 0;
    *(uint8_t *)far_host(ANIM_REC) = 0;
    // Scripts 2..25 are available to be picked (set constant at 20d4:01b3).
    USED_SCRIPTS(0) = 0xFC;
    USED_SCRIPTS(1) = 0xFF;
    USED_SCRIPTS(2) = 0xFF;
    USED_SCRIPTS(3) = 0x03;

    // Attackers: the player's groups and planet forces at this planet.
    ds.SpWar_AttackerShipCount = 0;
    int16_t lists[2] = {ds.Game_GroupCount, ds.Game_PlanetForceCount};
    for (int16_t li = 0; li < 2; li++) {
        for (int16_t g = 1; g <= lists[li]; g++) {
            TGroup *grp = Groups_At(li, g);
            if (!(grp->System == SYS && grp->Planet == PLANET
                  && (grp->Status == 1 || grp->Status == 2 || grp->Status == 7)
                  && (grp->Kind == 1 || grp->Kind == 3 || grp->Kind == 5)))
                continue;
            for (int16_t j = 1; j <= 4; j++) {
                TGroupSlot *sl = &grp->Units[0][j - 1];
                int32_t dmg = lmul(sl->Weapons[3], L(0x5D4)) + lmul(sl->Weapons[2], L(0x5D0))
                              + lmul(sl->Weapons[1], L(0x5CC)) + lmul(sl->Weapons[0], L(0x5C8));
                dmg = (int32_t)((uint32_t)dmg);
                if (ds.Game_StaffExp[2] == 0)
                    dmg = lmul(dmg, 2) / 3;
                else
                    dmg = lmul((uint16_t)(ds.Game_StaffExp[2] + 200), dmg) / 0xDC;
                int32_t per = sl->ShipType < 1 ? 0 : dmg / sl->ShipType;
                if ((uint16_t)ds.SpWar_AttackerShipCount < 500) {
                    for (int16_t a = 1; a <= sl->ShipType; a++)
                        add_ship(ATT_LIST, &ds.SpWar_AttackerShipCount, (uint8_t)(li * 100 + g),
                                 (uint8_t)j, (uint16_t)per, (uint16_t)ds.SpWar_ClassA[j - 1], false);
                }
            }
        }
    }

    // Buildings (race forces) at this planet.
    ds.SpWar_DefenderShipCount = 0;
    for (int16_t r = 2; r <= 12; r++) {
        TRace *race = &ds.Races_Races[r - 2];
        FarPtr list;
        int16_t *count;
        if (!((race->Status == 2 || race->Status == 6) && race->ForceCount > 0))
            continue;
        for (int16_t k = 1; k <= race->ForceCount; k++) {
            TRaceForce *f = &race->Forces[k - 1];
            race_side(race, &list, &count);
            set_side(race);
            if (f->System == SYS && f->Body == PLANET && f->Active == 1) {
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
    ds.Game_CurMoon[ds.Game_CurSystem - 1] = MOON;
    PlanetInfo_SelectCurBody();
    ATT_START = ds.SpWar_AttackerShipCount;
    DEF_START = ds.SpWar_DefenderShipCount;
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
    ANIM_SCRIPT = n;
    ANIM_FRAME = 1;
    ANIM_DELAY = 0;
    ANIM_REPEAT = W(0x569A + n * 2);
    size_t size;
    uint8_t *file = fs_load("war\\anim\\anim.def", &size);
    if (file == nullptr)
        return;
    // Seek to script n, BlockRead 0x230 bytes (a short file leaves the rest).
    size_t from = (size_t)(n - 1) * 0x230;
    if (from < size)
        memcpy(far_host(ANIM_DEF), file + from, size - from < 0x230 ? size - from : 0x230);
    SDL_free(file);
}

// 20d4:0a14
void SpWar_FreeWarAnims(void)
{
    if (ANIM_SCRIPT > 0) {
        for (int16_t i = 1; i <= ANIM_FRAME; i++) {
            uint8_t anim = ((uint8_t *)far_host(ANIM_DEF))[i * 4 - 4];
            if (anim != 0)
                Anim_FreeWarAnim(anim);
        }
        ANIM_SCRIPT = 0;
    }
}

// Entry of a script: {anim, frame, sound, delay}.
static inline uint8_t *script_entry(int16_t i)
{
    return (uint8_t *)far_host(ANIM_DEF) + i * 4 - 4;
}

// The 5-byte SA record of anim a (DS:555B): frames, width, rows, x, y.
static inline uint8_t *sa_rect(uint8_t a)
{
    return (uint8_t *)ds_at(0x555B + a * 5);
}

// Shows the script entry at ANIM_FRAME and sets the delay to its time.
static void play_entry(void)
{
    uint8_t *e = script_entry(ANIM_FRAME);
    if (e[0] != 0) {
        uint8_t *r = sa_rect(e[0]);
        Anim_LoadWarAnim(e[0]);
        Anim_DrawAnimKeyed(e[1], VIEW);
        Mouse_HideCursor();
        Blit_CopyRectStrided(VIEW, far_ptr(ds.Gfx_ScreenSeg, (uint16_t)(r[4] * 0x140 + r[3])), r[1],
                             r[2], r[1], 0x140);
        Mouse_ShowCursor();
        if (e[2] != 0)
            Sound_PlayWarSnd(e[2]);
    }
    ANIM_DELAY = (int8_t)e[3];
}

// Starts script n: curtain rectangle reset, pause of 10..19 ticks.
static void restart_script(int16_t n)
{
    CURTAIN_L = 0xA0;
    CURTAIN_R = 0x13F;
    CURTAIN_T = 0x31;
    CURTAIN_B = 199;
    SpWar_FreeWarAnims();
    SpWar_LoadWarAnimDef(n);
    ANIM_DELAY = (int16_t)(System_Random(10) + 10);
}

// 20d4:0a85
void SpWar_PlayWarAnimScript(void)
{
    SpWar_StepSpaceBattle();
    RETREATED = 0;
    if (BATTLE_END)
        SpWar_EndSpaceBattle();
    if (BATTLE_END)
        return;

    // Curtain wipe: the new animation area is revealed from the outside in
    // by 10 pixels per tick.
    if (CURTAIN_L < 1000) {
        if (CURTAIN_R - CURTAIN_L < 0x15 || CURTAIN_B - CURTAIN_T < 0x15) {
            Blit_FillRect(CURTAIN_L, CURTAIN_T, CURTAIN_R - CURTAIN_L + 1, CURTAIN_B - CURTAIN_T + 1,
                          0x40);
            CURTAIN_L = 2000;
        } else {
            Mouse_HideCursor();
            Blit_FillRect(CURTAIN_L, CURTAIN_T, CURTAIN_R - CURTAIN_L + 1, 10, 0x40);
            Blit_FillRect(CURTAIN_L, CURTAIN_B - 10, CURTAIN_R - CURTAIN_L + 1, 0xB, 0x40);
            Blit_FillRect(CURTAIN_L, CURTAIN_T, 10, CURTAIN_B - CURTAIN_T + 1, 0x40);
            Blit_FillRect(CURTAIN_R - 10, CURTAIN_T, 0xB, CURTAIN_B - CURTAIN_T + 1, 0x40);
            Mouse_ShowCursor();
            CURTAIN_L += 10;
            CURTAIN_R -= 10;
            CURTAIN_T += 10;
            CURTAIN_B -= 10;
        }
    }
    if (ANIM_SCRIPT == 0)
        return;
    if (ANIM_DELAY != 0) {
        ANIM_DELAY--; // 20d4:0dc6
        return;
    }
    int16_t next = ANIM_SCRIPT;
    ANIM_FRAME++;
    if (script_entry(ANIM_FRAME)[3] == 0xFF) {
        // End of the script: pick the next one from the unused set (refilled
        // when it is empty), unless the script has repeats left.
        static const uint8_t all[4] = {0xFC, 0xFF, 0xFF, 0x03};
        bool empty = true;
        for (int i = 0; i < 4; i++)
            if (USED_SCRIPTS(i) != 0)
                empty = false;
        if (empty)
            memcpy(ds_at(0x77F6), all, 4);
        if (ANIM_REPEAT == 0) {
            next = (int16_t)(System_Random(0x19) + 1);
            while (!(USED_SCRIPTS(next >> 3) & (1 << (next & 7))))
                next = (int16_t)(System_Random(0x18) + 2);
            USED_SCRIPTS(next >> 3) &= (uint8_t) ~(1 << (next & 7));
        } else {
            ANIM_REPEAT--;
        }
        // 20d4:0c44
        if (next != ANIM_SCRIPT) {
            restart_script(next);
            return;
        }
        ANIM_FRAME = 1;
    }
    play_entry();
}

// 20d4:0dcc
void SpWar_DrawPilotFace(void)
{
    Blit_BlitTransparentStrided(FACE, far_ptr(ds.Gfx_ScreenSeg, 0xB42A), 0x50, 0x38, 0x50, 0x140,
                                0x40);
}

static inline uint8_t *view_at(int16_t ofs)
{
    return (uint8_t *)far_host(VIEW) + (uint16_t)ofs;
}

static inline uint8_t *sprite_at(int16_t ofs)
{
    return (uint8_t *)far_host(SPRITES) + (uint16_t)ofs;
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

// One side's ships fly their routes, shoot at random enemies and the dead
// are swapped to the end of their list. Defenders mirror the x movement.
static void step_side(FarPtr own, int16_t ownCount, FarPtr foe, int16_t *foeCount, bool defender)
{
    for (int16_t i = 1; i <= ownCount; i++) {
        SpShip *s = Ship(own, i);
        if (!(s->hp > 0))
            continue;
        draw_ship(s, defender ? 0x280 : 0);
        if (s->cool == 0) {
            uint16_t row = (uint16_t)(B(0x6E7 + s->path) * 0x78 + (s->len - 1) * 0x14 + s->pos + 0x56B);
            uint8_t dir = B(0x6E6 + s->path * 9 + B(row));
            s->x = (uint8_t)(s->x + B((defender ? 0x793 : 0x789) + dir));
            s->y = (uint8_t)(s->y + B(0x79D + dir));
            s->pos++;
            row = (uint16_t)(B(0x6E7 + s->path) * 0x78 + (s->len - 1) * 0x14 + s->pos + 0x56B);
            if (B(row) == 0) {
                // End of the route: choose a new one from where the ship is.
                int zx = s->x < 0x32 ? 0 : s->x < 0x6F ? 1 : 2;
                if (defender)
                    zx = 2 - zx;
                int zy = s->y < 0x28 ? 0 : s->y < 0x6F ? 1 : 2;
                s->len = (uint8_t)(System_Random(6) + 2);
                s->path = B(0x72F + (zx + zy * 3 + 1) * 9 + System_Random(8));
                s->pos = 1;
            }
            uint16_t r = System_Random(U16(0x7AE + s->cls * 2));
            s->cool = (uint8_t)(U16(0x7A6 + s->cls * 2) + r);
        } else {
            s->cool--;
        }
        if (*foeCount == 0) {
            BATTLE_END = 1;
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
                uint16_t chance = U16(0x7CA + s->cls * 4 + tg->cls * 2);
                if ((int16_t)chance >= 0 && r <= chance)
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
static void draw_explosions(FarPtr list, int16_t live, int16_t start)
{
    for (int16_t i = live + 1; i <= start; i++) {
        SpShip *s = Ship(list, i);
        if (s->pos <= B(0x7CB + s->cls)) {
            SpWar_DrawExplosion((int16_t)(s->pos * 8 + 0x278 + s->cls * 0x280),
                                (int16_t)(s->x + s->y * 0xA0));
            s->pos++;
            STEP_FLAG = 1;
        }
    }
}

// 20d4:0efd
void SpWar_StepSpaceBattle(void)
{
    uint8_t wasOver = BATTLE_END;
    STEP_NO++;
    if (STEP_NO > 4)
        STEP_NO = 1;
    if (STEP_NO != 1)
        return;
    STEP_FLAG = 0;
    Blit_CopyBytes(BACKGROUND, VIEW, 0x5E60);
    step_side(ATT_LIST, ds.SpWar_AttackerShipCount, DEF_LIST, &ds.SpWar_DefenderShipCount, false);
    step_side(DEF_LIST, ds.SpWar_DefenderShipCount, ATT_LIST, &ds.SpWar_AttackerShipCount, true);
    draw_explosions(ATT_LIST, ds.SpWar_AttackerShipCount, ATT_START);
    draw_explosions(DEF_LIST, ds.SpWar_DefenderShipCount, DEF_START);
    Mouse_HideCursor();
    Anim_BlitWedgeToScreen(VIEW);
    SpWar_DrawPilotFace();
    Mouse_ShowCursor();
    if (ds.SpWar_AttackerShipCount == 0)
        BATTLE_END = 1;
    if (ds.SpWar_DefenderShipCount == 0)
        BATTLE_END = 1;
    if (STEP_FLAG) // wait for the last explosion
        BATTLE_END = 0;
    if (wasOver == 0 && BATTLE_END != 0) {
        SpWar_FreeWarAnims();
        GfxPath_SetToolbarAndShow(0x1E);
    }
}

// Ships of one id and class that are still alive in a list.
static int32_t count_alive(FarPtr list, int16_t n, int id, int cls)
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
        ATT_LOSS(c) = 0;
        DEF_LOSS(c) = 0;
    }
    // (the first loop in the original zeroes 77B6.., then 77C6..)
    for (int16_t li = 0; li < 2; li++) {
        for (int16_t g = 1; g <= lists[li]; g++) {
            TGroup *grp = Groups_At(li, g);
            if (!(grp->System == SYS && grp->Planet == PLANET
                  && (grp->Status == 1 || grp->Status == 2 || grp->Status == 7)
                  && (grp->Kind == 1 || grp->Kind == 3 || grp->Kind == 5)))
                continue;
            for (int16_t j = 1; j <= 4; j++) {
                TGroupSlot *sl = &grp->Units[0][j - 1];
                int32_t alive = count_alive(ATT_LIST, ds.SpWar_AttackerShipCount, li * 100 + g, j);
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
                ATT_LOSS(j) += orig - now;
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
            FarPtr list;
            int16_t *count;
            race_side(race, &list, &count);
            set_side(race);
            LOSS_PTR = far_ptr(MEM_DS_SEG, race->Status == 2 ? 0x77C6 : 0x77B6);
            if (f->System == SYS && f->Body == PLANET && f->Active == 1) {
                for (int16_t j = 1; j <= 4; j++) {
                    int32_t alive = count_alive(list, *count, r * 10 + k, j);
                    uint16_t orig = (uint16_t)f->Units[j - 1];
                    int32_t now = alive + ((int32_t)orig - alive) / 5;
                    int32_t *loss = (int32_t *)far_host(LOSS_PTR) + (j - 1);
                    *loss += (int32_t)orig - now;
                    f->Units[j - 1] = (int16_t)now;
                }
            }
        }
    }

    uint8_t moons = moon_count();
    for (int16_t m = 0; m <= moons; m++) {
        select_moon(m);
        TBody *body = (TBody *)far_host(ds.Game_CurBody);
        if (!((int8_t)body->Owner > 1))
            continue;
        TRace *race = &ds.Races_Races[body->Owner - 2];
        FarPtr list;
        int16_t *count;
        if (!race_side(race, &list, &count))
            continue;
        set_side(race);
        for (int16_t j = 1; j <= 4; j++) {
            int32_t alive = count_alive(list, *count, m + 200, j);
            int32_t orig = body->Stores[j - 1];
            int32_t now = alive + (orig - alive) / 5;
            // BUG? LOSS_PTR still points where the last force loop left it
            // (or at nothing), not at the side of this owner.
            int32_t *loss = (int32_t *)far_host(LOSS_PTR) + (j - 1);
            *loss += orig - now;
            body->Stores[j - 1] = now;
        }
    }
    ds.Game_CurMoon[ds.Game_CurSystem - 1] = MOON;
    PlanetInfo_SelectCurBody();
}

// 20d4:1de8
void SpWar_EndSpaceBattle(void)
{
    SpWar_ApplySpaceLosses();
    SpWar_FreeWarAnims();
    ds.SpWar_PlayerWon = !(ds.SpWar_AttackerShipCount == 0 || RETREATED != 0);
    FarPtr buf = System_GetMem(0xBCC0);
    PString name;
    System_StrLoad(&name, PSTR("war\\"));
    System_StrConcat(&name, ds.SpWar_PlayerWon ? PSTR("spvict") : PSTR("splost"));
    Gfx_LoadPic((const uint8_t *)&name, buf, 1, 0x97, 0, 0, 0x140, true, Gfx_PalettePtr(), 0, 0xB0,
                0x40, 0, 0xBE);
    Blit_CopyRect(buf, far_ptr(ds.Gfx_ScreenSeg, 0x3D40), 0x140, 0x97);
    System_FreeMem(buf, 0xBCC0);
    PString num;
    for (int c = 1; c <= 4; c++) {
        System_StrLong(ATT_LOSS(c), 4, &num, 255);
        Gfx_DrawTextF9((const uint8_t *)&num, 0x57, c * 9 + 0x4F, 4);
    }
    for (int c = 1; c <= 4; c++) {
        System_StrLong(DEF_LOSS(c), 4, &num, 255);
        Gfx_DrawTextF9((const uint8_t *)&num, 0x57, c * 9 + 0x86, 4);
    }
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
}
