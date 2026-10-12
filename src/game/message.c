#include "game/message.h"

#include <SDL3/SDL.h>
#include <string.h>

#include "game/blit.h"
#include "game/dseg.h"
#include "game/gfx.h"
#include "game/le.h"
#include "game/mouse.h"
#include "game/sound.h"
#include "game/system.h"
#include "game/textfile.h"
#include "platform/input.h"

enum : uint16_t { LOG_ENTRY = 0x35, LOG_MAX = 15 };

// Log entry i (1-based): { kind: integer; text: string[50] }.
static uint8_t *log_entry(int16_t i)
{
    return ds.Message_Log + (uint16_t)(i * LOG_ENTRY - LOG_ENTRY);
}

// 34b0:0001
void Message_AddMessageLogEntry(int16_t kind, const char *text, const char *voice)
{
    char t[256];
    SDL_strlcpy(t, text, sizeof t);
    if (voice[0] != '\0')
        Sound_PlayButtonSfx(voice);
    ds.Gfx_FlashStep = 0x3D;
    ds.Gfx_FlashCount = 3;
    Gfx_PaletteFlashTick();
    if (ds.Message_LogCount >= LOG_MAX) {
        int16_t last = (int16_t)(ds.Message_LogCount - 1);
        for (int16_t i = 1; i <= last; i++)
            SDL_memcpy(log_entry(i), log_entry((int16_t)(i + 1)), LOG_ENTRY);
        ds.Message_LogCount--;
    }
    ds.Message_LogCount++;
    uint8_t *e = log_entry(ds.Message_LogCount);
    wr16le(e, kind);
    pstr_set(e + 2, 0x32, t);
    if (ds.Game_CurrentScreen == 0x0B)
        Message_DrawMessageLog();
}

// 34b0:011a
void Message_ShowMessageLine(int16_t lineNo)
{
    char s[256] = "";
    TextFile_ReadTextLine("message.txt", lineNo, s, sizeof s);
    Message_MessageBox(s, "attentio", true, nullptr, false, 0);
}

// 34b0:0158
void Message_ShowMessage(const char *text)
{
    Message_MessageBox(text, "message", true, nullptr, false, 0);
}

// 34b0:0197
void Message_ShowMessageNoLog(const char *text)
{
    Message_MessageBox(text, "hiba", false, nullptr, false, 0);
}

// 34b0:01df: a space on each side (34b0:01dd is the string ' ').
void Message_ShowKitalalLine(int16_t lineNo)
{
    char line[256] = "", tmp[256];
    TextFile_ReadTextLine("kitalal.txt", lineNo, line, sizeof line);
    SDL_snprintf(tmp, sizeof tmp, " %s ", line);
    Message_ShowMessage(tmp);
}

// The blinking "Press mouse button" in the status bar: counter 1..50, text
// shown at 25, cleared at 50.
static void blink(uint16_t *counter)
{
    *counter = (uint16_t)(*counter % 50 + 1);
    if (*counter == 25)
        Gfx_DrawStatusText("");
    if (*counter == 50)
        Gfx_DrawStatusText("Press mouse button");
}

static bool button_down(void)
{
    Mouse_Update();
    return ds.Mouse_BtnDown != 0;
}

