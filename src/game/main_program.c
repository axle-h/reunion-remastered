// The main program body of REUNION.PRG (1000:01a1 Main), ported from the
// Ghidra decompile, with argument lists and table offsets taken from the
// disassembly where the decompile drops or aliases them. The large inline
// screens (planet surface, pub dialogue, battles, alien talk, colony list)
// are static helpers called at the original's place in the dispatch chain.
#include "game/main_program.h"

#include "game/advisor.h"
#include "game/alientalk.h"
#include "game/anim.h"
#include "game/baseeff.h"
#include "game/cargo.h"
#include "game/colonize.h"
#include "game/command.h"
#include "game/credits.h"
#include "game/droids.h"
#include "game/dseg.h"
#include "game/ems.h"
#include "game/gfx.h"
#include "game/gfxpath.h"
#include "game/groups.h"
#include "game/grwar.h"
#include "game/grwarunits.h"
#include "game/hire.h"
#include "game/intel.h"
#include "game/launch.h"
#include "game/message.h"
#include "game/minmax.h"
#include "game/mouse.h"
#include "game/options.h"
#include "game/music.h"
#include "game/planetinfo.h"
#include "game/planetlist.h"
#include "game/pub.h"
#include "game/rawtext.h"
#include "game/research.h"
#include "game/satellite.h"
#include "game/saveload.h"
#include "game/shipinfo.h"
#include "game/shipmove.h"
#include "game/shop.h"
#include "game/sim.h"
#include "game/sound.h"
#include "game/spwar.h"
#include "game/surface.h"
#include "game/system.h"
#include "game/textfile.h"
#include "game/title.h"
#include "game/vecload.h"
#include "platform/timing.h"

// 26eb:0000 Cheat::Apply (unit not ported; Cheat_Requested is never set).
void Cheat_Apply(void);

// CurPlanet/CurMoon by Pascal index.
#define CUR_PLANET(s) ds.Game_CurPlanet[(s) - 1]
#define CUR_MOON(s) ds.Game_CurMoon[(s) - 1]
#define CUR_SYS ds.Game_CurSystem

static inline const uint8_t *ps(const PString *p)
{
    return (const uint8_t *)p;
}
#define LIT(s) ps(PSTR(s))

// Ui::Screens[screen]: TScreenInfo bytes +09 (show picture), +0A (reload
// menu bar), +0B (menu command that opens it, shortint).
static inline uint8_t *screen_info(int16_t screen)
{
    return (uint8_t *)&ds.Ui_Screens[screen - 1];
}

static inline TBody *cur_body(void)
{
    return (TBody *)far_host(ds.Game_CurBody);
}

// Game::Buildings^[i] (1-based).
static inline TBuilding *building(int16_t i)
{
    return (TBuilding *)far_host(ds.Game_Buildings) + (i - 1);
}

// Surface::BuildingTypes[k]. Of the unnamed bytes, _10 is the builder skill
// needed, _12[0] needs a builder plant, _12[1] needs a vehicle plant and
// _12[1 + SurfaceType] allows that surface.
static inline TBuildingType *building_type(int16_t k)
{
    return &ds.Surface_BuildingTypes[k - 1];
}

static void add_credits(int32_t d)
{
    ds.Game_Credits = (int32_t)((uint32_t)ds.Game_Credits + (uint32_t)d);
}

// --- Start-up -------------------------------------------------------------------

void Game_InitUnits(void)
{
    // System::InitSystem is system_init() (called by the platform start-up);
    // Crt, Gus, SoundDrv and AdlibMusic are replaced by platform code.
    MinMax_Init();
    Ems_Init();
    Sound_Init();
    Music_Init();
    Gfx_Init();
    RawText_Init();
    VecLoad_Init();
    GfxPath_Init();
    Message_Init();
    Surface_Init();
    Groups_Init();
    Intel_Init();
    Satellite_Init();
    SaveLoad_Init();
    GrWar_Init();
}

// 1000:0205..0592
static void startup(void)
{
    ds.Gfx_ScreenSeg = ds.System_SegA000;
    System_Randomize();
    Music_InitMusicDevices();
    Gfx_SetVideoMode13();
    ds.Mouse_HotspotCount = 0;
    Command_SetupCommandHotspots();
    ds.Mouse_CursorFrozen = 0;
    ds.Blit_HookRow = 999;
    Mouse_InitMouse();
    Mouse_HideCursor();
    ds.Mouse_HeldPhase = 0;
    ds.Mouse_LastPushable = 0;
    Title_InitTitleAndMainMenu();

    // 238,000 bytes, 266,000 with a GUS. The port's heap is the DOS one, so
    // the check stays meaningful.
    ds.Main_I = ds.SoundDrv_GusReady ? 28000 : 0;
    if (System_MemAvail() < ds.Main_I + 0x3A1B0)
        Gfx_OutOfMemoryExit();

    Title_SetMainMenuPalette();
    Groups_InitPlanetForce();
    Music_ApplyMusicTrack();
    Command_LoadCommandGfx();
    GfxPath_SetToolbarAndShow(1);
    ds.GfxPath_PageFlagB = 1;
    ds.GfxPath_SavedPage = 1;  // where MenuBar[0].Command would be
    ds.GfxPath_PageFlagA = 1;
    GfxPath_ShowToolbar();
    if (ds.Game_StartChoice != 2)
        Gfx_FadeIn(0, 0xFF, Gfx_PalettePtr());

    ds.Game_CurrentScreen = 1;
    ds.Game_PrevScreen = 1;
    ds.Game_HotspotHit = 0;
    ds.Game_PressHotspot = 0;
    ds.Game_ReleaseHotspot = 0;
    ds.Game_ScreenSwitch = 0;
    ds.Game_ClockRunning = 1;
    // Command centre animation state.
    ds.Command_LightFrame = 1;
    ds.Command_LightTimer = 1;
    ds.Command_DoorTimer = 1;
    ds.Command_DoorCountdown = 1;
    ds.Command_Light2Frame = 1;
    ds.Command_Light2Timer = 1;
    ds.Command_ShowHologramOnEntry = 0;
    ds.VecLoad_Spinning = 0;
    ds.VecLoad_Angles[0] = 0;
    ds.VecLoad_Angles[1] = 0;
    ds.VecLoad_Angles[2] = 0;
    for (ds.Main_I = 1; ds.Main_I <= 6; ds.Main_I++)
        ds.Game_HomeStock[ds.Main_I - 1] = 0;
    CUR_SYS = 1;
    for (ds.Main_I = 1; ds.Main_I <= 8; ds.Main_I++)
        CUR_PLANET(ds.Main_I) = 0;
    for (ds.Main_I = 1; ds.Main_I <= 8; ds.Main_I++)
        CUR_MOON(ds.Main_I) = 0;
    ds.Game_ColonySetupCount = 0;
    ds.Game_BuildingCount = 0;
    ds.Game_CurResearch = 0;
    ds.Research_FrameCounter = 0;
    ds.Game_GroupList = 0;
    ds.Game_SelGroup = 0;
    ds.Game_SelGroup1 = 0;
    ds.Game_SelGroup0 = 0;
    ds.Game_GroupsEnabled = 0;
    for (ds.Main_I = 1; ds.Main_I <= 4; ds.Main_I++)
        ds.Game_GroupKindAllowed[ds.Main_I - 1] = 0;
    ds.Message_LogCount = 0;
    ds.Gfx_FlashStep = 0;
    ds.Gfx_FlashCount = 0;
    for (int i = 0; i < 12; i++)
        ds.Pub_StoryFlags[i] = 0;
    ds.Game_StoryStage = 1;
    for (ds.Main_I = 1; ds.Main_I <= 0x23; ds.Main_I++)
        ds.Game_IdeaTimer[ds.Main_I - 1] = -1;
    ds.Intel_Race2Timer3 = (int16_t)(System_Random(400) + 6000);
    ds.Cheat_Used = 0;
    ds.Cheat_Requested = 0;
    ds.Game_BattleFlag = 0;
    ds.Advisor_TrainingTimer = 0;
    ds.Advisor_TrainingKind = 0;
    ds.Advisor_TrainingCourse = 0;
    for (int i = 0; i < 4; i++)
        ds.Game_StaffExp[i] = 0;
    // BUG? i runs to 5 over DevSkill[1..4]; the fifth word is ProductionRate,
    // set just below.
    for (ds.Main_I = 1; ds.Main_I <= 5; ds.Main_I++)
        *(ds.Main_I <= 4 ? &ds.Game_DevSkill[ds.Main_I - 1] : &ds.Game_ProductionRate) = 0;
    for (int i = 0; i < 4; i++)
        ds.Game_StaffHired[i] = 0;
    ds.Game_ProductionRate = 10;
    ds.Game_Credits = 120000;
    ds.Game_Year = 2927;
    ds.Game_Month = 8;
    ds.Game_Day = 13;
    ds.Game_Hour = 23;
    Gfx_DrawGameDate();
    ds.Sound_SampleBuf = 0;
    ds.TextFile_InputActive = 0;
    SaveLoad_LoadInit();
    if (ds.Game_StartChoice == 2) {
        ds.Game_PrevScreen = 1;
        ds.Game_CurrentScreen = 12;
        ds.Game_ScreenSwitch = 1;
    }
    Mouse_ShowCursor();
}

// --- Per-frame work ---------------------------------------------------------------

// One game hour. Phase B (button held) runs ProgressProduction before
// AlienTalk_VerifyManualAnswer; phase A the other way round.
static void advance_hour(bool held)
{
    Sim_AdvanceHour();
    Sim_ProcessStoryTimers();
    Sim_ProgressResearch();
    if (held) {
        Sim_ProgressProduction();
        AlienTalk_VerifyManualAnswer();
    } else {
        AlienTalk_VerifyManualAnswer();
        Sim_ProgressProduction();
    }
    if (ds.Game_Hour == 0)
        Sim_ProcessAllBodyEvents();
    Sim_GrowStaffExperience();
    Sim_UpdateBuildings();
    Sim_UpdateColonySetups();
    Sim_MineAllBodies();
    Sim_ProcessTravellingGroups();
    Sim_CheckEnemyAttacks();
    Sim_ProcessPubTimers();
    ds.Game_Ticks = 0;
    Gfx_DrawCredits();
}

