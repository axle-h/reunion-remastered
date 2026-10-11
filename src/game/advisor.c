#include "game/advisor.h"

#include "game/blit.h"
#include "game/gfx.h"
#include "game/gfxpath.h"
#include "game/mouse.h"
#include "game/system.h"
#include "game/textfile.h"

#define PRODUCT(k) ds.Research_Products[(k) - 1]

static const uint8_t *ps(const PString *p)
{
    return (const uint8_t *)p;
}
#define LIT(s) ps(PSTR(s))

// Menu line i (1..5) of MenuLines.
static uint8_t *menu_line(int i)
{
    FarPtr b = ds.Advisor_MenuLines;
    return far_host(far_ptr((uint16_t)(b >> 16), (uint16_t)((uint16_t)b + i * 0x3D - 0x3D)));
}

// Required developer skill k (1..4) of the hired developer at the current story stage.
static uint8_t dev_need(int k)
{
    return ds.Staff_DevSkills[ds.Game_StoryStage][ds.Game_StaffHired[3] - 1][k - 1];
}

// 1991:0000
void Advisor_AdvisorScreen(void)
{
    ds.Game_ClockRunning = false;
    if (ds.Game_ScreenEntered) {
        GfxPath_ShowScreenBackdrop();
        ds.Advisor_MenuNumber = 1;
        Advisor_LoadAdvisorPictures();
        Advisor_LoadAdvisorMenu();
        Advisor_DrawAdvisorMenu();
        Advisor_SetupAdvisorHotspots();
        Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
    }
    if (ds.Game_HotspotHit != 0x15)
        return;
    Advisor_PollAdvisorMenu();
    if (ds.Command_AdvisorLine <= 0)
        return;
    ds.Advisor_Question = ds.Advisor_CurMenu.Items[ds.Command_AdvisorLine - 1];
    switch (ds.Advisor_Question) {
    case 1:
        ds.Advisor_Answer = (int16_t)(System_Random(2) + 2);
        break;
    case 2: {
        int16_t item = Advisor_FindAdvisorItem(false);
        if (System_Random(0x32) < 10)
            ds.Advisor_Answer = 4;
        else if (item == 0)
            ds.Advisor_Answer = 6;
        else
            ds.Advisor_Answer = 5;
        break;
    }
    case 3: ds.Advisor_Answer = 7; break;
    case 4: ds.Advisor_Answer = 8; break;
    case 5:
        if (ds.Advisor_TrainingKind < 1)
            ds.Advisor_Answer = ds.Command_StaffPicked == 4 ? 0xE : 10;
        else
            ds.Advisor_Answer = 9;
        if (ds.Command_StaffPicked != 4) {
            int16_t kind = ds.Command_StaffPicked;
            uint8_t ability = ds.Staff_Ability[kind - 1][ds.Game_StaffHired[kind - 1] - 1];
            if (ability <= ds.Game_StaffExp[kind - 1])
                ds.Advisor_Answer = 0xF;
        }
        break;
    case 6:
        if (ds.Game_Credits < ds.Advisor_TrainingPrice) {
            ds.Advisor_Answer = 0xC;
        } else {
            ds.Game_Credits -= ds.Advisor_TrainingPrice;
            Gfx_DrawCredits();
            ds.Advisor_Answer = 0xD;
            ds.Advisor_TrainingKind = ds.Command_StaffPicked;
            ds.Advisor_TrainingTimer = (int16_t)(System_Random(0x14) + 0x32);
        }
        break;
    case 7: ds.Advisor_Answer = 0xB; break;
    case 8:
        ds.Advisor_Answer = 10;
        ds.Advisor_TrainingCourse = 1;
        if (dev_need(1) <= ds.Game_DevSkill[0] && dev_need(2) <= ds.Game_DevSkill[1])
            ds.Advisor_Answer = 0xF;
        break;
    case 9:
        ds.Advisor_Answer = 10;
        ds.Advisor_TrainingCourse = 2;
        if (dev_need(2) <= ds.Game_DevSkill[1] && dev_need(3) <= ds.Game_DevSkill[2])
            ds.Advisor_Answer = 0xF;
        break;
    case 10:
        ds.Advisor_Answer = 10;
        ds.Advisor_TrainingCourse = 3;
        if (dev_need(1) <= ds.Game_DevSkill[0] && dev_need(3) <= ds.Game_DevSkill[2])
            ds.Advisor_Answer = 0xF;
        break;
    case 11:
        ds.Advisor_Answer = 10;
        ds.Advisor_TrainingCourse = 4;
        if (dev_need(4) <= ds.Game_DevSkill[3])
            ds.Advisor_Answer = 0xF;
        break;
    default:
        break;
    }
    Advisor_DrawAdvisorQuestion();
    Advisor_ShowAdvisorAnswer();
    if (ds.Advisor_MenuNumber == 0) {
        ds.Game_CurrentScreen = 1;
        ds.Game_ScreenSwitch = true;
    } else {
        Advisor_LoadAdvisorMenu();
        Advisor_DrawAdvisorMenu();
    }
}

