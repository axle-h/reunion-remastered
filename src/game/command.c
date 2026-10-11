#include "game/command.h"

#include "game/anim.h"
#include "game/blit.h"
#include "game/crt.h"
#include "game/gfx.h"
#include "game/mouse.h"
#include "game/sound.h"
#include "game/system.h"
#include "game/vecload.h"

// Not ported yet: see stubs.c.
int16_t GrWar_PickBattleAnim(void); // 1537:11f2 (what Game::GrWarProc points to)

enum : uint16_t {
    SCREEN_AT = 0x3D40, // grafika\main window: screen row 49
};

// The original's offsets wrap inside the segment; these stay inside their
// buffers, so the port's pointers agree.
static inline uint8_t *fp_add(FarPtr p, uint16_t add)
{
    return (uint8_t *)far_host(p) + add;
}

// Byte off from the start of p's segment (not from p).
static inline uint8_t *fp_at(FarPtr p, uint16_t off)
{
    return (uint8_t *)far_host(far_ptr((uint16_t)(p >> 16), 0)) + off;
}

static inline uint8_t *screen_at(uint16_t off)
{
    return gfx_screen() + off;
}

static inline const uint8_t *ps(const PString *p)
{
    return (const uint8_t *)p;
}

#define LIT(s) ps(PSTR(s))

// Cc::Rooms holds the 14 war animations; StartMainAnim also looks up room
// animations there, and the endings' ids 15 and 16 read on into the table
// after it (SpWar::ScriptPlays), as in the original.
static const TWarAnim *war_anim(int16_t id)
{
    return ds.Cc_Rooms + (id - 1);
}

// Gfx_LoadPic with the arguments shared by every command-centre sheet.
static void load_sheet(const uint8_t *name, FarPtr dest, uint16_t rows, uint16_t stride)
{
    Gfx_LoadPic(name, far_host(dest), 1, rows, 0, 0, stride, false, Gfx_PalettePtr(), 0, 0xB0, 0x40,
                0, 0xBE);
}

// Sound names of the staff kinds (pilots, builders, fighters, developers).
static const char *const staff_sfx[5] = {"", "pilots", "builders", "fighters", "develope"};

static void play_sfx(const char *name)
{
    PString s;
    uint8_t n = 0;
    while (name[n] != 0) {
        s.s[n] = name[n];
        n++;
    }
    s.len = n;
    Sound_PlayButtonSfx(ps(&s));
}

// The research/messages hotspots start a room animation and play its frames
// out before the screen changes.
static void play_room_anim(int16_t anim)
{
    Command_StartMainAnim(anim);
    ds.Command_AnimDelay = 0;
    uint16_t frames = ds.Cc_Doors[ds.Command_AnimId - 1].Frames;
    if (frames != 0) {
        for (uint16_t i = 1;; i++) {
            Command_TickRoomAnim();
            Gfx_WaitFrames(2);
            if (i == frames)
                break;
        }
    }
}

// 3abd:005c
void Command_HandleCommandCentre(void)
{
    if (ds.Game_ScreenEntered) {
        Command_DrawCommandBackdrop();
        Command_SetupCommandHotspots();
        ds.Command_LightFrame = 1;
        if (ds.Command_ShowHologramOnEntry != 0) {
            ds.Command_ShowHologramOnEntry = 0;
            Command_ShowHologram(9);
        }
    }
    if (ds.Game_HotspotHit != 0x14 && (uint16_t)(ds.Game_HotspotHit - 0x14) < 5) {
        ds.Command_StaffPicked = ds.Command_StaffHotspotKind[ds.Game_HotspotHit - 0x15];
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = 0x18;
        if (ds.Command_StaffPicked >= 1 && ds.Command_StaffPicked <= 4)
            play_sfx(staff_sfx[ds.Command_StaffPicked]);
    }
    if (ds.Game_HotspotHit == 0x19) {
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = 3;
        play_sfx("research");
        play_room_anim(0xB);
    }
    if (ds.Game_HotspotHit == 0x1A) {
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = 0xB;
        play_sfx("messages");
        play_room_anim(0xC);
    }
    if (ds.Game_HotspotHit == 0x1B) {
        play_sfx("door1");
        Command_OpenPanelMaina2();
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = 2;
    }
    if (ds.Game_HotspotHit == 0x1C) {
        play_sfx("door2");
        Command_OpenPanelMaina1();
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = 5;
    }
    if (ds.Game_HotspotHit == 0x1D) {
        play_sfx("starmap");
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = 7;
    }
    if (ds.Game_HotspotHit == 0x1E) {
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = 0x17;
        play_sfx("local");
    }
    if (ds.Game_HotspotHit == 0x1F) {
        play_sfx("door3");
        Command_OpenPanelMaina3And4();
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = 0x10;
    }
    if (ds.Game_HotspotHit == 0x20) {
        play_sfx("surface");
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = 0x14;
    }
}

