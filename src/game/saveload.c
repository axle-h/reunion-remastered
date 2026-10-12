#include "game/saveload.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "game/anim.h"
#include "game/blit.h"
#include "game/gfx.h"
#include "game/gfxpath.h"
#include "game/mouse.h"
#include "game/music.h"
#include "game/options.h"
#include "game/sound.h"
#include "game/system.h"
#include "game/textfile.h"
#include "platform/fs.h"

enum { SLOT_SIZE = 0x16, SLOT_COUNT = 12 };

static uint8_t *slot_entry(int16_t n)
{
    return ds.SaveLoad_SlotTable + (n - 1) * SLOT_SIZE;
}

// --- Files -------------------------------------------------------------------

static char *save_dir;
static bool save_dir_set;

void SaveLoad_SetSaveDir(const char *dir)
{
    SDL_free(save_dir);
    save_dir = nullptr;
    save_dir_set = dir != nullptr;
    if (!dir)
        return;
    size_t n = strlen(dir);
    bool slash = n > 0 && (dir[n - 1] == '/' || dir[n - 1] == '\\');
    SDL_asprintf(&save_dir, "%s%s", dir, slash ? "" : "/");
    SDL_CreateDirectory(save_dir);
}

static const char *player_dir(void)
{
    if (!save_dir_set) {
        save_dir_set = true;
        char *pref = SDL_GetPrefPath("Amnesty Design", "Reunion");
        if (pref) {
            SDL_asprintf(&save_dir, "%ssave/", pref);
            SDL_CreateDirectory(save_dir);
            SDL_free(pref);
        }
    }
    return save_dir;
}

// Opens save\spidysav.<slot>. Writing goes to the player's directory; reading
// tries it first, then the game data directory.
static FILE *open_slot(int16_t slot, bool write)
{
    char name[32];
    SDL_snprintf(name, sizeof name, "spidysav.%d", slot);
    const char *dir = player_dir();
    if (write) {
        if (!dir)
            return nullptr;
        char path[1024];
        SDL_snprintf(path, sizeof path, "%s%s", dir, name);
        return fopen(path, "wb");
    }
    if (dir) {
        char path[1024];
        SDL_snprintf(path, sizeof path, "%s%s", dir, name);
        FILE *f = fopen(path, "rb");
        if (f)
            return f;
    }
    char dos[48];
    SDL_snprintf(dos, sizeof dos, "SAVE\\%s", name);
    char *host = fs_resolve(dos);
    FILE *f = host ? fopen(host, "rb") : nullptr;
    SDL_free(host);
    return f;
}

// --- The block list -----------------------------------------------------------

typedef struct SaveBlock {
    uint16_t off;   // the field, or the pointer field whose block is saved (ptr)
    uint16_t size;
    bool ptr;
} SaveBlock;

// A field, its first `size` bytes, or `size` bytes of the block it points to.
// The fields are the game's data in its own little-endian layout (records
// are packed as in the original), so their bytes are the file's.
#define F(field) { offsetof(DSeg, field), sizeof(((DSeg *)nullptr)->field), false }
#define PART(field, size) { offsetof(DSeg, field), (size), false }
#define P(field, size) { offsetof(DSeg, field), (size), true }

