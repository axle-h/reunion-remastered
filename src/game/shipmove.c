// Unit ShipMove (22ca): see shipmove.h.
#include "game/shipmove.h"

#include <stdio.h>
#include <string.h>

#include "game/blit.h"
#include "game/dseg.h"
#include "game/gfx.h"
#include "game/groups.h"
#include "game/mouse.h"
#include "game/satellite.h"
#include "game/sound.h"
#include "game/system.h"

// Routines of units that are not ported yet (stubs in stubs.c).
void PlanetInfo_SelectCurBody(void);                                             // 357b:3253
void Message_ShowMessageLine(int16_t lineNo);                                    // 34b0:011a
void Message_ShowMessageNoLog(const uint8_t *text);                              // 34b0:0197
void Intel_OfferInventionForAnalysis(int16_t idea);                              // 29b9:1409
void Intel_ScheduleIdea(int16_t idea, int16_t delayBase, int16_t delayRandom);   // 29b9:136e
void Intel_SetGroupTarget(int16_t group, int16_t system, int16_t planet, int16_t moon); // 29b9:0e66
void AlienTalk_StartAlienScene(int16_t scene);                                   // 2e4b:0000

#define PS(lit) ((const uint8_t *)PSTR(lit))

// Far pointer plus a (wrapping) 16-bit offset, as the original computes them.
static FarPtr far_add(FarPtr p, int32_t delta)
{
    return far_ptr((uint16_t)(p >> 16), (uint16_t)((uint16_t)p + delta));
}

// Host pointer into a heap block, for the blitters (the offsets stay inside
// the block, so the original's 16-bit wrap never comes into play).
static uint8_t *buf_at(FarPtr p, int32_t delta) { return (uint8_t *)far_host(p) + delta; }

static TBody *bodyp(FarPtr p) { return (TBody *)far_host(p); }

// Research::Products[p].State: 0 while the invention is unknown, so the landing
// events below happen once.
static int16_t product_state(int16_t p) { return ds.Research_Products[p - 1].State; }

// Pascal string "planets\nagy<n>" in buf.
static const uint8_t *nagy_name(uint8_t buf[256], int16_t n)
{
    int len = snprintf((char *)buf + 1, 255, "planets\\nagy%d", n);
    buf[0] = (uint8_t)len;
    return buf;
}

static void load_view_pic(const uint8_t *name, FarPtr dest)
{
    Gfx_LoadPic(name, far_host(dest), 0, 0x97, 0, 0, 0x140, false, Gfx_PalettePtr(), 0, 0x6f, 0x80,
                0, 0x6f);
}

static void set_caption(int16_t pascalIndex, const char *text)
{
    THotspot *h = Mouse_Hotspot(pascalIndex);
    uint8_t n = (uint8_t)strlen(text);
    h->Caption.len = n > 18 ? 18 : n;
    memcpy(h->Caption.s, text, h->Caption.len);
}

static void set_rect(int16_t pascalIndex, int16_t x, uint8_t y, int16_t w, uint8_t hgt)
{
    THotspot *h = Mouse_Hotspot(pascalIndex);
    h->X = x;
    h->Y = y;
    h->W = w;
    h->H = hgt;
    h->_1A = 0;
}