// 34b0:0237
void Message_MessageBox(const char *text, const char *voice, bool logIt, uint8_t *buf,
                        bool edit, uint8_t maxLen)
{
    char lines[7][55] = {};  // string[54], 1..6 (the original has no room for more)
    int16_t len[7] = {};

    if (voice[0] != '\0')
        Sound_PlayButtonSfx(voice);

    int16_t lineCount = 1;
    int16_t editCol = 0, editLine = 0;
    for (size_t i = 0; text[i] != '\0' && i < 255; i++) {
        if (text[i] != '|') {
            // BUG? more than 6 lines overflow the original's array; extra text is dropped here.
            if (lineCount <= 6 && len[lineCount] < 54)
                lines[lineCount][len[lineCount]++] = text[i];
        } else {
            lineCount++;
        }
        if (text[i] == '&' && lineCount <= 6) {
            editCol = len[lineCount];
            editLine = lineCount;
        }
    }
    if (lineCount > 6)
        lineCount = 6;

    int16_t height = (int16_t)(lineCount * 9);
    int16_t maxw = 0;
    for (int i = 1; i <= lineCount; i++) {
        if (len[i] > maxw)
            maxw = len[i];
    }
    int16_t x0 = (int16_t)(0xA0 - maxw * 6 / 2);
    int16_t y0 = (int16_t)(0x7C - height / 2);
    int16_t boxw = (int16_t)(maxw * 6 + 6);
    int16_t boxh = (int16_t)(height + 6);
    uint16_t bytes = (uint16_t)(boxh * boxw);  // 16-bit product, as sized in the original
    uint16_t screen_ofs = (uint16_t)((y0 - 3) * 0x140 + (x0 - 3));

    // The original shows the box only when MaxAvail has room to save the
    // screen under it (the port's heap always has).
    uint8_t *save = game_alloc(bytes);
    Mouse_HideCursor();
    Blit_CopyRectStrided(gfx_screen() + screen_ofs, save, boxw, boxh, 0x140, boxw);
    Blit_FillRect((int16_t)(x0 - 3), (int16_t)(y0 - 3), boxw, boxh, 2);
    Gfx_DrawRectOutline((int16_t)(x0 - 2), (int16_t)(y0 - 2), (int16_t)(maxw * 6 + x0 + 3),
                        (int16_t)(y0 + height + 3), 4);
    Mouse_ShowCursor();
    for (int16_t i = 1; i <= lineCount; i++)
        Gfx_DrawTextF9(lines[i], x0, (int16_t)(y0 + 1 + (i - 1) * 9), maxw);

    if (edit) {
        TextFile_InputLine(buf, (int16_t)((editCol - 1) * 6 + x0),
                           (int16_t)(y0 + 1 + (editLine - 1) * 9), maxLen, false);
    } else {
        Gfx_DrawStatusText("Press mouse button");
        uint16_t counter = 0;
        // Wait for the button to be released (if held), pressed, released.
        while (button_down() && !TextFile_KeyPressed() && !input_quit_requested()) {
            Gfx_WaitVRetrace();
            blink(&counter);
        }
        while (!button_down() && !TextFile_KeyPressed() && !input_quit_requested()) {
            Gfx_WaitVRetrace();
            blink(&counter);
        }
        while (button_down() && !TextFile_KeyPressed() && !input_quit_requested()) {
            Gfx_WaitVRetrace();
            blink(&counter);
        }
        Gfx_DrawStatusText("");
        if (TextFile_KeyPressed())
            (void)TextFile_ReadKey();
    }

    Mouse_HideCursor();
    Blit_CopyRectStrided(save, gfx_screen() + screen_ofs, boxw, boxh, boxw, 0x140);
    Mouse_ShowCursor();
    game_free(save);

    if (logIt) {
        // Leading blanks of each line are dropped; empty lines are not logged.
        const char *logged[7];
        int16_t count = 0;
        for (int16_t i = 1; i <= lineCount; i++) {
            logged[i] = lines[i] + strspn(lines[i], " ");
            if (logged[i][0] != '\0')
                count++;
        }
        if (ds.Message_LogCount + count > LOG_MAX) {
            // BUG? with count > LogCount the count goes negative.
            int16_t keep = (int16_t)(ds.Message_LogCount - count);
            for (int16_t i = 1; i <= keep; i++)
                SDL_memcpy(log_entry(i), log_entry((int16_t)(i + count)), LOG_ENTRY);
            ds.Message_LogCount = (int16_t)(ds.Message_LogCount - count);
        }
        int16_t next = 1;
        for (int16_t i = 1; i <= lineCount; i++) {
            uint8_t *e = log_entry((int16_t)(ds.Message_LogCount + next));
            if (logged[i][0] != '\0') {
                wr16le(e, 99);
                pstr_set(e + 2, 0x32, logged[i]);
                next++;
            }
        }
        ds.Message_LogCount = (int16_t)(ds.Message_LogCount + count);
    }
    // 34b0:0870: logged or not, the message log screen is redrawn.
    if (ds.Game_CurrentScreen == 0x0B)
        Message_DrawMessageLog();
}

// Satellite animation helpers (nested procedures of PlaySatelliteAnim).
typedef struct {
    uint8_t *sprites;  // 10 cells of 20h columns per band of 1040h
    uint8_t *backup;
    uint16_t ofs;      // screen offset of the sprite
} SatAnim;

