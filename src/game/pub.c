#include "game/pub.h"

#include "game/blit.h"
#include "game/gfx.h"
#include "game/mouse.h"
#include "game/sound.h"
#include "game/system.h"
#include "game/textfile.h"

// Not ported yet: see stubs.c.
bool Intel_IsRaceActive(int16_t race);  // 29b9:1461

// Unnamed globals (see pub.h).
#define PIRATES_BUF (*(FarPtr *)ds_at(0x773A))
#define PERSON (*(int16_t *)ds_at(0x7738))
#define FLAG_TOPICS (*(uint8_t *)ds_at(0x774A))
#define FLAG(k) (*(uint8_t *)ds_at((uint16_t)(0x774A + (k)))) // 774B..7754
#define HOVER_ITEM (*(int16_t *)ds_at(0x7758))
#define HOVER_FROZEN (*(uint8_t *)ds_at(0x775A))
#define HOVER_COL (*(uint16_t *)ds_at(0x775C))
#define HOVER_ROW (*(int16_t *)ds_at(0x775E))
#define NUM_STR ((PString *)ds_at(0x7760)) // string[2]
#define MENU_IDX (*(int16_t *)ds_at(0x7764))
#define Q_LINE (*(int16_t *)ds_at(0x7766))
#define R_LINE (*(int16_t *)ds_at(0x7768))
#define QCODE (*(int16_t *)ds_at(0x776A))
#define LINES_BUF (*(FarPtr *)ds_at(0x776C))
#define OFFER_ITEM (*(int32_t *)ds_at(0x7770))
#define OFFER_PRICE (*(int32_t *)ds_at(0x7774))
#define LINE_COUNT (*(int16_t *)ds_at(0x7778))
#define LINE_MAP(j) (*(uint8_t *)ds_at((uint16_t)(0x7779 + (j))))
#define LAST_SOUND (*(int16_t *)ds_at(0x7782))
#define ANIM_BUF (*(FarPtr *)ds_at(0x7784))

static const uint8_t *ps(const PString *p)
{
    return (const uint8_t *)p;
}
#define LIT(s) ps(PSTR(s))

static FarPtr off_ptr(FarPtr base, uint16_t off)
{
    return far_ptr((uint16_t)(base >> 16), (uint16_t)((uint16_t)base + off));
}

static uint8_t *line_at(int i) // question line i (1..5), string[255]
{
    return far_host(off_ptr(LINES_BUF, (uint16_t)((i - 1) * 256)));
}

// 1fd2:0000
void Pub_DrawPubSprite(int16_t idx)
{
    const TPubPerson *p = &ds.Pub_People[idx - 1];
    if ((p->State == 2 || p->State == 3) && p->Timer == 0) {
        Mouse_HideCursor();
        FarPtr src = off_ptr(PIRATES_BUF, (uint16_t)(p->SrcY * 0x140 + p->SrcX + 0x141));
        FarPtr dst = far_ptr(ds.Gfx_ScreenSeg, (uint16_t)((p->Y + 0x31) * 0x140 + p->X + 0x141));
        Blit_BlitTransparent(src, dst, (int16_t)(p->W - 2), (int16_t)(p->H - 2), 0x40);
        Mouse_ShowCursor();
    }
}

// 1fd2:00d9
void Pub_LoadPubGraphics(void)
{
    PIRATES_BUF = System_GetMem(0xAF00);
    Gfx_LoadPic(LIT("grafika\\pirates"), PIRATES_BUF, 1, 0x8C, 0, 0, 0x140, true, Gfx_PalettePtr(), 0, 0xB0, 0x40, 0, 0xBE);
    FarPtr dest = System_GetMem(0xBCC0);
    Gfx_LoadPic(LIT("grafika\\kocsma"), dest, 1, 0x97, 0, 0, 0x140, false, Gfx_PalettePtr(), 0, 0xB0, 0x40, 0, 0xBE);
    Blit_CopyRect(dest, far_ptr(ds.Gfx_ScreenSeg, 0x3D40), 0x140, 0x97);
    System_FreeMem(dest, 0xBCC0);
    ANIM_BUF = System_GetMem(64000);
    Gfx_LoadPic(LIT("grafika\\kocsmaan"), ANIM_BUF, 1, 200, 0, 0, 0x140, false, Gfx_PalettePtr(), 0, 0xB0, 0x40, 0, 0xBE);
    for (int16_t i = 10; i >= 1; i--)
        Pub_DrawPubSprite(i);
    Gfx_FadeIn(0x40, 0xFE, Gfx_PalettePtr());
}