// 22ca:0105
void ShipMove_GroupScreen(void)
{
    TGroup *g = Groups_Selected();
    int16_t surf;

    if (ds.Game_ScreenEntered) {
        if (ds.Game_PrevScreen == 0x12 && ds.Launch_DestinationActive == 0)
            Intel_SetGroupTarget(ds.Game_SelGroup, ds.Game_CurSystem,
                                 ds.Game_CurPlanet[ds.Game_CurSystem - 1],
                                 ds.Game_CurMoon[ds.Game_CurSystem - 1]);
        if (g->Status == 1 || g->Status == 2) {
            ds.Game_CurSystem = g->System;
            ds.Game_CurPlanet[ds.Game_CurSystem - 1] = g->Planet;
            ds.Game_CurMoon[ds.Game_CurSystem - 1] = g->Moon;
            PlanetInfo_SelectCurBody();
        }
        ds.ShipMove_InRestrictedArea = 0;
        if (g->System == 1 && g->Planet == 7 && g->Moon == 0
            && bodyp(ds.Galaxy_Bodies[0])[6].Owner != 1 && bodyp(ds.Galaxy_Bodies[0])[6].Owner != 2)
            ds.ShipMove_InRestrictedArea = 1;
        Satellite_SetupGroupScreenMenu();
        surf = bodyp(ds.Game_CurBody)->SurfaceType;
        if (ds.ShipMove_InRestrictedArea)
            surf = 12;
        if (g->Status == 1)
            ShipMove_InitDockView(surf);
        else
            ShipMove_InitOrbitView();
        ShipMove_InitStarfield();
        ShipMove_SetupGroupScreenHotspots();
    }

    // Every pass: CurBody := the group's body.
    if (g->Moon == 0) {
        ds.Game_CurBody = far_add(ds.Galaxy_Bodies[g->System - 1], g->Planet * 0x41 - 0x41);
    } else {
        uint8_t idx = *((uint8_t *)far_host(ds.Galaxy_MoonLists[g->System - 1]) + g->Planet * 9
                        + g->Moon - 9);
        ds.Game_CurBody = far_add(ds.Galaxy_Bodies[g->System - 1], idx * 0x41 - 0x41);
    }
    Satellite_HandleBodyActions();

    if (ds.Game_HotspotHit == 0x15) {
        if (g->Status == 1 || g->Status == 2) {
            Sound_PlayButtonSfx(PS("surface"));
            ds.Game_ScreenSwitch = 1;
            ds.Game_CurrentScreen = 0x14;
        }
    }

    if (ds.Game_HotspotHit == 0x16 && (g->Status == 1 || g->Status == 2)) {
        // Launch / Docking
        TBody *body = bodyp(ds.Game_CurBody);
        // One expression: a PS() literal inside an if branch would end with
        // the branch (docs/port-bugs.md #32).
        const uint8_t *msg =
            body->SurfaceType == 2
                ? PS(" You cannot land on this type of planet, |          since it is gaseous!")
            : g->Kind == 4 && (g->System != 1 || g->Planet != 5 || g->Moon != 0)
                ? PS(" The satellite carriers can only land |          on your main planet")
            : Groups_IsGroupEmpty(ds.Game_GroupList, ds.Game_SelGroup)
                ? PS(" No ships in this unit ! ")
                : nullptr;
        if (msg) {
            Message_ShowMessageNoLog(msg);
        } else {
            body = bodyp(ds.Game_CurBody);
            if (body->InfoLevel < 6)
                body->InfoLevel = 6;
            g->Status = (uint8_t)(3 - g->Status);
            ds.Game_HotspotHit = 0;
            Gfx_DrawStatusText(PS(""));
            surf = bodyp(ds.Game_CurBody)->SurfaceType;
            if (ds.ShipMove_InRestrictedArea)
                surf = 12;
            if (g->System == 1 && g->Planet == 7 && g->Moon == 0
                && bodyp(ds.Galaxy_Bodies[0])[6].Owner != 2 && product_state(12) == 0) {
                Message_ShowMessageLine(0xD);
                Intel_OfferInventionForAnalysis(0xC);
            }
            if (g->System == 3 && g->Planet == 2 && g->Moon == 1 && product_state(22) == 0) {
                Message_ShowMessageLine(0x15);
                Intel_OfferInventionForAnalysis(0x16);
                Intel_OfferInventionForAnalysis(0x17);
                AlienTalk_StartAlienScene(4);
            }
            if (g->System == 7 && g->Planet == 1 && g->Moon == 0 && product_state(31) == 0) {
                Message_ShowMessageLine(0x1F);
                Intel_OfferInventionForAnalysis(0x1F);
                Intel_OfferInventionForAnalysis(0x20);
                Intel_ScheduleIdea(0x22, 0x64, 0x14);
                AlienTalk_StartAlienScene(6);
            }
            if (g->Status == 1)
                ShipMove_DockingAnimation(surf);
            else
                ShipMove_LaunchAnimation(surf);
            ShipMove_SetupGroupScreenHotspots();
        }
    }

    if (ds.Game_HotspotHit == 0x17) {
        Sound_PlayButtonSfx(PS("transfer"));
        ds.Game_ScreenSwitch = 1;
        ds.Game_CurrentScreen = 0xD;
    }

    if (ds.Game_HotspotHit == 0x18 && g->Status > 1) {
        // Move / Change destination
        if (ds.Game_StaffExp[0] > 0 || g->Kind == 4) {
            if (Groups_IsGroupEmpty(ds.Game_GroupList, ds.Game_SelGroup)) {
                Message_ShowMessageNoLog(PS(" No ships in this unit! "));
            } else {
                Sound_PlayButtonSfx(PS("controll"));
                for (int16_t i = 1; i <= 4; i++) {
                    ShipMove_DrawButtonAnimFrame(i);
                    Gfx_WaitFrames(3);
                }
                ds.Game_CurSystem = g->System;
                ds.Game_CurPlanet[ds.Game_CurSystem - 1] = g->Planet;
                ds.Game_CurMoon[ds.Game_CurSystem - 1] = 0;
                ds.Game_ScreenSwitch = 1;
                ds.Game_CurrentScreen = 0x12;
            }
        } else {
            for (int16_t i = 1; i <= 4; i++) {
                ShipMove_DrawButtonAnimFrame(i);
                Gfx_WaitFrames(3);
            }
            Message_ShowMessageNoLog(PS(" You haven't got a pilot! "));
            for (int16_t i = 4; i >= 1; i--) {
                ShipMove_DrawButtonAnimFrame(i);
                Gfx_WaitFrames(3);
            }
        }
    }

    if (ds.Game_HotspotHit == 0x19) {
        Sound_PlayButtonSfx(PS("baseeff"));
        ds.Game_ScreenSwitch = 1;
        ds.Game_CurrentScreen = 0x10;
    }
    if (ds.Game_HotspotHit == 0x1A) {
        Sound_PlayButtonSfx(PS("group"));
        ds.Game_ScreenSwitch = 1;
        ds.Game_CurrentScreen = 0x16;
    }
    if (ds.Game_MenuCommand == 0x2E) {
        ds.Game_CurSystem = g->System;
        ds.Game_CurPlanet[ds.Game_CurSystem - 1] = g->Planet;
        ds.Game_CurMoon[ds.Game_CurSystem - 1] = g->Moon;
        ds.Game_ScreenSwitch = 1;
        ds.Game_CurrentScreen = 7;
    }
}