// The per-screen animations run once per frame while no switch is pending.
static void animate_screens(void)
{
    if (ds.Game_ScreenSwitch)
        return;
    int16_t s = ds.Game_CurrentScreen;
    if (s == 20) {
        if (ds.Game_Ticks % 10 == 8
            && (ds.Surface_Mode == 1 || ds.Surface_Mode == 2 || ds.Surface_Mode == 6)) {
            ds.Surface_AnimTileFrame = (int16_t)(ds.Surface_AnimTileFrame % 3 + 1);
            Surface_DrawMapView();
        }
        Surface_BlinkModeButton();
    }
    if (ds.VecLoad_Spinning)
        VecLoad_DrawFrame();
    if (ds.TextFile_InputActive)
        TextFile_InputTick();
    if (s == 3)
        Research_AnimateCurrentResearch();
    if (s == 3)
        Research_AnimateSelectionFrame();
    if (s == 1)
        Command_TickAmbientLights();
    if (s == 1)
        Command_TickPanelDoorAnim();
    if (s == 17)
        ShipMove_AnimateGroupView();
    if (s == 29 && ds.SpWar_BattleOver == 0)
        SpWar_PlayWarAnimScript();
    if (s == 32 && ds.GrWar_BattleOver == 0)
        GrWar_BattleTick();
    if (s == 32 && ds.GrWar_BattleOver != 0)
        GrWar_AnimateResultScreen();
    if (s == 7 || s == 18)
        PlanetInfo_UpdateOrbitPositions();
    if (s == 25)
        Pub_UpdateQuestionHover();
    if (s == 34)
        AlienTalk_HandleTalkMouse();
    if (s == 37 && ds.Game_Ticks == 0)
        PlanetList_RefreshList();
    if (s == 23)
        Pub_UpdatePubAnimations();
}

// The menu-bar hit test with the second toolbar page mapping 1..6 to 7..12.
static void hit_test(int16_t *hit)
{
    Mouse_HitTest((uint16_t *)hit);
    if (ds.GfxPath_PageFlagB == 0 && (uint16_t)*hit >= 1 && (uint16_t)*hit <= 6)
        *hit += 6;
}

// Hotspot repeats while held (records of Mouse_Hotspot), is the toolbar
// toggle, or is a menu button with a command.
static bool hit_pushable(int16_t hit)
{
    uint16_t h = (uint16_t)hit;
    if (h == 0xD)
        return true;
    if (h >= 0x14 && Mouse_Hotspot((int16_t)(h - 0x14))->Repeat_ != 0)
        return true;
    return h >= 1 && h <= 12 && ds.GfxPath_MenuBar[h - 1].Command != 0;
}

// The hint line for the hotspot under the mouse.
static void draw_hint(int16_t hit)
{
    uint16_t h = (uint16_t)hit;
    if (h >= 1 && h <= 12)
        Gfx_DrawStatusText((const uint8_t *)&ds.GfxPath_MenuBar[h - 1].Caption);
    else if (h == 0 || h == 0xD)
        Gfx_DrawStatusText(LIT(""));
    else if (h == 0xE)
        Gfx_DrawStatusText(LIT("Time"));
    else
        Gfx_DrawStatusText((const uint8_t *)&Mouse_Hotspot((int16_t)(h - 0x14))->Caption);
}

// The vertical-retrace wait of both frame loops (two spins on port 3DAh).
// Returns false once the user closed the window.
static bool wait_retrace(void)
{
    return timing_wait_retrace();
}

uint64_t Game_EventWaitFrames;

// Phase A: frames until a press, a release or a screen switch.
static bool wait_for_event(void)
{
    do {
        if (ds.Game_FrameCount > 30000 && ds.Game_Ticks > 99 && ds.Game_ClockRunning
            && System_Random(200) == 0)
            PlanetInfo_RandomizeProtectPage();
        Sound_FreeSampleIfIdle();
        if (ds.Game_ClockRunning)
            ds.Game_Ticks++;
        if (ds.Game_Ticks > 100)
            advance_hour(false);
        Mouse_Poll();
        if (ds.Cheat_Requested)
            Cheat_Apply();
        Options_Frame();  // port addition: the port menu's changes, frozen cheats
        if (!wait_retrace())
            return false;
        Game_EventWaitFrames++;  // port addition
        Gfx_PaletteFlashTick();
        ds.Game_FrameCount = (int32_t)((uint32_t)ds.Game_FrameCount + 1);
        animate_screens();

        ds.Game_PrevHotspotHit = ds.Game_HotspotHit;
        hit_test(&ds.Game_HotspotHit);
        if (ds.Game_HotspotHit != ds.Game_PrevHotspotHit)
            draw_hint(ds.Game_HotspotHit);
        ds.Mouse_HotPushable = hit_pushable(ds.Game_HotspotHit);
    } while (!ds.Game_ScreenSwitch && !ds.Mouse_QuitFlag
             && (ds.Mouse_Buttons == 0 || ds.Game_HotspotHit == 0
                 || (ds.Mouse_HotPushable && !ds.Mouse_ButtonsChanged))
             && (ds.Mouse_Buttons != 0 || ds.Mouse_HotPushable || !ds.Mouse_ButtonsChanged
                 || ds.Game_HotspotHit != ds.Game_PrevHotspotHit)
             && (ds.Mouse_CursorHidden == 0 || ds.Mouse_HeldRight != 0));
    return true;
}

// Phase B: frames while the button stays down; the click counts only if it
// is released over the hotspot it was pressed on.
static bool wait_for_release(void)
{
    do {
        Sound_FreeSampleIfIdle();
        if (ds.Game_ClockRunning)
            ds.Game_Ticks++;
        if (ds.Game_CurrentScreen != 29 && ds.Game_Ticks > 100)
            advance_hour(true);
        Mouse_Poll();
        if (ds.Cheat_Requested)
            Cheat_Apply();
        if (!wait_retrace())
            return false;
        Gfx_PaletteFlashTick();
        animate_screens();
    } while (ds.Mouse_Buttons != 0 && !ds.Game_ScreenSwitch);
    hit_test(&ds.Game_ReleaseHotspot);
    if (ds.Game_ReleaseHotspot != ds.Game_PressHotspot) {
        ds.Game_HotspotHit = 0;
        Gfx_DrawStatusText(LIT(""));
    }
    return true;
}

// --- Leaving a screen -------------------------------------------------------------

static bool leaving(int16_t screen)
{
    return ds.Game_CurrentScreen != screen && ds.Game_PrevScreen == screen && ds.Game_ScreenEntered;
}

static void free_left_screen_buffers(void)
{
    if (leaving(1) && ds.Command_MainFace != 0) {
        System_FreeMem(ds.Command_MainFace, 0xE600);
        System_FreeMem(ds.Command_Heroes, 0x4C40);
        System_FreeMem(ds.Command_MainA5, 0x5280);
        System_FreeMem(ds.Command_MainA4, 0x5B00);
        Command_FreeRoomAnim();
    }
    if (leaving(2))
        System_FreeMem(ds.RawText_SzFace, 0x7B0);
    if (leaving(3))
        System_FreeMem(ds.Research_IconBuffer, 0x6901);
    if (leaving(4))
        System_FreeMem(ds.Droids_DigitsPic, 0x8980);
    if (leaving(5)) {
        ds.VecLoad_Spinning = 0;
        VecLoad_FreeVecBuffers();
        System_FreeMem(ds.VecLoad_Buffer, 0xAF00);
        System_FreeMem(ds.RawText_SzTalal, 0x1DAB);
    }
    if (leaving(7)) {
        System_FreeMem(ds.PlanetInfo_Sprites, 0x6900);
        System_FreeMem(ds.PlanetInfo_Backdrop, 0xBCC0);
        System_FreeMem(ds.PlanetInfo_Icons, 24000);
    }
    if (leaving(8))
        System_FreeMem(ds.RawText_SzFaj, 0x1440);
    if (leaving(12))
        System_FreeMem(ds.SaveLoad_SlotTable, 0x108);
    if (leaving(16)) {
        System_FreeMem(ds.ShipInfo_Backdrop, 0xBCC0);
        System_FreeMem(ds.ShipInfo_Sprites, 0x5000);
    }
    if (leaving(12)) {
        Anim_FreeMainAnim(0x11);
        System_FreeMem(ds.Anim_Record, 0x349);
        System_FreeMem(ds.Anim_Script, 0x230);
        System_FreeMem(ds.SaveLoad_BufFrame, 0x1324);
        System_FreeMem(ds.SaveLoad_BufBack, 0xBCC0);
        System_FreeMem(ds.SaveLoad_BufButtons, 0x5140);
    }
    if (leaving(17)) {
        System_FreeMem(ds.ShipMove_AnimSheet, 0x9100);
        System_FreeMem(ds.ShipMove_Backdrop, 0xBCC0);
        System_FreeMem(ds.ShipMove_Stars, 0x1E1B);
    }
    if (leaving(18)) {
        System_FreeMem(ds.PlanetInfo_Sprites, 0x6900);
        System_FreeMem(ds.PlanetInfo_Backdrop, 0xBCC0);
        System_FreeMem(ds.PlanetInfo_Icons, 24000);
    }
    if (leaving(19)) {
        if (ds.Game_CurrentScreen != 32) {
            System_FreeMem(ds.GrWarUnits_UnitLists[0], 0x333);
            System_FreeMem(ds.GrWarUnits_UnitLists[1], 0x333);
        }
        System_FreeMem(ds.GrWar_UnitIcons, 0x3C00);
        if (ds.Game_CurrentScreen == 7)
            Music_ApplyMusicTrack();
    }
    if (leaving(20)) {
        int16_t t = ds.Surface_TileSet;
        System_FreeMem(ds.Surface_RadarColors, 0x820);
        if (ds.Surface_RadarDouble == 0)
            System_FreeMem(ds.Surface_RadarBuf, (uint16_t)(ds.Surface_MapWidth * ds.Surface_MapHeight));
        else
            System_FreeMem(ds.Surface_RadarBuf,
                           (uint16_t)(ds.Surface_MapWidth * 4 * ds.Surface_MapHeight));
        System_FreeMem(ds.Surface_BufA, 0x24FF);
        System_FreeMem(ds.Surface_BufC, 0xF000);
        if (ds.Surface_FanimHeight[t - 1] != 0)
            System_FreeMem(ds.Surface_BufD, (uint16_t)(ds.Surface_FanimHeight[t - 1] * 0x140));
        System_FreeMem(ds.Surface_BufB, (uint16_t)(ds.Surface_FelszHeight[t - 1] * 0x140));
        System_FreeMem(ds.Surface_BuildingLayer, 0x2A30);
        System_FreeMem(ds.Surface_OccupyLayer, 0x1518);
        System_FreeMem(ds.Surface_TileLayer, 0x1518);
    }
    if (leaving(22))
        System_FreeMem(ds.Groups_IconBuffer, 0x2300);
    if (leaving(23)) {
        System_FreeMem(ds.Pub_PiratesSheet, 0xAF00);
        System_FreeMem(ds.Pub_AnimSheet, 64000);
    }
    if (leaving(24))
        System_FreeMem(ds.Advisor_MenuLines, 0x131);
    if (leaving(25))
        System_FreeMem(ds.Pub_Lines, 0x500);
    if (leaving(29)) {
        System_FreeMem(ds.SpWar_View, 0x5E60);
        System_FreeMem(ds.SpWar_Background, 0x5E60);
        System_FreeMem(ds.SpWar_Sprites, 0xF00);
        System_FreeMem(ds.SpWar_Face, 0x1180);
        System_FreeMem(ds.SpWar_AttackerList, 0x177C);
        System_FreeMem(ds.SpWar_DefenderList, 0x177C);
        System_FreeMem(ds.Anim_Record, 0x349);
        System_FreeMem(ds.Anim_Script, 0x230);
        if (ds.Game_CurrentScreen == 7)
            Music_ApplyMusicTrack();
    }
    if (leaving(32)) {
        System_FreeMem(ds.GrWarUnits_UnitLists[0], 0x333);
        System_FreeMem(ds.GrWarUnits_UnitLists[1], 0x333);
        System_FreeMem(ds.GrWar_LostFrames, 0x2860);
        if (ds.Game_CurrentScreen == 7)
            Music_ApplyMusicTrack();
    }
    if (leaving(33))
        Music_RestartMainTheme();
    if (leaving(34)) {
        System_FreeMem(ds.AlienTalk_QuestionBuf, 0x300);
        Music_RestartMainTheme();
    }
    if (leaving(37))
        System_FreeMem(ds.PlanetList_Entries, 0x4B0);
}