// The original's order. It writes the twelve Pub_StoryFlags one by one, the
// same bytes as the array; Staff_DevSkills only the first grade's.
static const SaveBlock blocks[] = {
    F(Staff_Level), PART(Staff_DevSkills, sizeof(ds.Staff_DevSkills[0])), P(Message_Log, 0x31B),
    F(Message_LogCount), F(Pub_People), F(Pub_Deals), F(Races_Hostile), F(Pub_StoryFlags),
    F(Galaxy_AmnestyNames), F(Galaxy_AmnestyBodies), F(Galaxy_PhoenixNames),
    F(Galaxy_PhoenixBodies), F(Galaxy_MirachNames), F(Galaxy_MirachBodies), F(Galaxy_AntaresNames),
    F(Galaxy_AntaresBodies), F(Galaxy_OrionisNames), F(Galaxy_OrionisBodies), F(Galaxy_LyraeNames),
    F(Galaxy_LyraeBodies), F(Galaxy_RigelNames), F(Galaxy_RigelBodies), F(Galaxy_SunNames),
    F(Galaxy_SunBodies), F(Galaxy_PlanetStatus), F(Galaxy_SystemStatus), F(Sim_Observatories),
    F(Sim_UniversityCount), F(Game_CurSystem), F(Game_CurPlanet), F(Game_CurMoon), F(Game_Hero),
    F(Game_HeroSpriteX), F(Research_Products), F(Game_SelInvention), F(Game_CurResearch),
    F(Game_StaffExp), F(Game_DevSkill), F(Game_ProductionRate), F(Game_StaffHired), F(Game_Credits),
    F(Game_HomeStock), F(Game_Year), F(Game_Month), F(Game_Day), F(Game_Hour),
    F(Game_ColonySetupCount), F(Game_ColonySetups), F(Game_BuildingCount), P(Game_Buildings, 14000),
    F(Races_Races), F(Game_ShownGroupCount), F(Game_GroupCount), F(Game_PlanetForceCount),
    F(Game_SelGroup), F(Game_SelGroup0), F(Game_SelGroup1), F(Game_GroupList),
    P(Game_Groups, 0x2840), F(Game_GroupsEnabled), F(Game_GroupKindAllowed), F(Game_IdeaTimer),
    F(Game_StoryStage), F(Intel_MorgrulAttack3Won), F(Intel_Flag5D3D), F(Intel_KallForceSent),
    F(Intel_KallTurnCancelled), F(Intel_LaunchTimer), F(Intel_LaunchRace), F(Intel_LaunchForce),
    F(Intel_LaunchMission), F(Intel_FirstJumpSeen), F(Intel_SatTheftTimer), F(Intel_SatTheftDone),
    F(Intel_Race2Timer1), F(Intel_Race2Done1), F(Intel_DevAwayTimer), F(Intel_Race2Timer2),
    F(Intel_Race2Done2), F(Intel_Race2Timer3), F(Intel_Race2Done3), F(Intel_Race2Timer4),
    F(Intel_Race2Done4), F(Intel_Invention14Timer), F(Intel_Invention14Revealed),
    F(Intel_MorgrulAttack1Timer), F(Intel_MorgrulAttack1Done), F(Intel_FirstBattleWon),
    F(Intel_FirstGroundLost), F(Intel_MorgrulAttack2Timer), F(Intel_MorgrulAttack2Done),
    F(Intel_MorgrulAttack2Open), F(Intel_Phoenix1Timer), F(Intel_Phoenix1Done),
    F(Intel_Phoenix1Armed), F(Intel_MorgrulAttack3Timer), F(Intel_MorgrulAttack3Done),
    F(Intel_MorgrulAttack3Open), F(Intel_Race4Timer), F(Intel_Race4Done), F(Intel_KallTurnTimer),
    F(Intel_KallTurnDone), F(Intel_JointAttackTimer), F(Intel_JointAttackDone), F(Intel_Race5Timer),
    F(Intel_Race5Done), F(Intel_Msg14Timer), F(Intel_Msg14Done), F(Intel_System3Timer),
    F(Intel_MorgrulBeaten), F(Intel_System4DestroyTimer), F(Intel_System4Destroyed),
    F(Intel_Pub4HintOff), F(Intel_System4WarnTimer), F(Intel_System4Warned),
    F(Intel_System4TalkArmed), F(Intel_Race7to10Timer), F(Intel_Race7to10Done), F(Intel_Pub10Done),
    F(Intel_Pub7Done), F(Advisor_TrainingTimer), F(Advisor_TrainingKind), F(Advisor_TrainingCourse),
    F(Advisor_TrainingPrice),
};

#undef F
#undef PART
#undef P

// 1a3e:0240 Block (nested in SaveLoadGame): one BlockWrite/BlockRead + CheckIO.
static bool block(FILE *f, void *buf, uint16_t size, bool save)
{
    return (save ? fwrite(buf, 1, size, f) : fread(buf, 1, size, f)) == size;
}

// The header, the slot name as string[19] (the first 20 bytes of the
// string[20] at `name`), then the blocks.
static bool transfer(FILE *f, uint8_t *name, bool save)
{
    bool ok = block(f, name, 0x14, save);
    for (size_t i = 0; ok && i < sizeof blocks / sizeof blocks[0]; i++) {
        void *p = (uint8_t *)&ds + blocks[i].off;
        if (blocks[i].ptr)
            memcpy(&p, p, sizeof p);  // the block the pointer field points to
        ok = block(f, p, blocks[i].size, save);
    }
    // Port addition: frozen cheats (game/options.h) hold the loaded stocks.
    // Saves of earlier port versions may end in a cheats trailer; it is
    // never read.
    if (ok && !save)
        Options_Rebase();
    return ok;
}

