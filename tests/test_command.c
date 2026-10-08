// command and title: entering the command-centre screen draws the backdrop
// and sets up the hotspot table; the ambient lights and the main-menu
// palette. Needs the original game data (GRAFIKA\MAIN.PIC and friends).
#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/command.h"
#include "game/dseg.h"
#include "game/gfx.h"
#include "game/mouse.h"
#include "game/system.h"
#include "game/title.h"
#include "platform/fs.h"
#include "platform/video.h"

static bool ok = true;
static uint8_t *scr; // A000:0000 as a flat array
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static int retraces;
static bool count_retrace(void)
{
    retraces++;
    return true;
}

static void reset(void)
{
    mem_init();
    SDL_memset(vid_palette, 0, sizeof vid_palette);
    CHECK(dseg_init());
    system_init();
    ds.System_SegA000 = 0xA000;
    ds.Gfx_ScreenSeg = 0xA000;
    ds.Gfx_LineStride = 320;
    ds.Gfx_LineStrideCopy = 320;
    ds.Mouse_CursorState = 1; // hidden
    gfx_wait_retrace = count_retrace;
    scr = (uint8_t *)vid_screen;
}

static int nonzero(uint32_t from, uint32_t to)
{
    int n = 0;
    for (uint32_t i = from; i < to; i++)
        n += scr[i] != 0;
    return n;
}

static void menu_palette(void)
{
    reset();
    Title_SetMainMenuPalette();
    CHECK(ds.Gfx_Palette[1][0] == 0x1E && ds.Gfx_Palette[1][2] == 0x1E);
    CHECK(ds.Gfx_Palette[0x35][0] == 0x0F && ds.Gfx_Palette[0x35][1] == 0x22 && ds.Gfx_Palette[0x35][2] == 0x38);
    CHECK(ds.Gfx_Palette[0xFA][0] == 0x2D && ds.Gfx_Palette[0xFA][1] == 0x2A);
    // F9..FE are copied down to F1..F6.
    for (int i = 0; i < 6; i++)
        CHECK(SDL_memcmp(ds.Gfx_Palette[0xF1 + i], ds.Gfx_Palette[0xF9 + i], 3) == 0);
    CHECK(ds.Gfx_Palette[0xF5][0] == 0x39 && ds.Gfx_Palette[0xF6][0] == 0x29 && ds.Gfx_Palette[0xF6][2] == 4);
}

static void enter_command_centre(void)
{
    reset();
    ds.Game_Hero = 1;
    ds.Game_HeroSpriteX = 0x45;
    ds.Game_StaffHired[0] = 1; // a pilot hired: hotspot 1
    ds.Game_StaffHired[3] = 2; // and a developer: hotspot 2
    ds.Game_ScreenEntered = true;
    Command_HandleCommandCentre();

    // The backdrop fills the window below the toolbar (rows 49..199).
    CHECK(nonzero(0x3D40, 320 * 200) > 40000);
    CHECK(nonzero(0, 0x3D40) == 0);
    // FadeIn(0, FF) leaves the DAC equal to the game palette, which now holds
    // the picture colours from 40h.
    CHECK(SDL_memcmp(vid_palette, ds.Gfx_Palette, sizeof vid_palette) == 0);
    bool colours = false;
    for (int i = 0x40; i < 0xF0; i++)
        colours |= ds.Gfx_Palette[i][0] || ds.Gfx_Palette[i][1] || ds.Gfx_Palette[i][2];
    CHECK(colours);
    CHECK(retraces > 0);
    // Every pixel of the window uses a colour index of the picture (40h..F0h).
    for (uint32_t i = 0x3D40 + 320 * 20; i < 0x3D40 + 320 * 100; i++)
        if (scr[i] != 0 && (scr[i] < 0x40 || scr[i] > 0xF0)) {
            CHECK(false);
            break;
        }

    // Hotspot table.
    CHECK(ds.Mouse_HotspotCount == 12);
    CHECK(*(int16_t *)ds_at(0x89AA) == 2);
    CHECK(*(int16_t *)ds_at(0x89A2) == 1 && *(int16_t *)ds_at(0x89A4) == 4);
    THotspot *h = Mouse_Hotspot(1);
    CHECK(h->X == (int16_t)(*(uint16_t *)ds_at(0x54CE) + 1));
    CHECK(h->W == (int16_t)(*(uint16_t *)ds_at(0x5510 - 2) - 2));
    CHECK(h->Caption.len > 0 && h->Caption.len <= 18 && h->Repeat_ == 1 && h->_1A == 0);
    // Pilot candidate 1's name (Staff::Names[0][0]).
    CHECK(SDL_memcmp(h->Caption.s, ds.Staff_Names[0][0].s, h->Caption.len) == 0);
    h = Mouse_Hotspot(2);
    CHECK(SDL_memcmp(h->Caption.s, ds.Staff_Names[3][1].s, h->Caption.len) == 0 && h->Caption.len > 0);
    for (int j = 3; j <= 4; j++) {
        h = Mouse_Hotspot(j);
        CHECK(h->X == 0 && h->Y == 0 && h->W == 0 && h->H == 0 && h->Caption.len == 0 && h->Repeat_ == 1);
    }
    h = Mouse_Hotspot(5);
    CHECK(h->X == 0x2A && h->Y == 0x47 && h->W == 0x26 && h->H == 0x3C);
    CHECK(h->Caption.len == 15 && SDL_memcmp(h->Caption.s, "RESEARCH-DESIGN", 15) == 0);
    h = Mouse_Hotspot(12);
    CHECK(h->X == 0x49 && h->Y == 0x9D && h->W == 0xBE && h->H == 0x2B);
    CHECK(h->Caption.len == 11 && SDL_memcmp(h->Caption.s, "PLANET MAIN", 11) == 0);
    for (int j = 5; j <= 12; j++)
        CHECK(Mouse_Hotspot(j)->Repeat_ == 1 && Mouse_Hotspot(j)->_1A == 0);

    // A room animation is running, with its random start delay of 100..299.
    int16_t anim = *(int16_t *)ds_at(0x7E2A), delay = *(int16_t *)ds_at(0x7E2E);
    CHECK(anim >= 1 && anim <= 6);
    CHECK(delay >= 100 && delay <= 299);

    // The ambient lights repaint their pixels in the picture colours.
    uint8_t before[0x20];
    SDL_memcpy(before, scr + 0x3D65, sizeof before);
    for (int i = 0; i < 40; i++)
        Command_TickAmbientLights();
    CHECK(*(int16_t *)ds_at(0x7E14) != 1);
    for (int i = 1; i < 12; i++) {
        uint8_t px = scr[*(uint16_t *)ds_at((uint16_t)(0x5523 + 3 * i)) + 0x3D40];
        CHECK(px >= 0x40 && px < 0xF0);
    }
    for (int i = 0; i < 400; i++)
        Command_TickPanelDoorAnim();

    // Leaving: the staff hotspots and screen switches.
    ds.Game_ScreenEntered = false;
    ds.Game_HotspotHit = 0x20; // PLANET-MAIN -> surface (screen 14h)
    ds.Game_ScreenSwitch = false;
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    menu_palette();
    enter_command_centre();
    return ok ? 0 : 1;
}
