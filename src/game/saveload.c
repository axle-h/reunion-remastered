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

// --- Globals without a name in dseg.h ---------------------------------------
#define SLOT_SEL (*(int16_t *)ds_at(0x7732))         // selected slot 1..12
#define SLOT_TABLE (*(FarPtr *)ds_at(0x7734))        // 12 x {Used: boolean; Name: string[20]}
#define NOSAVE_FLAG (*(uint8_t *)ds_at(0x7C4B))      // set when InputLine was cancelled
#define SCREEN_FLAG (*(uint8_t *)ds_at(0x7C63))      // set to 1 when leaving the screen
#define FIRST_SHOW (*(uint8_t *)ds_at(0x5DAA))       // 2 on the first visit (palette not faded)
#define BUF_BACK (*(FarPtr *)ds_at(0x7726))          // disk.pic, 320x151
#define BUF_BUTTONS (*(FarPtr *)ds_at(0x772A))       // diskbr.pic, 320x65
#define BUF_FRAME (*(FarPtr *)ds_at(0x772E))         // animation frame, 100x49

enum { SLOT_SIZE = 0x16, SLOT_COUNT = 12 };

static uint8_t *slot_entry(int16_t n)
{
    return (uint8_t *)far_host(SLOT_TABLE) + (n - 1) * SLOT_SIZE;
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

// --- The block list (docs/formats/save.md) ----------------------------------

typedef struct SaveBlock {
    uint16_t off;   // DS offset, or the DS offset of the far pointer when ptr
    uint16_t size;
    bool ptr;
} SaveBlock;

#define B(o, s) { (o), (s), false }
#define P(o, s) { (o), (s), true }

static const SaveBlock blocks[] = {
    B(0x57CC, 0x18), B(0x57F0, 0x0C), P(0x7ADE, 0x31B), B(0x7AE2, 2), B(0x013A, 0x10E),
    B(0x0424, 0xF0), B(0x6518, 0x0C),
    B(0x773E, 1), B(0x773F, 1), B(0x7740, 1), B(0x7741, 1), B(0x7742, 1), B(0x7743, 1),
    B(0x7744, 1), B(0x7745, 1), B(0x7746, 1), B(0x7747, 1), B(0x7748, 1), B(0x7749, 1),
    B(0x1A9C, 0x1A0), B(0x1C3C, 0x820), B(0x248A, 0x9C), B(0x2526, 0x30C),
    B(0x2860, 0xEA), B(0x294A, 0x492), B(0x2DF8, 0x5B), B(0x2E54, 0x1C7),
    B(0x3038, 0xA9), B(0x30E2, 0x34D), B(0x3470, 0x12B), B(0x359C, 0x5D7),
    B(0x3B86, 0x41), B(0x3BC8, 0x145), B(0x3D56, 0x1A0), B(0x3EF6, 0x820),
    B(0x47D6, 0x40), B(0x4816, 8), B(0x481E, 0x10), B(0x482E, 2),
    B(0x7AE4, 2), B(0x7AE6, 0x10), B(0x7AF6, 0x10), B(0x9276, 2), B(0x9278, 2),
    B(0x5DAC, 0x73F), B(0x95A0, 2), B(0x95A2, 2), B(0x95A4, 8), B(0x95AC, 8),
    B(0x95B4, 2), B(0x95B6, 8), B(0x95BE, 4), B(0x95C6, 0x18),
    B(0x95DE, 2), B(0x95E0, 2), B(0x95E2, 2), B(0x95E4, 2),
    B(0xA21A, 2), B(0xA21C, 0xA0), B(0xA2C0, 2), P(0xA2BC, 14000), B(0x6BCE, 0x9CC),
    B(0xA2C2, 2), B(0xA2C4, 2), B(0xA2C6, 2), B(0xA2C8, 2), B(0xA2CA, 2), B(0xA2CC, 2),
    B(0xA2CE, 2), P(0xA2D0, 0x2840), B(0xA2D4, 1), B(0xA2D6, 4), B(0x91A0, 0x46),
    B(0x91EA, 2),
    // 61 unnamed story/event globals, each saved on its own.
    B(0x5D3C, 1), B(0x5D3D, 1), B(0x5D3E, 1), B(0x5D3F, 1), B(0x5D40, 2), B(0x5D42, 2),
    B(0x5D44, 2), B(0x5D46, 2), B(0x5D48, 1), B(0x5D4A, 2), B(0x5D4C, 1), B(0x5D4E, 2),
    B(0x5D50, 1), B(0x5D52, 2), B(0x5D54, 2), B(0x5D56, 1), B(0x5D58, 2), B(0x5D5A, 1),
    B(0x5D5C, 2), B(0x5D5E, 1), B(0x5D60, 2), B(0x5D62, 1), B(0x5D64, 2), B(0x5D66, 1),
    B(0x5D67, 1), B(0x5D68, 1), B(0x5D6A, 2), B(0x5D6C, 1), B(0x5D6D, 1), B(0x5D6E, 2),
    B(0x5D70, 1), B(0x5D71, 1), B(0x5D72, 2), B(0x5D74, 1), B(0x5D75, 1), B(0x5D76, 2),
    B(0x5D78, 1), B(0x5D7A, 2), B(0x5D7C, 1), B(0x5D7E, 2), B(0x5D80, 1), B(0x5D82, 2),
    B(0x5D84, 1), B(0x5D86, 2), B(0x5D88, 1), B(0x5D8A, 2), B(0x5D8C, 1), B(0x5D8E, 2),
    B(0x5D90, 1), B(0x5D91, 1), B(0x5D92, 2), B(0x5D94, 1), B(0x5D95, 1), B(0x5D96, 2),
    B(0x5D98, 1), B(0x5D99, 1), B(0x5D9A, 1), B(0x5D9C, 2), B(0x5D9E, 2), B(0x5DA0, 2),
    B(0x5DA2, 4),
};

#undef B
#undef P

// 1a3e:0240 Block (nested in SaveLoadGame): one BlockWrite/BlockRead + CheckIO.
static bool block(FILE *f, void *buf, uint16_t size, bool save)
{
    return (save ? fwrite(buf, 1, size, f) : fread(buf, 1, size, f)) == size;
}

bool SaveLoad_Transfer(bool save)
{
    if (!save && !*slot_entry(SLOT_SEL))
        return false;
    FILE *f = open_slot(SLOT_SEL, save);
    if (!f)
        return false;
    // Header: the slot name, string[19] (the first 20 bytes of the string[20]).
    bool ok = block(f, slot_entry(SLOT_SEL) + 1, 0x14, save);
    for (size_t i = 0; ok && i < sizeof blocks / sizeof blocks[0]; i++) {
        void *p = blocks[i].ptr ? far_host(*(FarPtr *)ds_at(blocks[i].off)) : ds_at(blocks[i].off);
        ok = block(f, p, blocks[i].size, save);
    }
    // Port addition: the cheats' trailer (game/options.h), only in saves of
    // games with one on.
    uint8_t trailer[OPTIONS_SAVE_SIZE];
    if (ok && save) {
        size_t n = Options_SaveState(trailer);
        ok = fwrite(trailer, 1, n, f) == n;
    } else if (ok) {
        Options_LoadState(trailer, fread(trailer, 1, sizeof trailer, f));
    }
    fclose(f);
    return ok;
}

// 1a3e:029c
void SaveLoad_SaveLoadGame(bool save)
{
    if (!save && !*slot_entry(SLOT_SEL))
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
    memcpy(far_host(ds.Game_Buildings), d + 2, 14000);
    SDL_free(d);
}

// 1a3e:0c40
void SaveLoad_ReadSlotNames(void)
{
    // Allocated afresh on every call, as in the original.
    SLOT_TABLE = System_GetMem(0x108);
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
    SLOT_SEL = 1;
}

// --- Disk screen ---------------------------------------------------------------

// The original's button geometry: word tables in the data segment.
static int16_t tab(uint16_t base, int16_t idx)
{
    return *(int16_t *)ds_at((uint16_t)(base + idx * 2));
}

static FarPtr screen_at(uint16_t off)
{
    return far_ptr(ds.Gfx_ScreenSeg, off);
}

static FarPtr buf_at(FarPtr buf, int32_t off)
{
    return far_ptr(buf >> 16, (uint16_t)((uint16_t)buf + off));
}

// 1a3e:0d39
static void DrawSlotNames(void)
{
    for (int16_t n = 1; n <= SLOT_COUNT; n++) {
        const uint8_t *name = slot_entry(n) + 1;
        if (n == SLOT_SEL)
            Gfx_DrawTextB3(name, 0x10, (n - 1) * 9 + 0x40, 0x13);
        else
            Gfx_DrawTextB0(name, 0x10, (n - 1) * 9 + 0x40, 0x13);
    }
}

// 1a3e:0da2: animation frame 'frame' of disk.ani to the screen.
static void DrawDiskFrame(int16_t frame)
{
    Anim_DrawAnimKeyed((uint16_t)(frame + 1), BUF_FRAME);
    Mouse_HideCursor();
    Blit_CopyRectStrided(BUF_FRAME, screen_at(0xBC26), 100, 0x31, 100, 0x140);
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
    Blit_BlitTransparent(buf_at(BUF_BUTTONS, frame * 0x4F + 0x141),
                         screen_at((uint16_t)(y * 0x140 + 0xB0)), 0x4E, 0x12, 0x40);
}

// 1a3e:0e64
static void RestoreKnobBackground(int16_t step)
{
    int16_t y = (int16_t)(step * -5 + 0x98);
    if (y < 0x31)
        y = 0x31;
    Blit_CopyRect(buf_at(BUF_BACK, (y - 0x31) * 0x140 + 0xB0), screen_at((uint16_t)(y * 0x140 + 0xB0)),
                  0x4E, 0x12);
}

// 1a3e:11ea: button idx (1..6) from the "on" (diskbr) or "off" (disk) picture.
static void DrawOptionButton(int16_t idx, bool on)
{
    int16_t x = tab(0xD8, idx), y = tab(0xE4, idx);
    FarPtr dest = screen_at((uint16_t)((y + 0x31) * 0x140 + x));
    if (on)
        Blit_CopyRect(buf_at(BUF_BUTTONS, (y - 0x27) * 0x140 + x - 0x89), dest, tab(0xF0, idx),
                      tab(0xFC, idx));
    else
        Blit_CopyRect(buf_at(BUF_BACK, y * 0x140 + x), dest, tab(0xF0, idx), tab(0xFC, idx));
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
    int16_t x = tab(0x108, idx), y = tab(0x110, idx);
    FarPtr dest = screen_at((uint16_t)(x + 0x110 + (y + 0x9A) * 0x140));
    if (pressed)
        Blit_CopyRect(buf_at(BUF_BUTTONS, (y + 0x15) * 0x140 + x), dest, tab(0x118, idx), tab(0x120, idx));
    else
        Blit_CopyRect(buf_at(BUF_BACK, (y + 0x69) * 0x140 + x + 0x110), dest, tab(0x118, idx),
                      tab(0x120, idx));
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
    *(FarPtr *)ds_at(0x7E40) = System_GetMem(0x349);
    *(FarPtr *)ds_at(0x7E44) = System_GetMem(0x230);
    BUF_FRAME = System_GetMem(0x1324);
    *(uint8_t *)far_host(*(FarPtr *)ds_at(0x7E40)) = 0;
    Anim_LoadMainAnim(0x11);
    BUF_BACK = System_GetMem(0xBCC0);
    BUF_BUTTONS = System_GetMem(0x5140);
    FarPtr pal = far_ptr(MEM_DS_SEG, offsetof(DSeg, Gfx_Palette));
    Gfx_LoadPic((const uint8_t *)PSTR("grafika\\disk"), BUF_BACK, 1, 0x97, 0, 0, 0x140, FIRST_SHOW != 2, pal, 0, 0xB0,
                0x40, 0, 0xBE);
    Gfx_LoadPic((const uint8_t *)PSTR("grafika\\diskbr"), BUF_BUTTONS, 1, 0x41, 0, 0, 0x140, false, pal, 0, 0xB0,
                0x40, 0, 0xBE);
    Blit_CopyRect(BUF_BACK, screen_at(0x3D40), 0x140, 0x97);
    DrawDiskFrame(ds.Game_MusicTrack == 0 ? 0x16 : 0);
    DrawOptionButtons();
    DrawSlotNames();
    Gfx_FadeIn(0x40, 0xFE, pal);
    if (FIRST_SHOW == 2)
        FIRST_SHOW = 0;
}

// 1a3e:103b
static void SetupDiskHotspots(void)
{
    ds.Mouse_HotspotCount = 5;
    Mouse_SetHotspot(1, 0x111, 0x9B, 0x10, 0x0D, (const uint8_t *)PSTR("Music 1"), 1, 0);
    Mouse_SetHotspot(2, 0x122, 0x9B, 0x10, 0x0D, (const uint8_t *)PSTR("Music 2"), 1, 0);
    Mouse_SetHotspot(3, 0x111, 0xA9, 0x22, 0x0D, (const uint8_t *)PSTR("Stop music"), 1, 0);
    Mouse_SetHotspot(4, 0x111, 0xB7, 0x22, 0x0D, (const uint8_t *)PSTR("Effects on/off"), 1, 0);
    Mouse_SetHotspot(5, 0x0B, 0x40, 0x82, 0x6C, (const uint8_t *)PSTR("Select slot"), 0, 0);
}

// --- Options panel (port addition, game/options.h) -----------------------------
//
// OPTIONS swaps the toolbar for BACK TO M.SCREEN and BACK TO DISK OPS and
// draws the panel in place of the slot list, in the same 12 rows. The music
// and effects buttons stay; hotspots 5..8 (5 was the slot list) are the cheat
// rows: hits 19h..1Ch.

enum { ROW_MONEY = 4, ROW_ORE = 5, ROW_ADD_MONEY = 7, ROW_ADD_ORE = 8 };

static bool options_open;

static void DrawOptionsRow(int16_t row, const char *text, bool on)
{
    uint8_t s[32];
    s[0] = (uint8_t)SDL_snprintf((char *)s + 1, sizeof s - 1, "%s", text);
    if (on)
        Gfx_DrawTextB3(s, 0x10, (row - 1) * 9 + 0x40, 0x13);
    else
        Gfx_DrawTextB0(s, 0x10, (row - 1) * 9 + 0x40, 0x13);
}

static void DrawAddRow(int16_t row, bool pressed)
{
    char text[32];
    if (row == ROW_ADD_MONEY)
        SDL_snprintf(text, sizeof text, "Add %d credits", OPTIONS_ADD_MONEY);
    else
        SDL_snprintf(text, sizeof text, "Add %d ore each", OPTIONS_ADD_ORE);
    DrawOptionsRow(row, text, pressed);
}

static void DrawOptionsPanel(void)
{
    for (int16_t row = 1; row <= SLOT_COUNT; row++)
        DrawOptionsRow(row, "", false);
    DrawOptionsRow(1, "Options", false);
    DrawOptionsRow(2, "-------------------", false);
    DrawOptionsRow(ROW_MONEY, options.freeze_money ? "Freeze money:    on" : "Freeze money:   off",
                   options.freeze_money);
    DrawOptionsRow(ROW_ORE, options.freeze_ore ? "Freeze ore:      on" : "Freeze ore:     off",
                   options.freeze_ore);
    DrawAddRow(ROW_ADD_MONEY, false);
    DrawAddRow(ROW_ADD_ORE, false);
}

static void SetOptionsRowHotspot(int16_t index, int16_t row, const uint8_t *caption)
{
    Mouse_SetHotspot(index, 0x10, (uint8_t)((row - 1) * 9 + 0x40), 0x72, 8, caption, 0, 0);
}

static void OpenOptions(void)
{
    static const int8_t toolbar[] = {0x18, OPTIONS_BACK};  // BACK TO M.SCREEN
    options_open = true;
    GfxPath_SetToolbarCommands(toolbar, 2);
    GfxPath_ShowToolbar();
    DrawOptionsPanel();
    ds.Mouse_HotspotCount = 8;
    SetOptionsRowHotspot(5, ROW_MONEY, (const uint8_t *)PSTR("Freeze money"));
    SetOptionsRowHotspot(6, ROW_ORE, (const uint8_t *)PSTR("Freeze ore"));
    SetOptionsRowHotspot(7, ROW_ADD_MONEY, (const uint8_t *)PSTR("Add money"));
    SetOptionsRowHotspot(8, ROW_ADD_ORE, (const uint8_t *)PSTR("Add ore"));
}

static void CloseOptions(void)
{
    options_open = false;
    GfxPath_SetToolbarAndShow(0x0C);
    DrawSlotNames();
    SetupDiskHotspots();
}

// Hits 19h..1Ch: hotspots 5..8 of OpenOptions.
static void OptionsClick(int16_t hit)
{
    Sound_PlayButtonSfx((const uint8_t *)PSTR("x"));
    if (hit == 0x19)
        Options_SetFreezeMoney(!options.freeze_money);
    else if (hit == 0x1A)
        Options_SetFreezeOre(!options.freeze_ore);
    else if (hit == 0x1B)
        Options_AddMoney();
    else
        Options_AddOre();
    // An add row lights up while the button is down.
    if (hit >= 0x1B)
        DrawAddRow(hit == 0x1B ? ROW_ADD_MONEY : ROW_ADD_ORE, true);
    else
        DrawOptionsPanel();
    // The main loop hands a non-repeating hotspot over on every frame the
    // button is down (dragging across the slot list selects as it goes):
    // act once per click.
    do {
        Mouse_Poll();
        Gfx_WaitFrames(1);
    } while (ds.Mouse_Buttons != 0);
    if (hit >= 0x1B)
        DrawOptionsPanel();
}

// 1a3e:0008
void SaveLoad_DiskScreen(void)
{
    if (ds.Game_ScreenEntered) {
        options_open = false;
        GfxPath_SetToolbarAndShow(FIRST_SHOW == 2 ? 0x26 : 0x0C);
        SaveLoad_ReadSlotNames();
        DiskScreenInit();
        SetupDiskHotspots();
    }
    // Port addition: the options panel.
    if (ds.Game_MenuCommand == OPTIONS_COMMAND)
        OpenOptions();
    if (options_open) {
        if (ds.Game_MenuCommand == OPTIONS_BACK)
            CloseOptions();
        else if (ds.Game_HotspotHit >= 0x19 && ds.Game_HotspotHit <= 0x1C)
            OptionsClick(ds.Game_HotspotHit);
        if (options_open && (uint16_t)ds.Game_HotspotHit >= 0x19)
            return;
    }
    if (ds.Game_MenuCommand == 0x2F && *slot_entry(SLOT_SEL) != 0) {
        SaveLoad_LoadGame();
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = 1;
        SCREEN_FLAG = 1;
    }
    if (ds.Game_MenuCommand == 0x30) {
        uint8_t *name = slot_entry(SLOT_SEL) + 1;
        if (*slot_entry(SLOT_SEL) == 0) {
            // "Save <n>" for an empty slot, truncated to 20 characters.
            char tmp[32];
            int len = SDL_snprintf(tmp, sizeof tmp, "Save %d", SLOT_SEL);
            name[0] = (uint8_t)len;
            memcpy(name + 1, tmp, (size_t)len);
        }
        TextFile_SetStringPtr(far_ptr((uint16_t)(SLOT_TABLE >> 16),
                                      (uint16_t)((uint16_t)SLOT_TABLE + (SLOT_SEL - 1) * SLOT_SIZE + 1)));
        TextFile_InputLine(name, 0x10, (SLOT_SEL - 1) * 9 + 0x40, 0x13, true);
        if (NOSAVE_FLAG == 0)
            SaveLoad_SaveGame();
        ds.Game_ScreenSwitch = true;
        ds.Game_CurrentScreen = 1;
        SCREEN_FLAG = 1;
    }
    if (ds.Game_MenuCommand == 0x49)
        Gfx_FatalExit();
    if ((uint16_t)ds.Game_HotspotHit > 0x14 && (uint16_t)ds.Game_HotspotHit < 0x19) {
        Sound_PlayButtonSfx((const uint8_t *)PSTR("x"));
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
        if (SLOT_SEL != slot) {
            SLOT_SEL = slot;
            DrawSlotNames();
        }
    }
}

// 1a3e:14b5
void SaveLoad_Init(void)
{
}
