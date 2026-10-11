// pub: date arithmetic of the deal windows, hotspots from the people table, topic flags.
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "game/dseg.h"
#include "game/mouse.h"
#include "game/pub.h"
#include "game/system.h"
#include "game/textfile.h"
#include "platform/fs.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    mem_init();
    CHECK(dseg_init());

    // ((year * 20 + month) * 40 + day) * 30 + hour
    CHECK(Pub_DateToHours(0, 0, 0, 5) == 5);
    CHECK(Pub_DateToHours(0, 0, 1, 0) == 30);
    CHECK(Pub_DateToHours(0, 1, 0, 0) == 1200);
    CHECK(Pub_DateToHours(1, 0, 0, 0) == 24000);

    // Carries: 24 hours, 30 days, 12 months; hour/day/month add as bytes.
    uint8_t h = 20, d = 29, m = 12;
    int16_t y = 3000;
    Pub_AddToDate(5, 1, 0, 2, &h, &d, &m, &y);
    CHECK(h == 1 && d == 1 && m == 1 && y == 3003);   // 25 -> 1 +1 day; 31 -> 1 +1 month; 13 -> 1 +1 year
    h = 23; d = 30; m = 12; y = 1;
    Pub_AddToDate(0, 0, 0, 0, &h, &d, &m, &y);
    CHECK(h == 23 && d == 30 && m == 12 && y == 1);   // already at the maximum: no carry

    // Deals: windows start at now + offset; FindActiveDeal returns the last open one.
    ds.Game_Year = 3000; ds.Game_Month = 2; ds.Game_Day = 3; ds.Game_Hour = 4;
    for (int k = 0; k < 10; k++) {
        TPubDeal *dl = &ds.Pub_Deals[k];
        dl->Year = 0; dl->Month = 0; dl->Day = 0; dl->Hour = 0;
        dl->Active = 0; dl->_15[0] = 0; dl->_15[1] = 0; dl->_15[2] = 0;
    }
    // Deal 3: starts in 2 hours, ends in 5 hours; deal 6: starts now, ends in 1 hour.
    ds.Pub_Deals[2].Hour = 2;  ds.Pub_Deals[2]._15[2] = 5;
    ds.Pub_Deals[5].Hour = 0;  ds.Pub_Deals[5]._15[2] = 1;
    Pub_StartPubDealWindows();
    CHECK(ds.Pub_Deals[2].Year == 3000 && ds.Pub_Deals[2].Month == 2 && ds.Pub_Deals[2].Day == 3 && ds.Pub_Deals[2].Hour == 6);
    CHECK(ds.Pub_Deals[2].Active == 3000 && ds.Pub_Deals[2]._15[0] == 2 && ds.Pub_Deals[2]._15[1] == 3 && ds.Pub_Deals[2]._15[2] == 9);
    CHECK(Pub_FindActiveDeal() == 0);          // now (4h) is before 6h and not after deal 6's start
    ds.Game_Hour = 7;
    CHECK(Pub_FindActiveDeal() == 3);          // inside [6h, 9h)
    ds.Game_Hour = 9;
    CHECK(Pub_FindActiveDeal() == 0);          // the end is exclusive

    // People: only present (state 0 or 2) people without a timer get a hotspot.
    for (int k = 0; k < 10; k++) {
        ds.Pub_People[k].State = 1;
        ds.Pub_People[k].Timer = 0;
        ds.Pub_People[k].X = (int16_t)(10 * k);
        ds.Pub_People[k].Y = (uint8_t)(5 * k);
        ds.Pub_People[k].W = 40;
        ds.Pub_People[k].H = 30;
    }
    ds.Pub_People[0].State = 0;
    ds.Pub_People[1].State = 2;
    ds.Pub_People[2].State = 2;
    ds.Pub_People[2].Timer = 3;
    Pub_SetupPubHotspots();
    CHECK(ds.Mouse_HotspotCount == 10);
    CHECK(Mouse_Hotspot(1)->X == 1 && Mouse_Hotspot(1)->Y == 0x32 && Mouse_Hotspot(1)->W == 38 && Mouse_Hotspot(1)->H == 28);
    CHECK(Mouse_Hotspot(1)->Caption.len == ds.Pub_People[0].Name.len);
    CHECK(Mouse_Hotspot(2)->X == 11 && Mouse_Hotspot(2)->Y == 0x32 + 5);
    CHECK(Mouse_Hotspot(3)->W == 0 && Mouse_Hotspot(3)->Caption.len == 0);   // cleared
    CHECK(Mouse_Hotspot(4)->W == 0);

    // Without the other units no race is active, so no topics are offered.
    Pub_UpdateTopicAvailability();
    CHECK(ds.Pub_AnyTopic == 0);
    for (int i = 0; i < 10; i++)
        CHECK(ds.Pub_TopicAvail[i] == 0);

    // A reply's two-digit code is the next question menu, MenuIdx (0 leaves
    // the dialog); QuestionCode is left alone (1fd2:075f).
    system_init();
    // The charset lives in the game data block, which this test doesn't
    // load; blank glyphs instead.
    static uint8_t charset[0x2880];
    ds.Gfx_Charset = charset;
    ds.Pub_Person = 1;                                             // not the trader
    memcpy(&ds.Pub_PersonStr, "\x01" "1", 2);                      // '1'
    for (int16_t r = 1; r <= 3; r++) {
        uint8_t line[256];
        TextFile_ReadTextLine((const uint8_t *)"\x0B" "valasz1.loc", r, line);
        int16_t want = (int16_t)((line[1] - '0') * 10 + (line[2] - '0'));
        ds.Pub_ReplyLine = r;
        ds.Pub_MenuIdx = 99;
        ds.Pub_QuestionCode = 0x55;
        Pub_ShowReply();
        CHECK(ds.Pub_MenuIdx == want);
        CHECK(ds.Pub_QuestionCode == 0x55);
    }

    fs_shutdown();
    printf("%s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