// 22ca:065e: Pascal hotspot k = Mouse_Hotspot(k); the decompile counts from 0.
void ShipMove_SetupGroupScreenHotspots(void)
{
    TGroup *g = Groups_Selected();
    ds.Mouse_HotspotCount = 6;

    set_rect(1, 0, 0x31, 0x140, 0x57);
    set_caption(1, "Planet main");
    Mouse_Hotspot(1)->Repeat_ = 1;
    if (g->Status > 2)
        Mouse_ClearHotspots(1, 1);

    set_rect(2, 0, 0x95, 0x47, 0x33);
    switch (g->Status) {
    case 6: case 5: case 4: set_caption(2, "No effect"); break;
    case 2: set_caption(2, "Docking"); break;
    case 1: set_caption(2, "Launch"); break;
    default: break; // caption keeps its old value
    }
    Mouse_Hotspot(2)->Repeat_ = 1;

    set_rect(3, 0x4D, 0x8D, 0x2A, 0x28);
    set_caption(3, "Transfer");
    Mouse_Hotspot(3)->Repeat_ = 1;
    if (g->Kind != 2 && g->Kind != 3)
        Mouse_ClearHotspots(3, 3);

    set_rect(4, 0x83, 0x89, 0x3B, 0x3F);
    switch (g->Status) {
    case 6: case 5: case 4: set_caption(4, "Change destination"); break;
    case 2: set_caption(4, "Move"); break;
    case 1: set_caption(4, "No effect"); break;
    default: break;
    }
    Mouse_Hotspot(4)->Repeat_ = 1;

    set_rect(5, 0xCA, 0x91, 0x29, 0x27);
    set_caption(5, "Ships");
    Mouse_Hotspot(5)->Repeat_ = 1;

    set_rect(6, 0xFF, 0x99, 0x41, 0x2F);
    set_caption(6, "Group");
    Mouse_Hotspot(6)->Repeat_ = 1;
}

