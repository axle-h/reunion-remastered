#include "game/message.h"

#include <SDL3/SDL.h>

#include "game/blit.h"
#include "game/dseg.h"
#include "game/gfx.h"
#include "game/mouse.h"
#include "game/sound.h"
#include "game/system.h"
#include "game/textfile.h"
#include "platform/input.h"

enum : uint16_t { LOG_ENTRY = 0x35, LOG_MAX = 15 };

static inline const uint8_t *ps(const PString *s)
{
    return (const uint8_t *)s;
}

// Log entry i (1-based): { kind: integer; text: string[50] }.
static uint8_t *log_entry(int16_t i)
{
    return (uint8_t *)far_host(ds.Message_Log) + (uint16_t)(i * LOG_ENTRY - LOG_ENTRY);
}

// 34b0:0001
void Message_AddMessageLogEntry(int16_t kind, const uint8_t *text, const uint8_t *voice)
{
    uint8_t t[256], v[256];
    SDL_memcpy(t, text, (size_t)text[0] + 1);
    SDL_memcpy(v, voice, (size_t)voice[0] + 1);
    if (v[0] != 0)
        Sound_PlayButtonSfx(v);
    ds.Gfx_FlashStep = 0x3D;
    ds.Gfx_FlashCount = 3;
    Gfx_PaletteFlashTick();
    if (ds.Message_LogCount >= LOG_MAX) {
        int16_t last = (int16_t)(ds.Message_LogCount - 1);
        for (int16_t i = 1; i <= last; i++)
            System_MoveFwd(log_entry((int16_t)(i + 1)), log_entry(i), LOG_ENTRY);
        ds.Message_LogCount--;
    }
    ds.Message_LogCount++;
    uint8_t *e = log_entry(ds.Message_LogCount);
    *(int16_t *)e = kind;
    System_StrStore((const PString *)t, (PString *)(e + 2), 0x32);
    if (ds.Game_CurrentScreen == 0x0B)
        Message_DrawMessageLog();
}

// 34b0:011a
void Message_ShowMessageLine(int16_t lineNo)
{
    uint8_t s[256];
    TextFile_ReadTextLine(ps(PSTR("message.txt")), lineNo, s);
    Message_MessageBox(s, ps(PSTR("attentio")), true, s, false, 0);
}

// 34b0:0158
void Message_ShowMessage(const uint8_t *text)
{
    uint8_t s[256];
    SDL_memcpy(s, text, (size_t)text[0] + 1);
    Message_MessageBox(s, ps(PSTR("message")), true, s, false, 0);
}

// 34b0:0197
void Message_ShowMessageNoLog(const uint8_t *text)
{
    uint8_t s[256];
    SDL_memcpy(s, text, (size_t)text[0] + 1);
    Message_MessageBox(s, ps(PSTR("hiba")), false, s, false, 0);
}

// 34b0:01df
void Message_ShowKitalalLine(int16_t lineNo)
{
    uint8_t line[256];
    TextFile_ReadTextLine(ps(PSTR("kitalal.txt")), lineNo, line);
    PString tmp;
    // A space on each side (34b0:01dd is the string ' ').
    System_StrLoad(&tmp, PSTR(" "));
    System_StrConcat(&tmp, (const PString *)line);
    System_StrConcat(&tmp, PSTR(" "));
    Message_ShowMessage(ps(&tmp));
}

// The blinking "Press mouse button" in the status bar: counter 1..50, text
// shown at 25, cleared at 50.
static void blink(uint16_t *counter)
{
    *counter = (uint16_t)(*counter % 50 + 1);
    if (*counter == 25)
        Gfx_DrawStatusText(ps(PSTR("")));
    if (*counter == 50)
        Gfx_DrawStatusText(ps(PSTR("Press mouse button")));
}

static bool button_down(void)
{
    Mouse_Update();
    return ds.Mouse_BtnDown != 0;
}