// 1fd2:01ef
void Pub_SetupPubHotspots(void)
{
    ds.Mouse_HotspotCount = 10;
    for (int16_t k = 1; k <= 10; k++) {
        const TPubPerson *p = &ds.Pub_People[k - 1];
        if ((p->State == 0 || p->State == 2) && p->Timer == 0) {
            THotspot *h = Mouse_Hotspot(k);
            h->X = (int16_t)(p->X + 1);
            h->Y = (uint8_t)(p->Y + 0x32);
            h->W = (int16_t)(p->W - 2);
            h->H = (uint8_t)(p->H - 2);
            System_StrStore((const PString *)&p->Name, (PString *)&h->Caption, 0x12);
            h->Repeat_ = 1;
            h->_1A = 0;
        } else {
            Mouse_ClearHotspots(k, k);
        }
    }
}

// 1fd2:02f4
void Pub_LoadPersonPortrait(void)
{
    PString suffix, name, num;
    if (PERSON == 6)
        System_StrStore(PSTR("2"), &suffix, 255);
    else
        suffix.len = 0;
    LINES_BUF = System_GetMem(0x500);
    FarPtr buf = System_GetMem(0xBCC0);
    System_StrLoad(&name, PSTR("grafika\\beszed"));
    System_StrConcat(&name, &suffix);
    Gfx_LoadPic(ps(&name), buf, 1, 0x97, 0, 0, 0x140, true, Gfx_PalettePtr(), 0, 0xB0, 0x40, 0, 0xBE);
    Blit_CopyRect(buf, far_ptr(ds.Gfx_ScreenSeg, 0x3D40), 0x140, 0x97);
    System_StrLong(PERSON, 0, NUM_STR, 2);
    if (PERSON != 6) {
        System_StrLoad(&name, PSTR("alien\\kocsmb"));
        System_StrConcat(&name, NUM_STR);
        Gfx_LoadPic(ps(&name), buf, 1, 0x4E, 0, 0, 0x52, false, Gfx_PalettePtr(), 0, 0xB0, 0x40, 0, 0xBE);
        Blit_CopyRectStrided(off_ptr(buf, 0x53), off_ptr(buf, 16000), 0x4F, 0x4C, 0x52, 0x140);
        Mouse_HideCursor();
        Blit_BlitTransparent(off_ptr(buf, 16000), far_ptr(ds.Gfx_ScreenSeg, 0x3D8D), 0x4F, 0x4C, 0x40);
        Mouse_ShowCursor();
    }
    System_StrLong(ds.Pub_People[PERSON - 1].Portrait, 0, &num, 255);
    System_StrLoad(&name, PSTR("alien\\alien"));
    System_StrConcat(&name, &num);
    Gfx_LoadPic(ps(&name), buf, 1, 0x97, 0, 0, 0x62, false, Gfx_PalettePtr(), 0, 0x3F, 0xB0, 0, 0xBE);
    Mouse_HideCursor();
    Blit_CopyRectStrided(off_ptr(buf, 99), far_ptr(ds.Gfx_ScreenSeg, 0x3F60), 0x5F, 0x8F, 0x62, 0x140);
    Mouse_ShowCursor();
    System_FreeMem(buf, 0xBCC0);
}

// 1fd2:04f0
void Pub_SetupTalkHotspot(void)
{
    ds.Mouse_HotspotCount = 1;
    THotspot *h = Mouse_Hotspot(1);
    h->X = 6;
    h->Y = 0x86;
    h->W = 0xD2;
    h->H = 0x3F;
    System_StrStore(PSTR("Talking"), (PString *)&h->Caption, 0x12);
    h->Repeat_ = 0;
    h->_1A = 0;
    HOVER_COL = 0xFFFF;
    HOVER_ROW = (int16_t)0xFFFF;
}