// 1991:02c9
int16_t Advisor_FindAdvisorItem(bool flag)
{
    int16_t found = 0;
    for (int16_t i = 1; i <= 9; i++) {
        if (ds.Advisor_Hints[i - 1].Who == ds.Command_StaffPicked) {
            int16_t st = PRODUCT(ds.Advisor_Hints[i - 1].Product).State;
            if (flag && st == 5)
                found = i;
            if (!flag && (st == 1 || st == 2 || st == 3 || st == 4))
                found = i;
        }
    }
    return found;
}

// 1991:0355
void Advisor_LoadAdvisorPictures(void)
{
    ds.Advisor_MenuLines = System_GetMem(0x131);
    FarPtr buf = System_GetMem(0xFFFF);
    // Offsets into buf wrap inside its segment, as in the original.
    uint8_t *seg = far_host(far_ptr((uint16_t)(buf >> 16), 0));
    uint16_t off = (uint16_t)buf;
    Gfx_LoadPic(ps(PSTR("grafika\\you")), far_host(buf), 1, 0x6E, 0, 0, 0x140, false,
                Gfx_PalettePtr(), 0, 0xAF, 0x40, 0, 0xAF);
    // Our own face: hero 2 sits at the left of the sheet, hero 1 further right.
    uint16_t hero_off = ds.Game_Hero == 2 ? 0x141 : 0x1A5;
    Blit_CopyRect(seg + (uint16_t)(off + hero_off), gfx_screen() + 0x7159, 0x64, 0x6D);
    PString name;
    System_StrLoad(&name, PSTR("grafika\\"));
    System_StrConcat(&name, (const PString *)&ds.Staff_FacePics[ds.Command_StaffPicked - 1]);
    Gfx_LoadPic(ps(&name), far_host(buf), 1, 0x6E, 0, 0, 0x140, false, Gfx_PalettePtr(), 0, 0x8F,
                0x40, 0, 0x8F);
    int16_t col = (int16_t)((int16_t)(ds.Game_StaffHired[ds.Command_StaffPicked - 1] - 1) * 0x6A);
    Blit_CopyRect(seg + (uint16_t)(off + 0x145 + col), gfx_screen() + 0x7085, 0x62, 0x6D);
    System_FreeMem(buf, 0xFFFF);
}

// 1991:0487
void Advisor_DrawAdvisorQuestion(void)
{
    PString s;
    TextFile_ReadTextLine(LIT("kerdes1.sp"), ds.Advisor_Question, (uint8_t *)&s);
    Gfx_TypeTextFC(ps(&s), 6, 0x36, 0x34);
}

// The name of the invention the advisor wants (hint v), in s2.
static void hint_name(PString *s2, int16_t v)
{
    System_StrStore((const PString *)&PRODUCT(ds.Advisor_Hints[v - 1].Product).Name, s2, 255);
}

