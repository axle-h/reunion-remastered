// Unit GrWar (1537): the ground battle. See grwar.h.
#include "game/grwar.h"

#include <string.h>

#include "game/blit.h"
#include "game/command.h"
#include "game/gfx.h"
#include "game/gfxpath.h"
#include "game/minmax.h"
#include "game/mouse.h"
#include "game/sound.h"
#include "game/system.h"

// Unnamed globals (see grwar.h for what they are).
#define W(off) (*(int16_t *)ds_at(off))
#define B(off) (*(uint8_t *)ds_at(off))
#define FPV(off) (*(FarPtr *)ds_at(off))

#define RENDER_BUF FPV(0x7628)
#define BACKGROUND FPV(0x762C)
#define UNIT_ICONS FPV(0x7630)
#define EFFECT_ICONS FPV(0x7634)
#define LOST_FRAMES FPV(0x7638)
#define LOST_SHOWN B(0x763C)
#define LOST_FRAME W(0x763E)
#define LOST_TICK W(0x7640)
#define BATTLE_OVER B(0x770B)
#define PAUSED B(0x770C)
#define PATH_DIR W(0x770E)
#define DS_7710 W(0x7710)
#define SELECTED W(0x7716)
#define SELECTED_ATT B(0x771A)
#define BASE_STYLE B(0x778B)
#define RETREATED B(0x77DB)
#define TARGET_SYSTEM W(0x7798)
#define TARGET_PLANET W(0x779A)
#define TARGET_MOON W(0x779C)
#define WAR_ANIM_BUF FPV(0x7E36)
#define ANIM_FRAME W(0x7E2C)

enum { GRID_STRIDE = 11, GRID_ORG = 0xC, BASE_SLOT = 21 };

static FarPtr fp_add(FarPtr p, uint16_t add) { return far_ptr(p >> 16, (uint16_t)(p + add)); }
static uint8_t *host_add(FarPtr p, uint16_t add) { return (uint8_t *)far_host(p) + add; }
static uint8_t *screen_at(uint16_t ofs) { return gfx_screen() + ofs; }
// The original addresses the off-screen field as RENDER_BUF's segment with its
// own offsets, dropping the pointer's offset (GetMem's 0 or 8).
static uint8_t *render_at(uint16_t ofs)
{
    return (uint8_t *)far_host(far_ptr(RENDER_BUF >> 16, 0)) + ofs;
}
static const uint8_t *lit(const PString *s) { return (const uint8_t *)s; }

static uint8_t *grid(void) { return (uint8_t *)far_host(ds.GrWar_BattleGrids); }
static int slot_count(int side) { return W((uint16_t)(0x764C + 2 * side)); }
static int16_t *proj_count(int side) { return (int16_t *)ds_at((uint16_t)(0x7654 + 2 * side)); }

// Slot i (1-based) of side's list. The original adds i * 27h - 27h to the
// list's offset in 16 bits, so i = 0 (an empty grid cell, a unit without a
// target) wraps to the end of the list's segment, not the 39 bytes before it.
static GrBattleUnit *unit(int side, int i)
{
    FarPtr list = FPV((uint16_t)(0x7642 + 4 * side));
    return (GrBattleUnit *)far_host(fp_add(list, (uint16_t)((i - 1) * 0x27)));
}

static int16_t dx_of(uint8_t dir) { return W((uint16_t)(0x44 + 2 * dir)); }
static int16_t dy_of(uint8_t dir) { return W((uint16_t)(0x4E + 2 * dir)); }

// Pixel position of a unit: tile * 16 plus the partial step, with the field's origin.
static uint16_t pos_x(const GrBattleUnit *u)
{
    return (uint16_t)(u->x * 16 + (uint16_t)(u->step * (uint16_t)dx_of(u->dir)) + 0x40);
}
static uint16_t pos_y(const GrBattleUnit *u)
{
    return (uint16_t)(u->y * 16 + (uint16_t)(u->step * (uint16_t)dy_of(u->dir)) + 3);
}

static int32_t get32(uint16_t off)
{
    int32_t v;
    memcpy(&v, ds_at(off), 4);
    return v;
}
static void set32(uint16_t off, int32_t v) { memcpy(ds_at(off), &v, 4); }
// Table of longints [side][type] at 7646 + base + 16*side + 4*type.
static int32_t tbl(uint16_t base, int side, int type)
{
    return get32((uint16_t)(0x7646 + base + 16 * side + 4 * type));
}