// --- Screen 20: planet surface ---------------------------------------------------

// Map cell under the click; `pre` adds 1 before the wrap, `post` after.
static int16_t click_cell_x(int16_t pre, int16_t post)
{
    int16_t c = (int16_t)((int16_t)(ds.Mouse_ClickX - 0x5D) / 16 + pre + ds.Surface_ScrollX);
    return (int16_t)(c % (int16_t)ds.Surface_MapWidth + post);
}

static int16_t click_cell_y(int16_t pre, int16_t post)
{
    int16_t c = (int16_t)((int16_t)(ds.Mouse_ClickY - 0x35) / 16 + pre + ds.Surface_ScrollY);
    return (int16_t)(c % (int16_t)ds.Surface_MapHeight + post);
}

static void surface_enter(void)
{
    if (ds.Game_PrevScreen == 1) {
        CUR_SYS = 1;
        CUR_PLANET(1) = 5;  // CurPlanet[0]/CurMoon[0] in the decompile: New Earth
        CUR_MOON(1) = 0;
    }
    if (ds.Game_PrevScreen == 17 || ds.Game_PrevScreen == 22 || ds.Game_PrevScreen == 16) {
        TGroup *g = Groups_Selected();
        CUR_SYS = g->System;
        CUR_PLANET(CUR_SYS) = g->Planet;
        CUR_MOON(CUR_SYS) = g->Moon;
    }
    PlanetInfo_SelectCurBody();
    TBody *body = cur_body();
    GfxPath_SetToolbar(0x14);
    if (body->Owner == 1 && body->Colony != 0)
        GfxPath_AddToolbarIcon(0x42);
    if (ds.Game_PrevScreen == 17)
        GfxPath_AddToolbarIcon(0x10);
    Satellite_AddBodyActionItems(CUR_SYS, CUR_PLANET(CUR_SYS), CUR_MOON(CUR_SYS),
                                 (bool *)&ds.Main_HasBuilderPlant);
    GfxPath_ShowToolbar();
    body = cur_body();
    ds.Surface_TileSet = ds.Surface_Tileset[(int8_t)body->SurfaceType - 1];
    ds.Surface_MapVariant = (int8_t)body->_16;
    ds.Surface_CanBuild = ds.Galaxy_SurfaceColonisable[ds.Surface_TileSet - 1] != 0
                          && body->Colony != 0 && cur_body()->Owner == 1;
    ds.Surface_FelszTiles = (uint16_t)ds.Surface_FelszTileCount[ds.Surface_TileSet - 1];
    ds.Surface_Mode = 1;
    ds.Surface_AnimTileFrame = 3;
    ds.Surface_SelKind = 1;
    Surface_EnterSurface();
    Surface_SetupSurfaceHotspots();
    if (ds.Game_PrevScreen != 4) {
        ds.Surface_ScrollX = (int16_t)((int16_t)ds.Surface_MapWidth / 2 - 7);
        ds.Surface_ScrollY = (int16_t)((int16_t)ds.Surface_MapHeight / 2 - 4);
    }
    Surface_DrawBuildingPreview(ds.Surface_SelKind);
    Surface_RegisterBuildingAnims(CUR_SYS, CUR_PLANET(CUR_SYS), CUR_MOON(CUR_SYS));
    ds.Surface_BlinkOn = 1;
    Surface_DrawMapView();
    Surface_DrawKindArrows();
    Surface_RebuildRadar();
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
    RawText_Nop();
}

// Clicks on the map (hotspot 0x15) with the left button: place, select or
// destroy a building.
static void surface_map_click(void)
{
    ds.Surface_PrevKind = ds.Surface_SelKind;
    if (ds.Surface_Mode == 2) {
        Surface_UpdateModeForCell(click_cell_x(0, 1), click_cell_y(0, 1));
        if (ds.Surface_Mode == 2) {
            Surface_BuildOnCurrentPlanet(ds.Surface_SelKind, click_cell_x(1, 0), click_cell_y(1, 0));
            add_credits(-building_type(ds.Surface_SelKind)->Cost);
            Gfx_DrawCredits();
            ds.Surface_Mode = 4;
            Surface_ShowBuildingInfoView();
            if (ds.Surface_PrevKind != ds.Surface_SelKind)
                Surface_DrawBuildingPreview(ds.Surface_SelKind);
            Surface_SetupSimpleInfoHotspots();
            if (ds.Surface_SelKind == 7) {
                uint16_t r = System_Random(500);
                cur_body()->Population = (int32_t)((uint32_t)cur_body()->Population + (uint16_t)(r + 800));
            }
        } else {
            ds.Surface_Mode = 2;
            Sound_PlayButtonSfx(LIT("hiba"));
            ds.Mouse_HeldPhase = 1;
            Surface_DrawMapView();
        }
    } else if (ds.Surface_Mode == 1) {
        Surface_UpdateModeForCell(click_cell_x(0, 1), click_cell_y(0, 1));
        if (ds.Surface_Mode == 4) {
            if ((ds.Surface_SelKind == 4 || ds.Surface_SelKind == 0x19)
                && building(ds.Surface_SelBuilding)->Health == 0) {
                Sound_PlayButtonSfx(LIT("mine"));
                ds.Game_CurrentScreen = 4;
                ds.Game_ScreenSwitch = 1;
            } else {
                Sound_PlayButtonSfx(LIT("x"));
                Surface_DrawKindArrows();
                Surface_ShowBuildingInfoView();
                if (ds.Surface_PrevKind != ds.Surface_SelKind)
                    Surface_DrawBuildingPreview(ds.Surface_SelKind);
                Surface_SetupSimpleInfoHotspots();
            }
        }
    } else if (ds.Surface_Mode == 6) {
        Surface_UpdateModeForCell(click_cell_x(0, 1), click_cell_y(0, 1));
        if (ds.Surface_Mode == 6 && ds.Surface_SelKind != 1
            && building(ds.Surface_SelBuilding)->Health == 0) {
            Sound_PlayButtonSfx(LIT("destruct"));
            Surface_DeleteBuilding(ds.Surface_SelBuilding);
            PlanetInfo_BalanceBuildingPower(CUR_SYS, CUR_PLANET(CUR_SYS), CUR_MOON(CUR_SYS));
            Surface_LoadMap();
            Surface_RegisterBuildingAnims(CUR_SYS, CUR_PLANET(CUR_SYS), CUR_MOON(CUR_SYS));
            Surface_RebuildRadar();
            Surface_DrawMapView();
            add_credits(-2000);
            Gfx_DrawCredits();
        } else {
            Sound_PlayButtonSfx(LIT("x"));
        }
        ds.Surface_SelKind = ds.Surface_PrevKind;
        ds.Surface_Mode = 1;
    }
    if (ds.Surface_Mode == 2 || ds.Surface_Mode == 6)
        Surface_BlinkModeButton();
    else
        Surface_DrawModeButtons();
    ds.Game_HotspotHit = 0;
}

// Can building kind k be chosen on the current body? (1000:1bd3..1c70)
static bool kind_selectable(int16_t k)
{
    TBody *body = cur_body();
    if (k == 4 && body->_03 == 0)
        return false;
    if (k == 5 && (body->_03 == 0 || body->Deposits[0] < 10))
        return false;
    const TBuildingType *bt = building_type(k);
    if (bt->NeedProduct == 0) {
        if ((int32_t)ds.Game_StaffExp[1] < (int32_t)(int8_t)bt->_10)
            return false;  // falls into the product check with product 0
    } else if ((int8_t)bt->NeedProduct < 1
               || ds.Research_Products[(int8_t)bt->NeedProduct - 1].State != 5) {
        return false;
    }
    return bt->_12[1 + (int8_t)body->SurfaceType] != 0;
}

