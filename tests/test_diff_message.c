// Differential test of unit Message (34b0): the message log, the message
// boxes (dismissed by the queued Enter, or edited with random keys) and the
// satellite animation, from the command centre and the message log screen.
// See docs/difftest.md. Needs the game data.
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "diff/diff.h"
#include "game/dseg.h"
#include "game/message.h"

#define S(i) ((uint8_t *)far_host((FarPtr)a[i]))
#define I(i) ((int16_t)a[i])

DIFF_PROC(Message_AddMessageLogEntry, Message_AddMessageLogEntry(I(0), S(1), S(2)));
DIFF_PROC(Message_ShowMessageLine, Message_ShowMessageLine(I(0)));
DIFF_PROC(Message_ShowMessage, Message_ShowMessage(S(0)));
DIFF_PROC(Message_ShowMessageNoLog, Message_ShowMessageNoLog(S(0)));
DIFF_PROC(Message_ShowKitalalLine, Message_ShowKitalalLine(I(0)));
DIFF_PROC(Message_MessageBox, Message_MessageBox(S(0), S(1), (bool)a[2], S(3), (bool)a[4], (uint8_t)a[5]));
DIFF_PROC(Message_PlaySatelliteAnim, Message_PlaySatelliteAnim(I(0)));
DIFF_PROC(Message_PromptPlanetName, Message_PromptPlanetName(I(0), I(1)));
DIFF_PROC(Message_DrawMessageLog, Message_DrawMessageLog());

static FarPtr scratch(uint16_t ofs)
{
    uint32_t lin = far_linear(DIFF_SCRATCH) + ofs;
    return far_ptr((uint16_t)(lin >> 4), (uint16_t)(lin & 15));
}

static FarPtr put_str(uint16_t ofs, const char *s)
{
    uint8_t *p = far_host(scratch(ofs));
    p[0] = (uint8_t)SDL_min(strlen(s), (size_t)255);
    memcpy(p + 1, s, p[0]);
    return scratch(ofs);
}

static const char WORDS[] = "abcdefghij klmnop QRSTUV 0123 .,!?- ";

// Random text with `bars` line breaks at most (and an '&' edit mark, maybe).
static FarPtr random_text(DiffRng *r, uint16_t ofs, int maxlen, int bars, bool amp)
{
    uint8_t *p = far_host(scratch(ofs));
    int n = diff_pick(r, 0, 1, maxlen, diff_range(r, 0, maxlen), diff_range(r, 0, maxlen));
    p[0] = (uint8_t)n;
    for (int i = 1; i <= n; i++)
        p[i] = (uint8_t)WORDS[diff_rand(r, sizeof WORDS - 1)];
    for (int b = 0; b < bars && n > 0; b++)
        if (diff_chance(r, 70))
            p[1 + diff_rand(r, (uint32_t)n)] = '|';
    if (amp && n > 0)
        p[1 + diff_rand(r, (uint32_t)n)] = '&';
    return scratch(ofs);
}

// The log: some entries already, of kinds 0, 1, 99.
static void random_log(DiffRng *r)
{
    int n = diff_pick(r, 0, 1, 14, 15, diff_range(r, 0, 15));
    ds.Message_LogCount = (int16_t)n;
    uint8_t *log = far_host(ds.Message_Log);
    for (int i = 0; i < 15; i++) {
        uint8_t *e = log + i * 0x35;
        *(int16_t *)e = (int16_t)diff_pick(r, 0, 1, 99, 2);
        int len = diff_range(r, 0, 50);
        e[2] = (uint8_t)len;
        for (int j = 0; j < len; j++)
            e[3 + j] = (uint8_t)WORDS[diff_rand(r, sizeof WORDS - 1)];
    }
}

// The pointer over the box, settled there (the port polls the mouse while
// it waits, which must find nothing new).
static void cursor(DiffRng *r)
{
    if (diff_chance(r, 30)) {
        ds.Mouse_X = (int16_t)diff_range(r, 100, 220);
        ds.Mouse_Y = (int16_t)diff_range(r, 90, 160);
        diff_quiesce();
    }
}

