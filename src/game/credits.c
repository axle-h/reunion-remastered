// Unit Credits (264a): see credits.h.
#include "game/credits.h"

#include "game/blit.h"
#include "game/gfx.h"
#include "game/mouse.h"
#include "game/system.h"


// 264a:01b8
void Credits_ShowCredits(void)
{
    uint8_t *pal = Gfx_PalettePtr();
    Gfx_FadeOut(0x40, 0xFE, pal);
    Mouse_HideCursor();
    Blit_FillRect(0, 0x31, 0x140, 0x97, 0x40);
    Mouse_ShowCursor();
    Gfx_FadeIn(0x40, 0xFE, pal);

    int16_t x = 0x32, y = 0x35;
    Gfx_TypeTextFC("             R E U N I O N", x + 3, y + 0, 0x28);
    Gfx_TypeTextF9("                   by", x, y + 0x09, 0x28);
    Gfx_TypeTextF9("             Amnesty Design", x, y + 0x12, 0x28);
    Gfx_TypeTextF9("", x, y + 0x1B, 0x28);
    Gfx_TypeTextF9("     PROGRAMMED BY:   ISTVAN KISS", x, y + 0x24, 0x28);
    Gfx_TypeTextF9("  AMIGA VERSION BY:   JANOS KISTAMAS", x, y + 0x2D, 0x28);
    Gfx_TypeTextF9("                      KRISZTIAN JAMBOR", x, y + 0x36, 0x28);
    Gfx_TypeTextF9("         DESIGN BY:   CSABA GYARMATI", x, y + 0x3F, 0x28);
    Gfx_TypeTextF9("        ARTWORK BY:   TAMAS FODOR", x, y + 0x48, 0x28);
    Gfx_TypeTextF9("          MUSIC BY:   TAMAS KREINER", x, y + 0x51, 0x28);
    Gfx_TypeTextF9("   PROGRAM MANAGER:   STEVE SARGENT", x, y + 0x63, 0x28);
    Gfx_TypeTextF9("                      GABOR FEHER", x, y + 0x6C, 0x28);
    Gfx_TypeTextF9("COPYRIGHT (C) 1994 GRANDSLAM VIDEO LIMITED", x, y + 0x7E, 0x2A);
    Gfx_TypeTextF9("            ALL RIGHTS RESERVED", x, y + 0x87 + 3, 0x28);
}