static void surface_build_button(void)
{
    const TBuildingType *bt = building_type(ds.Surface_SelKind);
    Surface_DrawModeButtons();
    ds.Game_HotspotHit = 0;
    if (ds.Surface_Mode == 2) {
        Sound_PlayButtonSfx(LIT("x"));
        ds.Surface_Mode = 1;
        Surface_DrawModeButtons();
        return;
    }
    int16_t sys = CUR_SYS, planet = CUR_PLANET(sys), moon = CUR_MOON(sys);
    ds.Main_HasBuilderPlant = Surface_CountBuildings(0x16, sys, planet, moon, true) > 0;
    ds.Main_HasVehiclePlant = Surface_CountBuildings(0x17, sys, planet, moon, true) > 0;
    if (bt->Cost > ds.Game_Credits) {
        Sound_PlayButtonSfx(LIT("hiba"));
        Gfx_FlashCredits();
    } else if (ds.Game_StaffExp[1] == 0) {
        Message_ShowMessageNoLog(LIT(" You have no builder! "));
    } else if (ds.Surface_SelKind == 1) {
        Message_ShowMessageNoLog(LIT(" You cannot have two command centres built! "));
    } else if (ds.Surface_SelKind == 0x19) {
        Message_ShowMessageNoLog(LIT(" You already have a colony! "));
    } else if (bt->_12[0] != 0 && ds.Main_HasBuilderPlant == 0) {
        Message_ShowMessageNoLog(LIT(" This building needs a builder plant! "));
    } else if (bt->_12[1] != 0 && ds.Main_HasVehiclePlant == 0) {
        Message_ShowMessageNoLog(LIT(" This building needs a vehicle plant! "));
    } else if ((int32_t)ds.Game_StaffHired[1] >= (int32_t)(int8_t)bt->_10) {
        Sound_PlayButtonSfx(LIT("build"));
        ds.Surface_Mode = 2;
        ds.Surface_BlinkCounter = 0;
        Surface_BlinkModeButton();
    } else {
        PString msg;
        // Staff::Names[2, StaffHired[2]]; none hired reads Names[1, 3].
        const PStr18 *name = &ds.Staff_Names[0][0] + 2 + ds.Game_StaffHired[1];
        System_StrLoad(&msg, (const PString *)name);
        System_StrConcat(&msg, PSTR(" is not able to construct this building"));
        Message_ShowMessageNoLog(ps(&msg));
    }
}

static void screen_surface(void)
{
    ds.Game_ClockRunning = 1;
    if (ds.Game_ScreenEntered)
        surface_enter();
    Satellite_HandleBodyActions();
    if (ds.Satellite_ActionDone) {
        Surface_RebuildRadar();
        Surface_DrawBuildingPreview(ds.Surface_SelKind);
        ds.Surface_BlinkOn = 1;
        if (ds.Surface_Mode < 3)
            Surface_DrawMapView();
        if (ds.Surface_Mode == 6)
            Surface_DrawMapView();
    }
    if (ds.Game_MenuCommand == 0x2E) {
        CUR_MOON(CUR_SYS) = 0;
        ds.Game_ScreenSwitch = 1;
        ds.Game_CurrentScreen = 7;
    }
    if (ds.Game_MenuCommand == 0x2B) {
        ds.Game_ScreenSwitch = 1;
        ds.Game_CurrentScreen = 16;
    }
    if (ds.Game_MenuCommand == 0x42) {
        ds.Game_ScreenSwitch = 1;
        ds.Game_CurrentScreen = 22;
    }
    int16_t mode = ds.Surface_Mode;
    if (ds.Game_HotspotHit == 0x15 && ds.Mouse_Buttons && (mode == 1 || mode == 2 || mode == 6)
        && ds.Mouse_HeldLeft)
        surface_map_click();

    ds.Surface_EdgeScrollDir = 0;
    mode = ds.Surface_Mode;
    if (ds.Game_HotspotHit == 0x15 && ds.Mouse_Buttons && !ds.Mouse_HeldLeft
        && (mode == 1 || mode == 2 || mode == 6)) {
        int16_t col = (int16_t)((int16_t)(ds.Mouse_ClickX - 0x5D) / 16 + 1);
        int16_t row = (int16_t)((int16_t)(ds.Mouse_ClickY - 0x35) / 16 + 1);
        ds.Surface_EdgeScrollDir = ds.Surface_EdgeScroll[row - 1][col - 1];
    }
    if (ds.Game_HotspotHit == 0x16 && ds.Mouse_Buttons && (mode == 1 || mode == 2 || mode == 6)) {
        // Click on the radar: centre the view there.
        ds.Surface_PrevScrollX = ds.Surface_ScrollX;
        ds.Surface_PrevScrollY = ds.Surface_ScrollY;
        if (ds.Surface_RadarDouble) {
            ds.Surface_ScrollX = (int16_t)((ds.Mouse_ClickX - ds.Surface_RadarX) / 2 - 6);
            ds.Surface_ScrollY = (int16_t)((ds.Mouse_ClickY - ds.Surface_RadarY) / 2 - 3);
        }
        if (!ds.Surface_RadarDouble) {
            ds.Surface_ScrollX = (int16_t)(ds.Mouse_ClickX - ds.Surface_RadarX - 6);
            ds.Surface_ScrollY = (int16_t)(ds.Mouse_ClickY - ds.Surface_RadarY - 3);
        }
        if (ds.Surface_ScrollX < 0)
            ds.Surface_ScrollX = 0;
        if ((int16_t)ds.Surface_MapWidth - 14 < ds.Surface_ScrollX)
            ds.Surface_ScrollX = (int16_t)(ds.Surface_MapWidth - 14);
        if (ds.Surface_ScrollY < 0)
            ds.Surface_ScrollY = 0;
        if ((int16_t)ds.Surface_MapHeight - 9 < ds.Surface_ScrollY)
            ds.Surface_ScrollY = (int16_t)(ds.Surface_MapHeight - 9);
        bool moved = ds.Surface_ScrollX != ds.Surface_PrevScrollX
                     || ds.Surface_ScrollY != ds.Surface_PrevScrollY;
        if (moved)
            ds.Surface_BlinkOn = 1;
        if (moved)
            Surface_DrawMapView();
        if (moved)
            Surface_DrawRadar();
    }

    // Previous / next building kind.
    if ((ds.Game_HotspotHit == 0x17 || ds.Game_HotspotHit == 0x18)
        && (mode == 1 || mode == 2 || mode == 3 || mode == 6) && ds.Surface_CanBuild) {
        ds.Main_PrevKind = ds.Surface_SelKind;
        if (ds.Game_HotspotHit == 0x17) {
            int16_t last = (int16_t)(ds.Surface_SelKind - 1);
            for (ds.Main_I = 1; ds.Main_I <= last; ds.Main_I++)
                if (kind_selectable((int16_t)ds.Main_I))
                    ds.Surface_SelKind = (int16_t)ds.Main_I;
        } else {
            int16_t last = (int16_t)(ds.Surface_SelKind + 1);
            for (ds.Main_I = 0x19; ds.Main_I >= last; ds.Main_I--)
                if (kind_selectable((int16_t)ds.Main_I))
                    ds.Surface_SelKind = (int16_t)ds.Main_I;
        }
        if (ds.Main_PrevKind != ds.Surface_SelKind) {
            Sound_PlayButtonSfx(LIT("x"));
            if (ds.Surface_Mode == 2)
                ds.Surface_Mode = 1;
            if (ds.Surface_Mode == 6)
                ds.Surface_Mode = 1;
            Surface_DrawModeButtons();
            Surface_DrawBuildingPreview(ds.Surface_SelKind);
            Surface_DrawKindArrows();
            if (ds.Surface_Mode == 3)
                Surface_ShowBuildingPicture();
        }
    }
    if (ds.Game_HotspotHit == 0x19 && ds.Surface_CanBuild) {
        ds.Surface_Mode = 3;
        Sound_PlayButtonSfx(LIT("x"));
        Surface_DrawModeButtons();
        Surface_ShowBuildingInfoView();
        Surface_SetupInfoHotspots();
    }

    // Scroll buttons and edge scrolling.
    int16_t dir = ds.Surface_EdgeScrollDir;
    ds.Surface_Scrolled = 0;
    if ((ds.Game_HotspotHit == 0x1A || dir == 3 || dir == 5 || dir == 7) && ds.Surface_ScrollX > 0
        && ds.Mouse_Buttons) {
        ds.Surface_ScrollX--;
        ds.Surface_Scrolled = 1;
    }
    if ((ds.Game_HotspotHit == 0x1B || dir == 4 || dir == 6 || dir == 8)
        && ds.Surface_ScrollX < (int16_t)ds.Surface_MapWidth - 14 && ds.Mouse_Buttons) {
        ds.Surface_ScrollX++;
        ds.Surface_Scrolled = 1;
    }
    if ((ds.Game_HotspotHit == 0x1C || dir == 1 || dir == 5 || dir == 6) && ds.Surface_ScrollY > 0
        && ds.Mouse_Buttons) {
        ds.Surface_ScrollY--;
        ds.Surface_Scrolled = 1;
    }
    if ((ds.Game_HotspotHit == 0x1D || dir == 2 || dir == 7 || dir == 8)
        && ds.Surface_ScrollY < (int16_t)ds.Surface_MapHeight - 9 && ds.Mouse_Buttons) {
        ds.Surface_ScrollY++;
        ds.Surface_Scrolled = 1;
    }
    if (ds.Surface_Scrolled) {
        ds.Surface_BlinkOn = 1;
        Surface_DrawMapView();
        Surface_DrawRadar();
    }

    if (ds.Game_HotspotHit == 0x1E && ds.Surface_CanBuild)
        surface_build_button();
    if (ds.Game_HotspotHit == 0x1F && ds.Surface_CanBuild) {
        Surface_DrawModeButtons();
        ds.Game_HotspotHit = 0;
        Sound_PlayButtonSfx(LIT("x"));
        if (ds.Surface_Mode == 6) {
            ds.Surface_Mode = 1;
        } else {
            ds.Surface_Mode = 6;
            ds.Surface_BlinkCounter = 0;
            Surface_BlinkModeButton();
        }
    }
    if ((ds.Game_HotspotHit == 0x15 || ds.Game_HotspotHit == 0x16)
        && (ds.Surface_Mode == 3 || ds.Surface_Mode == 4)) {
        Sound_PlayButtonSfx(LIT("x"));
        ds.Surface_BlinkOn = 1;
        Surface_DrawMapView();
        Surface_SetupSurfaceHotspots();
        ds.Surface_Mode = 1;
    }
}

// --- Screens 23/25: pub ------------------------------------------------------------