// 1fd2:0544
void Pub_ReadQuestionCode(void)
{
    PString name, line, c1;
    System_StrLoad(&name, PSTR("kerdes"));
    System_StrConcat(&name, NUM_STR);
    System_StrConcat(&name, PSTR(".loc"));
    TextFile_ReadTextLine(ps(&name), Q_LINE, (uint8_t *)&line);
    // The code is the first two characters of the line.
    System_CharToStr(&name, line.s[0]);
    System_CharToStr(&c1, line.s[1]);
    System_StrConcat(&name, &c1);
    System_StrStore(&name, &line, 255);
    int16_t code;
    QCODE = (int16_t)System_ValLong(&line, &code);
}

// 1fd2:05fe
void Pub_ShowReply(void)
{
    PString name, line, t;
    int16_t lines;
    System_StrLoad(&name, PSTR("valasz"));
    System_StrConcat(&name, NUM_STR);
    System_StrConcat(&name, PSTR(".loc"));
    TextFile_ReadTextLine(ps(&name), R_LINE, (uint8_t *)&line);
    if (PERSON == 2 && R_LINE > 9 && R_LINE < 0xE) {
        // The trader names a price: quantity * unit price of the item.
        int32_t price = (int32_t)((uint32_t)ds.Pub_ItemQty[OFFER_ITEM - 1]
                                  * (uint32_t)ds.Pub_ItemPrice[R_LINE - 10]);
        OFFER_PRICE = price;
        ds.Pub_People[1].Deal = (uint8_t)((R_LINE - 9) * 0x14 + OFFER_ITEM);
        System_StrLong(price, 0, &line, 255);
        ds.Pub_CurMenu.Count = 1;
        if (System_Random(2) == 0)
            System_StrLoad(&t, PSTR("09 It will be "));
        else
            System_StrLoad(&t, PSTR("09 My work will cost "));
        System_StrConcat(&t, &line);
        System_StrConcat(&t, PSTR(" credits"));
        System_StrStore(&t, &line, 255);
        System_StrStore(&line, (PString *)far_host(LINES_BUF), 255);
    }
    Mouse_HideCursor();
    Blit_FillRect(6, 0x86, 0xD8, 0x3F, 0x40);
    Mouse_ShowCursor();
    // The reply's leading code is the next question menu (0 = leave), DS:7764
    // (1fd2:075f), not the question code at 776A.
    TextFile_DrawReply(ps(&line), 6, 0x86, 0x24, false, true, &MENU_IDX, &lines);
}

// 1fd2:0788
void Pub_BuildQuestionMenu(void)
{
    int16_t lines = 0;
    LINE_COUNT = 0;
    FarPtr lists = ds.Pub_MenuLists[PERSON - 1];
    System_MoveFwd(far_host(off_ptr(lists, (uint16_t)(MENU_IDX * 6))), &ds.Pub_CurMenu, 6);
    TMenu *m = &ds.Pub_CurMenu;
    // The trader's (2) and the informant's menus list only what is available.
    if (PERSON == 2 && MENU_IDX == 6) {
        m->Count = FLAG(1) != 0;
        if (m->Count)
            m->Items[0] = 10;
        static const uint8_t topic[5] = {0xB, 0xC, 0xD, 0xE, 0x1A};
        for (int k = 0; k < 5; k++) {
            if (FLAG(2 + k) != 0) {
                m->Items[m->Count] = (int8_t)topic[k];
                m->Count++;
            }
        }
    }
    if (PERSON == 2 && MENU_IDX == 7) {
        m->Count = FLAG(7) != 0;
        if (m->Count)
            m->Items[0] = 0xF;
        static const uint8_t topic[3] = {0x10, 0x11, 0x12};
        for (int k = 0; k < 3; k++) {
            if (FLAG(8 + k) != 0) {
                m->Items[m->Count] = (int8_t)topic[k];
                m->Count++;
            }
        }
    }
    uint16_t n = m->Count;
    for (uint16_t i = 1; i <= n && n != 0; i++) {
        PString name;
        System_StrLoad(&name, PSTR("kerdes"));
        System_StrConcat(&name, NUM_STR);
        System_StrConcat(&name, PSTR(".loc"));
        TextFile_ReadTextLine(ps(&name), m->Items[i - 1], line_at(i));
        lines = TextFile_CountLines(line_at(i));
        for (int16_t j = (int16_t)(LINE_COUNT + 1); j <= LINE_COUNT + lines; j++)
            LINE_MAP(j) = (uint8_t)i;
        LINE_COUNT = (int16_t)(LINE_COUNT + lines);
    }
    if (PERSON == 9 && MENU_IDX == 4) {
        m->Count = 1;
        TextFile_ReadTextLine(LIT("pirate.txt"), (int16_t)(Pub_FindActiveDeal() + 10), far_host(LINES_BUF));
    }
    // BUG? clears as many entries as the last text had lines, not the unused ones.
    for (int16_t j = (int16_t)(LINE_COUNT + 1); j <= LINE_COUNT + lines; j++)
        LINE_MAP(j) = 0;
}

