// gfx: palette fades and flashes, lines, text drawing from the game's
// charset, LoadPic into the screen (against assets/pic.h) and ICON.ALL
// chunks (against assets/icons.h). Retrace waits go to a counting hook.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "assets/icons.h"
#include "assets/pic.h"
#include "game/dseg.h"
#include "game/gfx.h"
#include "game/system.h"
#include "platform/fs.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

// The DAC entry 1 as seen at each retrace wait (i.e. before that step's write).
static int retraces;
static uint8_t seen[16][3];
static uint8_t seen40[16];  // red of DAC entry 40h, for LoadPic's fade
static bool record_retrace(void)
{
    if (retraces < 16) {
        SDL_memcpy(seen[retraces], vid_palette[1], 3);
        seen40[retraces] = vid_palette[0x40][0];
    }
    retraces++;
    return true;
}

static void reset(void)
{
    mem_init();
    SDL_memset(vid_palette, 0, sizeof vid_palette);
    ds.Gfx_LineStride = 320;
    ds.Gfx_LineStrideCopy = 320;
    ds.Mouse_CursorState = 1;  // hidden: Hide/ShowCursor leave the screen alone
    gfx_wait_retrace = record_retrace;
    retraces = 0;
}

// reset() plus the typed constants and an empty heap, for the file tests.
static void reset_data(void)
{
    reset();
    CHECK(dseg_init());
    system_init();
    ds.Gfx_LineStride = 320;
    ds.Gfx_LineStrideCopy = 320;
    ds.Mouse_CursorState = 1;
    ds.Mouse_X = 0;
    ds.Mouse_Y = 0;
}

static void palette(void)
{
    reset();
    uint8_t *pal = Gfx_PalettePtr();
    for (int i = 0; i < 256; i++)
        for (int c = 0; c < 3; c++)
            ds.Gfx_Palette[i][c] = (uint8_t)((i + c) & 63);
    ds.Gfx_Palette[1][0] = 63;
    ds.Gfx_Palette[1][1] = 10;
    ds.Gfx_Palette[1][2] = 0;

    // Fade in, 4 steps: v * s div 4 for s = 0..4, then the whole palette.
    Gfx_FadeInSteps(1, 1, pal, 4);
    CHECK(retraces == 5);
    static const uint8_t in_r[5] = {0, 0, 15, 31, 47};  // DAC before each step's write
    for (int s = 0; s < 5; s++)
        CHECK(seen[s][0] == in_r[s]);
    CHECK(seen[3][1] == 5);  // 10 * 2 div 4
    CHECK(SDL_memcmp(vid_palette, ds.Gfx_Palette, sizeof vid_palette) == 0);

    // Fade out, 4 steps: v * (4 - s) div 4; no final SetPalette.
    retraces = 0;
    Gfx_FadeOutSteps(1, 1, pal, 4);
    static const uint8_t out_r[5] = {63, 63, 47, 31, 15};
    for (int s = 0; s < 5; s++)
        CHECK(seen[s][0] == out_r[s]);
    CHECK(vid_palette[1][0] == 0 && vid_palette[1][1] == 0);
    CHECK(vid_palette[2][0] == 2);  // outside the range: untouched

    // To and from white.
    Gfx_FadeToWhite(1, 1, pal, 3);
    CHECK(vid_palette[1][0] == 63 && vid_palette[1][1] == 63 && vid_palette[1][2] == 63);
    Gfx_FadeFromWhite(1, 1, pal, 3);
    CHECK(vid_palette[1][0] == 63 && vid_palette[1][1] == 10 && vid_palette[1][2] == 0);

    // Whiten: 63 - (63 - v) * level div 15.
    Gfx_WhitenPalette(1, 1, 5, pal);
    CHECK(vid_palette[1][0] == 63 && vid_palette[1][1] == 63 - 53 * 5 / 15 && vid_palette[1][2] == 42);

    // FadeIn/FadeOut use 5 steps (6 retraces).
    retraces = 0;
    Gfx_FadeOut(0, 255, pal);
    CHECK(retraces == 6 && vid_palette[200][0] == 0 && vid_palette[255][2] == 0);
    retraces = 0;
    Gfx_FadeIn(0, 255, pal);
    CHECK(retraces == 6 && SDL_memcmp(vid_palette, ds.Gfx_Palette, sizeof vid_palette) == 0);

    // GetPalette reads the DAC back.
    static uint8_t buf[768];
    Gfx_GetPalette(buf);
    CHECK(SDL_memcmp(buf, vid_palette, 768) == 0);

    // Blank zeroes the source too; the DAC keeps 6 bits of what it is given.
    retraces = 0;
    Gfx_BlankPalette(pal, 3, 4);
    CHECK(retraces == 1 && ds.Gfx_Palette[3][0] == 0 && vid_palette[4][1] == 0 && vid_palette[5][0] == 5);
    ds.Gfx_Palette[7][0] = 0xFF;
    Gfx_SetPalette(pal);
    CHECK(vid_palette[7][0] == 63);
}