static int32_t long_mul(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
static int32_t long_div(int32_t a, int32_t b)
{
    if (b == 0)
        System_RunError(200);
    if (b == -1)
        return (int32_t)(0u - (uint32_t)a);
    return a / b;
}
static int16_t idiv16(int16_t a, int16_t b)
{
    if (b == 0)
        System_RunError(200);
    if (b == -1)
        return (int16_t)(0u - (uint16_t)a);
    return (int16_t)(a / b);
}
static int16_t abs16(int16_t v) { return (int16_t)(v < 0 ? -v : v); }

// 1537:0029
void GrWar_EnterGroundBattle(void)
{
    PString num, name;

    Sound_AllocWarSndBuf();
    BACKGROUND = System_GetMem(64000);
    UNIT_ICONS = System_GetMem(0x3C00);
    EFFECT_ICONS = System_GetMem(0x2800);
    FarPtr side_pic = System_GetMem(0x3200);
    Gfx_LoadPic(lit(PSTR("war\\grwar")), far_host(side_pic), 1, 200, 0, 0, 0x40, true,
                Gfx_PalettePtr(), 0, 0xB0, 0x40, 0, 0xBE);
    const TBody *body = (const TBody *)far_host(ds.Game_CurBody);
    System_StrLong((int8_t)body->SurfaceType, 0, &num, 0xFF);
    System_StrLoad(&name, PSTR("war\\grf"));
    System_StrConcat(&name, &num);
    Gfx_LoadPic(lit(&name), far_host(BACKGROUND), 1, 200, 0, 0, 0x140, false, Gfx_PalettePtr(), 0,
                0x3F, 0xB0, 0, 0x3F);
    Blit_CopyRectStrided(far_host(side_pic), far_host(BACKGROUND), 0x40, 200, 0x40, 0x140);
    Blit_CopyRect(host_add(BACKGROUND, 0x3D40), screen_at(0x3D40), 0x140, 0x97);
    Gfx_LoadPic(lit(PSTR("war\\gricon")), far_host(UNIT_ICONS), 1, 0x30, 0, 0, 0x140, false,
                Gfx_PalettePtr(), 0, 0x6F, 0x40, 0, 0x6F);
    Gfx_LoadPic(lit(PSTR("war\\gricon2")), far_host(EFFECT_ICONS), 1, 0x20, 0, 0, 0x140, false,
                Gfx_PalettePtr(), 0, 0x6F, 0x40, 0, 0x6F);
    GrWar_BattleRound();
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
    System_FreeMem(side_pic, 0x3200);
    WAR_ANIM_BUF = System_GetMem(0x1220);
    Command_InitRoomAnim();
    int16_t anim = GrWar_PickBattleAnim(); // (*Game::GrWarProc)()
    Command_StartMainAnim(anim);
    ANIM_FRAME = 0;
    RETREATED = 0;
    if (BATTLE_OVER != 0)
        GrWar_EndGroundBattle();
}

// 1537:0200
void GrWar_InitBattleUnits(void)
{
    ds.Game_BattleFlag = false;
    ds.GrWar_BattleGrids = System_GetMem(0xC6);
    ds.GrWar_Projectiles = System_GetMem(0x708);
    for (int x = 0; x <= 15; x++)
        for (int y = 0; y <= 8; y++)
            grid()[x * GRID_STRIDE + y + GRID_ORG] = 0;
    BATTLE_OVER = 0;
    DS_7710 = 0;

    // Drop empty attacker slots.
    int i = 1;
    while (i <= ds.GrWarUnits_AttackerSlotCount) {
        if (unit(1, i)->count == 0) {
            int last = ds.GrWarUnits_AttackerSlotCount - 1;
            for (int j = i; j <= last; j++)
                System_MoveFwd(unit(1, j + 1), unit(1, j), 0x27);
            ds.GrWarUnits_AttackerSlotCount--;
        } else
            i++;
    }

    int16_t n = ds.GrWarUnits_AttackerSlotCount;
    for (i = 1; i <= n; i++) {
        GrBattleUnit *u = unit(1, i);
        int32_t m = MinMax_Min((int16_t)(n - i / 9), 8);
        int8_t half = (int8_t)(m / 2);
        u->x = (uint8_t)((i - 1) / 9 + 2);
        u->y = (uint8_t)((i - 1) % 9 + 4 - half);
        u->moving = 0;
        u->dir = 3;
        u->step = 0;
        u->moveDelay = 1;
        u->mode = 1;
        u->pathLen = 0;
        u->effect = 0;
        u->effTimer = 0;
        u->effDelay = 0;
        u->cooldown = 0;
    }
    n = ds.GrWarUnits_DefenderSlotCount;
    for (i = 1; i <= n; i++) {
        GrBattleUnit *u = unit(2, i);
        int32_t m = MinMax_Min((int16_t)(n - i / 9), 8);
        int8_t half = (int8_t)(m / 2);
        u->x = (uint8_t)(13 - (i - 1) / 9);
        u->y = (uint8_t)((i - 1) % 9 + 4 - half);
        u->mode = 3;
        u->target = (uint8_t)GrWar_FindNearestEnemy((int16_t)i, 2);
        u->pathLen = 0;
        u->effect = 0;
        u->effTimer = 0;
        u->effDelay = 0;
        u->cooldown = 0;
    }
    for (int s = 1; s <= 2; s++) {
        GrBattleUnit *b = unit(s, BASE_SLOT);
        b->type = 5;
        b->count = 1;
        b->hp = 100;
        b->x = 0;
        b->y = 4;
        b->target = 0;
        b->targetY = 0;
        b->mode = 0;
        b->moving = 0;
        b->dir = 1;
        b->step = 0;
        b->moveDelay = 0;
        b->effect = 0;
        b->effTimer = 0;
        b->effDelay = 0;
        b->cooldown = 0;
    }
    unit(1, BASE_SLOT)->x = 0;
    unit(1, BASE_SLOT)->y = 4;
    unit(2, BASE_SLOT)->x = BASE_STYLE == 0 ? 0xF : 0xE;
    unit(2, BASE_SLOT)->y = 4;
}

// 1537:053a: hotspots 1..3 and 5 (hotspot 4 is not touched; the count is 4).
void GrWar_SetupBattleHotspots(void)
{
    ds.Mouse_HotspotCount = 4;
    THotspot *h = Mouse_Hotspot(1);
    h->X = 0x40;
    h->Y = 0x34;
    h->W = 0x100;
    h->H = 0x90;
    h->Caption.len = 0;
    h->Repeat_ = false;
    h->_1A = 0;

    h = Mouse_Hotspot(2);
    h->X = 0;
    h->Y = 0xA8;
    h->W = 0x40;
    h->H = 0x10;
    System_StrStore(PSTR("Move"), (PString *)&h->Caption, 0x12);
    h->Repeat_ = true;
    h->_1A = 0;

    h = Mouse_Hotspot(3);
    h->X = 0;
    h->Y = 0xB8;
    h->W = 0x40;
    h->H = 0x10;
    System_StrStore(PSTR("Attack"), (PString *)&h->Caption, 0x12);
    h->Repeat_ = true;
    h->_1A = 0;

    h = Mouse_Hotspot(5);
    h->X = 0;
    h->Y = 0x31;
    h->W = 0x40;
    h->H = 0x77;
    h->Caption.len = 0;
    h->Repeat_ = true;
    h->_1A = 0;
}

// 1537:05ee
void GrWar_SetHotspotFlags(uint8_t flag)
{
    for (int j = 1; j <= ds.Mouse_HotspotCount; j++)
        Mouse_Hotspot((int16_t)j)->_1A = flag;
    ds.Game_HotspotHit = 0;
}

// 1537:0642: unit class name, Pcs/Str or Dmg for the selected unit (decompile failed; from the disassembly).
void GrWar_DrawSelectedUnitInfo(void)
{
    PString s, tmp;

    if (SELECTED == 0) {
        Gfx_DrawTextFC(lit(PSTR("")), 5, 0x88, 8);
        Gfx_DrawTextF9(lit(PSTR("")), 5, 0x92, 9);
        Gfx_DrawTextF9(lit(PSTR("")), 5, 0x9C, 9);
        return;
    }
    int side = SELECTED_ATT != 0 ? 1 : 2;
    GrBattleUnit *u = unit(side, SELECTED);
    int32_t strength;
    if (tbl(0, side, u->type) > 0) {
        int32_t q = long_mul(u->count, tbl(0x30, side, u->type)); // 7676: attack sum
        strength = long_div(q, tbl(0, side, u->type));
    } else
        strength = 0;
    System_StrStore((const PString *)ds_at((uint16_t)(0x5A13 + u->type * 9)), &s, 0x32);
    if (u->type == 5 && (BASE_STYLE ^ SELECTED_ATT) != 0)
        System_StrStore(PSTR("Colony"), &s, 0x32);
    Gfx_DrawTextFC(lit(&s), 5, 0x88, 8);
    if (u->type < 5) {
        System_StrLong(u->count, 0, &s, 0x32);
        System_StrLoad(&tmp, PSTR("Pcs:"));
        System_StrConcat(&tmp, &s);
        Gfx_DrawTextF9(lit(&tmp), 5, 0x92, 9);
        System_StrLong(strength, 0, &s, 0x32);
        System_StrLoad(&tmp, PSTR("Str:"));
        System_StrConcat(&tmp, &s);
        Gfx_DrawTextF9(lit(&tmp), 5, 0x9C, 9);
    } else {
        int16_t dmg = (int16_t)(100 - u->hp);
        System_StrLong(dmg, 0, &s, 0x32);
        System_StrLoad(&tmp, &s);
        System_StrConcat(&tmp, PSTR("%"));
        System_StrStore(&tmp, &s, 0x32);
        if (dmg == 0)
            System_StrStore(PSTR("none"), &s, 0x32);
        System_StrLoad(&tmp, PSTR("Dmg:"));
        System_StrConcat(&tmp, &s);
        Gfx_DrawTextF9(lit(&tmp), 5, 0x92, 9);
        Gfx_DrawTextF9(lit(PSTR("")), 5, 0x9C, 9);
    }
}

// 1537:08a4: 16x16 sprite of the gricon sheet into the off-screen field.
void GrWar_DrawUnitSprite(int16_t frame, int16_t sheet, int16_t x, int16_t y, int16_t mode)
{
    const uint8_t *src =
        host_add(UNIT_ICONS, (uint16_t)((sheet - 1) * 0x1400 + (frame - 1) * 0x10));
    uint8_t *dst = render_at((uint16_t)(x + 0x10 + y * 0x140));
    switch (mode) {
    case 1: Blit_BlitTransparent(src, dst, 0x10, 0x10, 0x40); break;
    case 2: Blit_BlitTransposed(src, dst, 0x10, 0x40); break;
    case 3: Blit_BlitRotate180(src, dst, 0x10, 0x40); break;
    case 4: Blit_BlitAntiTranspose(src, dst, 0x10, 0x40); break;
    default: break;
    }
}

// 1537:0961
void GrWar_DrawEffectSprite(int16_t frame, int16_t sheet, int16_t x, int16_t y, int16_t variant)
{
    const uint8_t *src = host_add(EFFECT_ICONS,
                        (uint16_t)((sheet - 1) * 0x1400 + (frame - 1) * 0x10 + (variant - 1) * 0x1400));
    Blit_BlitTransparent(src, render_at((uint16_t)(x + 0x10 + y * 0x140)), 0x10, 0x10, 0x40);
}

// 1537:09c2: 11x10 projectile sprite (kind 0..15) below the effect sheets.
void GrWar_DrawProjectile(int16_t x, int16_t y, int16_t kind)
{
    const uint8_t *src = host_add(EFFECT_ICONS, (uint16_t)(0x1400 + kind * 11 + 1));
    Blit_BlitTransparent(src, render_at((uint16_t)(x + 0x10 + y * 0x140)), 0xB, 10, 0x40);
}

// 1537:0a0c
void GrWar_DrawSelectionMarkers(void)
{
    if (SELECTED == 0)
        return;
    if (SELECTED_ATT == 0) {
        const GrBattleUnit *u = unit(2, SELECTED);
        uint16_t off = SELECTED == BASE_SLOT ? B((uint16_t)(BASE_STYLE * 2 + 0xB9)) : 0;
        GrWar_DrawEffectSprite(0xF, 2, (int16_t)(pos_x(u) + off), (int16_t)(pos_y(u) + off), 1);
    } else {
        const GrBattleUnit *u = unit(1, SELECTED);
        uint16_t off = SELECTED == BASE_SLOT ? B((uint16_t)(BASE_STYLE * 2 + 0xB8)) : 0;
        GrWar_DrawEffectSprite(0xF, 1, (int16_t)(pos_x(u) + off), (int16_t)(pos_y(u) + off), 1);
        if (u->mode == 2)
            GrWar_DrawEffectSprite(0x10, 2, (int16_t)(u->target * 0x10 + 0x40),
                                   (int16_t)(u->targetY * 0x10 + 3), 1);
        if (u->mode == 3) {
            const GrBattleUnit *t = unit(2, u->target);
            off = u->target == BASE_SLOT ? B((uint16_t)(BASE_STYLE * 2 + 0xB9)) : 0;
            GrWar_DrawEffectSprite(0x10, 1, (int16_t)(pos_x(t) + off), (int16_t)(pos_y(t) + off), 1);
        }
    }
}

// 1537:0ca0: the bases are drawn into the field and marked in the grid.
void GrWar_DrawBaseSprites(void)
{
    if (BASE_STYLE == 0) {
        Blit_CopyRect(host_add(BACKGROUND, 0x40), render_at(0x5410), 0x20, 0x20);
        Blit_BlitTransparent(host_add(BACKGROUND, 0x29C4), render_at(0x5500), 0x10, 0x10, 0xB0);
    } else {
        Blit_BlitTransparent(host_add(BACKGROUND, 0x2980), render_at(0x5410), 0x10, 0x10, 0xB0);
        Blit_BlitTransparent(host_add(BACKGROUND, 0xC0), render_at(0x54F0), 0x20, 0x20, 0xB0);
    }
    uint8_t *g = grid();
    g[0x10] = 0x15;
    g[0xB5] = 0x79;
    if (BASE_STYLE == 0) {
        g[0x11] = 0x15;
        g[0x1B] = 0x15;
        g[0x1C] = 0x15;
    } else {
        g[0xAA] = 0x79;
        g[0xAB] = 0x79;
        g[0xB6] = 0x79;
    }
}

// 1537:0d85
void GrWar_BattleTick(void)
{
    if (PAUSED != 2) {
        GrWar_BattleRound();
        RETREATED = 0;
        if (BATTLE_OVER != 0)
            GrWar_EndGroundBattle();
    }
}

// Effect records at DS:82, 6 bytes: first frame, sheet, frame count.
static int16_t fx_first(uint8_t e) { return W((uint16_t)(0x82 + e * 6)); }
static int16_t fx_sheet(uint8_t e) { return W((uint16_t)(0x84 + e * 6)); }
static int16_t fx_frames(uint8_t e) { return W((uint16_t)(0x86 + e * 6)); }

// 1537:0da8
void GrWar_RenderBattle(void)
{
    GrWar_DrawSelectedUnitInfo();
    RENDER_BUF = System_GetMem(0xBCD0);
    Blit_CopyRect(host_add(BACKGROUND, 0x3D80), render_at(0x50), 0x100, 0x97);
    GrWar_DrawBaseSprites();

    for (int s = 1; s <= 2; s++) {
        int n = slot_count(s);
        for (int i = 1; i <= n; i++) {
            GrBattleUnit *u = unit(s, i);
            if (!(u->count > 0 || u->effect != 0))
                continue;
            int16_t sheet = s == 1 ? 1 : 2;
            uint8_t dir = u->dir;
            if (u->effDelay == 0) {
                if (u->effTimer != 0)
                    u->effTimer--;
                if (u->effTimer == 0)
                    u->effect = 0;
                else
                    u->effDelay = 1;
            } else
                u->effDelay--;
            int16_t frame = (int16_t)((u->type - 1) * 5 + 1);
            int16_t sheet2 = sheet;
            if (u->effect != 0) {
                frame = (int16_t)(fx_first(u->effect) + fx_frames(u->effect) - u->effTimer);
                sheet2 = fx_sheet(u->effect);
                if (s == 2) {
                    if (u->effect < 5)
                        sheet2++;
                    if (u->effect == 5)
                        frame += 9;
                }
            }
            int16_t x = (int16_t)pos_x(u), y = (int16_t)pos_y(u);
            if (u->effect == 7 || u->effect == 8) {
                if (u->count > 0 || u->effTimer > 5)
                    GrWar_DrawUnitSprite((int16_t)((u->type - 1) * 5 + 1), sheet, x, y, dir);
                GrWar_DrawEffectSprite(frame, sheet2, x, y, 1);
            } else if (u->count > 0)
                GrWar_DrawUnitSprite(frame, sheet2, x, y, dir);
            if (SELECTED == i && ((s == 1 && SELECTED_ATT != 0) || (s == 2 && SELECTED_ATT == 0))
                && u->count == 0)
                SELECTED = 0;
        }
    }

    // The bases' hit effects.
    for (int s = 1; s <= 2; s++) {
        GrBattleUnit *b = unit(s, BASE_SLOT);
        if (b->effect != 0) {
            if (b->effDelay == 0) {
                if (b->effTimer != 0)
                    b->effTimer--;
                if (b->effTimer == 0)
                    b->effect = 0;
                else
                    b->effDelay = 1;
            } else
                b->effDelay--;
        }
        if (b->effect == 7 || b->effect == 8) {
            uint16_t off = B((uint16_t)(BASE_STYLE * 2 + s + 0xB7));
            GrWar_DrawEffectSprite((int16_t)(fx_first(b->effect) + fx_frames(b->effect) - b->effTimer),
                                   fx_sheet(b->effect), (int16_t)(b->x * 0x10 + 0x40 + off),
                                   (int16_t)(b->y * 0x10 + 3 + off), 1);
        }
    }
    GrWar_DrawSelectionMarkers();

    for (int s = 1; s <= 2; s++) {
        int n = *proj_count(s);
        for (int k = 1; k <= n; k++) {
            const int16_t *p = (const int16_t *)((uint8_t *)far_host(ds.GrWar_Projectiles)
                                                 + (s - 1) * 900 + (k - 1) * 0x12);
            // x0 y0 x1 y1 step total sprite
            int16_t px = (int16_t)(idiv16((int16_t)((int16_t)(p[2] - p[0]) * p[4]), p[5]) + p[0]);
            int16_t py = (int16_t)(idiv16((int16_t)((int16_t)(p[3] - p[1]) * p[4]), p[5]) + p[1]);
            GrWar_DrawProjectile(px, py, p[6]);
        }
    }
    Blit_CopyRect(render_at(0x50), screen_at(0x3D80), 0x100, 0x97);
    Command_TickGrAnim();
    System_FreeMem(RENDER_BUF, 0xBCD0);
}

// 1537:11f2: war animation 1..14 whose two unit types are both on the field (Game::GrWarProc).
int16_t GrWar_PickBattleAnim(void)
{
    for (;;) {
        uint16_t r = System_Random(0xE);
        int idx = (r + 1) * 5;
        uint8_t a = B((uint16_t)(0x5654 + idx));
        uint8_t b = B((uint16_t)(0x5655 + idx));
        bool ok = (tbl(0, 1, a) > 0 || tbl(0, 2, a) > 0) && (tbl(0, 1, b) > 0 || tbl(0, 2, b) > 0);
        if (ok)
            return (int16_t)(r + 1);
    }
}

static void swap_if_defender(int16_t *unit_, int16_t *enemy, int16_t side)
{
    if (side == 2) {
        int16_t t = *unit_;
        *unit_ = *enemy;
        *enemy = t;
    }
}

// 1537:12aa: (|dx| + |dy| - 5) >> 4 + 1 in 16 bits (logical shift, so tiny sums give 1000h).
int16_t GrWar_DistManhattanTiles(int16_t unit_, int16_t enemy, int16_t side)
{
    swap_if_defender(&unit_, &enemy, side);
    const GrBattleUnit *u = unit(1, unit_), *e = unit(2, enemy);
    int16_t dy = abs16((int16_t)(pos_y(u) - pos_y(e)));
    int16_t dx = abs16((int16_t)(pos_x(u) - pos_x(e)));
    uint16_t v = (uint16_t)(dx + dy - 5);
    return (int16_t)((v >> 4) + 1);
}

// 1537:13c7: (max(|dx|,|dy|) - 3) >> 4 + 1 as longints (the shift is logical).
int32_t GrWar_DistEuclidTiles(int16_t unit_, int16_t enemy, int16_t side)
{
    swap_if_defender(&unit_, &enemy, side);
    const GrBattleUnit *u = unit(1, unit_), *e = unit(2, enemy);
    int16_t dx = abs16((int16_t)(pos_x(u) - pos_x(e)));
    int16_t dy = abs16((int16_t)(pos_y(u) - pos_y(e)));
    int32_t m = MinMax_Max(dx, dy);
    return (int32_t)(((uint32_t)(m - 3) >> 4) + 1);
}

// 1537:14ff: (dx^2 + dy^2 - 1) >> 8 + 1 as longints.
int32_t GrWar_DistSquaredTiles(int16_t unit_, int16_t enemy, int16_t side)
{
    swap_if_defender(&unit_, &enemy, side);
    const GrBattleUnit *u = unit(1, unit_), *e = unit(2, enemy);
    int32_t dy = (int16_t)pos_y(u) - (int16_t)pos_y(e);
    int32_t dx = (int16_t)pos_x(u) - (int16_t)pos_x(e);
    int32_t sum = long_mul(dy, dy) + long_mul(dx, dx);
    return (int32_t)(((uint32_t)(sum - 1) >> 8) + 1);
}

// 1537:1652
bool GrWar_IsAdjacent(int16_t unit_, int16_t enemy, int16_t side)
{
    return (int16_t)GrWar_DistEuclidTiles(unit_, enemy, side) < 2;
}

// 1537:1674
bool GrWar_IsInArtilleryBand(int16_t unit_, int16_t enemy, int16_t side)
{
    int16_t d = (int16_t)GrWar_DistSquaredTiles(unit_, enemy, side);
    return !(d < 4 || d > 0x10);
}

// 1537:16a3
bool GrWar_InWeaponRange(int16_t weapon, int16_t unit_, int16_t enemy, int16_t side)
{
    int16_t manhattan = GrWar_DistManhattanTiles(unit_, enemy, side);
    int16_t euclid = (int16_t)GrWar_DistEuclidTiles(unit_, enemy, side);
    if ((weapon == 1 && euclid < 2) || (weapon == 2 && manhattan < 3) || (weapon == 3 && euclid < 3))
        return true;
    if (weapon == 4 && GrWar_IsAdjacent(unit_, enemy, side))
        return true;
    if (weapon == 4 && GrWar_IsInArtilleryBand(unit_, enemy, side))
        return true;
    return false;
}

// 1537:1725
int16_t GrWar_DirectionToward(int16_t selfX, int16_t selfY, int16_t targetX, int16_t targetY)
{
    int16_t dx = (int16_t)(selfX - targetX);
    int16_t dy = (int16_t)(selfY - targetY);
    if (abs16(dx) < abs16(dy))
        return dy < 1 ? 4 : 2;
    return dx < 1 ? 3 : 1;
}

// 1537:177e
int16_t GrWar_DirectionToward16(int16_t selfX, int16_t selfY, int16_t targetX, int16_t targetY)
{
    int16_t a = (int16_t)((GrWar_DirectionToward(selfX, selfY, targetX, targetY) - 1) * 4);
    int16_t tdx = (int16_t)(targetX - selfX), tdy = (int16_t)(targetY - selfY);
    int16_t ax = abs16(tdx), ay = abs16(tdy);
    if (a == 0)
        a = (int16_t)(4 - idiv16((int16_t)(tdy * 4 + ax * 5), (int16_t)(ax * 2)));
    else if (a == 4)
        a = (int16_t)(idiv16((int16_t)(tdx * 4 + ay * 5), (int16_t)(ay * 2)) + 4);
    else if (a == 8)
        a = (int16_t)(idiv16((int16_t)(tdy * 4 + ax * 5), (int16_t)(ax * 2)) + 8);
    else if (a == 12)
        a = (int16_t)(0x10 - idiv16((int16_t)(tdx * 4 + ay * 5), (int16_t)(ay * 2)));
    int16_t r = (int16_t)(a + 2);
    if (r > 0xF)
        r = (int16_t)(a - 0xE);
    return r;
}

// 1537:188f
void GrWar_StartUnitEffect(int16_t side, int16_t unit_, int16_t effect)
{
    GrBattleUnit *u = unit(side, unit_);
    u->effect = (uint8_t)effect;
    u->effTimer = B((uint16_t)(effect * 6 + 0x86));
    u->effDelay = 3;
}

// 1537:18d3: side 3 = both armies; defenders come back as index + 100.
int16_t GrWar_FindUnitNearPoint(int16_t x, int16_t y, int16_t side)
{
    int lo = side, hi = side;
    if (side == 3) {
        lo = 1;
        hi = 2;
    }
    int32_t best = 1000000;
    int found = 0, found_side = 0;
    for (int s = lo; s <= hi; s++) {
        int n = slot_count(s);
        for (int i = 1; i <= n; i++) {
            const GrBattleUnit *u = unit(s, i);
            if (u->count > 0) {
                int16_t ux = (int16_t)(pos_x(u) + 2), uy = (int16_t)(pos_y(u) + 0x33);
                int32_t dy = (int32_t)uy - y, dx = (int32_t)ux - x;
                int32_t d = long_mul(dx, dx) + long_mul(dy, dy);
                if (d < best) {
                    found = i;
                    found_side = s;
                    best = d;
                }
            }
        }
    }
    for (int s = lo; s <= hi; s++) {
        const GrBattleUnit *b = unit(s, BASE_SLOT);
        if (b->count > 0) {
            uint16_t off = B((uint16_t)(BASE_STYLE * 2 + s + 0xB7));
            int16_t ux = (int16_t)(b->x * 0x10 + 0x40 + off), uy = (int16_t)(b->y * 0x10 + 0x34 + off);
            int32_t dy = (int32_t)uy - y, dx = (int32_t)ux - x;
            int32_t d = long_mul(dx, dx) + long_mul(dy, dy);
            if (d < best) {
                found = BASE_SLOT;
                found_side = s;
                best = d;
            }
        }
    }
    if (side == 3 && found_side == 2)
        found += 100;
    if (best > 500)
        found = 0;
    return (int16_t)found;
}

// 1537:1b80
int16_t GrWar_FindNearestEnemy(int16_t unit_, int16_t side)
{
    int16_t best = 1000, res = 0;
    int other = 3 - side;
    int n = slot_count(other);
    for (int j = 1; j <= n; j++) {
        if (unit(other, j)->count > 0) {
            int16_t d = (int16_t)GrWar_DistSquaredTiles(unit_, (int16_t)j, side);
            if (d < best) {
                res = (int16_t)j;
                best = d;
            }
        }
    }
    return res;
}

// 1537:1c05
void GrWar_BattleRound(void)
{
    uint16_t sound = 0;
    GrBattleUnit *hit = nullptr; // the target of the last attack (also read when no target was found)
    uint16_t first = System_Random(2);

    for (int a = 1; a <= 2; a++) {
        int e = first == 0 ? a : 3 - a;
        int o = 3 - e;
        int n = slot_count(e);
        for (int i = 1; i <= n; i++) {
            GrBattleUnit *u = unit(e, i);
            if (u->count <= 0)
                continue;
            if (u->mode == 3 && unit(o, u->target)->count == 0) {
                if (e == 1)
                    u->mode = 1;
                else
                    u->target = (uint8_t)GrWar_FindNearestEnemy((int16_t)i, (int16_t)e);
            }
            if (slot_count(o) != 0 && u->effect == 0 && u->cooldown == 0) {
                int16_t tgt = 0;
                if (u->mode == 3 && GrWar_InWeaponRange(u->type, (int16_t)i, u->target, (int16_t)e))
                    tgt = u->target;
                if (tgt == 0 && slot_count(o) > 0) {
                    int no = slot_count(o);
                    for (int j = 1; j <= no; j++)
                        if (unit(o, j)->count > 0 && GrWar_InWeaponRange(u->type, (int16_t)i, (int16_t)j, (int16_t)e))
                            tgt = (int16_t)j;
                }
                if (tgt > 0) {
                    GrBattleUnit *t = unit(o, tgt);
                    hit = t;
                    if (t->count > 0) {
                        if (GrWar_IsAdjacent((int16_t)i, tgt, (int16_t)e) || u->type != 4) {
                            int32_t q = long_mul(u->count, tbl(0x30, e, u->type));
                            q = long_div(q, tbl(0, e, u->type));
                            q = long_mul((int16_t)W((uint16_t)(0x28 + t->type * 2)), q);
                            int32_t dmg = long_div(q, 500);
                            if (dmg < 1)
                                dmg = 1;
                            if (dmg > 0xF)
                                dmg = 0xF;
                            t->count = (int16_t)(t->count - (int16_t)dmg);
                            GrWar_StartUnitEffect((int16_t)e, (int16_t)i, u->type);
                            if (t->count < 1) {
                                t->count = 0;
                                sound = (uint16_t)(t->type + 5);
                                GrWar_StartUnitEffect((int16_t)o, tgt, 7);
                            } else {
                                sound = u->type;
                                GrWar_StartUnitEffect((int16_t)o, tgt, 8);
                            }
                            if (u->moving == 0)
                                u->dir = (uint8_t)GrWar_DirectionToward(u->x, u->y, t->x, t->y);
                            u->cooldown = W((uint16_t)(0x20 + u->type * 2));
                        } else {
                            // Artillery out of melee range fires a projectile.
                            int32_t dmg = long_div(long_mul(u->count, tbl(0x50, e, u->type)),
                                                   tbl(0, e, u->type));
                            sound = 5;
                            GrWar_StartUnitEffect((int16_t)e, (int16_t)i, 5);
                            int16_t *cnt = proj_count(e);
                            (*cnt)++;
                            int16_t *p = (int16_t *)((uint8_t *)far_host(ds.GrWar_Projectiles)
                                                     + (e - 1) * 900 + (*cnt - 1) * 0x12);
                            p[0] = (int16_t)pos_x(u);
                            p[1] = (int16_t)pos_y(u);
                            p[2] = (int16_t)pos_x(t);
                            p[3] = (int16_t)pos_y(t);
                            p[4] = 1;
                            p[5] = (int16_t)((uint16_t)GrWar_DistEuclidTiles((int16_t)i, tgt, (int16_t)e) << 3);
                            p[6] = GrWar_DirectionToward16(u->x, u->y, t->x, t->y);
                            memcpy(&p[7], &dmg, 4);
                            u->cooldown = W((uint16_t)(0x20 + u->type * 2));
                        }
                    }
                }
            }
            if (u->cooldown > 0)
                u->cooldown--;
        }
    }

    // Movement.
    uint8_t *g = grid();
    for (int e = 1; e <= 2; e++) {
        int n = slot_count(e);
        for (int i = 1; i <= n; i++) {
            GrBattleUnit *u = unit(e, i);
            if (u->count <= 0)
                continue;
            int base = (e - 1) * 100;
            uint16_t cell = g[(uint16_t)(u->y + dy_of(u->dir) + (u->x + dx_of(u->dir)) * GRID_STRIDE + GRID_ORG)];
            if (u->moveDelay != 0) {
                u->moveDelay--;
                continue;
            }
            if (u->moving == 0 || cell == 0 || i + base == cell || u->mode > 1) {
                uint8_t olddir = u->dir;
                if (cell == 0 || i + base == cell)
                    u->step++;
                if (u->step == 16 || u->moving == 0) {
                    g[u->y + u->x * GRID_STRIDE + GRID_ORG] = 0;
                    if (u->moving != 0)
                        g[(uint16_t)(u->y + dy_of(olddir) + (u->x + dx_of(olddir)) * GRID_STRIDE + GRID_ORG)] = 0;
                    if (u->moving != 0)
                        u->x = (uint8_t)(u->x + (int8_t)dx_of(olddir));
                    if (u->moving != 0)
                        u->y = (uint8_t)(u->y + (int8_t)dy_of(olddir));
                    u->moving = 1;
                    if (u->mode == 1) {
                        if (u->pathLen == 0)
                            PATH_DIR = 0;
                        else {
                            int cnt = u->pathLen - 1;
                            for (int m = 1; m <= cnt; m++)
                                u->path[m - 1] = u->path[m];
                            u->pathLen--;
                            PATH_DIR = u->pathLen == 0 ? 0 : u->path[0];
                        }
                    }
                    if (u->mode == 2) {
                        GrWar_FindPath(u->x, u->y, u->target, u->targetY, u->target, u->targetY,
                                       (uint16_t *)ds_at(0x770E));
                        if (u->x == u->target && u->y == u->targetY)
                            u->mode = 1;
                    }
                    if (u->mode == 3) {
                        const GrBattleUnit *t = unit(3 - e, u->target);
                        hit = (GrBattleUnit *)t;
                        GrWar_FindPath(u->x, u->y, t->x, t->y, t->x, t->y, (uint16_t *)ds_at(0x770E));
                        uint8_t saved = u->step;
                        if (u->step != 0)
                            u->step--;
                        if (GrWar_InWeaponRange(u->type, (int16_t)i, u->target, (int16_t)e))
                            u->moving = 0;
                        u->step = saved;
                    }
                    if (PATH_DIR < 1)
                        u->moving = 0;
                    else
                        u->dir = (uint8_t)PATH_DIR;
                    if (u->dir == 1 && u->x == 0)
                        u->moving = 0;
                    if (u->dir == 2 && u->y == 0)
                        u->moving = 0;
                    if (u->dir == 3 && u->x == 15)
                        u->moving = 0;
                    if (u->dir == 4 && u->y == 8)
                        u->moving = 0;
                    if (u->moving != 0) {
                        uint16_t ahead = g[(uint16_t)(u->y + dy_of(u->dir) + (u->x + dx_of(u->dir)) * GRID_STRIDE + GRID_ORG)];
                        if (ahead != 0 && i + base != ahead)
                            u->moving = 0;
                    }
                    g[u->y + u->x * GRID_STRIDE + GRID_ORG] = (uint8_t)(i + base);
                    if (u->moving != 0)
                        g[(uint16_t)(u->y + dy_of(u->dir) + (u->x + dx_of(u->dir)) * GRID_STRIDE + GRID_ORG)] = (uint8_t)(i + base);
                    u->step = 0;
                }
                u->moveDelay = B((uint16_t)(u->type * 2 + 0x7E));
            }
        }
    }

    // Projectiles in flight.
    for (int s = 1; s <= 2; s++) {
        int k = 1;
        while (k <= *proj_count(s)) {
            uint8_t *pb = (uint8_t *)far_host(ds.GrWar_Projectiles) + (s - 1) * 900;
            int16_t *p = (int16_t *)(pb + (k - 1) * 0x12);
            if (p[4] == p[5]) {
                int16_t cx = idiv16((int16_t)(p[2] - 0x40), 0x10);
                int16_t cy = idiv16((int16_t)(p[3] - 3), 0x10);
                int16_t c = g[(uint16_t)(cy + cx * GRID_STRIDE + GRID_ORG)];
                if (c != 0 && ((c < 100) != (s == 1))) {
                    if (c > 100)
                        c = (int16_t)(c - 100);
                    if (c > 0)
                        hit = unit(3 - s, c);
                    GrWar_StartUnitEffect((int16_t)(3 - s), c, 8);
                    int32_t pd;
                    memcpy(&pd, &p[7], 4);
                    int32_t q = long_mul((int16_t)W((uint16_t)(0x28 + hit->type * 2)), pd);
                    int32_t dmg = long_div(q, 0xB4);
                    if (dmg < 1)
                        dmg = 1;
                    if (dmg > 0xF)
                        dmg = 0xF;
                    hit->count = (int16_t)(hit->count - (int16_t)dmg);
                    if (hit->count < 1) {
                        hit->count = 0;
                        sound = (uint16_t)(hit->type + 5);
                        GrWar_StartUnitEffect((int16_t)(3 - s), c, 7);
                    } else {
                        sound = 7;
                        GrWar_StartUnitEffect((int16_t)(3 - s), c, 8);
                    }
                }
                int last = *proj_count(s) - 1;
                for (int m = k; m <= last; m++)
                    System_MoveFwd(pb + m * 0x12, pb + (m - 1) * 0x12, 0x12);
                (*proj_count(s))--;
            } else {
                p[4]++;
                k++;
            }
        }
    }
    if (sound != 0)
        Sound_PlayGrSnd((int16_t)sound);

    // Clear cells of dead units.
    for (int x = 0; x <= 15; x++) {
        for (int y = 0; y <= 8; y++) {
            uint8_t *cellp = &g[y + x * GRID_STRIDE + GRID_ORG];
            if (*cellp < 100) {
                if (unit(1, *cellp)->count == 0)
                    *cellp = 0;
            } else if (unit(2, *cellp - 100)->count == 0)
                *cellp = 0;
        }
    }
    uint8_t was_over = BATTLE_OVER;

    bool attackers_dead = true;
    for (int i = 1; i <= ds.GrWarUnits_AttackerSlotCount; i++)
        if (unit(1, i)->count > 0)
            attackers_dead = false;
    bool defenders_dead = true;
    for (int i = 1; i <= ds.GrWarUnits_DefenderSlotCount; i++)
        if (unit(2, i)->count > 0)
            defenders_dead = false;
    if (ds.Game_BattleFlag != false) {
        BATTLE_OVER = 1;
        ds.GrWar_PlayerWon = true;
    }
    if (attackers_dead || defenders_dead) {
        BATTLE_OVER = 1;
        ds.GrWar_PlayerWon = defenders_dead;
    }
    if (BATTLE_OVER != 0) {
        // Wait for the death animations and the projectiles to finish.
        for (int s = 1; s <= 2; s++) {
            int n = slot_count(s);
            for (int i = 1; i <= n; i++)
                if (unit(s, i)->count == 0 && unit(s, i)->effTimer != 0)
                    BATTLE_OVER = 0;
            if (*proj_count(s) != 0)
                BATTLE_OVER = 0;
        }
    }
    GrWar_RenderBattle();
    if (was_over == 0 && BATTLE_OVER != 0)
        GfxPath_SetToolbarAndShow(0x1E);
}

// Scales a ship or unit count by survivors/total of its type (0 when the type was absent).
static int32_t scale(int32_t v, int32_t survivors, int32_t total)
{
    if (total <= 0)
        return 0;
    return long_div(long_mul(v, survivors), total);
}

// 1537:2a19
void GrWar_ApplyGroundLosses(void)
{
    // Survivors per side and type (76B6 + 16*s + 4*t).
    for (int s = 1; s <= 2; s++)
        for (int t = 1; t <= 4; t++)
            set32((uint16_t)(0x76B6 + 16 * s + 4 * t), 0);
    for (int i = 1; i <= ds.GrWarUnits_AttackerSlotCount; i++) {
        const GrBattleUnit *u = unit(1, i);
        uint16_t off = (uint16_t)(0x76C6 + u->type * 4);
        set32(off, (int32_t)((uint32_t)get32(off) + (uint32_t)(int32_t)u->count));
    }
    for (int i = 1; i <= ds.GrWarUnits_DefenderSlotCount; i++) {
        const GrBattleUnit *u = unit(2, i);
        uint16_t off = (uint16_t)(0x76D6 + u->type * 4);
        set32(off, (int32_t)((uint32_t)get32(off) + (uint32_t)(int32_t)u->count));
    }
    for (int t = 1; t <= 4; t++) // units of the attackers not assigned to a slot survive too
        set32((uint16_t)(0x76C6 + 4 * t),
              (int32_t)((uint32_t)get32((uint16_t)(0x76C6 + 4 * t)) + (uint32_t)get32((uint16_t)(0x7676 + 4 * t))));
    // Losses = totals - survivors (76D6 + 16*s + 4*t).
    for (int s = 1; s <= 2; s++)
        for (int t = 1; t <= 4; t++)
            set32((uint16_t)(0x76D6 + 16 * s + 4 * t),
                  (int32_t)((uint32_t)tbl(0, s, t) - (uint32_t)get32((uint16_t)(0x76B6 + 16 * s + 4 * t))));

    // Fleets (list 0) and planet forces (list 1) at the battle site.
    int16_t counts[2] = {ds.Game_GroupCount, ds.Game_PlanetForceCount};
    for (int l = 0; l <= 1; l++) {
        for (int g = 1; g <= counts[l]; g++) {
            TGroup *grp = (TGroup *)((uint8_t *)far_host(ds.Game_Groups) + (l * 32 + g - 1) * (int)sizeof(TGroup));
            if (grp->System == TARGET_SYSTEM && grp->Planet == TARGET_PLANET && grp->Moon == TARGET_MOON
                && (grp->Status == 1 || grp->Status == 2 || grp->Status == 7)
                && (grp->Kind == 1 || (grp->Kind == 5 && grp->Moon == TARGET_MOON))) {
                for (int t = 1; t <= 4; t++) {
                    TGroupSlot *slot = &grp->Units[1][t - 1];
                    int32_t surv = get32((uint16_t)(0x76C6 + 4 * t)), total = tbl(0, 1, t);
                    slot->ShipType = (int16_t)scale(slot->ShipType, surv, total);
                    for (int w = 1; w <= 3; w++)
                        slot->Weapons[w - 1] = (int16_t)scale(slot->Weapons[w - 1], surv, total);
                }
            }
        }
    }

    // Race forces at the site: status 6 fights as attacker, status 2 as defender.
    for (int r = 2; r <= 12; r++) {
        TRace *race = &ds.Races_Races[r - 2];
        int cnt = race->ForceCount;
        for (int f = 1; f <= cnt; f++) {
            if (race->Status == 6) {
                TRaceForce *rf = &race->Forces[f - 1];
                if (rf->System == TARGET_SYSTEM && rf->Body == TARGET_PLANET && rf->Moon == TARGET_MOON
                    && rf->Active == 1)
                    for (int t = 1; t <= 4; t++)
                        rf->Units[t + 3] = (int16_t)scale((uint16_t)rf->Units[t + 3],
                                                          get32((uint16_t)(0x76C6 + 4 * t)), tbl(0, 1, t));
            }
        }
    }
    for (int r = 2; r <= 12; r++) {
        TRace *race = &ds.Races_Races[r - 2];
        int cnt = race->ForceCount;
        for (int f = 1; f <= cnt; f++) {
            if (race->Status == 2) {
                TRaceForce *rf = &race->Forces[f - 1];
                if (rf->System == TARGET_SYSTEM && rf->Body == TARGET_PLANET && rf->Moon == TARGET_MOON
                    && rf->Active == 1)
                    for (int t = 1; t <= 4; t++)
                        rf->Units[t + 3] = (int16_t)scale((uint16_t)rf->Units[t + 3],
                                                          get32((uint16_t)(0x76D6 + 4 * t)), tbl(0, 2, t));
            }
        }
    }

    // The planet's own stores.
    TBody *body = (TBody *)far_host(ds.Game_CurBody);
    int8_t owner = (int8_t)body->Owner;
    if (owner > 1 && *(int8_t *)ds_at((uint16_t)(0x6A06 + owner * 0xE4 + 0x1B)) == 6)
        for (int t = 1; t <= 4; t++)
            body->Stores[t + 3] = scale(body->Stores[t + 3], get32((uint16_t)(0x76C6 + 4 * t)), tbl(0, 1, t));
    owner = (int8_t)body->Owner;
    if (owner > 1 && *(int8_t *)ds_at((uint16_t)(0x6A06 + owner * 0xE4 + 0x1B)) == 2)
        for (int t = 1; t <= 4; t++)
            body->Stores[t + 3] = scale(body->Stores[t + 3], get32((uint16_t)(0x76D6 + 4 * t)), tbl(0, 2, t));
}

// 1537:312c
void GrWar_EndGroundBattle(void)
{
    PString tmp, num;

    System_FreeMem(ds.GrWar_Projectiles, 0x708);
    System_FreeMem(ds.GrWar_BattleGrids, 0xC6);
    System_FreeMem(BACKGROUND, 64000);
    System_FreeMem(UNIT_ICONS, 0x3C00);
    System_FreeMem(EFFECT_ICONS, 0x2800);
    System_FreeMem(WAR_ANIM_BUF, 0x1220);
    Command_FreeGrRoomAnim();
    GrWar_ApplyGroundLosses();
    if (ds.GrWar_PlayerWon == false || RETREATED != 0)
        ds.GrWar_PlayerWon = false;
    else
        ds.GrWar_PlayerWon = true;
    if (ds.GrWar_PlayerWon == false)
        System_StrStore(PSTR("grlost"), &num, 0xFF);
    else
        System_StrStore(PSTR("grvict"), &num, 0xFF);
    LOST_SHOWN = ds.GrWar_PlayerWon == false;
    FarPtr pic = System_GetMem(0xBCC0);
    System_StrLoad(&tmp, PSTR("war\\"));
    System_StrConcat(&tmp, &num);
    Gfx_LoadPic(lit(&tmp), far_host(pic), 1, 0x97, 0, 0, 0x140, true, Gfx_PalettePtr(), 0, 0xB0,
                0x40, 0, 0xBE);
    Blit_CopyRect(far_host(pic), screen_at(0x3D40), 0x140, 0x97);
    System_FreeMem(pic, 0xBCC0);
    LOST_FRAMES = System_GetMem(0x2860);
    if (ds.GrWar_PlayerWon == false)
        Gfx_LoadPic(lit(PSTR("war\\granim")), far_host(LOST_FRAMES), 1, 0x44, 0, 0, 0x98, false,
                    Gfx_PalettePtr(), 0, 0xB0, 0x40, 0, 0xBE);
    LOST_FRAME = 1;
    LOST_TICK = 0;
    GrWar_AnimateResultScreen();
    for (int t = 1; t <= 4; t++) {
        System_StrLong(get32((uint16_t)(0x76E6 + 4 * t)), 4, &num, 0xFF);
        Gfx_DrawTextF9(lit(&num), 0x57, (int16_t)(t * 9 + 0x4F), 4);
    }
    for (int t = 1; t <= 4; t++) {
        System_StrLong(get32((uint16_t)(0x76F6 + 4 * t)), 4, &num, 0xFF);
        Gfx_DrawTextF9(lit(&num), 0x57, (int16_t)(t * 9 + 0x86), 4);
    }
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
    ds.Mouse_HotspotCount = 0;
}

// 1537:3369: every 13th tick shows the next of the three lost-screen frames.
void GrWar_AnimateResultScreen(void)
{
    if (LOST_SHOWN != 0) {
        LOST_TICK = (int16_t)(LOST_TICK % 13 + 1);
        if (LOST_TICK == 1) {
            Mouse_HideCursor();
            Blit_CopyRectStrided(host_add(LOST_FRAMES, (uint16_t)((LOST_FRAME - 1) * 0x33)),
                                 screen_at(0x3F6B), 0x32, 0x3D, 0x98, 0x140);
            Mouse_ShowCursor();
            LOST_FRAME = (int16_t)(LOST_FRAME % 3 + 1);
        }
    }
}

// Variables shared between FindPath and its nested FloodFillStep.
typedef struct PathCtx {
    uint8_t *grid;
    uint8_t *next;     // queue the step appends to (words x, y per entry)
    int16_t count;     // entries in next
    int16_t layer;     // distance label being written
    int16_t a, b;      // the goal
    int16_t goalX, goalY;
    int16_t bestX, bestY, bestDist;
    bool found;
} PathCtx;

// 1537:33da: label a free cell with the current layer and queue it, remember the
// cell closest to the goal, or note that the goal (0FEh) was reached.
static void flood_fill_step(PathCtx *c, int16_t x, int16_t y)
{
    uint8_t *cell = &c->grid[(uint16_t)(x * GRID_STRIDE + y + GRID_ORG)];
    if (*cell == 0xFE) {
        c->found = true;
        c->goalX = x;
        c->goalY = y;
        c->bestDist = 0;
    } else if (*cell == 0) {
        c->count++;
        int16_t *q = (int16_t *)(c->next + c->count * 4 - 4);
        q[0] = x;
        q[1] = y;
        *cell = (uint8_t)c->layer;
        int16_t d = (int16_t)(abs16((int16_t)(c->b - y)) + abs16((int16_t)(c->a - x)));
        if (d < c->bestDist) {
            c->bestDist = d;
            c->bestX = x;
            c->bestY = y;
        }
    }
}

// 1537:34d4
void GrWar_FindPath(int16_t sx, int16_t sy, int16_t a, int16_t b, int16_t c, int16_t d,
                    uint16_t *dir)
{
    FarPtr gridp = System_GetMem(0xC6);
    FarPtr cur_q = System_GetMem(400);
    FarPtr next_q = System_GetMem(400);
    uint8_t *g = (uint8_t *)far_host(gridp);
    const uint8_t *bg = grid();

    for (int x = 0; x <= 15; x++)
        for (int y = 0; y <= 8; y++)
            g[x * GRID_STRIDE + y + GRID_ORG] = bg[x * GRID_STRIDE + y + GRID_ORG] == 0 ? 0 : 0xFF;
    // Wall of 0FDh around the field.
    for (int x = -1; x <= 16; x++)
        g[x * GRID_STRIDE + 0xB] = 0xFD;
    for (int x = -1; x <= 16; x++)
        g[x * GRID_STRIDE + 0x15] = 0xFD;
    for (int y = -1; y <= 9; y++)
        g[y + 1] = 0xFD;
    for (int y = -1; y <= 9; y++)
        g[y + 0xBC] = 0xFD;

    PathCtx ctx = {.grid = g, .layer = 1, .a = a, .b = b};
    g[sx * GRID_STRIDE + sy + GRID_ORG] = 1;
    int16_t *q0 = (int16_t *)far_host(next_q);
    q0[0] = sx;
    q0[1] = sy;
    // The goal cell, if free (the second test reads (a,b) again in the original).
    if (a >= 0 && b >= 0 && a < 16 && b < 9 && g[a * GRID_STRIDE + b + GRID_ORG] == 0)
        g[a * GRID_STRIDE + b + GRID_ORG] = 0xFE;
    // BUG? tests cell (a,b), not (c,d), so (c,d) is only marked if (a,b) was out of range.
    if (c >= 0 && d >= 0 && c < 16 && d < 9 && g[(uint16_t)(a * GRID_STRIDE + b + GRID_ORG)] == 0)
        g[c * GRID_STRIDE + d + GRID_ORG] = 0xFE;

    ctx.count = 1;
    ctx.bestX = -1;
    ctx.bestY = -1;
    ctx.bestDist = 1000;
    ctx.found = false;
    int16_t cx = 0, cy = 0;
    while (ctx.count > 0 && !ctx.found) {
        FarPtr t = cur_q;
        cur_q = next_q;
        next_q = t;
        int16_t n = ctx.count;
        ctx.count = 0;
        ctx.layer++;
        ctx.next = (uint8_t *)far_host(next_q);
        const int16_t *cq = (const int16_t *)far_host(cur_q);
        while (n > 0 && !ctx.found) {
            cx = cq[n * 2 - 2];
            cy = cq[n * 2 - 1];
            n--;
            if (cx > 0)
                flood_fill_step(&ctx, (int16_t)(cx - 1), cy);
            if (cx < 15)
                flood_fill_step(&ctx, (int16_t)(cx + 1), cy);
            if (cy > 0)
                flood_fill_step(&ctx, cx, (int16_t)(cy - 1));
            if (cy < 8)
                flood_fill_step(&ctx, cx, (int16_t)(cy + 1));
        }
    }
    if (!ctx.found && ctx.bestX != -1 && ctx.bestY != -1) {
        ctx.goalX = ctx.bestX;
        ctx.goalY = ctx.bestY;
        ctx.found = true;
    }
    if (ctx.found) {
        cx = ctx.goalX;
        cy = ctx.goalY;
        int label = ctx.layer;
        // Walk back along smaller labels to the neighbour of the start, trying the
        // axis in which the goal is further away first.
#define CELL(px, py) g[(uint16_t)((px) * GRID_STRIDE + (py) + GRID_ORG)]
#define LOWER(px, py) (CELL(px, py) > 0 && CELL(px, py) < label)
        if (abs16((int16_t)(sx - cx)) >= abs16((int16_t)(sy - cy))) {
            while (label > 2) {
                if (LOWER(cx, cy + 1))
                    cy++;
                else if (LOWER(cx, cy - 1))
                    cy--;
                else if (LOWER(cx + 1, cy))
                    cx++;
                else if (LOWER(cx - 1, cy))
                    cx--;
                label = CELL(cx, cy);
            }
        } else {
            while (label > 2) {
                if (LOWER(cx + 1, cy))
                    cx++;
                else if (LOWER(cx - 1, cy))
                    cx--;
                else if (LOWER(cx, cy + 1))
                    cy++;
                else if (LOWER(cx, cy - 1))
                    cy--;
                label = CELL(cx, cy);
            }
        }
#undef LOWER
#undef CELL
    }
    *dir = 0;
    if (sx + 1 == cx)
        *dir = 3;
    if (sy + 1 == cy)
        *dir = 4;
    if (sx - 1 == cx)
        *dir = 1;
    if (sy - 1 == cy)
        *dir = 2;
    if (!ctx.found)
        *dir = 0;
    if (abs16((int16_t)(b - sy)) + abs16((int16_t)(a - sx)) <= ctx.bestDist)
        *dir = 0;
    System_FreeMem(gridp, 0xC6);
    System_FreeMem(cur_q, 400);
    System_FreeMem(next_q, 400);
}

// 1537:3a71
void GrWar_Init(void)
{
    ds.Game_GrWarProc = far_ptr(0x1537, 0x11F2);
}