// 1fd2:09ea
void Pub_DrawQuestionMenu(void)
{
    int16_t row = 0, lines = 0;
    uint16_t n = ds.Pub_CurMenu.Count;
    for (uint16_t i = 1; i <= n && n != 0; i++) {
        TextFile_DrawReply(line_at(i), 6, (int16_t)(row * 9 + 0x86), 0x24, i == (uint16_t)HOVER_ITEM, false, &QCODE, &lines);
        row = (int16_t)(row + lines);
    }
    if (row > 7)
        row = 7;
    Mouse_HideCursor();
    Blit_FillRect(6, (int16_t)(row * 9 + 0x86), 0xD8, (int16_t)((7 - row) * 9 + 1), 0x40);
    Mouse_ShowCursor();
}

// 1fd2:0a8b
void Pub_UpdateQuestionHover(void)
{
    uint16_t old_col = HOVER_COL;
    int16_t old_row = HOVER_ROW;
    if (HOVER_FROZEN == 0) {
        HOVER_ROW = (int16_t)((ds.Mouse_ClickY - 0x7D) / 9);
        if (HOVER_ROW < 0)
            HOVER_ROW = 0;
        if (LINE_COUNT < HOVER_ROW)
            HOVER_ROW = 0;
        HOVER_COL = ds.Mouse_ClickX > 5;
        if (ds.Mouse_ClickX > 0xD8)
            HOVER_COL = 0;
        HOVER_ITEM = (int16_t)(HOVER_ROW * HOVER_COL);
        if (HOVER_ITEM > 0)
            HOVER_ITEM = LINE_MAP(HOVER_ITEM);
        if (HOVER_COL != old_col || HOVER_ROW != old_row)
            Pub_DrawQuestionMenu();
    }
}

// 1fd2:0b30
void Pub_UpdateTopicAvailability(void)
{
    FLAG(7) = Intel_IsRaceActive(7);
    FLAG(8) = Intel_IsRaceActive(8);
    FLAG(9) = Intel_IsRaceActive(9);
    FLAG(10) = Intel_IsRaceActive(10);
    FLAG(5) = FLAG(7) || FLAG(8) || FLAG(9) || FLAG(10);
    FLAG(1) = Intel_IsRaceActive(3) && ds.Galaxy_SystemStatus[2] > 0;
    FLAG(2) = Intel_IsRaceActive(4) && ds.Races_Races[1].Status >= 0;
    FLAG(3) = Intel_IsRaceActive(5) && ds.Races_Races[1].Status >= 0;
    FLAG(4) = Intel_IsRaceActive(6) && *(uint8_t *)ds_at(0x5D90) == 0;
    FLAG(6) = Intel_IsRaceActive(0xC);
    FLAG_TOPICS = FLAG(1) || FLAG(2) || FLAG(3) || FLAG(4) || FLAG(5) || FLAG(6);
}

// 1fd2:0c2b
void Pub_AddToDate(int8_t hour, int8_t day, int8_t month, int16_t year, uint8_t *pHour,
                   uint8_t *pDay, uint8_t *pMonth, int16_t *pYear)
{
    *pYear = (int16_t)(*pYear + year);
    *pMonth = (uint8_t)(*pMonth + month);
    *pDay = (uint8_t)(*pDay + day);
    *pHour = (uint8_t)(*pHour + hour);
    if (*pHour > 0x17) {
        *pDay = (uint8_t)(*pDay + 1);
        *pHour = (uint8_t)(*pHour - 0x18);
    }
    if (*pDay > 0x1E) {
        *pMonth = (uint8_t)(*pMonth + 1);
        *pDay = (uint8_t)(*pDay - 0x1E);
    }
    if (*pMonth > 0xC) {
        *pYear = (int16_t)(*pYear + 1);
        *pMonth = (uint8_t)(*pMonth - 0xC);
    }
}

