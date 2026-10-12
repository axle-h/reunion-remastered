#include "game/title.h"

#include <SDL3/SDL.h>
#include <string.h>

#include "game/blit.h"
#include "game/gfx.h"
#include "game/gfxpath.h"
#include "game/mouse.h"
#include "game/music.h"
#include "game/sound.h"
#include "game/system.h"

// Mouse::BtnDown is written by the INT 33h callback; here Mouse_Update stands in
// for it, and a retrace wait lets the platform layer run.
static void wait_button_release(void)
{
    for (;;) {
        Mouse_Update();
        if (ds.Mouse_BtnDown == 0)
            return;
        Gfx_WaitVRetrace();
    }
}

static void wait_button_press(void)
{
    for (;;) {
        Mouse_Update();
        if (ds.Mouse_BtnDown != 0)
            return;
        Gfx_WaitVRetrace();
    }
}

// 321d:000d
void Title_HeroSelect(void)
{
    uint8_t *pal = Gfx_PalettePtr();

    Mouse_ShowCursor();
    wait_button_release();
    wait_button_press();
    Mouse_HideCursor();
    ds.Game_Hero = (uint16_t)ds.Mouse_X < 0xA0 ? 2 : 1;
    ds.Game_HeroSpriteX = ds.Game_Hero == 1 ? 0x45 : 1;
    // Music and the sampled jingles share the one voice on non-GUS cards.
    if (ds.Sound_Enabled != 0 && ds.Sound_DeviceType != 1)
        Music_StopMusic();
    Sound_PreloadSelectSfx();
    Sound_PlaySelectSfx(1);
    Gfx_FadeToWhite(1, 0xFF, pal, 100);
    while (Sound_IsSbPlaying())
        Gfx_WaitVRetrace();
    Sound_PlaySelectSfx(2);
    char name[32];
    SDL_snprintf(name, sizeof name, "grafika\\hero%d", ds.Game_Hero);
    Gfx_LoadPic(name, gfx_screen(), 1, 200, 0, 0, 0x140, false, pal, 0, 0xFF, 0, 0, 0xFF);
    Gfx_WaitFrames(140);
    Gfx_FadeFromWhite(1, 0xFF, pal, 150);
    while (Sound_IsSbPlaying())
        Gfx_WaitVRetrace();
    Sound_PlaySelectSfx(3);
    Gfx_FadeOutSteps(0, 0xFF, pal, 120);
    Sound_FreeSelectSfx();
}

// 321d:0155
void Title_StartMenu(void)
{
    uint8_t *pal = Gfx_PalettePtr();

    Gfx_BlankPalette(pal, 0, 0xFF);
    Music_PlayTrack("choise");
    // Port deviation (docs/port.md, "Deliberate deviations"): no "choose your
    // hero" voice yet.
    Music_HoldHeroVoice(true);
    Gfx_LoadPic("grafika\\options", gfx_screen(), 1, 200, 0, 0, 0x140, false, pal,
                0, 0xFF, 0, 0, 0xFF);
    Gfx_FadeIn(0, 0xFF, pal);
    Mouse_ShowCursor();
    ds.Game_StartChoice = 0;
    while (ds.Game_StartChoice == 0) {
        wait_button_release();
        wait_button_press();
        uint16_t x = (uint16_t)ds.Mouse_X, y = (uint16_t)ds.Mouse_Y;
        if (x < 0x4B || x > 0xFA)
            continue;
        if (y >= 0x30 && y < 0x4C)
            ds.Game_StartChoice = 1;
        if (y >= 0x4C && y < 0x69)
            ds.Game_StartChoice = 2;
        if (y >= 0x69 && y < 0x85)
            ds.Game_StartChoice = 3;
    }
    Mouse_HideCursor();
}

