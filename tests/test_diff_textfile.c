// Differential test of unit TextFile (398f): the line editor (begin, tick,
// end and the blocking InputLine, driven by random keys), the text files and
// the reply/line helpers. See docs/difftest.md. Needs the game data.
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/textfile.h"

#define S(i) ((uint8_t *)far_host((FarPtr)a[i]))
#define I(i) ((int16_t)a[i])

DIFF_PROC(TextFile_InputLine, TextFile_InputLine(S(0), I(1), I(2), I(3), (bool)a[4]));
DIFF_PROC(TextFile_BeginInput, TextFile_BeginInput(S(0), I(1), I(2), I(3)));
DIFF_PROC(TextFile_EndInput, TextFile_EndInput());
DIFF_PROC(TextFile_InputTick, TextFile_InputTick());
DIFF_PROC(TextFile_ReadTextLine, TextFile_ReadTextLine(S(0), I(1), S(2)));
DIFF_PROC(TextFile_ShowBuildingInfo, TextFile_ShowBuildingInfo(I(0)));
DIFF_PROC(TextFile_DrawReply, TextFile_DrawReply(S(0), I(1), I(2), I(3), (bool)a[4], (bool)a[5],
                                                 (int16_t *)S(6), (int16_t *)S(7)));
DIFF_FUNC(TextFile_CountLines, TextFile_CountLines(S(0)));
DIFF_PROC(TextFile_DecodeLine, TextFile_DecodeLine(S(0)));

// Scratch pointers, normalised (offset < 16) as GetMem returns them: the
// editor keeps the caller's pointer (DS:7C50), the port rebuilds it so.
static FarPtr scratch(uint16_t ofs)
{
    uint32_t lin = far_linear(DIFF_SCRATCH) + ofs;
    return far_ptr((uint16_t)(lin >> 4), (uint16_t)(lin & 15));
}

static FarPtr put_str(uint16_t ofs, const char *s)
{
    uint8_t *p = far_host(scratch(ofs));
    p[0] = (uint8_t)strlen(s);
    memcpy(p + 1, s, p[0]);
    return scratch(ofs);
}

// Random bytes in a string buffer, then a random string of up to maxlen.
static FarPtr random_str(DiffRng *r, uint16_t ofs, int maxlen, const char *alphabet)
{
    uint8_t *p = far_host(scratch(ofs));
    for (int i = 0; i < 256; i++)
        p[i] = (uint8_t)diff_rand(r, 256);
    int n = diff_pick(r, 0, 1, maxlen, diff_range(r, 0, maxlen), diff_range(r, 0, maxlen));
    p[0] = (uint8_t)n;
    size_t na = strlen(alphabet);
    for (int i = 1; i <= n; i++)
        p[i] = diff_chance(r, 90) ? (uint8_t)alphabet[diff_rand(r, (uint32_t)na)] : (uint8_t)diff_rand(r, 256);
    return scratch(ofs);
}