// 1fd2:0c95
void Pub_StartPubDealWindows(void)
{
    for (int k = 0; k < 10; k++) {
        TPubDeal *d = &ds.Pub_Deals[k];
        // Start: the stored offsets plus now. End: the same, kept in
        // Active (year) and _15[0..2] (month, day, hour).
        int16_t year = (int16_t)d->Year;
        Pub_AddToDate((int8_t)ds.Game_Hour, (int8_t)ds.Game_Day, (int8_t)ds.Game_Month,
                      (int16_t)ds.Game_Year, &d->Hour, &d->Day, &d->Month, &year);
        d->Year = (uint16_t)year;
        year = (int16_t)d->Active;
        Pub_AddToDate((int8_t)ds.Game_Hour, (int8_t)ds.Game_Day, (int8_t)ds.Game_Month,
                      (int16_t)ds.Game_Year, &d->_15[2], &d->_15[1], &d->_15[0], &year);
        d->Active = (uint16_t)year;
    }
}

// 1fd2:0d2b
int32_t Pub_DateToHours(int32_t year, int32_t month, int32_t day, int32_t hour)
{
    int32_t v = (int32_t)((uint32_t)year * 0x14u);
    v = (int32_t)((uint32_t)(v + month) * 0x28u);
    v = (int32_t)((uint32_t)(v + day) * 0x1Eu);
    return v + hour;
}

// 1fd2:0d75
int16_t Pub_FindActiveDeal(void)
{
    int16_t found = 0;
    for (int16_t k = 1; k <= 10; k++) {
        const TPubDeal *d = &ds.Pub_Deals[k - 1];
        int32_t now = Pub_DateToHours(ds.Game_Year, ds.Game_Month, ds.Game_Day, ds.Game_Hour);
        if (Pub_DateToHours((int16_t)d->Year, d->Month, d->Day, d->Hour) < now) {
            int32_t end = Pub_DateToHours((int16_t)d->Active, d->_15[0], d->_15[1], d->_15[2]);
            if (now < end)
                found = k;
        }
    }
    return found;
}

// 1fd2:0e5f
void Pub_PlayRandomPubSound(void)
{
    if (ds.Sound_DeviceType != 1 && !Sound_IsSbPlaying()) {
        int16_t n;
        do {
            n = (int16_t)(System_Random(8) + 1);
        } while (LAST_SOUND == n);
        LAST_SOUND = n;
        PString num, name;
        System_StrLong(n, 0, &num, 10);
        System_StrLoad(&name, PSTR("local"));
        System_StrConcat(&name, &num);
        Sound_PlayButtonSfx(ps(&name));
    }
}

// 1fd2:0ec1
void Pub_AnimatePubPerson(int16_t idx)
{
    TPubAnim *a = &ds.Pub_Anims[idx - 1];
    a->Timer--;
    if (a->Timer == 0) {
        a->Timer = a->FrameDelay;
        a->Frame++;
        if (a->FrameCount < a->Frame) {
            a->Frame = 1;
            a->Timer = (int16_t)(a->MinDelay * 10 + System_Random((uint16_t)(a->RandDelay * 10)) + 1);
        }
        int f = a->Frame - 1;
        uint16_t src = (uint16_t)(a->SrcX + a->SrcY * 0x140 + (f % a->FramesPerRow) * a->W
                                  + (f / a->FramesPerRow) * (a->H + 1) * 0x140);
        uint16_t dst = (uint16_t)((a->Y + 0x31) * 0x140 + a->X);
        Blit_BlitTransparent(off_ptr(ANIM_BUF, src), far_ptr(ds.Gfx_ScreenSeg, dst), a->W, a->H, 0x40);
    }
}

// 1fd2:0fc2
void Pub_UpdatePubAnimations(void)
{
    Pub_PlayRandomPubSound();
    Pub_AnimatePubPerson(6);
    Pub_AnimatePubPerson(2);
    Pub_DrawPubSprite(10);
    if (ds.Pub_People[9].State != 2)
        Pub_AnimatePubPerson(3);
    if (ds.Pub_People[8].State != 2)
        Pub_AnimatePubPerson(5);
    if (ds.Pub_People[6].State != 2)
        Pub_AnimatePubPerson(8);
    if (ds.Pub_People[2].State != 2)
        Pub_AnimatePubPerson(9);
    Pub_AnimatePubPerson(1);
    Pub_AnimatePubPerson(4);
    Pub_AnimatePubPerson(7);
}
