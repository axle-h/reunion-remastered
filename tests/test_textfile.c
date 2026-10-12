// textfile: InputLine with queued keys, DecodeLine, CountLines, ReadTextLine.
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#include "assets/text.h"
#include "game/dseg.h"
#include "game/main_program.h"
#include "game/gfx.h"
#include "game/system.h"
#include "game/textfile.h"
#include "platform/fs.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static const char *queue;
static size_t qpos;
static int retraces;

static bool wait_retrace(void)
{
    retraces++;
    return true;
}

static bool key_pressed(void)
{
    if (queue[qpos] == '\0') {
        printf("FAILED: key queue exhausted before the editor finished\n");
        exit(1);
    }
    return true;
}

// Keys are given as chars; '\x01' stands for an extended key (0 + scan code).
static char read_key(void)
{
    char c = queue[qpos++];
    if (c == 1)
        return 0;
    return c;
}

static void reset(void)
{
    dseg_clear();
    vid_clear();
    dseg_init();
    system_init();
    Game_InitGameData();
    ds.Gfx_LineStride = 320;
    ds.Gfx_LineStrideCopy = 320;
    ds.Mouse_CursorState = 1;
    gfx_wait_retrace = wait_retrace;
    TextFile_KeyPressed = key_pressed;
    TextFile_ReadKey = read_key;
}

static void set(uint8_t *s, const char *c)
{
    s[0] = (uint8_t)SDL_strlen(c);
    SDL_memcpy(s + 1, c, s[0]);
}

static bool eq(const uint8_t *s, const char *c)
{
    return s[0] == SDL_strlen(c) && SDL_memcmp(s + 1, c, s[0]) == 0;
}

static void run(uint8_t *s, const char *keys, int width, bool cancel)
{
    queue = keys;
    qpos = 0;
    TextFile_InputLine(s, 10, 20, (int16_t)width, cancel);
}

static void editing(void)
{
    uint8_t s[256];
    uint32_t used = system_heap_used();

    set(s, "ab");
    run(s, "cd\r", 8, false);
    CHECK(eq(s, "abcd"));
    CHECK(system_heap_used() == used);  // the backup copy is freed

    set(s, "");
    run(s, "x\b\by\r", 8, false);  // backspace on empty is harmless
    CHECK(eq(s, "y"));

    // Enter on an empty string does not finish; the next keys go on.
    set(s, "");
    run(s, "\rq\r", 8, false);
    CHECK(eq(s, "q"));

    // Only space, letters and digits are accepted.
    set(s, "");
    run(s, "a-B !9\x01Kz\r", 8, false);
    CHECK(eq(s, "aB 9z"));

    // At the width the last character is overwritten.
    set(s, "");
    run(s, "abcdZ\r", 4, false);
    CHECK(eq(s, "abcZ"));

    // Esc is ignored without canCancel; with it the original comes back.
    set(s, "keep");
    run(s, "\x1b" "ok\r", 8, false);
    CHECK(eq(s, "keepok"));
    set(s, "keep");
    run(s, "xyz\x1b", 8, true);
    CHECK(eq(s, "keep"));
    CHECK(system_heap_used() == used);

    // The text is drawn in the field (glyph pixels + colour FCh).
    CHECK(retraces > 0);
}

static void decoding(void)
{
    char s[256];
    const char *plain = "Hello, world! 123";
    // Encode: the inverse of s[i] := (s[i] + 5780h - i*4Fh) mod E0h - 20h.
    size_t n = SDL_strlen(plain);
    for (size_t i = 1; i <= n; i++) {
        int e = ((plain[i - 1] + 0x20) - (0x5780 % 0xE0) + ((int)i * 0x4F) % 0xE0) % 0xE0;
        s[i - 1] = (char)((e + 0xE0) % 0xE0);
    }
    TextFile_DecodeLine(s, n);
    CHECK(SDL_memcmp(s, plain, n) == 0);

    CHECK(TextFile_CountLines("a") == 1);
    CHECK(TextFile_CountLines("a|bb||c") == 4);
    CHECK(TextFile_CountLines("") == 1);

    CHECK(TextFile_LineCode("07 first") == 7 && TextFile_LineCode("12") == 12);
    CHECK(TextFile_LineCode("7") == 0 && TextFile_LineCode("") == 0);
    CHECK(TextFile_LineCode("x1 y") == 0 && TextFile_LineCode(" 5x") == 5);
}

static void reading(void)
{
    char s[256] = "", small[8] = "";
    TextFile_ReadTextLine("message.txt", 1, s, sizeof s);
    char ref[TEXT_LINE_MAX];
    CHECK(text_read_line("text\\message.txt", 1, ref));
    CHECK(SDL_strcmp(s, ref) == 0 && s[0] != '\0');
    // A short destination gets the line's start.
    TextFile_ReadTextLine("message.txt", 1, small, sizeof small);
    CHECK(SDL_strncmp(small, ref, sizeof small - 1) == 0 && SDL_strlen(small) == sizeof small - 1);
}

static void reply(void)
{
    // "07x" code 7, then lines split at '|'.
    int16_t code, lines;
    TextFile_DrawReply("07 first|second||fourth", 0, 100, 20, false, false, &code, &lines);
    CHECK(code == 7);
    CHECK(lines == 4);
    TextFile_DrawReply("5", 0, 100, 20, false, false, &code, &lines);
    CHECK(code == 0 && lines == 0);
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    reset();
    editing();
    decoding();
    reading();
    reply();
    return ok ? 0 : 1;
}
