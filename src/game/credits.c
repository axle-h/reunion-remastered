// Unit Credits (264a): see credits.h.
#include "game/credits.h"

#include "game/blit.h"
#include "game/gfx.h"
#include "game/mouse.h"
#include "game/protect.h"
#include "game/system.h"

#define P(lit) ((const uint8_t *)PSTR(lit))

// 264a:01b8
void Credits_ShowCredits(void)
{
    FarPtr pal = Gfx_PalettePtr();
    Gfx_FadeOut(0x40, 0xFE, pal);
    Mouse_HideCursor();
    Blit_FillRect(0, 0x31, 0x140, 0x97, 0x40);
    Mouse_ShowCursor();
    Gfx_FadeIn(0x40, 0xFE, pal);

    int16_t x = 0x32, y = 0x35;
    Gfx_TypeTextFC(P("             R E U N I O N"), x + 3, y + 0, 0x28);
    Gfx_TypeTextF9(P("                   by"), x, y + 0x09, 0x28);
    Gfx_TypeTextF9(P("             Amnesty Design"), x, y + 0x12, 0x28);
    Gfx_TypeTextF9(P(""), x, y + 0x1B, 0x28);
    Gfx_TypeTextF9(P("     PROGRAMMED BY:   ISTVAN KISS"), x, y + 0x24, 0x28);
    Gfx_TypeTextF9(P("  AMIGA VERSION BY:   JANOS KISTAMAS"), x, y + 0x2D, 0x28);
    Gfx_TypeTextF9(P("                      KRISZTIAN JAMBOR"), x, y + 0x36, 0x28);
    Gfx_TypeTextF9(P("         DESIGN BY:   CSABA GYARMATI"), x, y + 0x3F, 0x28);
    Gfx_TypeTextF9(P("        ARTWORK BY:   TAMAS FODOR"), x, y + 0x48, 0x28);
    Gfx_TypeTextF9(P("          MUSIC BY:   TAMAS KREINER"), x, y + 0x51, 0x28);
    Gfx_TypeTextF9(P("   PROGRAM MANAGER:   STEVE SARGENT"), x, y + 0x63, 0x28);
    Gfx_TypeTextF9(P("                      GABOR FEHER"), x, y + 0x6C, 0x28);
    Gfx_TypeTextF9(P("COPYRIGHT (C) 1994 GRANDSLAM VIDEO LIMITED"), x, y + 0x7E, 0x2A);
    Gfx_TypeTextF9(P("            ALL RIGHTS RESERVED"), x, y + 0x87 + 3, 0x28);
}

// 264a:034a
void Credits_EnforceManualCheck(void)
{
    // Scheme A is only live while PageIndex holds a page number (1..21).
    if (ds.Protect_PageIndex == PROTECT_PAGE_UNARMED || ds.Protect_PageIndex < 1 || ds.Protect_PageIndex > 21)
        return;
    // REMOVED CHECK: if Key2 was never answered the original scribbles 20
    // random bytes over random DS offsets (once); a wrong answer corrupts 18
    // Galaxy name entries when Key1 reaches 91h. The port treats the page as
    // answered correctly: Key2 := PageCodes[PageIndex], then the correct-answer
    // branch below runs, as it does for a real player.
    Protect_PassManualCheck();
    if (ds.Protect_Key2 > PROTECT_KEY2_UNANSWERED && ds.Protect_Key2 == ds.Protect_PageCodes[ds.Protect_PageIndex - 1])
        ds.Protect_Key1 = System_Random(7) + 0x479;
}