// Shared tail of the two view initialisers: animation buffers.
static void alloc_anim_buffers(void)
{
    ds.ShipMove_AnimSheet = System_GetMem(0x9100);
    ds.ShipMove_Backdrop = System_GetMem(0xBCC0);
    Gfx_LoadPic(PS("planets\\muszianm"), far_host(ds.ShipMove_AnimSheet), 0, 0x74, 0, 0, 0x140,
                false, Gfx_PalettePtr(), 0, 0x3F, 0x40, 0, 0x3F);
}

// 22ca:08d4
void ShipMove_InitOrbitView(void)
{
    FarPtr dest = System_GetMem(0xBCC0);
    Gfx_LoadPicScreen(PS("planets\\muszi"), far_host(dest), 0x97, Gfx_PalettePtr());
    Blit_CopyRect(far_host(dest), gfx_screen() + 0x3D40, 0x140, 0x97);
    load_view_pic(PS("planets\\nagy0"), dest);
    Mouse_HideCursor();
    ShipMove_BlitViewport(far_host(dest));
    Mouse_ShowCursor();
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
    System_FreeMem(dest, 0xBCC0);
    alloc_anim_buffers();
}

// 22ca:09f9
void ShipMove_InitDockView(int16_t surfaceType)
{
    uint8_t name[256];
    FarPtr dest = System_GetMem(0xBCC0);
    Gfx_LoadPicScreen(PS("planets\\muszi"), far_host(dest), 0x97, Gfx_PalettePtr());
    Blit_CopyRect(far_host(dest), gfx_screen() + 0x3D40, 0x140, 0x97);
    load_view_pic(nagy_name(name, surfaceType), dest);
    Mouse_HideCursor();
    ShipMove_BlitViewport(buf_at(dest, 0x35C0));
    Mouse_ShowCursor();
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
    System_FreeMem(dest, 0xBCC0);
    alloc_anim_buffers();
}

// 22ca:0b32
void ShipMove_RedrawBackdrop(void)
{
    FarPtr dest = System_GetMem(0xBCC0);
    load_view_pic(PS("planets\\nagy0"), dest);
    Mouse_HideCursor();
    ShipMove_BlitViewport(far_host(dest));
    Mouse_ShowCursor();
    System_FreeMem(dest, 0xBCC0);
}

static void blit_row(FarPtr base, int16_t row)
{
    ShipMove_BlitViewport(buf_at(base, row * 0x140));
}

// One step of the launch fade: whiten the two palette halves as the original does.
static void launch_step(FarPtr base, int16_t i)
{
    Gfx_WaitVRetrace();
    if (0x2B - i > 0x1C)
        Gfx_WhitenPalette(0x80, 0xEF, i, Gfx_PalettePtr());
    if (0x2B - i > 0x29)
        Gfx_WhitenPalette(0x40, 0x7F, (int16_t)(i + 0xD), Gfx_PalettePtr());
    blit_row(base, i);
}