// 3abd:0275: grafika\main on the screen below the toolbar.
void Command_DrawCommandBackdrop(void)
{
    FarPtr dest = System_GetMem(0xBCC0);
    Gfx_LoadPic(LIT("grafika\\main"), far_host(dest), 1, 0x97, 0, 0, 200,
                ds.Command_BackdropShown != 0, Gfx_PalettePtr(), 0, 0xB0, 0x40, 0, 0xBE);
    Blit_CopyRect(far_host(dest), screen_at(SCREEN_AT), 0x140, 0x97);
    System_FreeMem(dest, 0xBCC0);
    Command_LoadCommandGfx();
    if (ds.Command_BackdropShown == 0)
        Gfx_FadeIn(0, 0xFF, Gfx_PalettePtr());
    else
        Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
    ds.Command_BackdropShown = 1;
}

// 3abd:0352
void Command_LoadCommandGfx(void)
{
    Command_InitRoomAnim();
    Command_StartMainAnim((int16_t)(System_Random(6) + 1));
    ds.Command_MainA5 = System_GetMem(0x5280);
    ds.Command_Heroes = System_GetMem(0x4C40);
    ds.Command_MainA4 = System_GetMem(0x5B00);
    ds.Command_MainFace = System_GetMem(0xE600);
    load_sheet(LIT("grafika\\mainface"), ds.Command_MainFace, 0xB8, 0x140);
    load_sheet(LIT("grafika\\heroes"), ds.Command_Heroes, 0x3D, 0x140);
    Command_DrawHeroPortrait();
    Command_DrawAllStaffIcons();
    load_sheet(LIT("grafika\\maina4"), ds.Command_MainA4, 0x5B, 0x100);
    load_sheet(LIT("grafika\\maina5"), ds.Command_MainA5, 0x42, 0x140);
}

// The edge lines of the hologram panel opening or closing: one pass of the
// "iris" (n from 0 to 0x39 when opening, back down when closing).
static void iris_lines(int16_t n)
{
    uint8_t *seg = fp_at(ds.Command_Offscreen, 0);
    Gfx_DrawLineCopy(gfx_screen(), (int16_t)(n + 0xA6), 0x31, 0xA6, 0xBA, seg);
    Gfx_DrawLineCopy(gfx_screen(), (int16_t)(0xA6 - n), 0x31, 0xA6, 0xBA, seg);
    if (n % 2 == 0) {
        Command_TickAmbientLights();
        Gfx_WaitVRetrace();
    }
}