static void flash(void)
{
    reset();
    ds.Gfx_Palette[0][0] = 9;
    ds.Gfx_FlashStep = 31;
    ds.Gfx_FlashCount = 1;
    Gfx_PaletteFlashTick();
    CHECK(vid_palette[0][0] == 60 && ds.Gfx_Palette[0][0] == 0 && ds.Gfx_FlashStep == 30);
    Gfx_PaletteFlashTick();
    CHECK(vid_palette[0][0] == 58);
    ds.Gfx_FlashStep = 1;
    Gfx_PaletteFlashTick();  // |1 - 31| = 30 -> 0, then reload from FlashCount
    CHECK(vid_palette[0][0] == 0 && ds.Gfx_FlashStep == 61 && ds.Gfx_FlashCount == 0);
    ds.Gfx_FlashStep = 61;
    Gfx_PaletteFlashTick();
    CHECK(vid_palette[0][0] == 0 && ds.Gfx_FlashStep == 60);
}

static bool line_is(const int (*pts)[2], int n, uint8_t colour)
{
    int count = 0;
    for (int y = 0; y < 10; y++)
        for (int x = 0; x < 10; x++)
            count += vid_screen[y][x] != 0;
    if (count != n)
        return false;
    for (int i = 0; i < n; i++)
        if (vid_screen[pts[i][1]][pts[i][0]] != colour)
            return false;
    return true;
}

static void lines(void)
{
    static const int shallow[5][2] = {{0, 0}, {1, 1}, {2, 1}, {3, 2}, {4, 2}};
    static const int steep[5][2] = {{0, 0}, {1, 1}, {1, 2}, {2, 3}, {2, 4}};
    static const int rising[5][2] = {{0, 2}, {1, 1}, {2, 1}, {3, 0}, {4, 0}};

    reset();
    Gfx_DrawLine(gfx_screen(), 0, 0, 4, 2, 7);
    CHECK(line_is(shallow, 5, 7));
    reset();
    Gfx_DrawLine(gfx_screen(), 4, 2, 0, 0, 7);  // endpoints swapped: same pixels
    CHECK(line_is(shallow, 5, 7));
    reset();
    Gfx_DrawLine(gfx_screen(), 0, 0, 2, 4, 7);
    CHECK(line_is(steep, 5, 7));
    reset();
    Gfx_DrawLine(gfx_screen(), 0, 2, 4, 0, 7);
    CHECK(line_is(rising, 5, 7));

    static const int hline[4][2] = {{1, 3}, {2, 3}, {3, 3}, {4, 3}};
    static const int vline[3][2] = {{5, 2}, {5, 3}, {5, 4}};
    reset();
    Gfx_DrawLine(gfx_screen(), 4, 3, 1, 3, 9);
    CHECK(line_is(hline, 4, 9));
    reset();
    Gfx_DrawLine(gfx_screen(), 5, 4, 5, 2, 9);
    CHECK(line_is(vline, 3, 9));

    // DrawLineCopy takes the pixels from the source segment.
    reset();
    static uint8_t src[4 * 320];
    for (int i = 0; i < 4 * 320; i++)
        src[i] = (uint8_t)(i % 320 + 1);
    Gfx_DrawLineCopy(gfx_screen(), 0, 0, 4, 2, src);
    CHECK(vid_screen[0][0] == 1 && vid_screen[1][1] == 2 && vid_screen[2][4] == 5 && vid_screen[0][1] == 0);

    // HLine/VLine/outline are 1-based.
    reset();
    Gfx_HLine(2, 4, 1, 3);
    CHECK(vid_screen[0][1] == 3 && vid_screen[0][3] == 3 && vid_screen[0][4] == 0 && vid_screen[0][0] == 0);
    Gfx_VLine(1, 2, 3, 4);
    CHECK(vid_screen[1][0] == 4 && vid_screen[2][0] == 4 && vid_screen[3][0] == 0);
    reset();
    Gfx_DrawRectOutline(1, 1, 4, 3, 5);
    CHECK(vid_screen[0][0] == 5 && vid_screen[2][3] == 5 && vid_screen[1][1] == 0 && vid_screen[1][3] == 5);

    reset();
    Gfx_DrawDitherRect(0, 0, 3, 1, 6);
    CHECK(vid_screen[0][0] == 6 && vid_screen[0][1] == 0 && vid_screen[1][1] == 6 && vid_screen[1][2] == 0);
    CHECK(vid_screen[0][4] == 0 && vid_screen[2][0] == 0);
}