static void screen_pub(void)
{
    ds.Game_ClockRunning = 1;
    if (ds.Game_ScreenEntered) {
        Pub_LoadPubGraphics();
        Pub_SetupPubHotspots();
    }
    if ((uint16_t)ds.Game_HotspotHit > 0x14 && (uint16_t)ds.Game_HotspotHit < 0x1F) {
        ds.Pub_Person = (int16_t)(ds.Game_HotspotHit - 0x14);
        Sound_PlayButtonSfx(LIT("talk"));
        ds.Game_CurrentScreen = 25;
        ds.Game_ScreenSwitch = 1;
    }
}

static void pub_dialog_enter(void)
{
    TPubPerson *p = ds.Pub_People;
    ds.Pub_MenuIdx = 1;
    ds.Pub_ReplyLine = 0;
    int16_t who = ds.Pub_Person;
    if (who == 1 && ds.Pub_StoryFlags[0]) {
        if (p[1].State == 2 && p[1].Timer < 1)
            ds.Pub_MenuIdx = 3;
        else
            ds.Pub_MenuIdx = 2;
    }
    if (who == 2 && ds.Pub_StoryFlags[2] && !ds.Pub_StoryFlags[1])
        ds.Pub_MenuIdx = 2;
    if (who == 2 && ds.Pub_StoryFlags[1])
        ds.Pub_MenuIdx = ds.Pub_AnyTopic == 0 ? 10 : 3;
    if (who == 3 && ds.Pub_StoryFlags[11])
        ds.Pub_MenuIdx = 2;
    if (who == 4 && !ds.Pub_StoryFlags[3] && p[8].State == 2)
        ds.Pub_MenuIdx = 3;
    if (who == 5 && ds.Pub_StoryFlags[4] && !ds.Pub_StoryFlags[5] && ds.Races_Races[5].Status == 2
        && ds.Races_Races[6].Status == 2)
        ds.Pub_MenuIdx = 2;
    if (who == 6 && p[1].State == 2 && !ds.Pub_StoryFlags[2])
        ds.Pub_MenuIdx = 2;
    if (who == 6 && !ds.Pub_StoryFlags[4]
        && (ds.Galaxy_SystemStatus[4] == 1 || ds.Galaxy_SystemStatus[5] == 1)
        && ds.Intel_Race7to10Done == 0)
        ds.Pub_MenuIdx = 4;
    if (who == 6 && p[8].State == 2 && !ds.Pub_StoryFlags[3] && !ds.Pub_StoryFlags[9])
        ds.Pub_MenuIdx = 3;
    if (who == 7 && ds.Pub_StoryFlags[10])
        ds.Pub_MenuIdx = ds.Pub_StoryFlags[6] == 0 ? 6 : 2;
    if (who == 7 && ds.Pub_StoryFlags[10] && ds.Pub_StoryFlags[7])
        ds.Pub_MenuIdx = 3;
    if (who == 9 && ds.Pub_StoryFlags[3]) {
        if (ds.Pub_StoryFlags[8] == 0)
            ds.Pub_MenuIdx = 2;
        else
            ds.Pub_MenuIdx = Pub_FindActiveDeal() < 1 ? 6 : 3;
    }
    Pub_LoadPersonPortrait();
    Pub_BuildQuestionMenu();
    Pub_DrawQuestionMenu();
    Pub_SetupTalkHotspot();
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
    ds.Pub_Replied = 0;
}

// Pub::Deals[deal].Kind; deal 0 (none active) reads the bytes before Deals,
// as the original does.
static uint8_t pub_deal_kind(int16_t deal)
{
    const TPubDeal *d = ds.Pub_Deals + (deal - 1);
    return d->Kind;
}

// The effects of the chosen question (1000:24d3..2a5e).
static void pub_answer(void)
{
    TPubPerson *p = ds.Pub_People;
    int16_t who = ds.Pub_Person, a = ds.Pub_QuestionLine;
    if (who == 1) {
        if (a == 1) {
            ds.Pub_StoryFlags[0] = 1;
            System_StrStore(PSTR("Spy-Hunter   "), (PString *)&p[0].Name, 13);
        } else if (a == 2) {
            ds.Pub_ReplyLine = ds.Pub_StoryFlags[1] == 0 ? 4 : 6;
        } else if (a == 4) {
            p[1].State = 1;
            ds.Intel_System3Timer = 200;
        }
    }
    ds.Main_I = 0;
    if (who == 2) {
        if (a == 8) {
            ds.Pub_StoryFlags[1] = 1;
            ds.Galaxy_SystemStatus[2] = 0;
        } else if (a == 5) {
            add_credits(-30000);
        } else if (a == 6) {
            add_credits(-40000);
        } else if (a == 7) {
            add_credits(-50000);
        } else {
            static const struct { int16_t answer, track; } tracks[] = {
                {10, 3}, {11, 4}, {12, 5}, {13, 6}, {15, 7}, {16, 8}, {17, 9}, {18, 10}, {0x1A, 0xC},
            };
            bool found = false;
            for (size_t i = 0; i < sizeof tracks / sizeof tracks[0]; i++) {
                if (a == tracks[i].answer) {
                    ds.Pub_OfferItem = tracks[i].track;
                    found = true;
                    break;
                }
            }
            if (!found && a == 0x17) {
                add_credits(-ds.Pub_OfferPrice);
                p[1].Timer = (int16_t)(System_Random(100) + 100);
            }
        }
    }
    if (who == 3)
        ds.Pub_StoryFlags[11] = 1;
    ds.Main_I = ds.Races_Races[1].Status > 0;
    if (ds.Intel_Race2Done4 && !ds.Intel_MorgrulAttack2Done)
        ds.Main_I = 2;
    if (ds.Galaxy_SystemStatus[1] == 1 && ds.Races_Races[3].Status == 0)
        ds.Main_I = 3;
    if (ds.Galaxy_SystemStatus[3] == 1 && ds.Intel_Pub4HintOff == 0)
        ds.Main_I = 4;
    if (who == 4) {
        if (a == 1) {
            ds.Pub_ReplyLine = 2;
            if ((uint16_t)ds.Main_I != 0)
                ds.Pub_ReplyLine = 3;
        } else if (a == 3) {
            add_credits(-500);
            ds.Pub_ReplyLine = (int16_t)(ds.Main_I + 4);
            if ((uint16_t)ds.Main_I == 3)
                ds.Galaxy_PlanetStatus[1][4] = 0;
        } else if (a == 5) {
            ds.Pub_StoryFlags[3] = 1;
            System_StrStore(PSTR("Pirate Leader"), (PString *)&p[8].Name, 13);
            add_credits(-500);
        }
    }
    if (who == 5) {
        if (a == 5) {
            ds.Pub_StoryFlags[5] = 1;
            p[4].Timer = (int16_t)(System_Random(100) + 100);
            p[4].Deal = 1;
        } else if (a == 6) {
            ds.Pub_StoryFlags[5] = 1;
            p[4].Timer = (int16_t)(System_Random(100) + 100);
            p[4].Deal = 2;
        }
    }
    if (who == 6) {
        if (a == 8) {
            ds.Pub_StoryFlags[2] = 1;
        } else if (a == 10) {
            ds.Pub_StoryFlags[2] = 1;
            System_StrStore(PSTR("Spy          "), (PString *)&p[1].Name, 13);
        } else if (a == 11) {
            ds.Pub_StoryFlags[9] = 1;
        } else if (a == 13) {
            ds.Pub_StoryFlags[4] = 1;
            add_credits(-5000);
        } else if (a == 6 || a == 15) {
            ds.Pub_ReplyLine = 3;
            if (ds.Races_Races[2].Status == 0)
                ds.Pub_ReplyLine = 8;
            if (ds.Races_Races[3].Status == 0)
                ds.Pub_ReplyLine = 8;
            if (ds.Races_Races[0].Status == 0)
                ds.Pub_ReplyLine = 7;
        }
    }
    if (who == 7) {
        if (a == 4 || a == 6) {
            add_credits(-100000);
            p[6].Timer = (int16_t)(System_Random(100) + 100);
            p[6].Deal = 2;
        } else if (a == 5) {
            ds.Pub_StoryFlags[7] = 1;
        }
    }
    if (who == 7) {
        ds.Pub_StoryFlags[10] = 1;
        System_StrStore(PSTR("Bounty-Hunter"), (PString *)&p[6].Name, 13);
    }
    if (who == 9) {
        if (a == 2) {
            ds.Pub_StoryFlags[8] = 1;
            Pub_StartPubDealWindows();
        } else if (a == 5) {
            ds.Pub_ReplyLine = 6;
            p[8].Timer = (int16_t)(System_Random(100) + 100);
            p[8].Deal = (uint8_t)(Pub_FindActiveDeal() + 10);
        } else if (a == 6 || a == 7 || a == 8) {
            static const int32_t price[] = {10000, 50000, 100000};
            if (pub_deal_kind(Pub_FindActiveDeal()) < a - 4) {
                ds.Pub_ReplyLine = 6;
                add_credits(-price[a - 6]);
                p[8].Timer = (int16_t)(System_Random(100) + 100);
                p[8].Deal = (uint8_t)Pub_FindActiveDeal();
            }
        }
    }
    if (who == 10) {
        if (a == 3) {
            add_credits(-100000);
        } else if (a == 4) {
            add_credits(-150000);
        } else if (a == 5) {
            add_credits(-200000);
        } else if (a == 8) {
            p[9].State = 1;
        } else if (a == 9 || a == 10) {
            ds.Pub_StoryFlags[6] = 1;
            ds.Intel_Pub10Done = 1;
            p[9].State = 1;
        }
    }
}

static void screen_pub_dialog(void)
{
    ds.Game_ClockRunning = 0;
    Pub_UpdateTopicAvailability();
    if (ds.Game_ScreenEntered)
        pub_dialog_enter();
    if (ds.Game_MenuCommand == 0x36) {
        ds.Game_CurrentScreen = 23;
        ds.Game_ScreenSwitch = 1;
    }
    if (ds.Game_HotspotHit == 0x15 && ds.Mouse_Buttons && !ds.Pub_Replied) {
        ds.Pub_HoverCol = 0xFFFF;
        ds.Pub_HoverRow = -1;
        if (ds.Pub_HoverItem > 0) {
            // CurMenu.Items[line]; the trader's menu 6 can list 6 questions, the
            // sixth in ItemQty[1] (1fd2:0788), so index the bytes.
            ds.Pub_QuestionLine = ((const int8_t *)&ds.Pub_CurMenu)[ds.Pub_HoverItem];
            ds.Pub_HoverItem = 0;
            Pub_ReadQuestionCode();
            ds.Pub_ReplyLine = ds.Pub_QuestionCode;
            pub_answer();
            Pub_ShowReply();
            ds.Pub_Replied = 1;
            ds.Game_HotspotHit = 0;
            ds.Mouse_HeldPhase = 1;
            ds.Mouse_Hotspots[0].Repeat_ = 1;
            Gfx_DrawCredits();
        }
    }
    if (ds.Game_HotspotHit == 0x15 && ds.Pub_Replied) {
        if (ds.Pub_MenuIdx == 0) {
            ds.Game_CurrentScreen = 23;
            ds.Game_ScreenSwitch = 1;
        } else {
            ds.Pub_Replied = 0;
            Pub_BuildQuestionMenu();
            Pub_DrawQuestionMenu();
            ds.Mouse_HeldPhase = 1;
            ds.Mouse_Hotspots[0].Repeat_ = 0;
        }
    }
}