// 3abd:04b4
void Command_ShowHologram(int16_t vec)
{
    uint8_t *pal = Gfx_PalettePtr();

    Command_FreeRoomAnim();
    System_FreeMem(ds.Command_MainA4, 0x5B00);
    System_FreeMem(ds.Command_MainFace, 0xE600);
    ds.Command_Offscreen = System_GetMem(64000);
    Sound_PlayButtonSfx(LIT("holo"));
    PString num, name;
    System_StrLong(ds.Game_Hero, 0, &num, 0xFF);
    System_StrLoad(&name, PSTR("grafika\\holo"));
    System_StrConcat(&name, &num);
    Gfx_LoadPic(ps(&name), fp_at(ds.Command_Offscreen, SCREEN_AT), 1, 0x97, 0, 0, 0x140, false, pal,
                0, 0xB0, 0x40, 0, 0xBE);

    uint8_t *seg = fp_at(ds.Command_Offscreen, 0);
    for (int16_t n = 1;; n++) {
        for (int16_t x = 0xA4; x <= 0xA8; x++)
            Gfx_DrawLineCopy(gfx_screen(), x, 0xBA, x, (int16_t)(0xBA - n), seg);
        if (n % 6 == 0) {
            Command_TickAmbientLights();
            Gfx_WaitVRetrace();
        }
        if (n == 0x87)
            break;
    }
    for (int16_t n = 0;; n++) {
        iris_lines(n);
        if (n == 0x39)
            break;
    }

    ds.VecLoad_Buffer = System_GetMem(0x8980);
    VecLoad_AllocWireBuffers();
    VecLoad_LoadWireVec(vec);
    VecLoad_SetWirePalette();
    Blit_CopyRect(fp_at(ds.Command_Offscreen, 0x3DBD), fp_at(ds.VecLoad_Buffer, 0x0DE0), 0x4E,
                  0x41);
    VecLoad_DrawWireScene();
    for (;;) {
        Mouse_Update();
        if (Crt_KeyPressed() || ds.Mouse_BtnDown != 0)
            break;
        Command_TickAmbientLights();
        Blit_CopyRect(fp_at(ds.VecLoad_Buffer, 0x0DE0), screen_at(0x3DBD), 0x4E, 0x41);
        Blit_CopyRect(fp_at(ds.Command_Offscreen, 0x3DBD), fp_at(ds.VecLoad_Buffer, 0x0DE0),
                      0x4E, 0x41);
        VecLoad_DrawWireScene();
        Gfx_WaitVRetrace();
    }
    if (Crt_KeyPressed())
        (void)Crt_ReadKey();
    VecLoad_FreeWireBuffers();
    System_FreeMem(ds.VecLoad_Buffer, 0x8980);

    Gfx_LoadPic(LIT("grafika\\main"), fp_at(ds.Command_Offscreen, SCREEN_AT), 1, 0x97, 0, 0, 0x140,
                false, pal, 0, 0xB0, 0x40, 0, 0xBE);
    Command_DrawHeroPortraitToBuffer();
    for (int16_t n = 0x39;; n--) {
        iris_lines(n);
        if (n == 0)
            break;
    }
    Sound_FreeSampleIfIdle();
    ds.Command_MainFace = System_GetMem(0xE600);
    load_sheet(LIT("grafika\\mainface"), ds.Command_MainFace, 0xB8, 0x140);
    Command_TickAmbientLights();
    Blit_CopyRect(fp_at(ds.Command_Offscreen, SCREEN_AT), screen_at(SCREEN_AT), 0x140, 0x97);
    Command_TickAmbientLights();
    Command_DrawHeroPortrait();
    Command_DrawAllStaffIcons();
    System_FreeMem(ds.Command_Offscreen, 64000);
    ds.Command_MainA4 = System_GetMem(0x5B00);
    load_sheet(LIT("grafika\\maina4"), ds.Command_MainA4, 0x5B, 0x100);
    Command_InitRoomAnim();
}

