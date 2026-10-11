// textfile: InputLine with queued keys, DecodeLine, CountLines, ReadTextLine.
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#include "assets/text.h"
#include "game/dseg.h"
#include "game/ems.h"
#include "game/gfx.h"
#include "game/system.h"
#include "game/textfile.h"
#include "platform/fs.h"

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
    mem_init();
    CHECK(dseg_init());
    system_init();
    Ems_Init();
    ds.System_SegA000 = 0xA000;
    ds.Gfx_ScreenSeg = 0xA000;
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
    int32_t avail = system_heap_free();

    set(s, "ab");
    run(s, "cd\r", 8, false);
    CHECK(eq(s, "abcd"));
    CHECK(system_heap_free() == avail);  // the backup copy is freed

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
    CHECK(system_heap_free() == avail);

    // The text is drawn in the field (glyph pixels + colour FCh).
    CHECK(retraces > 0);
}

static void decoding(void)
{
    uint8_t s[256];
    const char *plain = "Hello, world! 123";
    // Encode: the inverse of s[i] := (s[i] + 5780h - i*4Fh) mod E0h - 20h.
    set(s, plain);
    for (int i = 1; i <= (int)SDL_strlen(plain); i++) {
        int e = ((plain[i - 1] + 0x20) - (0x5780 % 0xE0) + (i * 0x4F) % 0xE0) % 0xE0;
        s[i] = (uint8_t)((e + 0xE0) % 0xE0);
    }
    TextFile_DecodeLine(s);
    CHECK(eq(s, plain));

    set(s, "");
    TextFile_DecodeLine(s);
    CHECK(s[0] == 0);

    set(s, "a");
    CHECK(TextFile_CountLines(s) == 1);
    set(s, "a|bb||c");
    CHECK(TextFile_CountLines(s) == 4);
    set(s, "");
    CHECK(TextFile_CountLines(s) == 1);
}

static void reading(void)
{
    uint8_t name[32], s[256];
    set(name, "message.txt");
    TextFile_ReadTextLine(name, 1, s);
    char ref[TEXT_LINE_MAX];
    CHECK(text_read_line("text\\message.txt", 1, ref));
    CHECK(s[0] == SDL_strlen(ref) && s[0] > 0 && SDL_memcmp(s + 1, ref, s[0]) == 0);
}

static void reply(void)
{
    // "07x" code 7, then lines split at '|'.
    uint8_t s[256];
    int16_t code, lines;
    set(s, "07 first|second||fourth");
    TextFile_DrawReply(s, 0, 100, 20, false, false, &code, &lines);
    CHECK(code == 7);
    CHECK(lines == 4);
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