// Index of c in CharsetChars as DrawText6x8 finds it (78 if absent).
static int glyph_index(uint8_t c)
{
    for (int k = 0; k < 78; k++)
        if ((uint8_t)ds.Gfx_CharsetChars.s[k] == c)
            return k;
    return 78;
}

static void text(void)
{
    reset_data();

    // As Title::InitTitleAndMainMenu: charset1 at Charset + 80h, 32 rows.
    ds.Gfx_Charset = System_GetMem(10368);
    uint8_t name[] = "\x10grafika\\charset1";
    static uint8_t tmp_pal[0x300];
    Gfx_LoadPic(name, ds.Gfx_Charset + 0x80, 1, 0x20, 0, 0, 320, false, tmp_pal, 0, 2, 0, 0, 2);

    Pic pic;
    CHECK(pic_load("GRAFIKA\\CHARSET1.PIC", &pic));
    if (!pic.pixels)
        return;
    CHECK(pic.width == 320);

    // "Hi 42" padded to 7 cells, colour 10h, at (10, 20).
    const uint8_t s[] = "\x05Hi 42";
    Gfx_DrawText6x8(s, 10, 20, 7, 0x10);
    const char *want = "Hi 42  ";
    bool same = true;
    for (int cell = 0; cell < 7; cell++) {
        int g = glyph_index((uint8_t)want[cell]);
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 6; c++) {
                uint8_t p = pic.pixels[((g / 20) * 8 + r) * pic.width + (g % 20) * 16 + c];
                same = same && vid_screen[20 + r][10 + cell * 6 + c] == (uint8_t)(p + 0x10);
            }
    }
    CHECK(same);
    CHECK(vid_screen[20][9] == 0 && vid_screen[20][52] == 0 && vid_screen[28][10] == 0);
    // Glyphs differ from the blank one (the text actually drew something).
    CHECK(SDL_memcmp(&vid_screen[23][10], &vid_screen[23][40], 6) != 0);
    pic_free(&pic);
}