// 3abd:08e0: hotspots 1..4 sit on the hired staff, 5..12 are fixed.
void Command_SetupCommandHotspots(void)
{
    ds.Mouse_HotspotCount = 12;
    ds.Command_StaffHotspotCount = 0;
    for (int16_t s = 1; s <= 4; s++) {
        uint16_t hired = ds.Game_StaffHired[s - 1];
        if ((int16_t)hired > 0 && ds.Advisor_TrainingKind != s
            && (s != 4 || ds.Intel_DevAwayTimer == 0)) {
            THotspot *h = Mouse_Hotspot((int16_t)(ds.Command_StaffHotspotCount + 1));
            h->X = (int16_t)(ds.Command_StaffX[s - 1] + 1);
            h->Y = (uint8_t)(ds.Command_StaffY[s - 1] + 0x32);
            h->W = (int16_t)(ds.Command_StaffW[s - 1] - 2);
            h->H = (uint8_t)(ds.Command_StaffH[s - 1] - 2);
            System_StrStore((const PString *)&ds.Staff_Names[s - 1][hired - 1],
                            (PString *)&h->Caption, 0x12);
            h->Repeat_ = 1;
            ds.Command_StaffHotspotCount++;
            ds.Command_StaffHotspotKind[ds.Command_StaffHotspotCount - 1] = s;
            h->_1A = 0;
        }
    }
    for (int16_t j = (int16_t)(ds.Command_StaffHotspotCount + 1); j < 5; j++) {
        THotspot *h = Mouse_Hotspot(j);
        h->X = 0;
        h->Y = 0;
        h->W = 0;
        h->H = 0;
        h->Caption.len = 0;
        h->Repeat_ = 1;
        h->_1A = 0;
    }
    Mouse_SetHotspot(5, 0x2A, 0x47, 0x26, 0x3C, LIT("RESEARCH-DESIGN"), 1, 0);
    Mouse_SetHotspot(6, 0, 0x47, 0x2A, 0x4F, LIT("MESSAGE"), 1, 0);
    Mouse_SetHotspot(7, 0xA3, 0x31, 0x1E, 0x36, LIT("COMMANDERS"), 1, 0);
    Mouse_SetHotspot(8, 0xFF, 0x31, 0x41, 0x58, LIT("INFO-BUY"), 1, 0);
    Mouse_SetHotspot(9, 0, 0x31, 0x50, 0x16, LIT("STARMAP"), 1, 0);
    Mouse_SetHotspot(10, 0x51, 0x31, 0x30, 0x31, LIT("SPACE LOCAL"), 1, 0);
    Mouse_SetHotspot(11, 0xC2, 0x31, 0x35, 0x52, LIT("SHIP INFO"), 1, 0);
    Mouse_SetHotspot(12, 0x49, 0x9D, 0xBE, 0x2B, LIT("PLANET MAIN"), 1, 0);
}

// 3abd:0ba1
void Command_InitRoomAnim(void)
{
    ds.Anim_Record = System_GetMem(0x349);
    ds.Anim_Script = System_GetMem(0x230);
    ds.Command_AnimId = 0;
    ds.Command_AnimFrame = 0;
    ds.Command_AnimTick = 1;
    *(uint8_t *)far_host(ds.Anim_Record) = 0; // Anim record: not loaded
}

// 3abd:0bdc
void Command_FreeRoomAnim(void)
{
    Command_StopMainAnim();
    System_FreeMem(ds.Anim_Record, 0x349);
    System_FreeMem(ds.Anim_Script, 0x230);
}

// 3abd:0c05: random delay 100..299 frames before the first frame.
void Command_StartMainAnim(int16_t anim)
{
    Command_StopMainAnim();
    ds.Command_AnimId = anim;
    ds.Command_AnimFrame = 1;
    ds.Command_AnimDelay = (int16_t)(System_Random(200) + 100);
    // Read from the 5-byte war table, not the 7-byte room table.
    ds.Command_AnimRepeat = war_anim(ds.Command_AnimId)->Repeat_;
    ds.Command_AnimSpeed = war_anim(ds.Command_AnimId)->Speed;
}

// 3abd:0c55
void Command_StopMainAnim(void)
{
    if (ds.Command_AnimId > 0) {
        Anim_FreeMainAnim((uint16_t)ds.Command_AnimId);
        ds.Command_AnimId = 0;
    }
}

