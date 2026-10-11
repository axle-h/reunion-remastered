// Boot: the whole start-up as main.c runs it (unit initialisers, Main), driven
// by the autopilot: the start menu, "new game", the hero choice and the first
// frames of the command centre. Each stage is checked against the picture it
// should show, decoded afresh from GRAFIKA. Needs the original game data.
#include <SDL3/SDL.h>
#include <setjmp.h>
#include <stdio.h>

#include "game/dseg.h"
#include "game/ems.h"
#include "game/gfx.h"
#include "game/main_program.h"
#include "game/mem.h"
#include "game/system.h"
#include "platform/audio.h"
#include "platform/autopilot.h"
#include "platform/config.h"
#include "platform/fs.h"
#include "platform/input.h"
#include "platform/timing.h"
#include "platform/video.h"

static bool ok = true;
#define CHECK(c) do { if (!(c)) { printf("FAILED line %d: %s\n", __LINE__, #c); ok = false; } } while (0)

static jmp_buf out;
static int halt_code;

[[noreturn]] static void on_halt(uint8_t code)
{
    halt_code = code;
    longjmp(out, 1);
}

[[noreturn]] static void on_quit(void)
{
    halt_code = -1;
    longjmp(out, 1);
}

// Boots and runs the game under `script` until it quits (-1) or halts.
static int run(const char *script)
{
    audio_shutdown();
    input_reset();
    autopilot_set_script(script);
    halt_code = -2;
    if (setjmp(out) == 0) {
        mem_init();
        if (!dseg_init())
            return -3;
        system_init();
        Game_InitUnits();
        system_add_exit_proc(far_ptr(0x405F, 0x1A62), Gfx_ExitHandler);
        Game_Main();
    }
    return halt_code;
}

// Percentage of the screen rows y0..y1 that equal the picture `name`
// (GRAFIKA\name.PIC) loaded with the given colour mapping to `dest_row`.
static int match_percent(const char *name, uint16_t rows, uint16_t last, uint16_t offset,
                         uint16_t fade_last, int dest_row)
{
    FarPtr buf = System_GetMem(64000);
    FarPtr pal = System_GetMem(768);
    uint8_t pname[64];
    pname[0] = (uint8_t)SDL_snprintf((char *)pname + 1, sizeof pname - 1, "grafika\\%s", name);
    Gfx_LoadPic(pname, far_host(buf), 1, rows, 0, 0, 320, false, far_host(pal), 0, last, offset, 0,
                fade_last);
    const uint8_t *pic = far_host(buf);
    int same = 0;
    for (int y = 0; y < rows; y++)
        for (int x = 0; x < VID_WIDTH; x++)
            same += vid_screen[dest_row + y][x] == pic[y * 320 + x];
    System_FreeMem(pal, 768);
    System_FreeMem(buf, 64000);
    return same * 100 / (rows * VID_WIDTH);
}

static bool palette_lit(void)
{
    int sum = 0;
    for (int i = 0; i < 256; i++)
        sum += vid_palette[i][0] + vid_palette[i][1] + vid_palette[i][2];
    return sum > 256 * 20;
}

// Mean brightness (0..255) of screen rows y0..y1 as the DAC shows them.
static int shown_level(int y0, int y1)
{
    long sum = 0;
    for (int y = y0; y <= y1; y++)
        for (int x = 0; x < VID_WIDTH; x++) {
            const uint8_t *c = vid_palette[vid_screen[y][x]];
            sum += (c[0] + c[1] + c[2]) * 255 / (3 * 63);
        }
    return (int)(sum / ((y1 - y0 + 1) * VID_WIDTH));
}

// Retraces the game waits for through Gfx (fades, LoadPic, reveals) after
// frame 900: the toolbar's and the picture area's brightness as presented.
static uint64_t shown_base;  // timing_frames() at the start of the run
static int shown_count;
static int shown_bar[256], shown_pic[256];
static bool record_shown(void)
{
    if (timing_frames() - shown_base >= 900 && shown_count < 256) {
        shown_bar[shown_count] = shown_level(0, 31);
        shown_pic[shown_count] = shown_level(49, 199);
        shown_count++;
    }
    return timing_wait_retrace();
}