static void load_pic(void)
{
    reset_data();

    for (int i = 0; i < 256; i++)
        ds.Gfx_Palette[i][0] = 40;  // the "old" palette that fades out
    size_t size;
    uint8_t *data = fs_load("GRAFIKA\\MAIN.PIC", &size);
    CHECK(data != nullptr);
    if (!data)
        return;
    int h = data[10] | data[11] << 8;
    uint16_t rows = (uint16_t)SDL_min(h, 200);

    vid_palette[0x3F][0] = 33;
    vid_palette[0xFE][0] = 33;
    vid_palette[0xFF][0] = 33;
    uint8_t name[] = "\x0cgrafika\\main";
    Gfx_LoadPicScreen(name, gfx_screen(), rows, Gfx_PalettePtr());

    static uint8_t want[320 * 200];
    static uint8_t want_pal[256][3];
    for (int i = 0; i < 256; i++)
        want_pal[i][0] = 40;
    PicOptions opt = {.rows = rows, .start = 0, .colour_offset = 0x40, .palette = want_pal,
                      .first_colour = 0, .last_colour = 0xB0};
    int w;
    CHECK(pic_decode_into(data, size, want, sizeof want, &opt, &w, nullptr));
    CHECK(SDL_memcmp(vid_screen, want, (size_t)w * rows) == 0);
    CHECK(SDL_memcmp(ds.Gfx_Palette, want_pal, sizeof want_pal) == 0);
    // The fade took the old colours to black in the DAC: picture colours
    // 0..BEh at +40h, so the toolbar's 0..3Fh stay (docs/port-bugs.md 6).
    CHECK(vid_palette[0x3F][0] == 33 && vid_palette[0x40][0] == 0 && vid_palette[0xFE][0] == 0
          && vid_palette[0xFF][0] == 33);
    // Shown over 5 retraces (the port's stand-in for the decoding time):
    // 32, 24, 16, 8 and 0 of the old level 40.
    CHECK(retraces == 5);
    CHECK(seen40[0] == 32 && seen40[1] == 24 && seen40[3] == 8 && seen40[4] == 0);
    // The file buffer went back to the heap.
    CHECK(system_heap_free() == system_heap_max_block());

    // rows is clipped to the picture height (405f:0ccb): nothing past it.
    uint8_t *buf = System_GetMem((uint16_t)(320 * (h + 4)));
    SDL_memset(buf, 0xEE, (size_t)320 * (h + 4));
    Gfx_LoadPic(name, buf, 1, (uint16_t)(h + 4), 0, 0, 320, false, Gfx_PalettePtr(), 0, 0xB0, 0x40,
                0, 0xBE);
    CHECK(SDL_memcmp(buf, want, (size_t)w * h) == 0);
    const uint8_t *tail = buf + w * h;
    CHECK(tail[0] == 0xEE && tail[320 * 4 - 1] == 0xEE);
    System_FreeMem(buf, (uint16_t)(320 * (h + 4)));
    SDL_free(data);
}

static void icon_all(void)
{
    reset_data();

    uint16_t size;
    Gfx_IconAllOpenSize(&size);
    Gfx_IconAllClose();
    Gfx_IconAllOpen();
    uint8_t *buf = System_GetMem(size);
    uint16_t lens[ICON_COUNT];
    for (int i = 0; i < ICON_COUNT; i++)
        Gfx_IconAllReadChunk(&buf, &lens[i]);
    Gfx_IconAllClose();
    CHECK(ds.Gfx_ReadPos == size - 2 * ICON_COUNT);

    // Unpack chunk n (PCX-style RLE, + 2 per pixel as the game's blitter
    // does) and compare with icons.h.
    Icons icons;
    CHECK(icons_load("ICON\\ICON.ALL", &icons));
    if (!icons.pixels)
        return;
    const uint8_t *p = buf;
    bool same = true;
    for (int n = 0; n < ICON_COUNT; n++) {
        uint8_t pix[ICON_WIDTH * ICON_HEIGHT + 64];
        int out = 0;
        for (int i = 0; i < lens[n] && out < ICON_WIDTH * ICON_HEIGHT;) {
            uint8_t b = p[i++];
            int count = 1;
            if (b >= 0xC0) {
                count = b & 0x3F;
                b = p[i++];
            }
            while (count-- > 0 && out < (int)sizeof pix)
                pix[out++] = (uint8_t)(b + 2);
        }
        same = same && SDL_memcmp(pix, icons_pixels(&icons, n), ICON_WIDTH * ICON_HEIGHT) == 0;
        p += lens[n];
    }
    CHECK(same);
    icons_free(&icons);
}

int main(int argc, char *argv[])
{
    palette();
    flash();
    lines();
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping the file tests\n");
        return ok ? 77 : 1;
    }
    text();
    load_pic();
    icon_all();
    return ok ? 0 : 1;
}
