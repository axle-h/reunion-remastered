#include "game/pub.h"

#include <SDL3/SDL.h>
#include <string.h>

#include "game/blit.h"
#include "game/gfx.h"
#include "game/lang.h"
#include "game/mouse.h"
#include "game/sound.h"
#include "game/system.h"
#include "game/textfile.h"

// Routines missing from their units' headers.
bool Intel_IsRaceActive(int16_t race);  // 29b9:1461

// Question line i (1..5): the original's string[255] array on the heap, here
// C strings.
static char *line_at(int i)
{
    return (char *)ds.Pub_Lines + (i - 1) * 256;
}

// "<prefix><person><suffix>" (kerdes1.loc, valasz1.loc, ...).
static char *person_file(char name[32], const char *prefix, const char *suffix)
{
    char person[4];
    pstr_get(person, sizeof person, &ds.Pub_PersonStr);
    SDL_snprintf(name, 32, "%s%s%s", prefix, person, suffix);
    return name;
}

// LineItem[j]. The original doesn't check the range: the trader's menu can
// have more than 8 text lines, and then entries 9 and 10 land in LastSound
// (as here) and 11.. in AnimSheet, its offset word first. AnimSheet is a
// host pointer now, so those go to a scratch byte; no run gets that far.
static uint8_t *line_item(int16_t j)
{
    static uint8_t scratch;
    if (j <= 8)
        return &ds.Pub_LineItem[j - 1];
    if (j <= 10)
        return (uint8_t *)&ds.Pub_LastSound + (j - 9);
    return &scratch;
}

// 1fd2:0000
void Pub_DrawPubSprite(int16_t idx)
{
    const TPubPerson *p = &ds.Pub_People[idx - 1];
    if ((p->State == 2 || p->State == 3) && p->Timer == 0) {
        Mouse_HideCursor();
        uint8_t *src = ds.Pub_PiratesSheet + (uint16_t)(p->SrcY * 0x140 + p->SrcX + 0x141);
        uint8_t *dst = gfx_screen() + (uint16_t)((p->Y + 0x31) * 0x140 + p->X + 0x141);
        Blit_BlitTransparent(src, dst, (int16_t)(p->W - 2), (int16_t)(p->H - 2), 0x40);
        Mouse_ShowCursor();
    }
}

// 1fd2:00d9
void Pub_LoadPubGraphics(void)
{
    ds.Pub_PiratesSheet = game_alloc(0xAF00);
    Gfx_LoadPic("grafika\\pirates", ds.Pub_PiratesSheet, 1, 0x8C, 0, 0, 0x140, true, Gfx_PalettePtr(), 0, 0xB0, 0x40, 0, 0xBE);
    uint8_t *dest = game_alloc(0xBCC0);
    Gfx_LoadPic("grafika\\kocsma", dest, 1, 0x97, 0, 0, 0x140, false, Gfx_PalettePtr(), 0, 0xB0, 0x40, 0, 0xBE);
    Blit_CopyRect(dest, gfx_screen() + 0x3D40, 0x140, 0x97);
    game_free(dest);
    ds.Pub_AnimSheet = game_alloc(64000);
    Gfx_LoadPic("grafika\\kocsmaan", ds.Pub_AnimSheet, 1, 200, 0, 0, 0x140, false, Gfx_PalettePtr(), 0, 0xB0, 0x40, 0, 0xBE);
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
            char name[256];
            pstr_set(&h->Caption, 0x12, pstr_get(name, sizeof name, &p->Name));
            h->Repeat_ = 1;
            h->Cursor = 0;
        } else {
            Mouse_ClearHotspots(k, k);
        }
    }
}

// 1fd2:02f4
void Pub_LoadPersonPortrait(void)
{
    char name[32], person[4];
    ds.Pub_Lines = game_alloc(0x500);
    uint8_t *buf = game_alloc(0xBCC0);
    SDL_snprintf(name, sizeof name, "grafika\\beszed%s", ds.Pub_Person == 6 ? "2" : "");
    Gfx_LoadPic(name, buf, 1, 0x97, 0, 0, 0x140, true, Gfx_PalettePtr(), 0, 0xB0, 0x40, 0, 0xBE);
    Blit_CopyRect(buf, gfx_screen() + 0x3D40, 0x140, 0x97);
    SDL_snprintf(person, sizeof person, "%d", ds.Pub_Person);
    pstr_set(&ds.Pub_PersonStr, 2, person);
    if (ds.Pub_Person != 6) {
        Gfx_LoadPic(person_file(name, "alien\\kocsmb", ""), buf, 1, 0x4E, 0, 0, 0x52, false,
                    Gfx_PalettePtr(), 0, 0xB0, 0x40, 0, 0xBE);
        Blit_CopyRectStrided(buf + 0x53, buf + 16000, 0x4F, 0x4C, 0x52, 0x140);
        Mouse_HideCursor();
        Blit_BlitTransparent(buf + 16000, gfx_screen() + 0x3D8D, 0x4F, 0x4C, 0x40);
        Mouse_ShowCursor();
    }
    SDL_snprintf(name, sizeof name, "alien\\alien%d", ds.Pub_People[ds.Pub_Person - 1].Portrait);
    Gfx_LoadPic(name, buf, 1, 0x97, 0, 0, 0x62, false, Gfx_PalettePtr(), 0, 0x3F, 0xB0, 0, 0xBE);
    Mouse_HideCursor();
    Blit_CopyRectStrided(buf + 99, gfx_screen() + 0x3F60, 0x5F, 0x8F, 0x62, 0x140);
    Mouse_ShowCursor();
    game_free(buf);
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
    pstr_set(&h->Caption, 0x12, Lang_TextAt(0x1fd2050f, "Talking"));
    h->Repeat_ = 0;
    h->Cursor = 0;
    ds.Pub_HoverCol = 0xFFFF;
    ds.Pub_HoverRow = (int16_t)0xFFFF;
}