// 22ca:0bbc
void ShipMove_LaunchAnimation(int16_t surfaceType)
{
    uint8_t name[256];
    Sound_PlayButtonSfx(PS("launch"));
    FarPtr buf = System_GetMem(0xBCC0);
    load_view_pic(nagy_name(name, surfaceType), buf);
    Mouse_HideCursor();
    ShipMove_DrawDockDoorFrame(2);
    for (int16_t i = 0x2B; i >= 0x2A; i--)
        launch_step(buf, i);
    ShipMove_DrawDockDoorFrame(3);
    for (int16_t i = 0x29; i >= 0x27; i--)
        launch_step(buf, i);
    ShipMove_DrawDockDoorFrame(4);
    for (int16_t i = 0x26; i >= 0; i--)
        launch_step(buf, i);
    Mouse_ShowCursor();
    load_view_pic(PS("planets\\nagy0"), buf);
    Gfx_WaitFrames(0x14);
    Mouse_HideCursor();
    ShipMove_BlitViewport(buf_at(buf, 0x3700));
    Mouse_ShowCursor();
    Mouse_HideCursor();
    for (int16_t i = 0x2B; i >= 0xB; i--) {
        Gfx_WaitVRetrace();
        if (0x2B - i < 0x10)
            Gfx_WhitenPalette(0x80, 0xEF, (int16_t)(0x2B - i), Gfx_PalettePtr());
        if (0x2B - i < 3)
            Gfx_WhitenPalette(0x40, 0x7F, (int16_t)(0x38 - i), Gfx_PalettePtr());
        blit_row(buf, i);
    }
    Gfx_WaitFrames(2);
    ShipMove_BlitViewport(buf_at(buf, 0xC80));
    Gfx_WaitVRetrace();
    ShipMove_BlitViewport(buf_at(buf, 0xB40));
    Gfx_WaitFrames(2);
    ShipMove_BlitViewport(buf_at(buf, 0xA00));
    Gfx_WaitVRetrace();
    ShipMove_BlitViewport(buf_at(buf, 0x8C0));
    ShipMove_DrawDockDoorFrame(3);
    Gfx_WaitFrames(2);
    ShipMove_BlitViewport(buf_at(buf, 0x780));
    ShipMove_DrawDockDoorFrame(2);
    Gfx_WaitFrames(2);
    ShipMove_BlitViewport(buf_at(buf, 0x640));
    ShipMove_DrawDockDoorFrame(1);
    Gfx_WaitFrames(2);
    ShipMove_BlitViewport(buf_at(buf, 0x500));
    Gfx_WaitFrames(3);
    ShipMove_BlitViewport(buf_at(buf, 0x3C0));
    Gfx_WaitFrames(3);
    ShipMove_BlitViewport(buf_at(buf, 0x280));
    Gfx_WaitFrames(4);
    ShipMove_BlitViewport(buf_at(buf, 0x140));
    Mouse_ShowCursor();
    System_FreeMem(buf, 0xBCC0);
}

static void dock_step(FarPtr base, int16_t i)
{
    Gfx_WaitVRetrace();
    if (i > 0x1C)
        Gfx_WhitenPalette(0x80, 0xEF, (int16_t)(0x2B - i), Gfx_PalettePtr());
    if (i > 0x29)
        Gfx_WhitenPalette(0x40, 0x7F, (int16_t)(0x38 - i), Gfx_PalettePtr());
    blit_row(base, i);
}

