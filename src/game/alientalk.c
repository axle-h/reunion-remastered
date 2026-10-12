// Unit AlienTalk (2e4b): see alientalk.h.
#include "game/alientalk.h"

#include <string.h>

#include "game/blit.h"
#include "game/command.h"
#include "game/gfx.h"
#include "game/gfxpath.h"
#include "game/mouse.h"
#include "game/sound.h"
#include "game/system.h"
#include "game/textfile.h"

#define P(lit) ((const uint8_t *)PSTR(lit))

static const uint8_t *ps(const PString *p) { return &p->len; }
static uint8_t *screen_at(uint16_t off) { return gfx_screen() + off; }

// "<prefix><race><suffix>" as a Pascal string.
static void text_name(PString *out, const char *prefix, const char *suffix)
{
    PString tmp;
    tmp.len = 0;
    for (const char *c = prefix; *c; c++)
        tmp.s[tmp.len++] = *c;
    System_StrConcat(&tmp, (PString *)&ds.AlienTalk_RaceStr);
    for (const char *c = suffix; *c; c++)
        tmp.s[tmp.len++] = *c;
    System_StrStore(&tmp, out, 255);
}

// 2e4b:0000
void AlienTalk_StartAlienScene(int16_t scene)
{
    ds.Game_ScreenSwitch = true;
    ds.Game_CurrentScreen = 0x21;
    ds.AlienTalk_Scene = scene;
}

// 2e4b:0044
void AlienTalk_PlayAlienScene(int16_t scene)
{
    Mouse_HideCursor();
    uint8_t *save = System_GetMem(0x1540);
    uint8_t *buf = System_GetMem(64000);
    Blit_CopyRect(screen_at(0x2800), save, 0x140, 0x11);
    uint8_t old_pal[0x300];
    memcpy(old_pal, ds.Gfx_Palette, 0x300);

    PString num, name;
    System_StrLong(scene, 0, &num, 255);
    System_StrLoad(&name, PSTR("pics\\pic"));
    System_StrConcat(&name, &num);
    uint8_t *pal = Gfx_PalettePtr();
    Gfx_LoadPic(ps(&name), buf, 1, 200, 0, 0, 0x140, true, pal, 0, 0xBF, 0x40, 0, 0xBF);
    Blit_CopyRect(buf, screen_at(0), 0x140, 200);
    System_FreeMem(buf, 64000);
    Gfx_FadeIn(0, 0xFF, pal);

    if (scene != 1 && scene != 2 && scene != 9 && scene != 10)
        Gfx_WaitFramesOrClick(30000);

    if (scene == 1) {
        Command_InitRoomAnim();
        Command_StartMainAnim(10);
        // Wait for a click, its release and the next click.
        for (;;) {
            Mouse_Update();
            if (ds.Mouse_BtnDown)
                break;
            Command_StepMainAnim();
            Gfx_WaitFrames(10);
        }
        for (;;) {
            Mouse_Update();
            if (!ds.Mouse_BtnDown)
                break;
            Command_StepMainAnim();
            Gfx_WaitFrames(10);
        }
        for (;;) {
            Mouse_Update();
            if (ds.Mouse_BtnDown)
                break;
            Command_StepMainAnim();
            Gfx_WaitFrames(10);
        }
        Command_FreeRoomAnim();
    }
    if (scene == 2) {
        Command_InitRoomAnim();
        Command_StartMainAnim(14);
        Gfx_WaitFramesOrClick(20);
        int16_t n = (int16_t)(ds.Cc_Doors[ds.Command_AnimId - 1].Frames - 1);
        for (int16_t i = 1; i <= n; i++) {
            Command_StepMainAnim();
            Gfx_WaitFrames(6);
        }
        Command_FreeRoomAnim();
        Gfx_WaitFramesOrClick(30000);
    }
    if (scene == 9) {
        Command_InitRoomAnim();
        Command_StartMainAnim(8);
        ds.Command_AnimFrame = 0;
        uint16_t n = ds.Cc_Doors[ds.Command_AnimId - 1].Frames;
        for (uint16_t i = 1; n != 0; i++) {
            if (i == 5)
                Sound_PlayButtonSfx(P("satrobb1"));
            Command_StepMainAnim();
            Gfx_WaitFrames(2);
            if (i == n)
                break;
        }
        Sound_PlayButtonSfx(P("satrobb2"));
        Command_StartMainAnim(9);
        ds.Command_AnimFrame = 0;
        n = ds.Cc_Doors[ds.Command_AnimId - 1].Frames;
        for (uint16_t i = 1; n != 0; i++) {
            if (i == 0x12)
                Sound_PlayButtonSfx(P("satrobb3"));
            Command_StepMainAnim();
            Gfx_WaitVRetrace();
            if (i > 0x1C)
                Gfx_WaitFrames(4);
            if (i == n)
                break;
        }
        Command_FreeRoomAnim();
    }
    if (scene == 10) {
        Command_InitRoomAnim();
        Command_StartMainAnim(13);
        ds.Command_AnimFrame = 0;
        uint16_t n = ds.Cc_Doors[ds.Command_AnimId - 1].Frames;
        for (uint16_t i = 1; n != 0; i++) {
            if (i == 10)
                Sound_PlayButtonSfx(P("tractor"));
            Command_StepMainAnim();
            Gfx_WaitFrames(5);
            if (i == n)
                break;
        }
        Sound_StopAndFreeSample();
        Command_FreeRoomAnim();
    }

    Gfx_FadeOut(0, 0xFF, pal);
    Blit_CopyRect(save, screen_at(0x2800), 0x140, 0x11);
    memcpy(ds.Gfx_Palette, old_pal, 0x300);
    System_FreeMem(save, 0x1540);
    Mouse_ShowCursor();
    ds.Command_BackdropShown = 0;
}