// 1fd2:0544
void Pub_ReadQuestionCode(void)
{
    char name[32], line[256] = "";
    TextFile_ReadTextLine(person_file(name, "kerdes", ".loc"), ds.Pub_QuestionLine, line,
                          sizeof line);
    // The code is the first two characters of the line.
    ds.Pub_QuestionCode = TextFile_LineCode(line);
}

// 1fd2:05fe
void Pub_ShowReply(void)
{
    char name[32], line[256] = "";
    int16_t lines;
    TextFile_ReadTextLine(person_file(name, "valasz", ".loc"), ds.Pub_ReplyLine, line,
                          sizeof line);
    if (ds.Pub_Person == 2 && ds.Pub_ReplyLine > 9 && ds.Pub_ReplyLine < 0xE) {
        // The trader names a price: quantity * unit price of the item.
        int32_t price = (int32_t)((uint32_t)ds.Pub_ItemQty[ds.Pub_OfferItem - 1]
                                  * (uint32_t)ds.Pub_ItemPrice[ds.Pub_ReplyLine - 10]);
        ds.Pub_OfferPrice = price;
        ds.Pub_People[1].Deal = (uint8_t)((ds.Pub_ReplyLine - 9) * 0x14 + ds.Pub_OfferItem);
        ds.Pub_CurMenu.Count = 1;
        if (System_Random(2) == 0)
            SDL_snprintf(line, sizeof line, "%s%d%s", Lang_TextAt(0x1fd206bd, "09 It will be "),
                         (int)price, Lang_TextAt(0x1fd206d2, " credits"));
        else
            SDL_snprintf(line, sizeof line, "%s%d%s", Lang_Text("09 My work will cost "), (int)price,
                         Lang_TextAt(0x1fd20707, " credits"));
        SDL_strlcpy(line_at(1), line, 256);
    }
    Mouse_HideCursor();
    Blit_FillRect(6, 0x86, 0xD8, 0x3F, 0x40);
    Mouse_ShowCursor();
    // The reply's leading code is the next question menu (0 = leave), MenuIdx
    // (1fd2:075f), not QuestionCode.
    TextFile_DrawReply(line, 6, 0x86, 0x24, false, true, &ds.Pub_MenuIdx, &lines);
}

// 1fd2:0788
void Pub_BuildQuestionMenu(void)
{
    int16_t lines = 0;
    ds.Pub_LineCount = 0;
    memcpy(&ds.Pub_CurMenu, ds.Pub_MenuLists[ds.Pub_Person - 1] + ds.Pub_MenuIdx, 6);
    TMenu *m = &ds.Pub_CurMenu;
    // The trader's (2) and the informant's menus list only what is available.
    if (ds.Pub_Person == 2 && ds.Pub_MenuIdx == 6) {
        m->Count = ds.Pub_TopicAvail[0] != 0;
        if (m->Count)
            m->Items[0] = 10;
        static const uint8_t topic[5] = {0xB, 0xC, 0xD, 0xE, 0x1A};
        for (int k = 0; k < 5; k++) {
            if (ds.Pub_TopicAvail[1 + k] != 0) {
                m->Items[m->Count] = (int8_t)topic[k];
                m->Count++;
            }
        }
    }
    if (ds.Pub_Person == 2 && ds.Pub_MenuIdx == 7) {
        m->Count = ds.Pub_TopicAvail[6] != 0;
        if (m->Count)
            m->Items[0] = 0xF;
        static const uint8_t topic[3] = {0x10, 0x11, 0x12};
        for (int k = 0; k < 3; k++) {
            if (ds.Pub_TopicAvail[7 + k] != 0) {
                m->Items[m->Count] = (int8_t)topic[k];
                m->Count++;
            }
        }
    }
    uint16_t n = m->Count;
    for (uint16_t i = 1; i <= n && n != 0; i++) {
        char name[32];
        TextFile_ReadTextLine(person_file(name, "kerdes", ".loc"), m->Items[i - 1], line_at(i),
                              256);
        lines = TextFile_CountLines(line_at(i));
        for (int16_t j = (int16_t)(ds.Pub_LineCount + 1); j <= ds.Pub_LineCount + lines; j++)
            *line_item(j) = (uint8_t)i;
        ds.Pub_LineCount = (int16_t)(ds.Pub_LineCount + lines);
    }
    if (ds.Pub_Person == 9 && ds.Pub_MenuIdx == 4) {
        m->Count = 1;
        TextFile_ReadTextLine("pirate.txt", (int16_t)(Pub_FindActiveDeal() + 10), line_at(1), 256);
    }
    // Clears as many entries as the last text had lines, not the unused ones, as
    // the original does: the hover is capped at the line count, so the rest are never read.
    for (int16_t j = (int16_t)(ds.Pub_LineCount + 1); j <= ds.Pub_LineCount + lines; j++)
        *line_item(j) = 0;
}