// 22ca:0fd3
void ShipMove_DockingAnimation(int16_t surfaceType)
{
    uint8_t name[256];
    Sound_PlayButtonSfx(PS("landing"));
    FarPtr buf = System_GetMem(0xBCC0);
    nagy_name(name, surfaceType); // the original formats it first, then loads nagy0
    load_view_pic(PS("planets\\nagy0"), buf);
    Mouse_HideCursor();
    ShipMove_DrawDockDoorFrame(2);
    for (int16_t i = 1; i <= 2; i++)
        dock_step(buf, i);
    ShipMove_DrawDockDoorFrame(3);
    for (int16_t i = 3; i <= 4; i++)
        dock_step(buf, i);
    ShipMove_DrawDockDoorFrame(4);
    for (int16_t i = 5; i <= 0x2B; i++)
        dock_step(buf, i);
    Mouse_ShowCursor();
    load_view_pic(name, buf);
    Gfx_WaitFrames(0x14);
    Mouse_HideCursor();
    ShipMove_BlitViewport(far_host(buf));
    Mouse_ShowCursor();
    Mouse_HideCursor();
    for (int16_t i = 1; i <= 0x2B; i++) {
        Gfx_WaitVRetrace();
        if (i < 0x10)
            Gfx_WhitenPalette(0x80, 0xEF, i, Gfx_PalettePtr());
        if (i < 3)
            Gfx_WhitenPalette(0x40, 0x7F, (int16_t)(i + 0xD), Gfx_PalettePtr());
        blit_row(buf, i);
    }
    Mouse_ShowCursor();
    ShipMove_DrawDockDoorFrame(3);
    Gfx_WaitVRetrace();
    ShipMove_BlitViewport(buf_at(buf, 0x3480));
    Gfx_WaitVRetrace();
    ShipMove_DrawDockDoorFrame(2);
    ShipMove_BlitViewport(buf_at(buf, 0x3340));
    Gfx_WaitVRetrace();
    ShipMove_BlitViewport(buf_at(buf, 0x3480));
    Gfx_WaitVRetrace();
    ShipMove_DrawDockDoorFrame(1);
    ShipMove_BlitViewport(buf_at(buf, 0x35C0));
    Gfx_WaitVRetrace();
    ShipMove_BlitViewport(buf_at(buf, 0x3480));
    Gfx_WaitVRetrace();
    ShipMove_BlitViewport(buf_at(buf, 0x35C0));
    System_FreeMem(buf, 0xBCC0);
}

// Generated from the disassembly of 22ca:132c: each rep movs / movs run, with
// the add si/di skips between them folded into the offsets.
const ShipMoveRun ShipMove_ViewRuns[] = {
    {30,259},{351,257},{640,1},{672,255},{959,3},{993,253},{1278,6},{1314,251},{1597,8},{1635,249},
    {1915,11},{1956,247},{2234,13},{2277,245},{2553,16},{2598,243},{2872,18},{2919,241},{3190,21},
    {3240,239},{3509,23},{3561,237},{3828,26},{3882,235},{4147,28},{4203,233},{4465,31},{4524,231},
    {4784,33},{4845,229},{5103,35},{5166,227},{5422,38},{5487,225},{5740,41},{5808,223},{6059,21},
    {6081,21},{6129,221},{6378,22},{6402,21},{6450,219},{6697,23},{6723,22},{6771,217},{7015,25},
    {7045,21},{7092,215},{7334,26},{7366,21},{7413,213},{7653,27},{7687,21},{7734,211},{7971,26},
    {8008,22},{8055,209},{8290,26},{8329,22},{8376,207},{8609,26},{8650,22},{8697,205},{8928,25},
    {8971,22},{9018,203},{9246,26},{9293,22},{9339,201},{9565,26},{9614,22},{9660,199},{9884,25},
    {9935,22},{9981,197},{10203,25},{10256,22},{10302,195},{10521,26},{10623,193},{10840,25},{10944,191},
    {11159,25},{11265,189},{11478,25},{11586,187},{11907,185},{12228,183},{12549,181},{12870,179},
    {13191,177},{13512,175},{13833,173},{14154,171},{14398,4},{14475,169},{14716,8},{14796,167},
    {15034,12},{15117,165},{15352,16},{15438,163},{15670,20},{15724,13},{15759,161},{15942,15},
    {15988,24},{16046,12},{16080,159},{16261,14},{16306,28},{16367,12},{16401,157},{16579,14},{16624,32},
    {16688,13},{16722,155},{16898,14},{16942,36},{17010,12},{17043,153},{17217,13},{17260,40},{17331,12},
    {17364,151},{17535,14},{17578,44},{17653,12},{17685,149},{17854,13},{17896,48},{17974,12},{18006,147},
    {18173,12},{18214,52},{18295,12},{18327,145},{18491,13},{18532,56},{18617,11},{18648,143},{18810,12},
    {18850,60},{18938,12},{18969,141},{19129,12},{19168,64},{19260,11},{19290,139},{19447,12},{19486,68},
    {19581,11},{19611,137},{19766,11},{19804,72},{19902,12},{19932,135},{20085,11},{20122,76},{20224,11},
    {20253,133},{20404,11},{20440,80},{20574,131},{20758,84},{20895,129},{21076,88},{21216,127},
    {21394,92},{21537,125},{21712,96},{21858,123},{22030,100},{22179,121},{22348,104},{22500,119},
    {22666,108},{22984,112},{23302,116},{23620,120},{23938,124},{24256,128},{24574,132},{24892,129},
    {25216,121},{25542,114},{25864,111},{26185,109},{26506,107},{26827,106},{27148,104},{27468,103},
    {27788,103},{28109,101},{28430,99},{28751,97},{29071,97},{29392,95},{29713,93},{30033,93},{30354,91},
    {30675,76},{31009,59},{31332,53},{31655,47},{31978,41},{32301,36},{32623,31},{32946,25},{33269,19},
    {33592,14},{33914,9},{34237,3},
};
const int ShipMove_ViewRunCount = (int)(sizeof ShipMove_ViewRuns / sizeof ShipMove_ViewRuns[0]);