bool SaveLoad_Transfer(bool save)
{
    if (!save && !*slot_entry(ds.SaveLoad_SelectedSlot))
        return false;
    FILE *f = open_slot(ds.SaveLoad_SelectedSlot, save);
    if (!f)
        return false;
    bool ok = transfer(f, slot_entry(ds.SaveLoad_SelectedSlot) + 1, save);
    fclose(f);
    return ok;
}

bool SaveLoad_TransferFile(const char *path, uint8_t name[0x14], bool save)
{
    FILE *f = fopen(path, save ? "wb" : "rb");
    if (!f)
        return false;
    bool ok = transfer(f, name, save);
    fclose(f);
    return ok;
}

// 1a3e:029c
void SaveLoad_SaveLoadGame(bool save)
{
    if (!save && !*slot_entry(ds.SaveLoad_SelectedSlot))
        return;
    if (!SaveLoad_Transfer(save))
        return; // original: run-time error 100
    Gfx_DrawGameDate();
    Gfx_DrawCredits();
}

// 1a3e:0b93
void SaveLoad_SaveGame(void)
{
    SaveLoad_SaveLoadGame(true);
}

// 1a3e:0b9d
void SaveLoad_LoadGame(void)
{
    SaveLoad_SaveLoadGame(false);
}

// 1a3e:0bb1
void SaveLoad_LoadInit(void)
{
    size_t size;
    uint8_t *d = fs_load("SAVE\\INIT", &size);
    if (!d || size < 2 + 14000) {
        SDL_free(d); // original: run-time error 100/2
        return;
    }
    memcpy(&ds.Game_BuildingCount, d, 2);
    memcpy(ds.Game_Buildings, d + 2, 14000);
    SDL_free(d);
}

// 1a3e:0c40
void SaveLoad_ReadSlotNames(void)
{
    // Allocated afresh on every call, as in the original.
    ds.SaveLoad_SlotTable = game_alloc(0x108);
    for (int16_t n = 1; n <= SLOT_COUNT; n++) {
        uint8_t *e = slot_entry(n);
        FILE *f = open_slot(n, false);
        e[0] = f != nullptr;
        if (!f) {
            static const uint8_t empty[] = "\x14" "Empty---------------";
            memcpy(e + 1, empty, 21);
        } else {
            if (fread(e + 1, 1, 0x14, f) != 0x14)
                memset(e + 1, 0, 0x14); // original: I/O error 100
            fclose(f);
        }
    }
    ds.SaveLoad_SelectedSlot = 1;
}

// --- Disk screen ---------------------------------------------------------------

static uint8_t *screen_at(uint16_t off)
{
    return gfx_screen() + off;
}

// 1a3e:0d39
static void DrawSlotNames(void)
{
    for (int16_t n = 1; n <= SLOT_COUNT; n++) {
        char name[256];
        pstr_get(name, sizeof name, slot_entry(n) + 1);
        if (n == ds.SaveLoad_SelectedSlot)
            Gfx_DrawTextB3(name, 0x10, (n - 1) * 9 + 0x40, 0x13);
        else
            Gfx_DrawTextB0(name, 0x10, (n - 1) * 9 + 0x40, 0x13);
    }
}

// 1a3e:0da2: animation frame 'frame' of disk.ani to the screen.
static void DrawDiskFrame(int16_t frame)
{
    Anim_DrawAnimKeyed((uint16_t)(frame + 1), ds.SaveLoad_BufFrame);
    Mouse_HideCursor();
    Blit_CopyRectStrided(ds.SaveLoad_BufFrame, screen_at(0xBC26), 100, 0x31, 100, 0x140);
    Mouse_ShowCursor();
}

// 1a3e:0de6: the sliding handle.
static void DrawKnob(int16_t step)
{
    int16_t frame = step / 5;
    int16_t y = (int16_t)(step * -5 + 0x98);
    if (frame > 3)
        frame = 3;
    if (y < 0x31)
        y = 0x31;
    Blit_BlitTransparent(ds.SaveLoad_BufButtons + frame * 0x4F + 0x141,
                         screen_at((uint16_t)(y * 0x140 + 0xB0)), 0x4E, 0x12, 0x40);
}

// 1a3e:0e64
static void RestoreKnobBackground(int16_t step)
{
    int16_t y = (int16_t)(step * -5 + 0x98);
    if (y < 0x31)
        y = 0x31;
    Blit_CopyRect(ds.SaveLoad_BufBack + (y - 0x31) * 0x140 + 0xB0,
                  screen_at((uint16_t)(y * 0x140 + 0xB0)), 0x4E, 0x12);
}