// 1991:0580
void Advisor_ShowAdvisorAnswer(void)
{
    PString s1, s2, s3, t, c;
    TextFile_ReadTextLine(LIT("valasz1.sp"), ds.Advisor_Answer, (uint8_t *)&s1);
    s2.len = 0;
    System_StrStore(&s1, &s3, 255);
    // The first two characters are the number of the next menu.
    System_CharToStr(&t, s1.s[0]);
    System_CharToStr(&c, s1.s[1]);
    System_StrConcat(&t, &c);
    System_StrStore(&t, &s1, 255);
    int16_t code;
    ds.Advisor_MenuNumber = (int16_t)System_ValLong(&s1, &code);
    System_Delete(&s3, 1, 3);
    // Text up to the first '|' is the first line, the rest the second.
    while (s3.len > 0 && s3.s[0] != '|') {
        System_StrLoad(&t, &s2);
        System_CharToStr(&c, s3.s[0]);
        System_StrConcat(&t, &c);
        System_StrStore(&t, &s2, 255);
        System_Delete(&s3, 1, 1);
    }
    if (s3.len > 0)
        System_Delete(&s3, 1, 1);

    if (ds.Advisor_Answer == 7) {
        s3.len = 0;
        int16_t v = Advisor_FindAdvisorItem(false);
        if (v == 0) {
            System_StrStore(PSTR("I don't need anything right now"), &s2, 255);
        } else {
            hint_name(&s2, v);
            if (System_Random(0x32) < 0x19) {
                System_StrLoad(&t, PSTR("We could develop a "));
                System_StrConcat(&t, &s2);
                System_StrConcat(&t, PSTR(""));
            } else {
                System_StrLoad(&t, PSTR("The most important thing would be a "));
                System_StrConcat(&t, &s2);
            }
            System_StrStore(&t, &s2, 255);
            TextFile_ReadTextLine(LIT("ajanlas.sp"), v, (uint8_t *)&s3);
        }
    }
    if (ds.Advisor_Answer == 8) {
        s3.len = 0;
        int16_t v = Advisor_FindAdvisorItem(true);
        if (v == 0) {
            System_StrStore(PSTR("I don't need anything right now."), &s2, 255);
        } else {
            hint_name(&s2, v);
            if (System_Random(0x32) < 0x19) {
                System_StrLoad(&t, PSTR("We might need "));
                System_StrConcat(&t, &s2);
                System_StrConcat(&t, PSTR("."));
            } else {
                System_StrLoad(&t, PSTR("We really need more "));
                System_StrConcat(&t, &s2);
                System_StrConcat(&t, PSTR("s"));
            }
            System_StrStore(&t, &s2, 255);
            TextFile_ReadTextLine(LIT("ajanlas.sp"), v, (uint8_t *)&s3);
        }
    }
    if (ds.Advisor_Answer == 10) {
        // Price of the help: (Random(10) + the staff member's experience) * 1000.
        uint16_t exp = ds.Game_StaffExp[ds.Command_StaffPicked - 1];
        ds.Advisor_TrainingPrice = (int32_t)((uint32_t)((uint16_t)System_Random(10) + exp) * 1000u);
        System_StrLong(ds.Advisor_TrainingPrice, 0, &s2, 255);
        System_StrLoad(&t, PSTR("It will be "));
        System_StrConcat(&t, &s2);
        System_StrConcat(&t, PSTR(" credits"));
        System_StrStore(&t, &s2, 255);
        s3.len = 0;
    }
    Gfx_TypeTextF9(ps(&s2), 0xC, 0x3E, 0x33);
    Gfx_TypeTextF9(ps(&s3), 0xC, 0x46, 0x33);
}

// 1991:08de
void Advisor_LoadAdvisorMenu(void)
{
    System_MoveFwd(&ds.Advisor_Menus[ds.Advisor_MenuNumber - 1], &ds.Advisor_CurMenu, 6);
    uint16_t n = ds.Advisor_CurMenu.Count;
    for (uint16_t i = 1; i <= n && n != 0; i++)
        TextFile_ReadTextLine(LIT("rkerdes1.sp"), ds.Advisor_CurMenu.Items[i - 1], menu_line((int)i));
}

// 1991:0946
void Advisor_DrawAdvisorMenu(void)
{
    int16_t n = ds.Advisor_CurMenu.Count;
    for (int16_t i = 1; i <= n; i++) {
        if (i == ds.Command_AdvisorLine)
            Gfx_DrawTextFC(menu_line(i), 0x70, (int16_t)(i * 9 + 0x54), 0x10);
        else
            Gfx_DrawTextF9(menu_line(i), 0x70, (int16_t)(i * 9 + 0x54), 0x10);
    }
    for (int16_t i = (int16_t)(ds.Advisor_CurMenu.Count + 1); i <= 5; i++)
        Gfx_DrawTextF9(LIT(""), 0x70, (int16_t)(i * 9 + 0x54), 0x10);
}

// 1991:09e8
void Advisor_PollAdvisorMenu(void)
{
    int16_t last_col = 0, last_row = 0; // BUG? uninitialised in the original for the first pass
    int16_t row, col;
    do {
        Mouse_Poll();
        row = (int16_t)((ds.Mouse_ClickY - 0x54) / 9);
        if (row < 0)
            row = 0;
        if (ds.Advisor_CurMenu.Count < row)
            row = 0;
        col = ds.Mouse_ClickX > 0x6F;
        if (ds.Mouse_ClickX > 0xD0)
            col = 0;
        ds.Command_AdvisorLine = (int16_t)(row * col);
        if (col != last_col || row != last_row)
            Advisor_DrawAdvisorMenu();
        last_col = col;
        last_row = row;
    } while (ds.Mouse_Buttons != 0 && col == 1 && row > 0);
}

// 1991:0a8b
void Advisor_SetupAdvisorHotspots(void)
{
    ds.Mouse_HotspotCount = 1;
    THotspot *h = Mouse_Hotspot(1);
    h->X = 0x70;
    h->Y = 0x5C;
    h->W = 0x60;
    h->H = 0x2D;
    System_StrStore(PSTR("Talking"), (PString *)&h->Caption, 0x12);
    h->Repeat_ = 0;
    h->_1A = 0;
}