// 1fd2:09ea
void Pub_DrawQuestionMenu(void)
{
    int16_t row = 0, lines = 0;
    uint16_t n = ds.Pub_CurMenu.Count;
    for (uint16_t i = 1; i <= n && n != 0; i++) {
        TextFile_DrawReply(line_at(i), 6, (int16_t)(row * 9 + 0x86), 0x24,
                           i == (uint16_t)ds.Pub_HoverItem, false, &ds.Pub_QuestionCode, &lines);
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
    uint16_t old_col = ds.Pub_HoverCol;
    int16_t old_row = ds.Pub_HoverRow;
    if (ds.Pub_Replied == 0) {
        ds.Pub_HoverRow = (int16_t)((ds.Mouse_ClickY - 0x7D) / 9);
        if (ds.Pub_HoverRow < 0)
            ds.Pub_HoverRow = 0;
        if (ds.Pub_LineCount < ds.Pub_HoverRow)
            ds.Pub_HoverRow = 0;
        ds.Pub_HoverCol = ds.Mouse_ClickX > 5;
        if (ds.Mouse_ClickX > 0xD8)
            ds.Pub_HoverCol = 0;
        ds.Pub_HoverItem = (int16_t)(ds.Pub_HoverRow * ds.Pub_HoverCol);
        if (ds.Pub_HoverItem > 0)
            ds.Pub_HoverItem = *line_item(ds.Pub_HoverItem);
        if (ds.Pub_HoverCol != old_col || ds.Pub_HoverRow != old_row)
            Pub_DrawQuestionMenu();
    }
}

// 1fd2:0b30
void Pub_UpdateTopicAvailability(void)
{
    ds.Pub_TopicAvail[6] = Intel_IsRaceActive(7);
    ds.Pub_TopicAvail[7] = Intel_IsRaceActive(8);
    ds.Pub_TopicAvail[8] = Intel_IsRaceActive(9);
    ds.Pub_TopicAvail[9] = Intel_IsRaceActive(10);
    ds.Pub_TopicAvail[4] = ds.Pub_TopicAvail[6] || ds.Pub_TopicAvail[7] || ds.Pub_TopicAvail[8]
                           || ds.Pub_TopicAvail[9];
    ds.Pub_TopicAvail[0] = Intel_IsRaceActive(3) && ds.Galaxy_SystemStatus[2] > 0;
    ds.Pub_TopicAvail[1] = Intel_IsRaceActive(4) && ds.Races_Races[1].Status >= 0;
    ds.Pub_TopicAvail[2] = Intel_IsRaceActive(5) && ds.Races_Races[1].Status >= 0;
    ds.Pub_TopicAvail[3] = Intel_IsRaceActive(6) && ds.Intel_System4Destroyed == 0;
    ds.Pub_TopicAvail[5] = Intel_IsRaceActive(0xC);
    ds.Pub_AnyTopic = ds.Pub_TopicAvail[0] || ds.Pub_TopicAvail[1] || ds.Pub_TopicAvail[2]
                      || ds.Pub_TopicAvail[3] || ds.Pub_TopicAvail[4] || ds.Pub_TopicAvail[5];
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
        // Start and end: the stored offsets plus now.
        int16_t year = (int16_t)d->Year;
        Pub_AddToDate((int8_t)ds.Game_Hour, (int8_t)ds.Game_Day, (int8_t)ds.Game_Month,
                      (int16_t)ds.Game_Year, &d->Hour, &d->Day, &d->Month, &year);
        d->Year = (uint16_t)year;
        year = (int16_t)d->EndYear;
        Pub_AddToDate((int8_t)ds.Game_Hour, (int8_t)ds.Game_Day, (int8_t)ds.Game_Month,
                      (int16_t)ds.Game_Year, &d->EndHour, &d->EndDay, &d->EndMonth, &year);
        d->EndYear = (uint16_t)year;
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
            int32_t end = Pub_DateToHours((int16_t)d->EndYear, d->EndMonth, d->EndDay, d->EndHour);
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
        } while (ds.Pub_LastSound == n);
        ds.Pub_LastSound = n;
        char name[16];
        SDL_snprintf(name, sizeof name, "local%d", n);
        Sound_PlayButtonSfx(name);
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
        Blit_BlitTransparent(ds.Pub_AnimSheet + src, gfx_screen() + dst, a->W, a->H, 0x40);
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