// 1a3e:11ea: button idx (1..6) from the "on" (diskbr) or "off" (disk) picture.
static void DrawOptionButton(int16_t idx, bool on)
{
    int16_t x = ds.SaveLoad_OptionX[idx - 1], y = ds.SaveLoad_OptionY[idx - 1];
    int16_t w = ds.SaveLoad_OptionW[idx - 1], h = ds.SaveLoad_OptionH[idx - 1];
    uint8_t *dest = screen_at((uint16_t)((y + 0x31) * 0x140 + x));
    if (on)
        Blit_CopyRect(ds.SaveLoad_BufButtons + (y - 0x27) * 0x140 + x - 0x89, dest, w, h);
    else
        Blit_CopyRect(ds.SaveLoad_BufBack + y * 0x140 + x, dest, w, h);
}

// 1a3e:12d3
static void DrawOptionButtons(void)
{
    int16_t backend = ds.Music_Backend, track = ds.Game_MusicTrack;
    DrawOptionButton(1, backend >= 1 && track >= 1);
    DrawOptionButton(2, backend >= 1 && track == 0);
    DrawOptionButton(3, ds.Game_SoundFx == 2);
    DrawOptionButton(4, backend > 0);
    DrawOptionButton(5, backend >= 1 && track == 1);
    DrawOptionButton(6, backend >= 1 && track == 2);
}

// 1a3e:1366
static void DrawPressedButton(int16_t idx, bool pressed)
{
    int16_t x = ds.SaveLoad_ButtonX[idx - 1], y = ds.SaveLoad_ButtonY[idx - 1];
    int16_t w = ds.SaveLoad_ButtonW[idx - 1], h = ds.SaveLoad_ButtonH[idx - 1];
    uint8_t *dest = screen_at((uint16_t)(x + 0x110 + (y + 0x9A) * 0x140));
    if (pressed)
        Blit_CopyRect(ds.SaveLoad_BufButtons + (y + 0x15) * 0x140 + x, dest, w, h);
    else
        Blit_CopyRect(ds.SaveLoad_BufBack + (y + 0x69) * 0x140 + x + 0x110, dest, w, h);
}

// 1a3e:146e
void SaveLoad_PressButton(int16_t which)
{
    for (int16_t i = 1; i <= 4; i++)
        DrawPressedButton(i, which == i);
}

// 1a3e:113b
static void DiskScreenOpenAnim(void)
{
    for (int16_t i = 0; i <= 0x15; i++) {
        DrawDiskFrame(i);
        Gfx_WaitFrames(3);
    }
    for (int16_t i = 0; i <= 0x15; i++) {
        DrawDiskFrame(0x16);
        DrawKnob(i);
        Gfx_WaitFrames(2);
        RestoreKnobBackground(i);
        DrawOptionButtons();
    }
    Gfx_WaitFrames(10);
}

// 1a3e:1197
static void DiskScreenCloseAnim(void)
{
    for (int16_t i = 0x15; i >= 0; i--) {
        DrawDiskFrame(0x16);
        DrawKnob(i);
        Gfx_WaitFrames(2);
        RestoreKnobBackground(i);
        DrawOptionButtons();
    }
    for (int16_t i = 0x15; i >= 0; i--) {
        DrawDiskFrame(i);
        Gfx_WaitFrames(3);
    }
}

// 1a3e:0ee5
static void DiskScreenInit(void)
{
    ds.Anim_Record = game_alloc(sizeof(TAnimRecord));
    ds.Anim_Script = game_alloc(0x230);
    ds.SaveLoad_BufFrame = game_alloc(0x1324);
    ds.Anim_Record->Loaded = 0;
    Anim_LoadMainAnim(0x11);
    ds.SaveLoad_BufBack = game_alloc(0xBCC0);
    ds.SaveLoad_BufButtons = game_alloc(0x5140);
    uint8_t *pal = Gfx_PalettePtr();
    Gfx_LoadPic("grafika\\disk", ds.SaveLoad_BufBack, 1, 0x97, 0,
                0, 0x140, ds.Game_StartChoice != 2, pal, 0, 0xB0, 0x40, 0, 0xBE);
    Gfx_LoadPic("grafika\\diskbr", ds.SaveLoad_BufButtons, 1, 0x41,
                0, 0, 0x140, false, pal, 0, 0xB0, 0x40, 0, 0xBE);
    Blit_CopyRect(ds.SaveLoad_BufBack, screen_at(0x3D40), 0x140, 0x97);
    DrawDiskFrame(ds.Game_MusicTrack == 0 ? 0x16 : 0);
    DrawOptionButtons();
    DrawSlotNames();
    Gfx_FadeIn(0x40, 0xFE, pal);
    // StartChoice 2 (load from the start menu): the first showing, palette not faded.
    if (ds.Game_StartChoice == 2)
        ds.Game_StartChoice = 0;
}