// --- Screen 29: space battle ----------------------------------------------------------

static void select_battle_body(void)
{
    CUR_SYS = ds.SpWar_BattleSystem;
    CUR_PLANET(ds.SpWar_BattleSystem) = ds.SpWar_BattlePlanet;
    CUR_MOON(CUR_SYS) = ds.SpWar_BattleMoon;
}

static void screen_space_battle(void)
{
    ds.Game_ClockRunning = 0;
    if (ds.Game_ScreenEntered) {
        select_battle_body();
        PlanetInfo_SelectCurBody();
        Music_PlayTrack(LIT("space"));
        Sound_StopAndFreeSample();
        SpWar_InitSpaceFleets();
        SpWar_EnterSpaceBattle();
        SpWar_ClearHotspots();
        SpWar_LoadWarAnimDef((int16_t)(System_Random(0x19) + 1));
        // ScriptsLeft := ScriptsLeft - [Script] (a set of 0..31, low byte of Script)
        uint8_t e = (uint8_t)ds.SpWar_Script;
        if (e < 32)
            ds.SpWar_ScriptsLeft[e / 8] &= (uint8_t)~(1u << (e % 8));
    }
    if (ds.Game_MenuCommand == 0x3E) {  // retreat
        SpWar_FreeWarAnims();
        Sound_StopSbVoice();
        Sound_PlayButtonSfx(LIT("retreat"));
        GfxPath_SetToolbarAndShow(0x1E);
        ds.SpWar_BattleOver = 1;
        ds.SpWar_Retreated = 1;
        SpWar_EndSpaceBattle();
    }
    if (ds.Game_MenuCommand == 0x41) {  // battle over
        Sound_FreeWarSndBuf();
        Sound_PlayButtonSfx(LIT("endbattl"));
        if (!ds.SpWar_PlayerWon)
            Intel_SpaceBattleLost(ds.SpWar_BattleSystem, ds.SpWar_BattlePlanet);
        else
            Intel_SpaceBattleWon(ds.SpWar_BattleSystem, ds.SpWar_BattlePlanet);
        select_battle_body();
        if (ds.Sim_PlayerAttacks)
            ds.Game_CurrentScreen = ds.SpWar_GroundBattle == 0 || !ds.SpWar_PlayerWon ? 7 : 19;
        if (ds.Sim_PlayerAttacks == 0)
            ds.Game_CurrentScreen = ds.SpWar_GroundBattle == 0 || ds.SpWar_PlayerWon ? 7 : 19;
        ds.Game_ScreenSwitch = 1;
        if (ds.SpWar_PlayerWon && ds.Intel_FirstBattleWon == 0) {
            ds.Intel_FirstBattleWon = 1;
            Intel_ScheduleIdea(0xD, 0x14, 0x14);
            Intel_ScheduleIdea(0x11, 0x5A, 0x32);
        }
        if (ds.SpWar_PlayerWon && ds.Intel_MorgrulAttack2Open) {
            ds.Intel_MorgrulAttack2Open = 0;
            Intel_ScheduleIdea(0x12, 3, 0);
            Message_ShowMessageLine(0x10);
            AlienTalk_StartAlienScene(3);
            ds.Game_StoryStage = 3;
        }
        if (ds.SpWar_PlayerWon && ds.Intel_MorgrulAttack3Open) {
            ds.Intel_MorgrulAttack3Open = 0;
            Message_ShowMessageLine(0x13);
            ds.Pub_People[1].State = 2;
            ds.Intel_MorgrulAttack3Won = 1;
            Intel_ScheduleIdea(0x14, 0x1E, 10);
            Intel_ScheduleIdea(0x19, 500, 100);
            AlienTalk_StartAlienScene(3);
            ds.Game_StoryStage = 4;
        }
        if (ds.SpWar_PlayerWon && ds.SpWar_BattleSystem == 8)
            ds.Pub_People[9].State = 2;
    }
}

// --- Screen 32: ground battle --------------------------------------------------------

static uint8_t *gw_unit_record(void)
{
    return (uint8_t *)far_host(ds.GrWarUnits_UnitLists[0]) + ds.GrWar_Selected * 0x27 - 0x27;
}

static void ground_battle_won_story(void)
{
    TBody *body = cur_body();
    if (!(body->_01 == 1 && (int8_t)body->Owner > 1))
        return;
    PString msg;
    System_StrLoad(&msg, PSTR(" Your forces have eliminated '"));
    System_StrConcat(&msg, (const PString *)&Intel_Race((int8_t)body->Owner)->Name);
    System_StrConcat(&msg, PSTR("' "));
    Message_ShowMessage(ps(&msg));
    if (body->Owner == 3) {
        Message_ShowMessageLine(0x23);
        ds.Intel_MorgrulBeaten = 1;
        ds.Intel_Phoenix1Armed = 0;
        ds.Intel_Phoenix1Done = 1;
        Intel_OfferInventionForAnalysis(0x13);
        Intel_ScheduleIdea(0x13, 0x32, 0x14);
        if (ds.Races_Races[2].Status > 0)
            ds.Races_Races[2].Status = 6;
        ds.Galaxy_SystemStatus[3] = 0;
        ds.Galaxy_SystemStatus[4] = 0;
        ds.Galaxy_SystemStatus[5] = 0;
        ds.Pub_People[8].State = 2;
        AlienTalk_StartAlienScene(3);
        ds.Game_StoryStage = 5;
    }
    if (body->Owner == 12) {
        ds.Game_CurrentScreen = 36;
        ds.Game_ScreenSwitch = 1;
    }
    if (ds.ShipMove_InRestrictedArea && ds.Research_Products[0xB].State == 0) {
        Message_ShowMessageLine(0xD);
        Intel_OfferInventionForAnalysis(0xC);
    }
    uint8_t o = body->Owner;
    const TRace *r = ds.Races_Races;
    if ((o == 7 || o == 8 || o == 9 || o == 10)
        && (r[5]._0F == 0 || r[5].Status == 6 || o == 7)
        && (r[6]._0F == 0 || r[6].Status == 6 || o == 8)
        && (r[7]._0F == 0 || r[7].Status == 6 || o == 9)
        && (r[8]._0F == 0 || r[8].Status == 6 || o == 10)) {
        Message_ShowMessageLine(0x20);
        AlienTalk_StartAlienScene(5);
        ds.Game_StoryStage = 6;
        ds.Galaxy_SystemStatus[6] = 0;
        Intel_GrantInvention(0x21);
    }
    Intel_EliminateRace((int8_t)body->Owner);
}