// 2e4b:03a5
void AlienTalk_StartAlienTalk(int16_t race)
{
    ds.Game_ScreenSwitch = true;
    ds.Game_CurrentScreen = 0x22;
    ds.AlienTalk_Race = race;
    System_StrLong(race, 0, (PString *)&ds.AlienTalk_RaceStr, 2);
}

// 2e4b:0400
void AlienTalk_DrawAlienTalkScreen(void)
{
    ds.AlienTalk_QuestionBuf = System_GetMem(0x300);
    uint8_t *pic = System_GetMem(0xBCC0);
    uint8_t *pal = Gfx_PalettePtr();
    PString num, name;
    uint8_t portrait = ds.AlienTalk_PortraitByRace[ds.AlienTalk_Race - 1];

    Gfx_LoadPic(P("pics\\atvezeto"), pic, 1, 0x97, 0, 0, 0x140, ds.Command_BackdropShown != 0, pal,
                0, 0xB0, 0x40, 0, 0xBE);
    Blit_CopyRect(pic, screen_at(0x3D40), 0x140, 0x97);
    if (ds.AlienTalk_Race == 6)
        GfxPath_ShowToolbarInstant();

    System_StrLong(portrait, 0, &num, 255);
    System_StrLoad(&name, PSTR("pics\\szek"));
    System_StrConcat(&name, &num);
    Gfx_LoadPic(ps(&name), pic, 1, 0x52, 0, 0, 0x6C, false, pal, 0, 0xB0, 0x40, 0, 0xBE);
    Blit_CopyRectStrided(pic + 0x6D, screen_at(0x3EF3), 0x6A, 0x50, 0x6C, 0x140);

    System_StrLong(ds.Game_Hero, 0, &num, 255);
    System_StrLoad(&name, PSTR("pics\\sajat"));
    System_StrConcat(&name, &num);
    Gfx_LoadPic(ps(&name), pic, 1, 0x52, 0, 0, 0x44, false, pal, 0, 0xB0, 0x40, 0, 0xBE);
    Blit_CopyRectStrided(pic + 0x45, screen_at(0x3E97), 0x41, 0x50, 0x44, 0x140);

    System_StrLong(portrait, 0, &num, 255);
    System_StrLoad(&name, PSTR("alien\\alien"));
    System_StrConcat(&name, &num);
    Gfx_LoadPic(ps(&name), pic, 1, 0x97, 0, 0, 0x62, false, pal, 0, 0x3F, 0xB0, 0, 0xBE);
    Mouse_HideCursor();
    Blit_CopyRectStrided(pic + 100, screen_at(0x3F60), 0x5F, 0x95, 0x62, 0x140);
    Mouse_ShowCursor();
    System_FreeMem(pic, 0xBCC0);
}

// 2e4b:0622
void AlienTalk_SetupTalkHotspots(void)
{
    ds.Mouse_HotspotCount = 1;
    THotspot *h = &ds.Mouse_Hotspots[0];
    h->X = 6;
    h->Y = 0x86;
    h->W = 0xD2;
    h->H = 0x3F;
    System_StrStore(PSTR("Talking"), (PString *)&h->Caption, 0x12);
    h->Repeat_ = 0;
    h->_1A = 0;
    ds.AlienTalk_HoverCol = 0xFFFF;
    ds.AlienTalk_HoverRow = -1;
}

// 2e4b:0675
void AlienTalk_LoadQuestionAnswerId(void)
{
    PString name, line, id;
    text_name(&name, "kerdes", ".at");
    TextFile_ReadTextLine(ps(&name), ds.AlienTalk_QuestionLine, (uint8_t *)&line);
    id.len = 2;
    id.s[0] = line.s[0];
    id.s[1] = line.s[1];
    int16_t code;
    ds.AlienTalk_AnswerId = (int16_t)System_ValLong(&id, &code);
}