// 3abd:0c70: one step of the window animation, with the hired builders
// drawn over it.
void Command_TickRoomAnim(void)
{
    if (ds.Command_AnimId == 0)
        return;
    if (ds.Command_AnimDelay != 0) {
        ds.Command_AnimDelay--;
        return;
    }
    ds.Command_AnimFrame++;
    if ((int16_t)ds.Cc_Doors[ds.Command_AnimId - 1].Frames < ds.Command_AnimFrame) {
        Command_StopMainAnim();
        Command_StartMainAnim((int16_t)(System_Random(6) + 1));
        return;
    }
    int16_t id = ds.Command_AnimId;
    uint16_t w = (uint16_t)ds.Cc_Doors[id - 1].W;
    Anim_LoadMainAnim((uint16_t)id);
    const TRoomAnim *room = &ds.Cc_Doors[ds.Command_AnimId - 1];
    uint16_t size = (uint16_t)(room->H * room->W);
    FarPtr buf = System_GetMem(size);
    uint16_t scr = (uint16_t)(room->Y * 0x140 + room->X);
    Mouse_HideCursor();
    Blit_CopyRectStrided(screen_at(scr), fp_add(buf, (uint16_t)(w + 1)), (int16_t)(w - 2),
                         (int16_t)(ds.Cc_Doors[id - 1].H - 2), 0x140, (int16_t)w);
    Mouse_ShowCursor();
    bool ok;
    Anim_DrawAnimFrame((uint16_t)ds.Command_AnimFrame, far_host(buf), &ok);
    uint16_t fighter = ds.Game_StaffHired[2];
    Mouse_HideCursor();
    if ((int16_t)fighter > 0 && ds.Command_AnimId < 7) {
        Blit_BlitTransparentStrided(
            fp_add(ds.Command_MainFace, (uint16_t)(ds.Command_StaffSrcY[2][fighter - 1] * 0x140
                                                   + ds.Command_StaffSrcX[2][fighter - 1] + 0x147)),
            fp_add(buf, (uint16_t)(w * 0x27 + 1)), 0x2A, 0x1A, 0x140, (int16_t)w, 0x40);
    }
    Blit_CopyRectStrided(fp_add(buf, (uint16_t)(w + 1)), screen_at(scr), (int16_t)(w - 2),
                         (int16_t)(ds.Cc_Doors[id - 1].H - 2), (int16_t)w, 0x140);
    Mouse_ShowCursor();
    ds.Command_AnimDelay = 0;
    System_FreeMem(buf, size);
}

// 3abd:0e5d: next frame (wrapping) straight to the screen, for the endings.
void Command_StepMainAnim(void)
{
    if (ds.Command_AnimId == 0)
        return;
    ds.Command_AnimFrame++;
    if ((int16_t)ds.Cc_Doors[ds.Command_AnimId - 1].Frames < ds.Command_AnimFrame)
        ds.Command_AnimFrame = 1;
    int16_t id = ds.Command_AnimId;
    Anim_LoadMainAnim((uint16_t)id);
    bool ok;
    Anim_DrawAnimFrame((uint16_t)ds.Command_AnimFrame,
                       screen_at((uint16_t)(ds.Cc_Doors[id - 1].Y * 0x140 + ds.Cc_Doors[id - 1].X)),
                       &ok);
}

// 3abd:0ed5
void Command_FreeGrRoomAnim(void)
{
    Command_StopGrAnim();
    System_FreeMem(ds.Anim_Record, 0x349);
    System_FreeMem(ds.Anim_Script, 0x230);
}

// 3abd:0efe
void Command_StopGrAnim(void)
{
    if (ds.Command_AnimId > 0) {
        Anim_FreeGrAnim((uint16_t)ds.Command_AnimId);
        ds.Command_AnimId = 0;
    }
}

// 3abd:0f19: the ground-battle animation; the next one comes from
// Game::GrWarProc (GrWar's PickBattleAnim).
void Command_TickGrAnim(void)
{
    if (ds.Command_AnimId == 0)
        return;
    ds.Command_AnimTick--;
    if (ds.Command_AnimTick >= 1)
        return;
    ds.Command_AnimTick = ds.Command_AnimSpeed;
    ds.Command_AnimFrame++;
    if ((int16_t)war_anim(ds.Command_AnimId)->Frames < ds.Command_AnimFrame) {
        ds.Command_AnimRepeat--;
        if (ds.Command_AnimRepeat < 1) {
            Command_StopGrAnim();
            int16_t anim = GrWar_PickBattleAnim();
            Command_StartMainAnim(anim); // BUG? not a StartGrAnim; the original calls 0c05
        }
        ds.Command_AnimTick = 1;
        ds.Command_AnimFrame = 0;
    } else {
        Anim_LoadGrAnim((uint16_t)ds.Command_AnimId);
        bool ok;
        Anim_DrawAnimFrame((uint16_t)ds.Command_AnimFrame, far_host(ds.Command_WarAnimBuffer), &ok);
        if (!ok) {
            ds.Command_AnimFrame = war_anim(ds.Command_AnimId)->Frames;
            ds.Command_AnimRepeat = 1;
        }
        Mouse_HideCursor();
        Blit_CopyRectStrided(fp_add(ds.Command_WarAnimBuffer, 0x3A), screen_at(0x4103), 0x3A, 0x4F,
                             0x3A, 0x140);
        Mouse_ShowCursor();
    }
}