static void screen_ground_battle(void)
{
    ds.Game_ClockRunning = 0;
    if (ds.Game_ScreenEntered) {
        select_battle_body();
        PlanetInfo_SelectCurBody();
        GrWar_InitBattleUnits();
        GrWar_EnterGroundBattle();
        GrWar_SetupBattleHotspots();
        ds.GrWar_Order = 1;
    }
    if (ds.Game_HotspotHit == 0x15) {
        if (ds.GrWar_Order == 1) {
            ds.Main_I = GrWar_FindUnitNearPoint((int16_t)(ds.Mouse_ClickX - 6),
                                                (int16_t)(ds.Mouse_ClickY - 6), 3);
            if (ds.Main_I > 0) {
                ds.GrWar_PrevSelected = ds.GrWar_Selected;
                ds.GrWar_PrevSelectedOwn = ds.GrWar_SelectedOwn;
                ds.GrWar_SelectedOwn = (int16_t)ds.Main_I < 100;
                ds.GrWar_Selected = (int16_t)ds.Main_I;
                if (!ds.GrWar_SelectedOwn)
                    ds.GrWar_Selected = (int16_t)(ds.Main_I - 100);
                GrWar_SetHotspotFlags(0);
                if (ds.GrWar_PrevSelected != ds.GrWar_Selected
                    || ds.GrWar_PrevSelectedOwn != ds.GrWar_SelectedOwn)
                    Sound_PlayGrSnd(0x14);
            }
        }
        if (ds.GrWar_Order == 3) {
            uint8_t *u = gw_unit_record();
            ds.GrWar_Order = 1;
            GrWar_SetHotspotFlags(0);
            ds.GrWar_TargetX = (int16_t)((ds.Mouse_ClickX - 0x3A) / 16);
            ds.GrWar_TargetY = (int16_t)((ds.Mouse_ClickY - 0x2E) / 16);
            if (ds.GrWar_TargetY > 8)
                ds.GrWar_TargetY = 8;
            u[0x0F] = 2;  // order: move to
            u[0x10] = (uint8_t)ds.GrWar_TargetX;
            u[0x11] = (uint8_t)ds.GrWar_TargetY;
            u[0x12] = 0;
            Sound_PlayGrSnd(0x15);
            ds.Mouse_HeldPhase = 1;
        }
        if (ds.GrWar_Order == 4) {
            uint8_t *u = gw_unit_record();
            ds.GrWar_Order = 1;
            GrWar_SetHotspotFlags(0);
            ds.Main_I = GrWar_FindUnitNearPoint(ds.Mouse_ClickX, ds.Mouse_ClickY, 2);
            if (ds.Main_I < 1) {
                Sound_PlayGrSnd(0x16);
            } else {
                u[0x0F] = 3;  // order: attack
                u[0x10] = (uint8_t)ds.Main_I;
                u[0x12] = 0;
                Sound_PlayGrSnd(0x15);
            }
            ds.Mouse_HeldPhase = 1;
        }
    }
    if (ds.Game_HotspotHit == 0x16) {
        if (ds.GrWar_Selected < 1 || ds.GrWar_Selected > 0x14 || !ds.GrWar_SelectedOwn) {
            Sound_PlayGrSnd(0x16);
        } else {
            ds.GrWar_Order = 3;
            GrWar_SetHotspotFlags(5);
            Sound_PlayGrSnd(0x1E);
        }
    }
    if (ds.Game_HotspotHit == 0x17) {
        bool ok = false;
        if (ds.GrWar_Selected > 0 && ds.GrWar_SelectedOwn) {
            if (ds.GrWar_Selected < 0x15)
                ok = true;
            else if (ds.Sim_PlayerAttacks
                     && Surface_CountBuildings(0x12, ds.SpWar_BattleSystem, ds.SpWar_BattlePlanet,
                                               ds.SpWar_BattleMoon, true) > 0)
                ok = true;
        }
        if (ok) {
            ds.GrWar_Order = 4;
            GrWar_SetHotspotFlags(5);
            Sound_PlayGrSnd(0x1F);
        } else {
            Sound_PlayGrSnd(0x16);
        }
    }
    if (ds.Game_HotspotHit == 0x18) {
        Sound_PlayGrSnd(0x16);
        ds.GrWar_Order = 1;
        GrWar_SetHotspotFlags(0);
    }
    if (ds.Game_HotspotHit == 0x19) {
        if (ds.GrWar_Order > 1)
            Sound_PlayGrSnd(0x16);
        ds.GrWar_Order = 1;
        GrWar_SetHotspotFlags(0);
    }
    if (ds.Game_MenuCommand == 0x48) {  // retreat
        Sound_StopSbVoice();
        Sound_PlayButtonSfx(LIT("retreat"));
        GfxPath_SetToolbarAndShow(0x1E);
        ds.GrWar_BattleOver = 1;
        ds.SpWar_Retreated = 1;
        GrWar_EndGroundBattle();
    }
    if (ds.Game_MenuCommand == 0x41) {  // battle over
        Sound_FreeWarSndBuf();
        Sound_PlayButtonSfx(LIT("endbattl"));
        ds.Game_ScreenSwitch = 1;
        ds.Game_CurrentScreen = 7;
        select_battle_body();
        PlanetInfo_SelectCurBody();
        if (!ds.GrWar_PlayerWon && ds.SpWar_BattleSystem == 1 && ds.SpWar_BattlePlanet == 5
            && ds.SpWar_BattleMoon == 0) {
            ds.Game_CurrentScreen = 35;  // Earth lost: death ending
            ds.Game_ScreenSwitch = 1;
        }
        if (ds.GrWar_PlayerWon)
            ground_battle_won_story();
        if (ds.GrWar_PlayerWon && ds.Intel_FirstBattleWon == 0) {
            ds.Intel_FirstBattleWon = 1;
            Intel_ScheduleIdea(0xD, 0x14, 0x14);
        }
        if (!ds.Game_ScreenSwitch && !ds.GrWar_PlayerWon && ds.Intel_FirstGroundLost == 0) {
            ds.Intel_FirstGroundLost = 1;
            Message_ShowMessageLine(0xE);
            Intel_ScheduleIdea(0x11, 5, 5);
        }
        if (ds.GrWar_PlayerWon && ds.Intel_MorgrulAttack2Open) {
            ds.Intel_MorgrulAttack2Open = 0;
            Message_ShowMessageLine(0x10);
            AlienTalk_StartAlienScene(3);
            ds.Game_StoryStage = 3;
        }
        if (ds.GrWar_PlayerWon && ds.Intel_MorgrulAttack3Open) {
            ds.Intel_MorgrulAttack3Open = 0;
            Message_ShowMessageLine(0x10);
            ds.Pub_People[1].State = 2;
            ds.Intel_MorgrulAttack3Won = 1;
            Intel_ScheduleIdea(0x14, 0x1E, 10);
            Intel_ScheduleIdea(0x19, 500, 100);
            AlienTalk_StartAlienScene(3);
            ds.Game_StoryStage = 4;
        }
        ds.GrWar_Selected = 0;
        if (ds.GrWar_PlayerWon && ds.Sim_PlayerAttacks)
            Intel_ClearBodyFull(ds.SpWar_BattleSystem, ds.SpWar_BattlePlanet, ds.SpWar_BattleMoon);
        if (!ds.GrWar_PlayerWon && ds.Sim_PlayerAttacks == 0)
            Intel_PurgeBody(ds.SpWar_BattleSystem, ds.SpWar_BattlePlanet, ds.SpWar_BattleMoon);
        if (ds.GrWar_PlayerWon && ds.Sim_PlayerAttacks)
            Intel_ClaimBodyForPlayer(ds.SpWar_BattleSystem, ds.SpWar_BattlePlanet,
                                     ds.SpWar_BattleMoon);
        if (!ds.GrWar_PlayerWon && ds.Sim_PlayerAttacks == 0)
            // BUG? the arguments are pushed as (system, planet, moon, owner)
            // against ClaimBodyForRace(owner, system, planet, moon).
            Intel_ClaimBodyForRace(ds.SpWar_BattleSystem, ds.SpWar_BattlePlanet,
                                   ds.SpWar_BattleMoon, ds.SpWar_EnemyRace);
    }
}

// --- Screen 33: story interstitial, 34: alien contact -----------------------------

static void screen_story_music(void)
{
    PString *track = (PString *)&ds.Main_SceneTrack;
    track->len = 0;
    switch (ds.AlienTalk_Scene) {
    case 1: System_StrStore(PSTR("1"), track, 0x14); break;
    case 2: System_StrStore(PSTR("5"), track, 0x14); break;
    case 3: System_StrStore(PSTR("2"), track, 0x14); break;
    case 4: System_StrStore(PSTR("5"), track, 0x14); break;
    case 5: System_StrStore(PSTR("4"), track, 0x14); break;
    case 6: System_StrStore(PSTR("3"), track, 0x14); break;
    default: break;
    }
    if (track->len != 0) {
        PString name;
        System_StrLoad(&name, PSTR("atv"));
        System_StrConcat(&name, track);
        Music_PlayTrack(ps(&name));
    }
    if (ds.AlienTalk_Scene == 4)
        Message_ShowMessageLine(0x24);
    if (ds.AlienTalk_Scene == 9)
        Message_ShowMessageLine(1);
    AlienTalk_PlayAlienScene(ds.AlienTalk_Scene);
    ds.Game_CurrentScreen = 1;
    ds.Game_PrevScreen = 33;
    ds.Game_ScreenSwitch = 1;
    ds.Game_ScreenEntered = 0;
}

static void alien_talk_answer(void)
{
    int16_t race = ds.AlienTalk_Race, q = ds.AlienTalk_QuestionLine;
    if (race == 2) {
        if (q == 4) {
            ds.Intel_Race2Done1 = 1;
            ds.Intel_DevAwayTimer = (int16_t)(System_Random(0x1E) + 0x3C);
        } else if (q == 5 || q == 6) {
            ds.Intel_Race2Done1 = 1;
            ds.Intel_DevAwayTimer = 0;
        }
    }
    if (race == 3 && q == 6) {
        Intel_OfferInventionForAnalysis(0xE);
        add_credits(-16000);
        Gfx_DrawCredits();
    }
    if (race == 4 && (q == 4 || q == 5)) {
        Intel_GrantInvention(10);
        Intel_GrantInvention(11);
        ds.Game_GroupKindAllowed[0] = 1;
    }
    if (race == 5 && q == 4) {
        Intel_OfferInventionForAnalysis(0x10);
        ds.Research_Products[0xF].Stock += 10;
    }
    if (race == 6) {
        if (q == 2) {
            ds.Intel_KallTurnCancelled = 1;
            ds.Intel_KallTurnTimer = 0;
            Intel_SetForceTarget(4, 2, 3, 3, 0);
        } else if (q == 5 && ds.Intel_MorgrulAttack3Done == 0
                   && ds.Intel_MorgrulAttack3Timer > 0) {
            ds.Intel_MorgrulAttack3Timer += 0xFA;
        }
    }
    if (race == 7) {
        int32_t *stock = &ds.Game_HomeStock[1];
        if (q == 5 || q == 8) {
            ds.Main_I = *stock;
            if (ds.Main_I > 10000)
                ds.Main_I = 10000;
            *stock -= ds.Main_I;
            add_credits(ds.Main_I * (q == 5 ? 10 : 12));
            Gfx_DrawCredits();
        } else if (q == 9) {
            *stock -= *stock / 2;
            Intel_GrantInvention(0x15);
        }
    }
    if (race == 9 && q == 3) {
        ds.Pub_People[4].State = 2;
        ds.Races_Races[4].Status = 6;
    }
    if (race == 10 && q == 6) {
        ds.Galaxy_SystemStatus[7] = 0;
        Intel_GrantInvention(0x23);
        ds.Pub_People[6].State = 2;
    }
}

static void screen_alien_talk(void)
{
    ds.Game_ClockRunning = 0;
    if (ds.Game_ScreenEntered) {
        Music_PlayTrack(LIT("talk"));
        ds.Command_BackdropShown = 1;
        if (ds.AlienTalk_Race == 6)
            AlienTalk_PlayAlienScene(10);
        ds.AlienTalk_MenuNumber = 1;
        AlienTalk_DrawAlienTalkScreen();
        AlienTalk_BuildQuestionMenu();
        AlienTalk_DrawQuestionMenu();
        AlienTalk_SetupTalkHotspots();
        Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
        ds.AlienTalk_Locked = 0;
        ds.Command_BackdropShown = 1;
    }
    if (ds.Game_HotspotHit == 0x15 && ds.Mouse_Buttons && !ds.AlienTalk_Locked) {
        ds.AlienTalk_HoverCol = 0xFFFF;
        ds.AlienTalk_HoverRow = -1;
        if ((int16_t)ds.AlienTalk_SelectedLine > 0) {
            ds.AlienTalk_QuestionLine = (int8_t)ds.Alien_CurMenu[ds.AlienTalk_SelectedLine];
            ds.AlienTalk_SelectedLine = 0;
            AlienTalk_LoadQuestionAnswerId();
            ds.AlienTalk_ReplyLine = ds.AlienTalk_AnswerId;
            alien_talk_answer();
            AlienTalk_DrawAlienReply();
            ds.AlienTalk_Locked = 1;
            ds.Game_HotspotHit = 0;
            ds.Mouse_Hotspots[0].Repeat_ = 1;
        }
    }
    if (ds.Game_HotspotHit == 0x15 && ds.AlienTalk_Locked) {
        if (ds.AlienTalk_MenuNumber == 0) {
            ds.Game_CurrentScreen = 1;
            ds.Game_ScreenSwitch = 1;
            if (ds.AlienTalk_Race == 9 && ds.Pub_People[4].State == 2)
                Message_ShowMessageLine(0x2E);
            if (ds.AlienTalk_Race == 6)
                Message_ShowMessageLine(0x30);
        } else {
            ds.AlienTalk_Locked = 0;
            AlienTalk_BuildQuestionMenu();
            AlienTalk_DrawQuestionMenu();
            ds.Mouse_HeldPhase = 1;
            ds.Mouse_Hotspots[0].Repeat_ = 0;
        }
    }
}