// 2e4b:0700
void AlienTalk_DrawAlienReply(void)
{
    PString name, line;
    int16_t lines;
    text_name(&name, "valasz", ".at");
    TextFile_ReadTextLine(ps(&name), ds.AlienTalk_ReplyLine, (uint8_t *)&line);
    Mouse_HideCursor();
    Blit_FillRect(6, 0x86, 0xD8, 0x3F, 0x40);
    Mouse_ShowCursor();
    TextFile_DrawReply(ps(&line), 6, 0x86, 0x24, false, true, &ds.AlienTalk_MenuNumber, &lines);
}

static PString *question(int k)
{
    return (PString *)(ds.AlienTalk_QuestionBuf + (k - 1) * 0x100);
}

// LineQuestion[i]. The original doesn't check the range: menus of more than
// 8 text lines run on into Surface::Mode (the clearing loop below does so
// for the Eran's first menu). Addressed through DS so the same bytes are
// read and written.
static uint8_t *line_question(int16_t i)
{
    return (uint8_t *)&ds + offsetof(DSeg, AlienTalk_LineQuestion) + (i - 1);
}

// 2e4b:0780
void AlienTalk_BuildQuestionMenu(void)
{
    ds.AlienTalk_MenuLineCount = 0;
    // The race's list of menus: 4 bytes each (count + 3 question numbers).
    const uint8_t *menu = ds.Alien_MenuLists[ds.AlienTalk_Race - 2] + ds.AlienTalk_MenuNumber * 4;
    memcpy(ds.Alien_CurMenu, menu, 4);
    uint16_t count = ds.Alien_CurMenu[0];
    int16_t lines = 0; // BUG?: the final clearing loop reuses the last question's line count
    PString name;
    for (uint16_t k = 1; k <= count; k++) {
        text_name(&name, "kerdes", ".at");
        TextFile_ReadTextLine(ps(&name), (int8_t)ds.Alien_CurMenu[k], (uint8_t *)question(k));
        lines = TextFile_CountLines(ps(question(k)));
        for (int16_t i = ds.AlienTalk_MenuLineCount + 1; i <= ds.AlienTalk_MenuLineCount + lines; i++)
            *line_question(i) = (uint8_t)k;
        ds.AlienTalk_MenuLineCount += lines;
    }
    // Zero as many entries after the last line as the last question had lines.
    for (int16_t i = ds.AlienTalk_MenuLineCount + 1; i <= ds.AlienTalk_MenuLineCount + lines; i++)
        *line_question(i) = 0;
}

// 2e4b:088d
void AlienTalk_DrawQuestionMenu(void)
{
    int16_t row = 0;
    uint16_t count = ds.Alien_CurMenu[0];
    for (uint16_t k = 1; k <= count; k++) {
        int16_t lines;
        TextFile_DrawReply(ps(question(k)), 6, (int16_t)(row * 9 + 0x86), 0x24,
                           k == ds.AlienTalk_SelectedLine, false, &ds.AlienTalk_AnswerId, &lines);
        row += lines;
    }
    if (row > 7)
        row = 7;
    Mouse_HideCursor();
    Blit_FillRect(6, (int16_t)(row * 9 + 0x86), 0xD8, (int16_t)((7 - row) * 9 + 1), 0x40);
    Mouse_ShowCursor();
}

// 2e4b:092e
void AlienTalk_HandleTalkMouse(void)
{
    uint16_t old_col = ds.AlienTalk_HoverCol;
    int16_t old_row = ds.AlienTalk_HoverRow;
    if (ds.AlienTalk_Locked != 0)
        return;
    ds.AlienTalk_HoverRow = (int16_t)((ds.Mouse_ClickY - 0x7D) / 9);
    if (ds.AlienTalk_HoverRow < 1)
        ds.AlienTalk_HoverRow = 0;
    if (ds.AlienTalk_MenuLineCount < ds.AlienTalk_HoverRow)
        ds.AlienTalk_HoverRow = 0;
    ds.AlienTalk_HoverCol = ds.Mouse_ClickX > 5;
    if (ds.Mouse_ClickX > 0xD8)
        ds.AlienTalk_HoverCol = 0;
    ds.AlienTalk_SelectedLine = (uint16_t)(ds.AlienTalk_HoverRow * ds.AlienTalk_HoverCol);
    if ((int16_t)ds.AlienTalk_SelectedLine > 0)
        ds.AlienTalk_SelectedLine = *line_question((int16_t)ds.AlienTalk_SelectedLine);
    if (ds.AlienTalk_HoverCol != old_col || ds.AlienTalk_HoverRow != old_row)
        AlienTalk_DrawQuestionMenu();
}