// 34b0:0237
void Message_MessageBox(const uint8_t *text, const uint8_t *voice, bool logIt, uint8_t *buf,
                        bool edit, uint8_t maxLen)
{
    uint8_t txt[256], vc[256];
    uint8_t lines[7][55];  // string[54], 1..6 (the original has no room for more)
    SDL_memcpy(txt, text, (size_t)text[0] + 1);
    SDL_memcpy(vc, voice, (size_t)voice[0] + 1);

    if (vc[0] != 0)
        Sound_PlayButtonSfx(vc);
    for (int i = 1; i <= 6; i++)
        lines[i][0] = 0;

    int16_t lineCount = 1;
    int16_t editCol = 0, editLine = 0;
    for (uint16_t i = 1; i <= txt[0]; i++) {
        if (txt[i] != '|') {
            // BUG? more than 6 lines overflow the original's array; extra text is dropped here.
            if (lineCount <= 6 && lines[lineCount][0] < 54)
                lines[lineCount][1 + lines[lineCount][0]++] = txt[i];
        } else {
            lineCount++;
        }
        if (txt[i] == '&' && lineCount <= 6) {
            editCol = lines[lineCount][0];
            editLine = lineCount;
        }
    }
    if (lineCount > 6)
        lineCount = 6;

    int16_t height = (int16_t)(lineCount * 9);
    int16_t maxw = 0;
    for (int i = 1; i <= lineCount; i++) {
        if (lines[i][0] > maxw)
            maxw = lines[i][0];
    }
    int16_t x0 = (int16_t)(0xA0 - maxw * 6 / 2);
    int16_t y0 = (int16_t)(0x7C - height / 2);
    int16_t boxw = (int16_t)(maxw * 6 + 6);
    int16_t boxh = (int16_t)(height + 6);
    uint16_t bytes = (uint16_t)(boxh * boxw);  // 16-bit product, as sized in the original
    uint16_t screen_ofs = (uint16_t)((y0 - 3) * 0x140 + (x0 - 3));

    bool saved = (int32_t)(int16_t)bytes <= System_MaxAvail();
    FarPtr save = 0;
    if (saved) {
        save = System_GetMem(bytes);
        Mouse_HideCursor();
        Blit_CopyRectStrided(gfx_screen() + screen_ofs, far_host(save), boxw, boxh, 0x140, boxw);
        Blit_FillRect((int16_t)(x0 - 3), (int16_t)(y0 - 3), boxw, boxh, 2);
        Gfx_DrawRectOutline((int16_t)(x0 - 2), (int16_t)(y0 - 2), (int16_t)(maxw * 6 + x0 + 3),
                            (int16_t)(y0 + height + 3), 4);
        Mouse_ShowCursor();
        for (int16_t i = 1; i <= lineCount; i++)
            Gfx_DrawTextF9(lines[i], x0, (int16_t)(y0 + 1 + (i - 1) * 9), maxw);
    }

    if (edit) {
        TextFile_InputLine(buf, (int16_t)((editCol - 1) * 6 + x0),
                           (int16_t)(y0 + 1 + (editLine - 1) * 9), maxLen, false);
    } else {
        Gfx_DrawStatusText(ps(PSTR("Press mouse button")));
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
        Gfx_DrawStatusText(ps(PSTR("")));
        if (TextFile_KeyPressed())
            (void)TextFile_ReadKey();
    }

    if (saved) {
        Mouse_HideCursor();
        Blit_CopyRectStrided(far_host(save), gfx_screen() + screen_ofs, boxw, boxh, boxw, 0x140);
        Mouse_ShowCursor();
        System_FreeMem(save, bytes);
    }

    if (logIt) {
        // Leading blanks of each line are dropped; empty lines are not logged.
        int16_t count = 0;
        for (int16_t i = 1; i <= lineCount;) {
            if (lines[i][0] != 0 && lines[i][1] == ' ') {
                System_Delete((PString *)lines[i], 1, 1);
                continue;
            }
            if (lines[i][0] != 0)
                count++;
            i++;
        }
        if (ds.Message_LogCount + count > LOG_MAX) {
            // BUG? with count > LogCount the count goes negative.
            int16_t keep = (int16_t)(ds.Message_LogCount - count);
            for (int16_t i = 1; i <= keep; i++)
                System_MoveFwd(log_entry((int16_t)(i + count)), log_entry(i), LOG_ENTRY);
            ds.Message_LogCount = (int16_t)(ds.Message_LogCount - count);
        }
        int16_t next = 1;
        for (int16_t i = 1; i <= lineCount; i++) {
            uint8_t *e = log_entry((int16_t)(ds.Message_LogCount + next));
            if (lines[i][0] != 0) {
                *(int16_t *)e = 99;
                System_StrStore((const PString *)lines[i], (PString *)(e + 2), 0x32);
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
    FarPtr sprites;   // 10 cells of 20h columns per band of 1040h
    FarPtr backup;
    uint16_t ofs;     // screen offset of the sprite
} SatAnim;

static void draw_sat_cell(const SatAnim *a, int16_t cell)
{
    uint16_t src = (uint16_t)((cell / 10) * 0x1040 + (cell % 10) * 0x20 + 1);
    Blit_BlitOverBackground((uint8_t *)far_host(a->sprites) + src, gfx_screen() + a->ofs, 0x17,
                            0xC);
}

static void save_sat_background(const SatAnim *a)
{
    Blit_CopyRect(gfx_screen() + a->ofs, far_host(a->backup), 0x17, 0xC);
}

static void restore_sat_background(const SatAnim *a)
{
    Blit_CopyRect(far_host(a->backup), gfx_screen() + a->ofs, 0x17, 0xC);
}

// 34b0:0953
void Message_PlaySatelliteAnim(int16_t satNo)
{
    if (System_MaxAvail() <= 0x199FF)
        return;
    Mouse_HideCursor();
    uint8_t saved_pal[0x300];
    System_MoveFwd(ds.Gfx_Palette, saved_pal, 0x300);
    uint8_t *pal = Gfx_PalettePtr();

    FarPtr pic = System_GetMem(0xBCC0);
    Gfx_LoadPic(ps(PSTR("grafika\\szatelit")), far_host(pic), 1, 0x97, 0, 0, 0x140, true, pal, 0,
                0xB0, 0x40, 0, 0xBE);
    FarPtr back = System_GetMem(0xBCC0);
    Blit_CopyRect(gfx_screen() + 0x3D40, far_host(back), 0x140, 0x97);
    Blit_CopyRect(far_host(pic), gfx_screen() + 0x3D40, 0x140, 0x97);
    System_FreeMem(pic, 0xBCC0);

    SatAnim a = {.sprites = System_GetMem(0x2080)};
    PString num;
    System_StrLong(satNo, 0, &num, 0xFF);
    PString name;
    System_StrLoad(&name, PSTR("grafika\\sat"));
    System_StrConcat(&name, &num);
    Gfx_LoadPic(ps(&name), far_host(a.sprites), 1, 0x1A, 0, 0, 0x140, false, pal, 0, 0xB0, 0x40, 0,
                0xBE);
    a.backup = System_GetMem(0x1040);
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
    System_FreeMem(a.backup, 0x1040);
    System_FreeMem(a.sprites, 0x2080);
    Gfx_FadeOut(0x40, 0xFE, pal);
    Blit_CopyRect(far_host(back), gfx_screen() + 0x3D40, 0x140, 0x97);
    System_MoveFwd(saved_pal, ds.Gfx_Palette, 0x300);
    Gfx_FadeIn(0x40, 0xFE, pal);
    System_FreeMem(back, 0xBCC0);
    Mouse_ShowCursor();
}

// 34b0:0bbe
void Message_PromptPlanetName(int16_t sys, int16_t planet)
{
    PString s;
    System_StrLoad(&s, PSTR("| The new planet's name in "));
    System_StrConcat(&s, (const PString *)&ds.Names_Systems[sys - 1]);
    System_StrConcat(&s, PSTR(" system? &xxxxxxxx |"));
    // The names of system sys are 13-byte records (string[12]) behind Galaxy::Names[sys].
    uint32_t table = ds.Galaxy_Names[sys - 1];
    FarPtr name = far_ptr((uint16_t)(table >> 16), (uint16_t)((uint16_t)table + planet * 13 - 13));
    TextFile_SetStringPtr(name);  // the editor keeps it (TextFile::InputStr)
    Message_MessageBox(ps(&s), ps(PSTR("")), false, far_host(name), true, 9);
}

// 34b0:0c14
void Message_DrawMessageLog(void)
{
    for (int16_t i = 1; i <= ds.Message_LogCount; i++) {
        uint8_t *e = log_entry(i);
        int16_t kind = *(int16_t *)e;
        if (kind == 1 || kind == 99)
            Gfx_DrawTextFC(e + 2, 10, (int16_t)(i * 9 + 0x30), 0x32);
        else
            Gfx_DrawTextF9(e + 2, 10, (int16_t)(i * 9 + 0x30), 0x32);
    }
}

// 34b0:0c8e
void Message_Init(void)
{
    ds.Message_Log = System_GetMem(0x31B);
}
