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

enum { GRID_STRIDE = 11, GRID_ORG = 0xC, BASE_SLOT = 21 };

static uint8_t *screen_at(uint16_t ofs) { return gfx_screen() + ofs; }
// The original addresses the off-screen field as RenderBuf's segment with its
// own offsets (a whole segment in the port: System_GetSegment).
static uint8_t *render_at(uint16_t ofs)
{
    return ds.GrWar_RenderBuf + ofs;
}
static const uint8_t *lit(const PString *s) { return (const uint8_t *)s; }

static uint8_t *grid(void) { return ds.GrWar_BattleGrids; }
static int slot_count(int side)
{
    return side == 1 ? ds.GrWarUnits_AttackerSlotCount : ds.GrWarUnits_DefenderSlotCount;
}
static int16_t *proj_count(int side) { return &ds.GrWar_ProjectileCount[side - 1]; }

// Slot i (1-based) of side's list. The original adds i * 27h - 27h to the
// list's offset in 16 bits, so i = 0 (an empty grid cell, a unit without a
// target) wraps to the end of the list's segment, not the 39 bytes before it
// (the lists are whole segments: System_GetSegment).
static GrBattleUnit *unit(int side, int i)
{
    return (GrBattleUnit *)(ds.GrWarUnits_UnitLists[side - 1] + (uint16_t)((i - 1) * 0x27));
}

// Tile step of direction 1..4, read for any byte as the original does: a
// wrapped slot (see unit()) has no meaningful dir.
static int16_t dx_of(uint8_t dir) { return (&ds.GrWar_DirX[0])[dir - 1]; }
static int16_t dy_of(uint8_t dir) { return (&ds.GrWar_DirY[0])[dir - 1]; }

// Pixel position of a unit: tile * 16 plus the partial step, with the field's origin.
static uint16_t pos_x(const GrBattleUnit *u)
{
    return (uint16_t)(u->x * 16 + (uint16_t)(u->step * (uint16_t)dx_of(u->dir)) + 0x40);
}
static uint16_t pos_y(const GrBattleUnit *u)
{
    return (uint16_t)(u->y * 16 + (uint16_t)(u->step * (uint16_t)dy_of(u->dir)) + 3);
}

// Units of side and type in the battle (GrWarUnits::Totals).
static int32_t total(int side, int type) { return ds.GrWarUnits_Totals[side - 1][type - 1]; }

