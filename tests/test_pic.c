// pic: decoding against values produced by a reference decoder.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "assets/pic.h"
#include "platform/fs.h"

static int failures;

static void check(bool cond, const char *what)
{
    if (!cond) {
        printf("FAILED: %s\n", what);
        failures++;
    }
}

static void check_pic(const char *path, int w, int h, uint32_t pixels_crc, uint32_t pal_crc)
{
    Pic pic;
    if (!pic_load(path, &pic)) {
        printf("FAILED: load %s\n", path);
        failures++;
        return;
    }
    check(pic.width == w && pic.height == h, path);
    check(SDL_crc32(0, pic.pixels, (size_t)pic.width * pic.height) == pixels_crc, "pixels crc");
    check(SDL_crc32(0, pic.palette, sizeof pic.palette) == pal_crc, "palette crc");
    pic_free(&pic);
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }

    check_pic("GRAFIKA\\MAIN.PIC", 320, 151, 0x536a457b, 0x1e7a1253);
    check_pic("INFO\\INFO1.PIC", 192, 127, 0x5baf342a, 0x8c661a23);
    // Header says 191 wide, rows hold 192.
    check_pic("INFO\\INFO26.PIC", 192, 127, 0x2c4e7111, 0xe74ddef4);

    // Game-style load: 100 rows at colour offset 40h, colours 0..B0h.
    static uint8_t dest[320 * 151];
    static uint8_t pal[256][3];
    PicOptions opt = {.rows = 100, .colour_offset = 0x40, .palette = pal,
                      .first_colour = 0, .last_colour = 0xB0};
    int w = 0, h = 0;
    check(pic_load_into("GRAFIKA\\MAIN.PIC", dest, sizeof dest, &opt, &w, &h), "load_into");
    check(w == 320 && h == 151, "load_into size");
    check(SDL_crc32(0, dest, 32000) == 0xfc2870fe, "load_into pixels");
    check(SDL_crc32(0, pal, sizeof pal) == 0x2736ba75, "load_into palette");

    // Bare 768-byte palette.
    uint8_t bare[256][3];
    check(pic_palette_load("CREDITS\\PAL3.PIC", bare), "bare palette");
    check(SDL_crc32(0, bare, sizeof bare) == 0xdd2862de, "bare palette crc");
    Pic none;
    check(!pic_load("CREDITS\\PAL3.PIC", &none), "bare palette is not an image");
    check(!pic_load("GRAFIKA\\NOSUCH.PIC", &none), "missing file");

    fs_shutdown();
    printf("%s\n", failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