static void draw_sat_cell(const SatAnim *a, int16_t cell)
{
    uint16_t src = (uint16_t)((cell / 10) * 0x1040 + (cell % 10) * 0x20 + 1);
    Blit_BlitOverBackground(a->sprites + src, gfx_screen() + a->ofs, 0x17, 0xC);
}

static void save_sat_background(const SatAnim *a)
{
    Blit_CopyRect(gfx_screen() + a->ofs, a->backup, 0x17, 0xC);
}

static void restore_sat_background(const SatAnim *a)
{
    Blit_CopyRect(a->backup, gfx_screen() + a->ofs, 0x17, 0xC);
}

// 34b0:0953
void Message_PlaySatelliteAnim(int16_t satNo)
{
    Mouse_HideCursor();
    uint8_t saved_pal[0x300];
    SDL_memcpy(saved_pal, ds.Gfx_Palette, 0x300);
    uint8_t *pal = Gfx_PalettePtr();

    uint8_t *pic = game_alloc(0xBCC0);
    Gfx_LoadPic("grafika\\szatelit", pic, 1, 0x97, 0, 0, 0x140, true, pal, 0, 0xB0, 0x40,
                0, 0xBE);
    uint8_t *back = game_alloc(0xBCC0);
    Blit_CopyRect(gfx_screen() + 0x3D40, back, 0x140, 0x97);
    Blit_CopyRect(pic, gfx_screen() + 0x3D40, 0x140, 0x97);
    game_free(pic);

    SatAnim a = {.sprites = game_alloc(0x2080)};
    char name[32];
    SDL_snprintf(name, sizeof name, "grafika\\sat%d", satNo);
    Gfx_LoadPic(name, a.sprites, 1, 0x1A, 0, 0, 0x140, false, pal, 0, 0xB0, 0x40, 0, 0xBE);
    a.backup = game_alloc(0x1040);
    Gfx_FadeIn(0x40, 0xFE, pal);

    a.ofs = 0xA93A;
    for (int16_t i = 1; i <= 74; i++) {
        const TSatStep *step = &ds.Sat_AnimPath[i - 1];
        save_sat_background(&a);
        draw_sat_cell(&a, step->Frame);
        Gfx_WaitFrames(3);
        restore_sat_background(&a);
        a.ofs = (uint16_t)(a.ofs + step->DX + step->DY * 0x140);
    }
    Gfx_WaitFrames(0x46);
    game_free(a.backup);
    game_free(a.sprites);
    Gfx_FadeOut(0x40, 0xFE, pal);
    Blit_CopyRect(back, gfx_screen() + 0x3D40, 0x140, 0x97);
    SDL_memcpy(ds.Gfx_Palette, saved_pal, 0x300);
    Gfx_FadeIn(0x40, 0xFE, pal);
    game_free(back);
    Mouse_ShowCursor();
}

// 34b0:0bbe
void Message_PromptPlanetName(int16_t sys, int16_t planet)
{
    char system[256], s[256];
    pstr_get(system, sizeof system, &ds.Names_Systems[sys - 1]);
    SDL_snprintf(s, sizeof s, "| The new planet's name in %s system? &xxxxxxxx |", system);
    // The names of system sys are 13-byte records (string[12]) behind Galaxy::Names[sys].
    uint8_t *name = (uint8_t *)&ds.Galaxy_Names[sys - 1][planet - 1];
    Message_MessageBox(s, "", false, name, true, 9);
}

// 34b0:0c14
void Message_DrawMessageLog(void)
{
    for (int16_t i = 1; i <= ds.Message_LogCount; i++) {
        uint8_t *e = log_entry(i);
        int16_t kind = rd16le(e);
        char text[256];
        pstr_get(text, sizeof text, e + 2);
        if (kind == 1 || kind == 99)
            Gfx_DrawTextFC(text, 10, (int16_t)(i * 9 + 0x30), 0x32);
        else
            Gfx_DrawTextF9(text, 10, (int16_t)(i * 9 + 0x30), 0x32);
    }
}

// 34b0:0c8e
void Message_Init(void)
{
    ds.Message_Log = game_alloc(0x31B);
}