// Damage factor of a target's type. The base (type 5) reads one past the
// table, GrWarUnits::SlotCap[1], as the original.
static int16_t vulnerability(uint8_t type)
{
    return type == 5 ? ds.GrWarUnits_SlotCap[0] : ds.GrWar_Vulnerability[type - 1];
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

// Offset of side's base sprite in its 32x32 cell.
static uint8_t base_offset(int side) { return ds.GrWar_BaseOffset[ds.Sim_PlayerAttacks][side - 1]; }

// 1537:0029
void GrWar_EnterGroundBattle(void)
{
    PString num, name;

    Sound_AllocWarSndBuf();
    ds.GrWar_Background = System_GetMem(64000);
    ds.GrWar_UnitIcons = System_GetMem(0x3C00);
    ds.GrWar_EffectIcons = System_GetMem(0x2800);
    uint8_t *side_pic = System_GetMem(0x3200);
    Gfx_LoadPic(lit(PSTR("war\\grwar")), side_pic, 1, 200, 0, 0, 0x40, true,
                Gfx_PalettePtr(), 0, 0xB0, 0x40, 0, 0xBE);
    const TBody *body = ds.Game_CurBody;
    System_StrLong((int8_t)body->SurfaceType, 0, &num, 0xFF);
    System_StrLoad(&name, PSTR("war\\grf"));
    System_StrConcat(&name, &num);
    Gfx_LoadPic(lit(&name), ds.GrWar_Background, 1, 200, 0, 0, 0x140, false,
                Gfx_PalettePtr(), 0, 0x3F, 0xB0, 0, 0x3F);
    Blit_CopyRectStrided(side_pic, ds.GrWar_Background, 0x40, 200, 0x40, 0x140);
    Blit_CopyRect(ds.GrWar_Background + 0x3D40, screen_at(0x3D40), 0x140, 0x97);
    Gfx_LoadPic(lit(PSTR("war\\gricon")), ds.GrWar_UnitIcons, 1, 0x30, 0, 0, 0x140, false,
                Gfx_PalettePtr(), 0, 0x6F, 0x40, 0, 0x6F);
    Gfx_LoadPic(lit(PSTR("war\\gricon2")), ds.GrWar_EffectIcons, 1, 0x20, 0, 0, 0x140,
                false, Gfx_PalettePtr(), 0, 0x6F, 0x40, 0, 0x6F);
    GrWar_BattleRound();
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
    System_FreeMem(side_pic, 0x3200);
    ds.Command_WarAnimBuffer = System_GetMem(0x1220);
    Command_InitRoomAnim();
    int16_t anim = GrWar_PickBattleAnim(); // (*Game::GrWarProc)()
    Command_StartMainAnim(anim);
    ds.Command_AnimFrame = 0;
    ds.SpWar_Retreated = 0;
    if (ds.GrWar_BattleOver != 0)
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
    ds.GrWar_BattleOver = 0;
    ds.GrWar_Spare = 0;

    // Drop empty attacker slots.
    int i = 1;
    while (i <= ds.GrWarUnits_AttackerSlotCount) {
        if (unit(1, i)->count == 0) {
            int last = ds.GrWarUnits_AttackerSlotCount - 1;
            for (int j = i; j <= last; j++)
                memcpy(unit(1, j), unit(1, j + 1), 0x27);
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
    unit(2, BASE_SLOT)->x = ds.Sim_PlayerAttacks == 0 ? 0xF : 0xE;
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

    if (ds.GrWar_Selected == 0) {
        Gfx_DrawTextFC(lit(PSTR("")), 5, 0x88, 8);
        Gfx_DrawTextF9(lit(PSTR("")), 5, 0x92, 9);
        Gfx_DrawTextF9(lit(PSTR("")), 5, 0x9C, 9);
        return;
    }
    int side = ds.GrWar_SelectedOwn != 0 ? 1 : 2;
    GrBattleUnit *u = unit(side, ds.GrWar_Selected);
    int32_t tot, atk, strength;
    if (u->type == 5) {
        // The base reads one past its side's row, as the original: the
        // defenders' type 1, or for the defenders the next table's first entry.
        tot = side == 1 ? ds.GrWarUnits_Totals[1][0] : ds.GrWarUnits_Pool[0];
        atk = side == 1 ? ds.GrWarUnits_Attack[1][0] : ds.GrWarUnits_Defence[0][0];
    } else {
        tot = total(side, u->type);
        atk = ds.GrWarUnits_Attack[side - 1][u->type - 1];
    }
    if (tot > 0) {
        int32_t q = long_mul(u->count, atk);
        strength = long_div(q, tot);
    } else
        strength = 0;
    System_StrStore((const PString *)&ds.Names_GroundUnits[u->type - 1], &s, 0x32);
    if (u->type == 5 && (ds.Sim_PlayerAttacks ^ ds.GrWar_SelectedOwn) != 0)
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
        ds.GrWar_UnitIcons + (uint16_t)((sheet - 1) * 0x1400 + (frame - 1) * 0x10);
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
    const uint8_t *src = ds.GrWar_EffectIcons
                         + (uint16_t)((sheet - 1) * 0x1400 + (frame - 1) * 0x10
                                      + (variant - 1) * 0x1400);
    Blit_BlitTransparent(src, render_at((uint16_t)(x + 0x10 + y * 0x140)), 0x10, 0x10, 0x40);
}

// 1537:09c2: 11x10 projectile sprite (kind 0..15) below the effect sheets.
void GrWar_DrawProjectile(int16_t x, int16_t y, int16_t kind)
{
    const uint8_t *src = ds.GrWar_EffectIcons + (uint16_t)(0x1400 + kind * 11 + 1);
    Blit_BlitTransparent(src, render_at((uint16_t)(x + 0x10 + y * 0x140)), 0xB, 10, 0x40);
}

// 1537:0a0c
void GrWar_DrawSelectionMarkers(void)
{
    if (ds.GrWar_Selected == 0)
        return;
    if (ds.GrWar_SelectedOwn == 0) {
        const GrBattleUnit *u = unit(2, ds.GrWar_Selected);
        uint16_t off = ds.GrWar_Selected == BASE_SLOT ? base_offset(2) : 0;
        GrWar_DrawEffectSprite(0xF, 2, (int16_t)(pos_x(u) + off), (int16_t)(pos_y(u) + off), 1);
    } else {
        const GrBattleUnit *u = unit(1, ds.GrWar_Selected);
        uint16_t off = ds.GrWar_Selected == BASE_SLOT ? base_offset(1) : 0;
        GrWar_DrawEffectSprite(0xF, 1, (int16_t)(pos_x(u) + off), (int16_t)(pos_y(u) + off), 1);
        if (u->mode == 2)
            GrWar_DrawEffectSprite(0x10, 2, (int16_t)(u->target * 0x10 + 0x40),
                                   (int16_t)(u->targetY * 0x10 + 3), 1);
        if (u->mode == 3) {
            const GrBattleUnit *t = unit(2, u->target);
            off = u->target == BASE_SLOT ? base_offset(2) : 0;
            GrWar_DrawEffectSprite(0x10, 1, (int16_t)(pos_x(t) + off), (int16_t)(pos_y(t) + off), 1);
        }
    }
}

// 1537:0ca0: the bases are drawn into the field and marked in the grid.
void GrWar_DrawBaseSprites(void)
{
    if (ds.Sim_PlayerAttacks == 0) {
        Blit_CopyRect(ds.GrWar_Background + 0x40, render_at(0x5410), 0x20, 0x20);
        Blit_BlitTransparent(ds.GrWar_Background + 0x29C4, render_at(0x5500), 0x10, 0x10, 0xB0);
    } else {
        Blit_BlitTransparent(ds.GrWar_Background + 0x2980, render_at(0x5410), 0x10, 0x10, 0xB0);
        Blit_BlitTransparent(ds.GrWar_Background + 0xC0, render_at(0x54F0), 0x20, 0x20, 0xB0);
    }
    uint8_t *g = grid();
    g[0x10] = 0x15;
    g[0xB5] = 0x79;
    if (ds.Sim_PlayerAttacks == 0) {
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
    if ((uint8_t)ds.GrWar_Order != 2) { // 2 = paused (the low byte of the word)
        GrWar_BattleRound();
        ds.SpWar_Retreated = 0;
        if (ds.GrWar_BattleOver != 0)
            GrWar_EndGroundBattle();
    }
}

// Effect records (GrWar::Effects): first frame, sheet, frame count.
static int16_t fx_first(uint8_t e) { return ds.GrWar_Effects[e - 1].First; }
static int16_t fx_sheet(uint8_t e) { return ds.GrWar_Effects[e - 1].Sheet; }
static int16_t fx_frames(uint8_t e) { return ds.GrWar_Effects[e - 1].Frames; }

// 1537:0da8
void GrWar_RenderBattle(void)
{
    GrWar_DrawSelectedUnitInfo();
    ds.GrWar_RenderBuf = System_GetSegment();  // GetMem(0xBCD0)
    Blit_CopyRect(ds.GrWar_Background + 0x3D80, render_at(0x50), 0x100, 0x97);
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
            if (ds.GrWar_Selected == i
                && ((s == 1 && ds.GrWar_SelectedOwn != 0) || (s == 2 && ds.GrWar_SelectedOwn == 0))
                && u->count == 0)
                ds.GrWar_Selected = 0;
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
            uint16_t off = base_offset(s);
            GrWar_DrawEffectSprite((int16_t)(fx_first(b->effect) + fx_frames(b->effect) - b->effTimer),
                                   fx_sheet(b->effect), (int16_t)(b->x * 0x10 + 0x40 + off),
                                   (int16_t)(b->y * 0x10 + 3 + off), 1);
        }
    }
    GrWar_DrawSelectionMarkers();

    for (int s = 1; s <= 2; s++) {
        int n = *proj_count(s);
        for (int k = 1; k <= n; k++) {
            const int16_t *p =
                (const int16_t *)(ds.GrWar_Projectiles + (s - 1) * 900 + (k - 1) * 0x12);
            // x0 y0 x1 y1 step total sprite
            int16_t px = (int16_t)(idiv16((int16_t)((int16_t)(p[2] - p[0]) * p[4]), p[5]) + p[0]);
            int16_t py = (int16_t)(idiv16((int16_t)((int16_t)(p[3] - p[1]) * p[4]), p[5]) + p[1]);
            GrWar_DrawProjectile(px, py, p[6]);
        }
    }
    Blit_CopyRect(render_at(0x50), screen_at(0x3D80), 0x100, 0x97);
    Command_TickGrAnim();
    System_FreeSegment(ds.GrWar_RenderBuf);
}

// 1537:11f2: war animation 1..14 whose two unit types are both on the field (Game::GrWarProc).
int16_t GrWar_PickBattleAnim(void)
{
    for (;;) {
        uint16_t r = System_Random(0xE);
        uint8_t a = ds.Cc_Rooms[r].UnitA;
        uint8_t b = ds.Cc_Rooms[r].UnitB;
        bool ok = (total(1, a) > 0 || total(2, a) > 0) && (total(1, b) > 0 || total(2, b) > 0);
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
    u->effTimer = (uint8_t)ds.GrWar_Effects[effect - 1].Frames;
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
            uint16_t off = base_offset(s);
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
                            int32_t q =
                                long_mul(u->count, ds.GrWarUnits_Attack[e - 1][u->type - 1]);
                            q = long_div(q, total(e, u->type));
                            q = long_mul(vulnerability(t->type), q);
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
                            u->cooldown = ds.GrWar_Cooldown[u->type - 1];
                        } else {
                            // Artillery out of melee range fires a projectile.
                            int32_t dmg = long_div(
                                long_mul(u->count, ds.GrWarUnits_Defence[e - 1][u->type - 1]),
                                total(e, u->type));
                            sound = 5;
                            GrWar_StartUnitEffect((int16_t)e, (int16_t)i, 5);
                            int16_t *cnt = proj_count(e);
                            (*cnt)++;
                            int16_t *p = (int16_t *)(ds.GrWar_Projectiles + (e - 1) * 900
                                                     + (*cnt - 1) * 0x12);
                            p[0] = (int16_t)pos_x(u);
                            p[1] = (int16_t)pos_y(u);
                            p[2] = (int16_t)pos_x(t);
                            p[3] = (int16_t)pos_y(t);
                            p[4] = 1;
                            p[5] = (int16_t)((uint16_t)GrWar_DistEuclidTiles((int16_t)i, tgt, (int16_t)e) << 3);
                            p[6] = GrWar_DirectionToward16(u->x, u->y, t->x, t->y);
                            memcpy(&p[7], &dmg, 4);
                            u->cooldown = ds.GrWar_Cooldown[u->type - 1];
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
                            ds.GrWar_PathDir = 0;
                        else {
                            int cnt = u->pathLen - 1;
                            for (int m = 1; m <= cnt; m++)
                                u->path[m - 1] = u->path[m];
                            u->pathLen--;
                            ds.GrWar_PathDir = u->pathLen == 0 ? 0 : u->path[0];
                        }
                    }
                    if (u->mode == 2) {
                        GrWar_FindPath(u->x, u->y, u->target, u->targetY, u->target, u->targetY,
                                       (uint16_t *)&ds.GrWar_PathDir);
                        if (u->x == u->target && u->y == u->targetY)
                            u->mode = 1;
                    }
                    if (u->mode == 3) {
                        const GrBattleUnit *t = unit(3 - e, u->target);
                        hit = (GrBattleUnit *)t;
                        GrWar_FindPath(u->x, u->y, t->x, t->y, t->x, t->y,
                                       (uint16_t *)&ds.GrWar_PathDir);
                        uint8_t saved = u->step;
                        if (u->step != 0)
                            u->step--;
                        if (GrWar_InWeaponRange(u->type, (int16_t)i, u->target, (int16_t)e))
                            u->moving = 0;
                        u->step = saved;
                    }
                    if (ds.GrWar_PathDir < 1)
                        u->moving = 0;
                    else
                        u->dir = (uint8_t)ds.GrWar_PathDir;
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
                u->moveDelay = (uint8_t)ds.GrWar_MoveDelay[u->type - 1];
            }
        }
    }

    // Projectiles in flight.
    for (int s = 1; s <= 2; s++) {
        int k = 1;
        while (k <= *proj_count(s)) {
            uint8_t *pb = ds.GrWar_Projectiles + (s - 1) * 900;
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
                    int32_t q = long_mul(vulnerability(hit->type), pd);
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
                    memcpy(pb + (m - 1) * 0x12, pb + m * 0x12, 0x12);
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
    uint8_t was_over = ds.GrWar_BattleOver;

    bool attackers_dead = true;
    for (int i = 1; i <= ds.GrWarUnits_AttackerSlotCount; i++)
        if (unit(1, i)->count > 0)
            attackers_dead = false;
    bool defenders_dead = true;
    for (int i = 1; i <= ds.GrWarUnits_DefenderSlotCount; i++)
        if (unit(2, i)->count > 0)
            defenders_dead = false;
    if (ds.Game_BattleFlag != false) {
        ds.GrWar_BattleOver = 1;
        ds.GrWar_PlayerWon = true;
    }
    if (attackers_dead || defenders_dead) {
        ds.GrWar_BattleOver = 1;
        ds.GrWar_PlayerWon = defenders_dead;
    }
    if (ds.GrWar_BattleOver != 0) {
        // Wait for the death animations and the projectiles to finish.
        for (int s = 1; s <= 2; s++) {
            int n = slot_count(s);
            for (int i = 1; i <= n; i++)
                if (unit(s, i)->count == 0 && unit(s, i)->effTimer != 0)
                    ds.GrWar_BattleOver = 0;
            if (*proj_count(s) != 0)
                ds.GrWar_BattleOver = 0;
        }
    }
    GrWar_RenderBattle();
    if (was_over == 0 && ds.GrWar_BattleOver != 0)
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
    // Survivors per side and type.
    for (int s = 1; s <= 2; s++)
        for (int t = 1; t <= 4; t++)
            ds.GrWar_Survivors[s - 1][t - 1] = 0;
    for (int i = 1; i <= ds.GrWarUnits_AttackerSlotCount; i++) {
        const GrBattleUnit *u = unit(1, i);
        ds.GrWar_Survivors[0][u->type - 1] =
            (int32_t)((uint32_t)ds.GrWar_Survivors[0][u->type - 1] + (uint32_t)(int32_t)u->count);
    }
    for (int i = 1; i <= ds.GrWarUnits_DefenderSlotCount; i++) {
        const GrBattleUnit *u = unit(2, i);
        ds.GrWar_Survivors[1][u->type - 1] =
            (int32_t)((uint32_t)ds.GrWar_Survivors[1][u->type - 1] + (uint32_t)(int32_t)u->count);
    }
    for (int t = 1; t <= 4; t++) // units of the attackers not assigned to a slot survive too
        ds.GrWar_Survivors[0][t - 1] =
            (int32_t)((uint32_t)ds.GrWar_Survivors[0][t - 1] + (uint32_t)ds.GrWarUnits_Pool[t - 1]);
    // Losses = totals - survivors.
    for (int s = 1; s <= 2; s++)
        for (int t = 1; t <= 4; t++)
            ds.GrWar_Losses[s - 1][t - 1] =
                (int32_t)((uint32_t)total(s, t) - (uint32_t)ds.GrWar_Survivors[s - 1][t - 1]);

    // Fleets (list 0) and planet forces (list 1) at the battle site.
    int16_t counts[2] = {ds.Game_GroupCount, ds.Game_PlanetForceCount};
    for (int l = 0; l <= 1; l++) {
        for (int g = 1; g <= counts[l]; g++) {
            TGroup *grp = &ds.Game_Groups[l][g - 1];
            if (grp->System == ds.SpWar_BattleSystem && grp->Planet == ds.SpWar_BattlePlanet
                && grp->Moon == ds.SpWar_BattleMoon
                && (grp->Status == 1 || grp->Status == 2 || grp->Status == 7)
                && (grp->Kind == 1 || (grp->Kind == 5 && grp->Moon == ds.SpWar_BattleMoon))) {
                for (int t = 1; t <= 4; t++) {
                    TGroupSlot *slot = &grp->Units[1][t - 1];
                    int32_t surv = ds.GrWar_Survivors[0][t - 1], all = total(1, t);
                    slot->ShipType = (int16_t)scale(slot->ShipType, surv, all);
                    for (int w = 1; w <= 3; w++)
                        slot->Weapons[w - 1] = (int16_t)scale(slot->Weapons[w - 1], surv, all);
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
                if (rf->System == ds.SpWar_BattleSystem && rf->Body == ds.SpWar_BattlePlanet
                    && rf->Moon == ds.SpWar_BattleMoon && rf->Active == 1)
                    for (int t = 1; t <= 4; t++)
                        rf->Units[t + 3] = (int16_t)scale((uint16_t)rf->Units[t + 3],
                                                          ds.GrWar_Survivors[0][t - 1],
                                                          total(1, t));
            }
        }
    }
    for (int r = 2; r <= 12; r++) {
        TRace *race = &ds.Races_Races[r - 2];
        int cnt = race->ForceCount;
        for (int f = 1; f <= cnt; f++) {
            if (race->Status == 2) {
                TRaceForce *rf = &race->Forces[f - 1];
                if (rf->System == ds.SpWar_BattleSystem && rf->Body == ds.SpWar_BattlePlanet
                    && rf->Moon == ds.SpWar_BattleMoon && rf->Active == 1)
                    for (int t = 1; t <= 4; t++)
                        rf->Units[t + 3] = (int16_t)scale((uint16_t)rf->Units[t + 3],
                                                          ds.GrWar_Survivors[1][t - 1],
                                                          total(2, t));
            }
        }
    }

    // The planet's own stores.
    TBody *body = ds.Game_CurBody;
    int8_t owner = (int8_t)body->Owner;
    if (owner > 1 && ds.Races_Races[owner - 2].Status == 6)
        for (int t = 1; t <= 4; t++)
            body->Stores[t + 3] =
                scale(body->Stores[t + 3], ds.GrWar_Survivors[0][t - 1], total(1, t));
    owner = (int8_t)body->Owner;
    if (owner > 1 && ds.Races_Races[owner - 2].Status == 2)
        for (int t = 1; t <= 4; t++)
            body->Stores[t + 3] =
                scale(body->Stores[t + 3], ds.GrWar_Survivors[1][t - 1], total(2, t));
}

// 1537:312c
void GrWar_EndGroundBattle(void)
{
    PString tmp, num;

    System_FreeMem(ds.GrWar_Projectiles, 0x708);
    System_FreeMem(ds.GrWar_BattleGrids, 0xC6);
    System_FreeMem(ds.GrWar_Background, 64000);
    System_FreeMem(ds.GrWar_UnitIcons, 0x3C00);
    System_FreeMem(ds.GrWar_EffectIcons, 0x2800);
    System_FreeMem(ds.Command_WarAnimBuffer, 0x1220);
    Command_FreeGrRoomAnim();
    GrWar_ApplyGroundLosses();
    if (ds.GrWar_PlayerWon == false || ds.SpWar_Retreated != 0)
        ds.GrWar_PlayerWon = false;
    else
        ds.GrWar_PlayerWon = true;
    if (ds.GrWar_PlayerWon == false)
        System_StrStore(PSTR("grlost"), &num, 0xFF);
    else
        System_StrStore(PSTR("grvict"), &num, 0xFF);
    ds.GrWar_LostShown = ds.GrWar_PlayerWon == false;
    uint8_t *pic = System_GetMem(0xBCC0);
    System_StrLoad(&tmp, PSTR("war\\"));
    System_StrConcat(&tmp, &num);
    Gfx_LoadPic(lit(&tmp), pic, 1, 0x97, 0, 0, 0x140, true, Gfx_PalettePtr(), 0, 0xB0,
                0x40, 0, 0xBE);
    Blit_CopyRect(pic, screen_at(0x3D40), 0x140, 0x97);
    System_FreeMem(pic, 0xBCC0);
    ds.GrWar_LostFrames = System_GetMem(0x2860);
    if (ds.GrWar_PlayerWon == false)
        Gfx_LoadPic(lit(PSTR("war\\granim")), ds.GrWar_LostFrames, 1, 0x44, 0, 0, 0x98,
                    false, Gfx_PalettePtr(), 0, 0xB0, 0x40, 0, 0xBE);
    ds.GrWar_LostFrame = 1;
    ds.GrWar_LostTick = 0;
    GrWar_AnimateResultScreen();
    for (int t = 1; t <= 4; t++) {
        System_StrLong(ds.GrWar_Losses[0][t - 1], 4, &num, 0xFF);
        Gfx_DrawTextF9(lit(&num), 0x57, (int16_t)(t * 9 + 0x4F), 4);
    }
    for (int t = 1; t <= 4; t++) {
        System_StrLong(ds.GrWar_Losses[1][t - 1], 4, &num, 0xFF);
        Gfx_DrawTextF9(lit(&num), 0x57, (int16_t)(t * 9 + 0x86), 4);
    }
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
    ds.Mouse_HotspotCount = 0;
}

// 1537:3369: every 13th tick shows the next of the three lost-screen frames.
void GrWar_AnimateResultScreen(void)
{
    if (ds.GrWar_LostShown != 0) {
        ds.GrWar_LostTick = (int16_t)(ds.GrWar_LostTick % 13 + 1);
        if (ds.GrWar_LostTick == 1) {
            Mouse_HideCursor();
            uint16_t ofs = (uint16_t)((ds.GrWar_LostFrame - 1) * 0x33);
            Blit_CopyRectStrided(ds.GrWar_LostFrames + ofs, screen_at(0x3F6B), 0x32, 0x3D,
                                 0x98, 0x140);
            Mouse_ShowCursor();
            ds.GrWar_LostFrame = (int16_t)(ds.GrWar_LostFrame % 3 + 1);
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
    uint8_t *g = System_GetMem(0xC6);
    int16_t *cur_q = System_GetMem(400);
    int16_t *next_q = System_GetMem(400);
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
    next_q[0] = sx;
    next_q[1] = sy;
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
        int16_t *t = cur_q;
        cur_q = next_q;
        next_q = t;
        int16_t n = ctx.count;
        ctx.count = 0;
        ctx.layer++;
        ctx.next = (uint8_t *)next_q;
        const int16_t *cq = cur_q;
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
    System_FreeMem(g, 0xC6);
    System_FreeMem(cur_q, 400);
    System_FreeMem(next_q, 400);
}

// 1537:3a71
void GrWar_Init(void)
{
    ds.Game_GrWarProc = GrWar_PickBattleAnim;
}