int main(int argc, char *argv[])
{
    if (argc < 2 || !fs_init(argv[1])) {
        printf("no game data, skipping\n");
        return 77;
    }
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_EVENTS)) {
        printf("SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    system_exit_handler = on_halt;
    timing_set_quit_handler(on_quit);

    // The start menu (grafika\options), faded in.
    CHECK(run("40:quit") == -1);
    int p = match_percent("options", 200, 0xFF, 0, 0xFF, 0);
    printf("start menu: %d%% of options.pic\n", p);
    CHECK(p >= 98);
    CHECK(palette_lit());

    // "New game", then the hero choice (grafika\choise).
    CHECK(run("40:click:160,60;120:quit") == -1);
    CHECK(ds.Game_StartChoice == 1);
    p = match_percent("choise", 200, 0xFF, 0, 0xFF, 0);
    printf("hero choice: %d%% of choise.pic\n", p);
    CHECK(p >= 95);

    // Pick the left hero; after the fades the game runs the command centre
    // (grafika\main below the toolbar and status lines) with the clock going.
    CHECK(run("40:click:160,60;120:click:80,100;1000:quit") == -1);
    CHECK(ds.Game_Hero == 2);
    CHECK(ds.Game_CurrentScreen == 1);
    CHECK(ds.Game_PrevScreen == 1);
    CHECK(ds.Game_Year == 2927 && ds.Game_Month == 8);
    CHECK(ds.Game_Credits >= 120000);
    p = match_percent("main", 0x97, 0xB0, 0x40, 0xBE, 49);
    printf("command centre: %d%% of main.pic\n", p);
    CHECK(p >= 80);
    CHECK(palette_lit());

    // The same with the GUS music (modules): the start-up memory check asks
    // for 28,000 bytes more, which in DOS needed EMS; the port always passes
    // the check (System_MemAvail).
    config.music = CONFIG_MUSIC_GUS;
    CHECK(run("40:click:160,60;120:click:80,100;1000:quit") == -1);
    config_reset();
    CHECK(ds.SoundDrv_GusReady);
    CHECK(Ems_GameDataBlock()->State == 1 && Ems_ToolbarBlock()->State == 1);
    CHECK(ds.Game_CurrentScreen == 1);
    p = match_percent("main", 0x97, 0xB0, 0x40, 0xBE, 49);
    printf("command centre (GUS music): %d%% of main.pic\n", p);
    CHECK(p >= 80);

    // A menu button switches screens: the research screen (command 2).
    CHECK(run("40:click:160,60;120:click:80,100;900:click:72,16;1100:quit") == -1);
    CHECK(ds.Game_CurrentScreen == 3);

    // Command centre -> planet surface (docs/port-bugs.md 6): the toolbar stays
    // lit throughout (LoadPic fades picture colours 0..BEh at +40h, not DAC
    // 0..BEh), and the old picture fades out over several retraces instead of
    // cutting to black, then the new one fades in.
    gfx_wait_retrace = record_shown;
    shown_count = 0;
    shown_base = timing_frames();
    CHECK(run("40:click:160,60;120:click:80,100;900:click:208,16;1000:quit") == -1);
    gfx_wait_retrace = timing_wait_retrace;
    CHECK(ds.Game_CurrentScreen == 20);
    int bar_min = 255, pic_full = shown_count ? shown_pic[0] : 0, first_black = -1, fading = 0;
    for (int i = 0; i < shown_count; i++) {
        bar_min = SDL_min(bar_min, shown_bar[i]);
        if (first_black < 0 && shown_pic[i] == 0)
            first_black = i;
        if (first_black < 0 && shown_pic[i] > 0 && shown_pic[i] < pic_full * 9 / 10)
            fading++;
    }
    printf("surface transition: %d retraces, toolbar min %d, %d fade-out steps\n", shown_count,
           bar_min, fading);
    CHECK(bar_min > 20);
    CHECK(first_black > 0 && fading >= 3);

    // "Exit to DOS" halts with code 0.
    CHECK(run("40:click:160,120;300:quit") == 0);

    audio_shutdown();
    SDL_Quit();
    return ok ? 0 : 1;
}