// 22ca:132c
void ShipMove_BlitViewport(const uint8_t *src)
{
    const uint8_t *s = src;
    uint8_t *d = gfx_screen() + 0x3D40;
    for (int i = 0; i < ShipMove_ViewRunCount; i++)
        memcpy(d + ShipMove_ViewRuns[i].offset, s + ShipMove_ViewRuns[i].offset,
               ShipMove_ViewRuns[i].length);
}

// 22ca:1d78
void ShipMove_DrawButtonAnimFrame(int16_t frame)
{
    Blit_CopyRect(buf_at(ds.ShipMove_AnimSheet, (frame - 1) * 0x16 + 0x2801),
                  gfx_screen() + 0xC9D7, 0x15, 0x27);
}

// 22ca:1db0
void ShipMove_DrawDockDoorFrame(int16_t frame)
{
    Blit_CopyRect(buf_at(ds.ShipMove_AnimSheet, (frame - 1) * 0x30 + 0x5B41),
                  gfx_screen() + 0xC44F, 0x2F, 0x2B);
}

// 22ca:1de8: every 6th tick draws a random 24x18 blink sprite
void ShipMove_AnimateBlinkLight(void)
{
    ds.ShipMove_BlinkCounter = (ds.ShipMove_BlinkCounter + 1) % 6;
    if (ds.ShipMove_BlinkCounter == 0) {
        uint16_t r = System_Random(6);
        Blit_CopyRect(buf_at(ds.ShipMove_AnimSheet, r * 0x1A + 0x3A07),
                      gfx_screen() + 0xE589, 0x18, 0x12);
    }
}

// 22ca:1e2f: 4-frame 10x16 sprite, a new frame every 8 ticks
void ShipMove_AnimateSpinner(void)
{
    ds.ShipMove_SpinnerCounter = (ds.ShipMove_SpinnerCounter + 1) % 0x20;
    Blit_CopyRect(buf_at(ds.ShipMove_AnimSheet, (ds.ShipMove_SpinnerCounter / 8) * 0xB + 0x43D9),
                  gfx_screen() + 0xBD81, 10, 0x10);
}