// Waits used while a panel slides: ambient animations, then one retrace.
static void panel_tick(void)
{
    Command_TickAmbientLights();
    Command_TickPanelDoorAnim();
    Gfx_WaitVRetrace();
}

// 3abd:102c
void Command_OpenPanelMaina1(void)
{
    FarPtr dest = System_GetMem(0x95D0);
    load_sheet(LIT("grafika\\maina1"), dest, 0x88, 0x11A);
    panel_tick();
    for (int16_t n = 1;; n++) {
        Mouse_HideCursor();
        Blit_CopyRectStrided(fp_add(dest, (uint16_t)((n - 1) * 0x2F)), screen_at(0x4851), 0x2F, 0x88,
                             0x11A, 0x140);
        Command_DrawStaffIcon(1);
        Mouse_ShowCursor();
        for (int i = 0; i < 4; i++)
            panel_tick();
        if (n == 6)
            break;
    }
    System_FreeMem(dest, 0x95D0);
}

// 3abd:1122
void Command_OpenPanelMaina2(void)
{
    FarPtr dest = System_GetMem(0x19BA);
    load_sheet(LIT("grafika\\maina2"), dest, 0x59, 0x4A);
    panel_tick();
    for (int16_t n = 1;; n++) {
        Mouse_HideCursor();
        Blit_CopyRectStrided(fp_add(dest, (uint16_t)((n - 1) * 0x12)), screen_at(0x3DEC), 0x12, 0x59,
                             0x4A, 0x140);
        Mouse_ShowCursor();
        Command_DrawHeroPortrait();
        for (int i = 0; i < 4; i++)
            panel_tick();
        if (n == 4)
            break;
    }
    System_FreeMem(dest, 0x19BA);
}

// 3abd:1221
void Command_OpenPanelMaina3And4(void)
{
    FarPtr buf = System_GetMem(0x5B00);
    load_sheet(LIT("grafika\\maina4"), buf, 0x5B, 0x100);
    Gfx_WaitVRetrace();
    for (int16_t n = 1;; n++) {
        int16_t strip = ds.Command_DoorDir == 0 ? n : (int16_t)(0x10 - n);
        Mouse_HideCursor();
        Blit_CopyRectStrided(fp_add(buf, (uint16_t)((strip - 1) * 0x10)), screen_at(0x584D), 0x10,
                             0x5B, 0x100, 0x140);
        Command_DrawStaffIcon(2);
        Mouse_ShowCursor();
        Gfx_WaitFrames(4);
        if (n == 6)
            break;
    }
    System_FreeMem(buf, 0x5B00);

    buf = System_GetMem(0x2346);
    load_sheet(LIT("grafika\\maina3"), buf, 0x69, 0x56);
    Gfx_WaitVRetrace();
    for (int16_t n = 1;; n++) {
        Mouse_HideCursor();
        Blit_CopyRectStrided(fp_add(buf, (uint16_t)((n - 1) * 0x15)), screen_at(0x46CA), 0x15, 0x69,
                             0x56, 0x140);
        Command_DrawStaffIcon(2);
        Mouse_ShowCursor();
        Gfx_WaitFrames(5);
        if (n == 4)
            break;
    }
    System_FreeMem(buf, 0x2346);
}