// --- Screen 37: colony list ---------------------------------------------------------

static void list_mode_button(int16_t mode, const uint8_t *sfx)
{
    if (ds.PlanetList_Mode == mode) {
        Sound_PlayButtonSfx(LIT("x"));
    } else {
        Sound_PlayButtonSfx(sfx);
        ds.PlanetList_Mode = mode;
        ds.PlanetList_Top = 1;
        PlanetList_RefreshList();
    }
}

static void screen_planet_list(void)
{
    if (ds.Game_ScreenEntered) {
        FarPtr entries = System_GetMem(0x4B0);
        ds.PlanetList_Top = 1;
        ds.PlanetList_Mode = 1;
        ds.PlanetList_Entries = entries;
        PlanetList_RefreshList();
        PlanetList_SetupScrollBarHotspot();
    }
    if (ds.Game_HotspotHit == 0x15)
        PlanetList_DragScrollBar();
    if (ds.Game_MenuCommand == 0x4B)
        list_mode_button(1, LIT("plyour"));
    if (ds.Game_MenuCommand == 0x4C)
        list_mode_button(2, LIT("pluseful"));
    if (ds.Game_MenuCommand == 0x4D)
        list_mode_button(3, LIT("plalien"));
}

// --- Dispatch ------------------------------------------------------------------------

static bool on(int16_t screen)
{
    return ds.Game_CurrentScreen == screen && !ds.Game_ScreenSwitch;
}

static void dispatch(void)
{
    if (on(1)) {
        ds.Game_ClockRunning = 1;
        Command_HandleCommandCentre();
    }
    if (on(3)) {
        ds.Game_ClockRunning = 1;
        Research_ResearchScreen();
    }
    if (on(2)) {
        ds.Game_ClockRunning = 1;
        Hire_HireScreen();
    }
    if (on(24))
        Advisor_AdvisorScreen();
    if (on(5)) {
        if (ds.Game_ScreenEntered)
            ds.Game_ClockRunning = 1;
        Shop_ShopScreen();
    }
    if (on(20))
        screen_surface();
    if (on(8)) {
        ds.Game_ClockRunning = 1;
        BaseEff_BaseEffScreen();
    }
    if (on(28)) {
        ds.Game_ClockRunning = 0;
        Colonize_ColonizeScreen();
    }
    if (on(4)) {
        ds.Game_ClockRunning = 1;
        Droids_DroidsScreen();
    }
    if (on(7)) {
        ds.Game_ClockRunning = 1;
        Launch_MapScreen();
    }
    if (on(18)) {
        ds.Game_ClockRunning = 0;
        Launch_DestinationScreen();
    }
    if (on(16)) {
        ds.Game_ClockRunning = 1;
        ShipInfo_ShipInfoScreen();
    }
    if (on(22)) {
        ds.Game_ClockRunning = 1;
        Groups_GroupsScreen();
    }
    if (on(21)) {
        ds.Game_ClockRunning = 0;
        Groups_NewGroupScreen();
    }
    if (on(17)) {
        ds.Game_ClockRunning = 1;
        ShipMove_GroupScreen();
    }
    if (on(13)) {
        ds.Game_ClockRunning = 1;
        Cargo_CargoScreen();
    }
    if (on(11)) {
        ds.Game_ClockRunning = 1;
        if (ds.Game_ScreenEntered) {
            GfxPath_ShowScreenBackdrop();
            Message_DrawMessageLog();
            Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
        }
    }
    if (on(23))
        screen_pub();
    if (on(25))
        screen_pub_dialog();
    if (on(10)) {
        ds.Game_ClockRunning = 0;
        if (ds.Game_ScreenEntered)
            Credits_ShowCredits();
    }
    if (on(12)) {
        ds.Game_ClockRunning = 0;
        SaveLoad_DiskScreen();
    }
    if (on(29))
        screen_space_battle();
    if (on(19)) {
        ds.Game_ClockRunning = 0;
        GrWarUnits_UnitAssignScreen();
    }
    if (on(32))
        screen_ground_battle();
    if (on(33)) {
        ds.Game_ClockRunning = 0;
        if (ds.Game_ScreenEntered)
            screen_story_music();
    }
    if (on(34))
        screen_alien_talk();
    if (on(35) && ds.Game_ScreenEntered) {
        Mouse_HideCursor();
        Music_PlayTrack(LIT("failure"));
        Command_ShowDeathEnding();
    }
    if (on(36) && ds.Game_ScreenEntered) {
        Mouse_HideCursor();
        Music_PlayTrack(LIT("failure"));
        Command_FadeOutAndHalt();
    }
    if (on(37))
        screen_planet_list();
}

// --- The loop ----------------------------------------------------------------------

void Game_Main(void)
{
    startup();
    for (;;) {
        ds.Mouse_HiddenClick = ds.Mouse_CursorHidden != 0 && ds.Mouse_BtnRight == 0;
        Ems_EmsMapGameData();
        if (!ds.Game_ScreenSwitch && !ds.Mouse_HeldPhase && !ds.Mouse_HiddenClick) {
            if (!wait_for_event())
                return;
        }
        if (ds.Game_ScreenSwitch) {
            ds.Game_PrevHotspotHit = ds.Game_HotspotHit;
            ds.Game_HotspotHit = 0;
            ds.Game_ScreenEntered = 1;
        }
        if (ds.Mouse_HiddenClick)
            ds.Game_HotspotHit = 999;
        ds.Game_PressHotspot = ds.Game_HotspotHit;
        ds.Mouse_HotPushable = !ds.Mouse_HiddenClick && hit_pushable(ds.Game_HotspotHit);
        if (ds.Mouse_LastPushable)
            ds.Mouse_HeldPhase = 1;
        ds.Mouse_LastPushable = ds.Mouse_HotPushable;
        ds.Mouse_HotPushable = 0;
        if (!ds.Mouse_QuitFlag && ds.Mouse_HeldPhase) {
            if (!wait_for_release())
                return;
        }
        ds.Mouse_HeldPhase = 0;
        if (ds.Game_ScreenSwitch) {
            ds.Game_PrevHotspotHit = ds.Game_HotspotHit;
            ds.Game_HotspotHit = 0;
            ds.Game_ScreenEntered = 1;
        }
        if (ds.Mouse_QuitFlag)
            Gfx_FatalExit();

        if (!ds.Game_ScreenSwitch && ds.Game_HotspotHit == 0xD && ds.GfxPath_PageFlagA) {
            Sound_PlayButtonSfx(LIT("icon"));
            if (ds.GfxPath_PageFlagB == 0)
                GfxPath_ScrollToolbarPrev();
            else
                GfxPath_ScrollToolbarNext();
        }
        // Clock button: run the next hour now (95 ticks while HeldLeft).
        ds.Main_I = ds.Mouse_HeldLeft == 0 ? 100 : 95;
        if (!ds.Game_ScreenSwitch && ds.Game_Ticks < (uint16_t)ds.Main_I && ds.Game_HotspotHit == 0xE)
            ds.Game_Ticks = (uint16_t)ds.Main_I;

        ds.Game_MenuCommand = 0;
        if (!ds.Game_ScreenSwitch)
            ds.Game_ScreenEntered = 0;
        if (ds.Game_HotspotHit != 0 && (uint16_t)ds.Game_HotspotHit < 13) {
            ds.Game_MenuCommand = ds.GfxPath_MenuBar[ds.Game_HotspotHit - 1].Command;
            // Last match wins (command 7 opens screen 9, not 6).
            for (ds.Main_I = 1; ds.Main_I <= 38; ds.Main_I++) {
                int16_t cmd = (int8_t)screen_info((int16_t)ds.Main_I)[0x0B];
                if (cmd >= 0 && cmd == ds.Game_MenuCommand) {
                    ds.Game_CurrentScreen = (int16_t)ds.Main_I;
                    ds.Game_ScreenEntered = 1;
                }
            }
        }
        if (ds.Game_ScreenEntered || ds.Game_ScreenSwitch)
            TextFile_EndInput();
        if (ds.Game_ScreenEntered)
            ds.Mouse_HotspotCount = 0;
        free_left_screen_buffers();
        if (ds.Game_CurrentScreen < 0)
            ds.Game_CurrentScreen = (int16_t)-ds.Game_CurrentScreen;
        if (!ds.Game_ScreenSwitch) {
            if (ds.Game_HotspotHit == 0 || (uint16_t)ds.Game_HotspotHit > 12) {
                ds.Game_MenuCommand = 0;
            } else {
                ds.Game_MenuCommand = ds.GfxPath_MenuBar[ds.Game_HotspotHit - 1].Command;
                if (ds.Game_MenuCommand != 0) {
                    if (ds.Game_SoundFx == 2)
                        Sound_PlayButtonSfx(
                            (const uint8_t *)&ds.Ui_ButtonSounds[ds.Game_MenuCommand - 1]);
                    else
                        Sound_PlayButtonSfx(LIT("x"));
                }
            }
        }
        if (ds.Game_ScreenEntered) {
            Gfx_DrawStatusText(LIT(""));
            uint8_t *info = screen_info(ds.Game_CurrentScreen);
            if (info[0x0A])
                GfxPath_SetToolbarAndShow(ds.Game_CurrentScreen);
            if (info[0x09])
                Gfx_ShowGrafikaPic(info);
            ds.Game_HotspotHit = 0;
            ds.Game_ScreenSwitch = 0;
        }
        dispatch();
        if (ds.Game_ScreenEntered)
            ds.Game_PrevScreen = ds.Game_CurrentScreen;
    }
}