static void setup_add(DiffRng *r, int32_t *a)
{
    random_log(r);
    a[0] = diff_pick(r, 0, 1, 99, 3);
    a[1] = (int32_t)random_text(r, 0, 70, 0, false);
    a[2] = (int32_t)(diff_chance(r, 70) ? put_str(0x100, "") : put_str(0x100, "message"));
    ds.Gfx_FlashStep = (int16_t)diff_pick(r, 0, 10);
}

static void setup_line(DiffRng *r, int32_t *a)
{
    random_log(r);
    cursor(r);
    a[0] = diff_pick(r, 1, 48, diff_range(r, 1, 48));
}

static void setup_kitalal(DiffRng *r, int32_t *a)
{
    random_log(r);
    cursor(r);
    a[0] = diff_pick(r, 1, 35, diff_range(r, 1, 35));
}

static void setup_show(DiffRng *r, int32_t *a)
{
    random_log(r);
    cursor(r);
    a[0] = (int32_t)random_text(r, 0, 200, 5, false);
}

// No key: the box waits for one; a few random keys before the Enter.
static void some_keys(DiffRng *r)
{
    static const int pool[] = {'a', 'x', 'Q', '7', ' ', 8, 27, 13, 0, 0x48};
    int keys[24];
    int n = diff_range(r, 0, 12);
    for (int i = 0; i < n; i++)
        keys[i] = pool[diff_rand(r, 8)];  // no extended keys: the editor would wait
    keys[n++] = 'k';
    keys[n++] = 13;
    diff_set_keys(keys, n);
}

static void setup_box(DiffRng *r, int32_t *a)
{
    random_log(r);
    cursor(r);
    a[4] = diff_chance(r, 40);  // edit
    a[0] = (int32_t)random_text(r, 0, 200, 5, a[4]);
    if (a[4] && S(0)[0] == 0)  // without '&' the original's edit position is uninitialised
        a[0] = (int32_t)put_str(0, "Name: &xxxx");
    a[1] = (int32_t)(diff_chance(r, 70) ? put_str(0x100, "") : put_str(0x100, "hiba"));
    a[2] = diff_range(r, 0, 1);
    a[3] = (int32_t)random_text(r, 0x200, 12, 0, false);
    a[5] = diff_pick(r, 9, 1, 20, diff_range(r, 1, 30));
    if (a[4])
        some_keys(r);
}

static void setup_prompt(DiffRng *r, int32_t *a)
{
    a[0] = diff_range(r, 1, 8);
    a[1] = diff_range(r, 1, SDL_max(1, ds.Galaxy_PlanetCount[a[0] - 1]));
    some_keys(r);
}

static void setup_sat(DiffRng *r, int32_t *a)
{
    a[0] = diff_range(r, 1, 4);
}

static void setup_draw_log(DiffRng *r, int32_t *a)
{
    (void)a;
    random_log(r);
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
    const struct {
        DiffFn *fn;
        DiffSetup setup;
        int cases;
    } fns[] = {
        {&diff_Message_AddMessageLogEntry, setup_add, 40},
        {&diff_Message_ShowMessageLine, setup_line, 12},
        {&diff_Message_ShowMessage, setup_show, 20},
        {&diff_Message_ShowMessageNoLog, setup_show, 12},
        {&diff_Message_ShowKitalalLine, setup_kitalal, 12},
        {&diff_Message_MessageBox, setup_box, 60},
        {&diff_Message_PromptPlanetName, setup_prompt, 16},
        {&diff_Message_PlaySatelliteAnim, setup_sat, 4},
        {&diff_Message_DrawMessageLog, setup_draw_log, 12},
    };
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; i++)
        diff_run(fns[i].fn, bases, STATES, fns[i].setup, diff_cases(fns[i].cases));
    for (int i = 0; i < STATES; i++)
        diff_free(bases[i]);
    return diff_finish();
}