// 3abd:1395: the door beside the builders' desk opens and closes at random
// intervals (period 70..319 steps of 4 frames).
void Command_TickPanelDoorAnim(void)
{
    ds.Command_DoorCountdown--;
    if (ds.Command_DoorCountdown >= 1)
        return;
    ds.Command_DoorCountdown = 4;
    ds.Command_DoorTimer--;
    if (ds.Command_DoorTimer == 0) {
        ds.Command_DoorTimer = (int16_t)(System_Random(0xFA) + 0x46);
        ds.Command_DoorDir = ds.Command_DoorDir == 0;
    }
    int16_t strip = ds.Command_DoorDir == 0 ? (int16_t)(0x10 - ds.Command_DoorTimer)
                                             : (int16_t)(ds.Command_DoorTimer - 1);
    if (strip >= 0 && strip < 0x10) {
        Mouse_HideCursor();
        Blit_CopyRectStrided(fp_add(ds.Command_MainA4, (uint16_t)(strip * 0x10)), screen_at(0x584D),
                             0x10, 0x5B, 0x100, 0x140);
        uint16_t hired = ds.Game_StaffHired[1];
        if ((int16_t)hired > 0) {
            Blit_BlitTransparent(
                fp_add(ds.Command_MainFace,
                       (uint16_t)(ds.Command_StaffSrcY[1][hired - 1] * 0x140
                                  + ds.Command_StaffSrcX[1][hired - 1] + 0x141)),
                screen_at((uint16_t)((ds.Command_StaffY[1] + 0x31) * 0x140 + ds.Command_StaffX[1]
                                     + 0x141)),
                (int16_t)(ds.Command_StaffW[1] - 2), (int16_t)(ds.Command_StaffH[1] - 2), 0x40);
        }
        Mouse_ShowCursor();
    }
    int16_t delay = ds.Command_AnimDelay;
    Command_TickRoomAnim();
    if (delay == 0)
        Command_DrawStaffIcon(4);
}

// 3abd:14c5: two small sprite loops from maina5, plus the blinking pixels.
void Command_TickAmbientLights(void)
{
    ds.Command_LightTimer--;
    if (ds.Command_LightTimer < 1) {
        ds.Command_LightTimer = 5;
        ds.Command_LightFrame = (int16_t)((ds.Command_LightFrame + 1) % 0x26);
        uint16_t f = (uint16_t)ds.Command_LightFrame;
        Blit_CopyRect(
            fp_add(ds.Command_MainA5, (uint16_t)((f >> 4) * 0x1B80 + (f % 0x10) * 0x13 + 0x140)),
            screen_at(0x3D65), 0x13, 0x15);
    }
    ds.Command_Light2Timer--;
    if (ds.Command_Light2Timer < 1) {
        ds.Command_Light2Timer = 7;
        ds.Command_Light2Frame = (int16_t)((ds.Command_Light2Frame + 1) % 10);
        Blit_CopyRect(fp_add(ds.Command_MainA5, (uint16_t)(ds.Command_Light2Frame * 0xD + 0x38B3)),
                      screen_at(0x6622), 0xC, 0xE);
    }
    for (uint16_t i = 1;; i++) {
        TLight *light = &ds.Command_Lights[i - 1];
        uint8_t *px = screen_at((uint16_t)(light->Ofs + SCREEN_AT));
        if (i < 0xC) {
            *px = (uint8_t)(System_Random(0xB0) + 0x40);
        } else {
            light->Count--;
            if (light->Count == 0)
                *px = 0x70;
            if (light->Count == 0x64)
                *px = 0x82;
            if (light->Count == 0)
                light->Count = (uint8_t)(System_Random(5) + 0x66);
            if (light->Count == 0x64)
                light->Count = (uint8_t)(System_Random(8) + 5);
        }
        if (i == 0x13)
            break;
    }
}

// 3abd:163f
void Command_DrawHeroPortrait(void)
{
    Mouse_HideCursor();
    Blit_BlitTransparent(fp_add(ds.Command_Heroes, (uint16_t)(0x140 + ds.Game_HeroSpriteX)),
                         screen_at(0x7C43), 0x43, 0x3B, 0x40);
    Mouse_ShowCursor();
}

// 3abd:167d
void Command_DrawHeroPortraitToBuffer(void)
{
    Mouse_HideCursor();
    Blit_BlitTransparent(fp_add(ds.Command_Heroes, (uint16_t)(0x140 + ds.Game_HeroSpriteX)),
                         fp_at(ds.Command_Offscreen, 0x7C43), 0x43, 0x3B, 0x40);
    Mouse_ShowCursor();
}

