#include "game/advisor.h"

#include "game/blit.h"
#include "game/gfx.h"
#include "game/gfxpath.h"
#include "game/mouse.h"
#include "game/system.h"
#include "game/textfile.h"

// Unnamed globals (see advisor.h).
#define MENU_NO (*(int16_t *)ds_at(0x771C))
#define QUESTION (*(int16_t *)ds_at(0x771E))
#define ANSWER (*(int16_t *)ds_at(0x7720))
#define MENU_LINES (*(FarPtr *)ds_at(0x7722))
#define KIND (*(int16_t *)ds_at(0x7E3C))
#define SEL_LINE (*(int16_t *)ds_at(0x7E3E))
#define STRIKE_TIMER (*(int16_t *)ds_at(0x5D9C))
#define STRIKE_KIND (*(int16_t *)ds_at(0x5D9E))
#define HELP_KIND (*(int16_t *)ds_at(0x5DA0))
#define HELP_PRICE (*(int32_t *)ds_at(0x5DA2))

#define PRODUCT(k) ds.Research_Products[(k) - 1]

static const uint8_t *ps(const PString *p)
{
    return (const uint8_t *)p;
}
#define LIT(s) ps(PSTR(s))

// Menu line i (1..5) of the buffer at 7722.
static uint8_t *menu_line(int i)
{
    FarPtr b = MENU_LINES;
    return far_host(far_ptr((uint16_t)(b >> 16), (uint16_t)((uint16_t)b + i * 0x3D - 0x3D)));
}

// Required developer skill k (1..4) of the hired developer at the current story stage.
static uint8_t dev_need(int k)
{
    return *(uint8_t *)ds_at((uint16_t)(0x57EB + ds.Game_StoryStage * 12 + ds.Game_StaffHired[3] * 4 + k));
}

// 1991:0000
void Advisor_AdvisorScreen(void)
{
    ds.Game_ClockRunning = false;
    if (ds.Game_ScreenEntered) {
        GfxPath_ShowScreenBackdrop();
        MENU_NO = 1;
        Advisor_LoadAdvisorPictures();
        Advisor_LoadAdvisorMenu();
        Advisor_DrawAdvisorMenu();
        Advisor_SetupAdvisorHotspots();
        Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
    }
    if (ds.Game_HotspotHit != 0x15)
        return;
    Advisor_PollAdvisorMenu();
    if (SEL_LINE <= 0)
        return;
    QUESTION = ds.Advisor_CurMenu.Items[SEL_LINE - 1];
    switch (QUESTION) {
    case 1:
        ANSWER = (int16_t)(System_Random(2) + 2);
        break;
    case 2: {
        int16_t item = Advisor_FindAdvisorItem(false);
        if (System_Random(0x32) < 10)
            ANSWER = 4;
        else if (item == 0)
            ANSWER = 6;
        else
            ANSWER = 5;
        break;
    }
    case 3: ANSWER = 7; break;
    case 4: ANSWER = 8; break;
    case 5:
        if (STRIKE_KIND < 1)
            ANSWER = KIND == 4 ? 0xE : 10;
        else
            ANSWER = 9;
        if (KIND != 4) {
            uint8_t ability = *(uint8_t *)ds_at((uint16_t)(0x57E0 + KIND * 3 + ds.Game_StaffHired[KIND - 1]));
            if (ability <= ds.Game_StaffExp[KIND - 1])
                ANSWER = 0xF;
        }
        break;
    case 6:
        if (ds.Game_Credits < HELP_PRICE) {
            ANSWER = 0xC;
        } else {
            ds.Game_Credits -= HELP_PRICE;
            Gfx_DrawCredits();
            ANSWER = 0xD;
            STRIKE_KIND = KIND;
            STRIKE_TIMER = (int16_t)(System_Random(0x14) + 0x32);
        }
        break;
    case 7: ANSWER = 0xB; break;
    case 8:
        ANSWER = 10;
        HELP_KIND = 1;
        if (dev_need(1) <= ds.Game_DevSkill[0] && dev_need(2) <= ds.Game_DevSkill[1])
            ANSWER = 0xF;
        break;
    case 9:
        ANSWER = 10;
        HELP_KIND = 2;
        if (dev_need(2) <= ds.Game_DevSkill[1] && dev_need(3) <= ds.Game_DevSkill[2])
            ANSWER = 0xF;
        break;
    case 10:
        ANSWER = 10;
        HELP_KIND = 3;
        if (dev_need(1) <= ds.Game_DevSkill[0] && dev_need(3) <= ds.Game_DevSkill[2])
            ANSWER = 0xF;
        break;
    case 11:
        ANSWER = 10;
        HELP_KIND = 4;
        if (dev_need(4) <= ds.Game_DevSkill[3])
            ANSWER = 0xF;
        break;
    default:
        break;
    }
    Advisor_DrawAdvisorQuestion();
    Advisor_ShowAdvisorAnswer();
    if (MENU_NO == 0) {
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
        if (ds.Advisor_Hints[i - 1].Who == KIND) {
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
    MENU_LINES = System_GetMem(0x131);
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
    System_StrConcat(&name, (const PString *)&ds.Staff_FacePics[KIND - 1]);
    Gfx_LoadPic(ps(&name), far_host(buf), 1, 0x6E, 0, 0, 0x140, false, Gfx_PalettePtr(), 0, 0x8F,
                0x40, 0, 0x8F);
    int16_t col = (int16_t)((int16_t)(ds.Game_StaffHired[KIND - 1] - 1) * 0x6A);
    Blit_CopyRect(seg + (uint16_t)(off + 0x145 + col), gfx_screen() + 0x7085, 0x62, 0x6D);
    System_FreeMem(buf, 0xFFFF);
}

// 1991:0487
void Advisor_DrawAdvisorQuestion(void)
{
    PString s;
    TextFile_ReadTextLine(LIT("kerdes1.sp"), QUESTION, (uint8_t *)&s);
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
    TextFile_ReadTextLine(LIT("valasz1.sp"), ANSWER, (uint8_t *)&s1);
    s2.len = 0;
    System_StrStore(&s1, &s3, 255);
    // The first two characters are the number of the next menu.
    System_CharToStr(&t, s1.s[0]);
    System_CharToStr(&c, s1.s[1]);
    System_StrConcat(&t, &c);
    System_StrStore(&t, &s1, 255);
    int16_t code;
    MENU_NO = (int16_t)System_ValLong(&s1, &code);
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

    if (ANSWER == 7) {
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
    if (ANSWER == 8) {
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
    if (ANSWER == 10) {
        // Price of the help: (Random(10) + the staff member's experience) * 1000.
        HELP_PRICE = (int32_t)((uint32_t)((uint16_t)System_Random(10) + ds.Game_StaffExp[KIND - 1]) * 1000u);
        System_StrLong(HELP_PRICE, 0, &s2, 255);
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
    System_MoveFwd(&ds.Advisor_Menus[MENU_NO - 1], &ds.Advisor_CurMenu, 6);
    uint16_t n = ds.Advisor_CurMenu.Count;
    for (uint16_t i = 1; i <= n && n != 0; i++)
        TextFile_ReadTextLine(LIT("rkerdes1.sp"), ds.Advisor_CurMenu.Items[i - 1], menu_line((int)i));
}

// 1991:0946
void Advisor_DrawAdvisorMenu(void)
{
    int16_t n = ds.Advisor_CurMenu.Count;
    for (int16_t i = 1; i <= n; i++) {
        if (i == SEL_LINE)
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
        SEL_LINE = (int16_t)(row * col);
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