static const char LETTERS[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";

// --- The line editor -----------------------------------------------------------

static int random_key(DiffRng *r)
{
    return diff_pick(r, 'a', 'Z', '5', ' ', 8, 8, 13, 27, '-', '.', 0, 0, '`', '{', '@', '[', '/', ':',
                     LETTERS[diff_rand(r, sizeof LETTERS - 1)]);
}

// Up to n random keys (an extended key is 0 and a scan code).
static int random_keys(DiffRng *r, int *keys, int n)
{
    int k = 0;
    while (k < n) {
        int key = random_key(r);
        if (key == 0 && k + 1 == n)
            key = 'x';
        keys[k++] = key;
        if (key == 0)
            keys[k++] = diff_pick(r, 0x48, 0x50, 0x3B);
    }
    return k;
}

// Where the edited string is: a heap-like block (scratch) or a global.
static FarPtr edit_string(DiffRng *r)
{
    if (diff_chance(r, 25)) {
        FarPtr p = far_ptr(MEM_DS_SEG, (uint16_t)(offsetof(DSeg, Names_Systems)
                                                  + sizeof(PStr8) * (uint16_t)diff_range(r, 0, 7)));
        uint8_t *s = far_host(p);
        s[0] = (uint8_t)SDL_min(s[0], 8);
        return p;
    }
    return random_str(r, 0x100, diff_pick(r, 0, 3, 9, 19, 40), LETTERS);
}

static void edit_args(DiffRng *r, int32_t *a)
{
    a[0] = (int32_t)edit_string(r);
    a[3] = diff_pick(r, 9, 19, 12, diff_range(r, 1, 40));
    a[1] = diff_range(r, 0, 320 - a[3] * 6);
    a[2] = diff_range(r, 20, 190);
    if (diff_chance(r, 30)) {  // the cursor over the field
        ds.Mouse_X = (int16_t)SDL_min(a[1] + diff_range(r, 0, 30), 318);
        ds.Mouse_Y = (int16_t)SDL_min(a[2] + diff_range(r, -5, 8), 198);
    }
}

static void setup_begin(DiffRng *r, int32_t *a)
{
    edit_args(r, a);
    if (diff_chance(r, 30)) {  // still editing another string: ended first
        int32_t b[4];
        edit_args(r, b);
        TextFile_BeginInput(far_host((FarPtr)b[0]), (int16_t)b[1], (int16_t)b[2], (int16_t)b[3]);
    }
}

// An edit in progress (begun by the port, which test BeginInput checks),
// at some point of the cursor's blink.
static void begin_edit(DiffRng *r)
{
    int32_t b[4];
    edit_args(r, b);
    TextFile_BeginInput(far_host((FarPtr)b[0]), (int16_t)b[1], (int16_t)b[2], (int16_t)b[3]);
    *(uint16_t *)ds_at(0x7C54) = (uint16_t)diff_pick(r, 0, 9, 19, 20, diff_range(r, 0, 20));
    *(uint8_t *)ds_at(0x7C56) = (uint8_t)diff_range(r, 0, 1);  // Esc allowed
    *(uint8_t *)ds_at(0x7C4B) = (uint8_t)diff_range(r, 0, 1);  // Esc restores
}

static void setup_end(DiffRng *r, int32_t *a)
{
    (void)a;
    if (diff_chance(r, 80))
        begin_edit(r);
}

static void setup_tick(DiffRng *r, int32_t *a)
{
    (void)a;
    begin_edit(r);
    int keys[4];
    int n = diff_chance(r, 20) ? 0 : random_keys(r, keys, diff_pick(r, 1, 2, 2));
    diff_set_keys(keys, n);
}

static void setup_input_line(DiffRng *r, int32_t *a)
{
    edit_args(r, a);
    a[4] = diff_range(r, 0, 1);
    int keys[64];
    int n = random_keys(r, keys, diff_range(r, 0, 40));
    keys[n++] = 'q';  // ends on a non-empty string
    keys[n++] = 13;
    diff_set_keys(keys, n);
}

// --- Files ------------------------------------------------------------------------

static void setup_read_line(DiffRng *r, int32_t *a)
{
    static const struct {
        const char *name;
        int lines;
    } files[] = {{"message.txt", 48}, {"kitalal.txt", 35}, {"pirate.txt", 20}, {"valami.txt", 14},
                 {"sz_felsz.txt", 155}, {"valasz1.loc", 10}, {"nosuch.txt", 1}};
    int f = diff_rand(r, sizeof files / sizeof files[0]);
    a[0] = (int32_t)put_str(0, files[f].name);
    a[1] = diff_pick(r, 1, files[f].lines, files[f].lines + 1, 0, -1, diff_range(r, 1, files[f].lines),
                     diff_range(r, 1, files[f].lines));
    a[2] = (int32_t)random_str(r, 0x100, 80, LETTERS);  // its old contents may show (port-bugs)
}

static void setup_building_info(DiffRng *r, int32_t *a)
{
    a[0] = diff_pick(r, 0, 6, 144, 149, diff_range(r, 0, 25) * 6, diff_range(r, 0, 150));
}

// --- Replies and lines ----------------------------------------------------------------

static void setup_reply(DiffRng *r, int32_t *a)
{
    FarPtr s = random_str(r, 0, 200, "abc defg|||0123456789ABC .,!?");
    uint8_t *p = far_host(s);
    if (p[0] >= 2 && diff_chance(r, 70)) {  // a two-digit code first
        p[1] = (uint8_t)('0' + diff_rand(r, 10));
        p[2] = (uint8_t)('0' + diff_rand(r, 10));
    }
    a[0] = (int32_t)s;
    a[3] = diff_pick(r, 50, 40, diff_range(r, 1, 53));  // the width
    a[1] = diff_range(r, 0, 320 - a[3] * 6);
    a[2] = diff_range(r, 0, 120);
    a[4] = diff_range(r, 0, 1);
    a[5] = diff_chance(r, 25);  // typed (slow)
    a[6] = (int32_t)scratch(0x300);
    a[7] = (int32_t)scratch(0x302);
}

static void setup_count(DiffRng *r, int32_t *a)
{
    a[0] = (int32_t)random_str(r, 0, 255, "ab |||");
}

static void setup_decode(DiffRng *r, int32_t *a)
{
    a[0] = (int32_t)random_str(r, 0, 255, "abcdefghijklmnopqrstuvwxyz");
}

static const char *const scripts[] = {"1500:quit", "900:click:264,16;1300:quit"};
enum { STATES = sizeof scripts / sizeof scripts[0] };

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv)) {
        printf("no game data, skipping\n");
        return 77;
    }
    DiffState *bases[STATES];
    for (int i = 0; i < STATES; i++) {
        if (!diff_boot(scripts[i]) || !diff_open_output()) {  // "Cannot Open" exits write to Output
            printf("FAILED to boot %s\n", scripts[i]);
            return 1;
        }
        bases[i] = diff_save();
    }
    // A missing file: the original's CheckIO leaves the code in InOutRes on
    // its way to RunError, the port's RunError does not; nothing runs after.
    static const DiffRange inoutres[] = {DIFF_DS(offsetof(DSeg, System_InOutRes), 2), {0, 0}};
    diff_TextFile_ReadTextLine.ignore = inoutres;
    const struct {
        DiffFn *fn;
        DiffSetup setup;
        int cases;
    } fns[] = {
        {&diff_TextFile_BeginInput, setup_begin, 30},
        {&diff_TextFile_EndInput, setup_end, 20},
        {&diff_TextFile_InputTick, setup_tick, 80},
        {&diff_TextFile_InputLine, setup_input_line, 40},
        {&diff_TextFile_ReadTextLine, setup_read_line, 50},
        {&diff_TextFile_ShowBuildingInfo, setup_building_info, 20},
        {&diff_TextFile_DrawReply, setup_reply, 40},
        {&diff_TextFile_CountLines, setup_count, 20},
        {&diff_TextFile_DecodeLine, setup_decode, 20},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, bases, STATES, fns[i].setup, diff_cases(fns[i].cases));
    for (int i = 0; i < STATES; i++)
        diff_free(bases[i]);
    return diff_finish();
}