// 1a3e:103b
static void SetupDiskHotspots(void)
{
    ds.Mouse_HotspotCount = 5;
    Mouse_SetHotspot(1, 0x111, 0x9B, 0x10, 0x0D, "Music 1", 1, 0);
    Mouse_SetHotspot(2, 0x122, 0x9B, 0x10, 0x0D, "Music 2", 1, 0);
    Mouse_SetHotspot(3, 0x111, 0xA9, 0x22, 0x0D, "Stop music", 1, 0);
    Mouse_SetHotspot(4, 0x111, 0xB7, 0x22, 0x0D, "Effects on/off", 1, 0);
    Mouse_SetHotspot(5, 0x0B, 0x40, 0x82, 0x6C, "Select slot", 0, 0);
}

// 1a3e:0008
void SaveLoad_DiskScreen(void)
{
    if (ds.Game_ScreenEntered) {
        GfxPath_SetToolbarAndShow(ds.Game_StartChoice == 2 ? 0x26 : 0x0C);
        SaveLoad_ReadSlotNames();
        DiskScreenInit();
        SetupDiskHotspots();
    }
    if (ds.Game_MenuCommand == 0x2F && *slot_entry(ds.SaveLoad_SelectedSlot) != 0) {
        SaveLoad_LoadGame();
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = 1;
        ds.GfxPath_PageFlagB = 1;
    }
    if (ds.Game_MenuCommand == 0x30) {
        int16_t sel = ds.SaveLoad_SelectedSlot;
        uint8_t *name = slot_entry(sel) + 1;
        if (*slot_entry(sel) == 0) {
            // "Save <n>" for an empty slot, truncated to 20 characters.
            char tmp[32];
            int len = SDL_snprintf(tmp, sizeof tmp, "Save %d", sel);
            name[0] = (uint8_t)len;
            memcpy(name + 1, tmp, (size_t)len);
        }
        TextFile_InputLine(name, 0x10, (sel - 1) * 9 + 0x40, 0x13, true);
        if (ds.TextFile_InputEscaped == 0)
            SaveLoad_SaveGame();
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = 1;
        ds.GfxPath_PageFlagB = 1;
    }
    if (ds.Game_MenuCommand == 0x49)
        Gfx_FatalExit();
    if ((uint16_t)ds.Game_HotspotHit > 0x14 && (uint16_t)ds.Game_HotspotHit < 0x19) {
        Sound_PlayButtonSfx("x");
        SaveLoad_PressButton((int16_t)(ds.Game_HotspotHit - 0x14));
        Gfx_WaitFrames(0x14);
        SaveLoad_PressButton(0);
    }
    int16_t hit = ds.Game_HotspotHit;
    if (hit == 0x15 && ds.Music_Backend > 0 && ds.Game_MusicTrack != 1) {
        if (ds.Game_MusicTrack > 0)
            DiskScreenOpenAnim();
        DiskScreenCloseAnim();
        ds.Game_MusicTrack = 1;
        Music_ApplyMusicTrack();
        DrawOptionButtons();
    }
    if (hit == 0x16 && ds.Music_Backend > 0 && ds.Game_MusicTrack != 2) {
        if (ds.Game_MusicTrack > 0)
            DiskScreenOpenAnim();
        DiskScreenCloseAnim();
        ds.Game_MusicTrack = 2;
        Music_ApplyMusicTrack();
        DrawOptionButtons();
    }
    if (hit == 0x17 && ds.Music_Backend > 0 && ds.Game_MusicTrack != 0) {
        DiskScreenOpenAnim();
        ds.Game_MusicTrack = 0;
        Music_ApplyMusicTrack();
        DrawOptionButtons();
    }
    if (hit == 0x18 && ds.Sound_Enabled != 0 && ds.Game_SoundFx > 0) {
        ds.Game_SoundFx = (int16_t)(3 - ds.Game_SoundFx);
        DrawOptionButtons();
    }
    if (hit == 0x19) {
        int16_t slot = (int16_t)((ds.Mouse_ClickY - 0x40) / 9 + 1);
        if (slot < 1)
            slot = 1;
        if (slot > 12)
            slot = 12;
        if (ds.SaveLoad_SelectedSlot != slot) {
            ds.SaveLoad_SelectedSlot = slot;
            DrawSlotNames();
        }
    }
}

// 1a3e:14b5
void SaveLoad_Init(void)
{
}