// 321d:026f
void Title_InitTitleAndMainMenu(void)
{
    // The temporary palette the original kept on its stack.
    static uint8_t tpal[0x300];

    GfxPath_IconAllIndex();
    uint16_t cells = (uint16_t)(ds.GfxPath_IconAllSize + 0x54FF);  // GfxPath's BG_OFS
    ds.GfxPath_CellBackgrounds = ds.GfxPath_InitBlock + cells;
    memset(tpal, 0, 768);
    Gfx_LoadPic("grafika\\charset1", ds.Gfx_Charset + 0x80, 1, 0x20, 0, 0, 0x140, false, tpal,
                0, 2, 0, 0, 2);
    // The characters of the game's own font (Gfx_HeroCharset) head the sheet.
    char hero[256];
    size_t n = SDL_strlen(pstr_get(hero, sizeof hero, &ds.Gfx_HeroCharset));
    SDL_memcpy(ds.Gfx_Charset, hero, n);

    uint8_t *icons = ds.GfxPath_ToolbarBlock.Data + cells;
    ds.GfxPath_CellBackgrounds = icons;
    Gfx_LoadPic("icon\\iconmain", icons, 1, 0x20, 0, 0, 0xF0, false, tpal, 0, 0x21,
                2, 0, 0x21);

    // Six 16x16 cursor shapes from iconmain; colour 2 is transparent.
    static const uint8_t base[6] = {0x8F, 0x9F, 0xAF, 0xBF, 0xCF, 0xDF};
    const uint8_t *sheet = ds.GfxPath_CellBackgrounds;
    for (int k = 0; k < 6; k++) {
        for (uint16_t row = 1; row <= 0x10; row++) {
            for (uint16_t col = 1; col <= 0x10; col++) {
                uint8_t v = sheet[col + row * 0xF0 + base[k]];
                ds.Mouse_CursorImages[k][(row - 1) * 16 + (col - 1)] = v == 2 ? 0 : v;
            }
        }
    }

    ds.Game_Hero = (int16_t)(System_Random(2) + 1);
    ds.Game_HeroSpriteX = ds.Game_Hero == 1 ? 0x45 : 1;
    Title_StartMenu();
    if (ds.Game_StartChoice == 3)
        Gfx_FatalExit();
    if (ds.Game_StartChoice == 1) {
        Gfx_BlankPalette(Gfx_PalettePtr(), 0, 0xFF);
        Gfx_LoadPic("grafika\\choise", gfx_screen(), 1, 200, 0, 0, 0x140, false,
                    Gfx_PalettePtr(), 0, 0xFF, 0, 0, 0xFF);
        Gfx_FadeIn(0, 0xFF, Gfx_PalettePtr());
        // Port deviation (docs/port.md, "Deliberate deviations"): the voice
        // plays from here.
        Music_HoldHeroVoice(false);
        Title_HeroSelect();
    }

    uint8_t *text = game_alloc(0x1540);
    uint8_t *main = game_alloc(0xBCC0);
    Gfx_LoadPic("grafika\\text", text, 1, 0x11, 0, 0, 0x140, false, tpal, 0, 0xF,
                0x24, 0, 0xF);
    Gfx_LoadPic("grafika\\main", main, 1, 0x97, 0, 0, 0x140, false, tpal, 0, 0xB0,
                0x40, 0, 0xBE);
    ds.Command_BackdropShown = 1;
    Gfx_BlankPalette(Gfx_PalettePtr(), 0, 0xFF);
    memcpy(ds.Gfx_Palette, tpal, 0x300);
    Blit_CopyBytes(text, gfx_screen() + 0x2800, 0x1540);
    Blit_CopyBytes(main, gfx_screen() + 0x3D40, 0xBCC0);
    game_free(main);
    game_free(text);
}

// 321d:0866: fixed colours for the menu bar and status line (not written to the DAC).
void Title_SetMainMenuPalette(void)
{
    static const struct { uint8_t index, r, g, b; } fixed[] = {
        {0x01, 0x1E, 0x1E, 0x1E}, {0x34, 0x00, 0x00, 0x00}, {0x35, 0x0F, 0x22, 0x38},
        {0x36, 0x07, 0x1A, 0x2E}, {0xF9, 0x00, 0x00, 0x00}, {0xFA, 0x2D, 0x2A, 0x00},
        {0xFB, 0x20, 0x1D, 0x00}, {0xFC, 0x00, 0x00, 0x00}, {0xFD, 0x39, 0x11, 0x06},
        {0xFE, 0x29, 0x0D, 0x04},
    };
    for (size_t i = 0; i < sizeof fixed / sizeof fixed[0]; i++) {
        ds.Gfx_Palette[fixed[i].index][0] = fixed[i].r;
        ds.Gfx_Palette[fixed[i].index][1] = fixed[i].g;
        ds.Gfx_Palette[fixed[i].index][2] = fixed[i].b;
    }
    for (int i = 0; i < 6; i++)
        memcpy(ds.Gfx_Palette[0xF1 + i], ds.Gfx_Palette[0xF9 + i], 3);
}