// 22ca:1e78: toggles one of 6 lights (1 in 2) and redraws all of them
void ShipMove_AnimateLights(void)
{
    uint16_t idx = System_Random(6) + 1;
    if (System_Random(2) == 0)
        ds.ShipMove_LightOn[idx - 1] = ds.ShipMove_LightOn[idx - 1] == 0;
    for (int i = 1; i <= 6; i++) {
        int32_t base = ds.ShipMove_LightOn[i - 1] == 0 ? 0x3619 : 0x2859;
        int32_t rel = ds.ShipMove_LightX[i - 1] + ds.ShipMove_LightY[i - 1] * 0x140;
        Blit_CopyRect(buf_at(ds.ShipMove_AnimSheet, base + rel),
                      gfx_screen() + (uint16_t)(rel + 0x9F89), ds.ShipMove_LightW[i - 1],
                      ds.ShipMove_LightH[i - 1]);
    }
}

typedef struct [[gnu::packed]] {
    int16_t x, y, depth;
    uint8_t speed;
} Star;
static_assert(sizeof(Star) == 7);

static int16_t abs16(int16_t v) { return (int16_t)(v < 0 ? -v : v); }

// 22ca:1fb5: star field in the Backdrop copy, then shown while the group travels
void ShipMove_AnimateStarfield(void)
{
    if (Groups_Selected()->Status <= 3)
        return;
    if (Groups_Selected()->Status != 5)
        Blit_FillBytes(far_host(ds.ShipMove_Backdrop), 0x8980, 0x7F);
    Star *stars = (Star *)far_host(ds.ShipMove_Stars);
    for (int i = 0; i <= 0x44C; i++) {
        Star *s = &stars[i];
        int16_t d = (int16_t)(s->depth + 100);
        if (d == 0)
            System_RunError(200);
        int16_t px = (int16_t)((int16_t)(s->x * 100) / d);
        int16_t py = (int16_t)((int16_t)(s->y * 100) / d);
        uint16_t off = (uint16_t)((uint16_t)ds.ShipMove_Backdrop + (uint16_t)((py + 0x31) * 0x140)
                                  + (uint16_t)px + 0xA0);
        *(uint8_t *)far_host(far_ptr((uint16_t)(ds.ShipMove_Backdrop >> 16), off)) =
            (uint8_t)(s->depth / 0x3C + 0x70);
        s->depth = (int16_t)(s->depth - s->speed);
        if (abs16(px) > 0x8C || abs16(py) > 0x2D)
            s->depth = (int16_t)(System_Random(500) + 500);
    }
    Mouse_HideCursor();
    ShipMove_BlitViewport(far_host(ds.ShipMove_Backdrop));
    Mouse_ShowCursor();
}

// 22ca:20f3: 1101 stars with random positions
void ShipMove_InitStarfield(void)
{
    ds.ShipMove_Stars = System_GetMem(0x1E1B);
    Star *stars = (Star *)far_host(ds.ShipMove_Stars);
    for (int i = 0; i <= 0x44C; i++) {
        Star *s = &stars[i];
        s->x = (int16_t)(System_Random(0x28E) - 0x147);
        s->y = (int16_t)(System_Random(0xD2) - 0x69);
        s->depth = (int16_t)(System_Random(900) + 0x73);
        s->speed = (uint8_t)(System_Random(10) + 5);
        if (abs16(s->x) < 0x32 && abs16(s->y) < 0x32) {
            s->x = (int16_t)((uint16_t)s->x << 1);  // SHL: x may be negative
            s->y = (int16_t)((uint16_t)s->y << 1);
        }
        if (abs16(s->x) < 0x1E && abs16(s->y) < 0x1E) {
            s->x = (int16_t)(abs16(s->x) + 0x1E);
            s->y = (int16_t)(abs16(s->y) + 0x1E);
        }
    }
    Blit_FillBytes(far_host(ds.ShipMove_Backdrop), 0x8980, 0x7F);
}

// 22ca:21ef
void ShipMove_AnimateGroupView(void)
{
    ShipMove_AnimateLights();
    ShipMove_AnimateBlinkLight();
    ShipMove_AnimateSpinner();
    ShipMove_AnimateStarfield();
}