// 3abd:16bc: staff kind of slot 1..4, unless unhired, away, or the developer
// is blocked.
void Command_DrawStaffIcon(int16_t slot)
{
    int16_t s = (int16_t)ds.Command_StaffOfSlot[slot - 1];
    uint16_t hired = ds.Game_StaffHired[s - 1];
    if ((s != 4 || ds.Intel_DevAwayTimer == 0) && ds.Advisor_TrainingKind != s
        && (int16_t)hired > 0) {
        Blit_BlitTransparent(
            fp_add(ds.Command_MainFace,
                   (uint16_t)(ds.Command_StaffSrcY[s - 1][hired - 1] * 0x140
                              + ds.Command_StaffSrcX[s - 1][hired - 1] + 0x141)),
            screen_at((uint16_t)((ds.Command_StaffY[s - 1] + 0x31) * 0x140
                                 + ds.Command_StaffX[s - 1] + 0x141)),
            (int16_t)(ds.Command_StaffW[s - 1] - 2), (int16_t)(ds.Command_StaffH[s - 1] - 2), 0x40);
    }
}

// 3abd:1794
void Command_DrawAllStaffIcons(void)
{
    Mouse_HideCursor();
    for (int16_t s = 1; s <= 4; s++)
        Command_DrawStaffIcon(s);
    Mouse_ShowCursor();
}

// 3abd:17dd: Earth lost (from screen 32) or won against (other): death
// pictures and animations, then the program ends.
void Command_ShowDeathEnding(void)
{
    uint8_t *pal = Gfx_PalettePtr();
    PString num, name;
    if (ds.Game_PrevScreen == 0x20)
        System_StrStore(PSTR("1"), &num, 0xFF);
    else
        System_StrStore(PSTR("2"), &num, 0xFF);
    FarPtr buf = System_GetMem(64000);
    System_StrLoad(&name, PSTR("grafika\\deathsz"));
    System_StrConcat(&name, &num);
    Gfx_LoadPic(ps(&name), far_host(buf), 0, 200, 0, 0, 0x140, true, pal, 0, 0xFF, 0, 0, 0xFF);
    Blit_CopyRect(far_host(buf), gfx_screen(), 0x140, 200);
    Gfx_FadeInSteps(0, 0xFF, pal, 0x8C);
    Gfx_WaitFrames(0x2D0);
    Gfx_FadeOutSteps(0, 0xFF, pal, 0x46);
    System_StrLong(ds.Game_Hero, 0, &num, 0xFF);
    System_StrLoad(&name, PSTR("grafika\\death"));
    System_StrConcat(&name, &num);
    Gfx_LoadPic(ps(&name), far_host(buf), 0, 200, 0, 0, 0x140, false, pal, 0, 0xB0, 0x40, 0, 0xB0);
    Blit_CopyRect(far_host(buf), gfx_screen(), 0x140, 200);
    Gfx_FadeInSteps(0, 0xFF, pal, 0x8C);
    if (ds.Game_Hero == 2) {
        Command_InitRoomAnim();
        Command_StartMainAnim(0xF);
        for (int16_t i = 1;; i++) {
            Command_StepMainAnim();
            Gfx_WaitFrames(7);
            if (i == 0xED)
                break;
        }
        Command_FreeRoomAnim();
    } else {
        Gfx_WaitFrames(0x67C);
    }
    Gfx_FadeOutSteps(0, 0xFF, pal, 0x46);
    Gfx_LoadPic(LIT("grafika\\death"), far_host(buf), 0, 200, 0, 0, 0x140, false, pal, 0, 0xB0, 0x40,
                0, 0xB0);
    Blit_CopyRect(far_host(buf), gfx_screen(), 0x140, 200);
    System_FreeMem(buf, 64000);
    Gfx_WaitFrames(0x46);
    Gfx_FadeInSteps(0, 0xFF, pal, 0x46);
    Command_InitRoomAnim();
    Command_StartMainAnim(0x10);
    for (int16_t i = 1;; i++) {
        Command_StepMainAnim();
        Gfx_WaitFrames(0xC);
        if (i == 0x3D)
            break;
    }
    Command_FreeRoomAnim();
    Gfx_FadeOutSteps(0, 0xFF, pal, 0x118);
    Gfx_SetTextMode();
    System_Halt(15);  // 3abd:1a3d: start.exe runs the game again
}

// 3abd:1a47
void Command_FadeOutAndHalt(void)
{
    Gfx_FadeOut(0, 0xFF, Gfx_PalettePtr());
    Gfx_SetTextMode();
    System_Halt(10);  // 3abd:1a5f: start.exe runs VICTORY.PRG
}
