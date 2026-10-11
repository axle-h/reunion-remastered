// The units shared by the cut-scene programs, and the runner that stands in
// for start.exe launching one of them. Per-program scripts are in credits.c,
// intro.c and victory.c; docs/cutscenes.md describes the programs.
//
// Port decisions (all three programs could run with a GUS or a Sound Blaster
// player, which differ in timing; the port follows the GUS configuration):
// - Music: libxmp through platform/audio. The cue position is the GUS
//   player's (1-based order and row, no Sound Blaster row correction). Without
//   an audio device a silent libxmp sequencer, stepped by the retrace clock,
//   provides the same timeline. SETUP.SPD device 4/5 (AdLib/none) plays the
//   music silently, as the original did.
// - The 1 ms timer (GUS ISR) is derived from the retrace count, so pacing is
//   identical in real time and in unpaced autopilot runs.
// - Halt (the exit routine, ESC, window close) unwinds with longjmp to
//   cutscene_play, like main.c does for the game.
#include "cutscene/cutscene.h"

#include <SDL3/SDL.h>
#include <setjmp.h>
#include <xmp.h>

#include "assets/ani.h"
#include "assets/pic.h"
#include "cutscene/cs.h"
#include "platform/audio.h"
#include "platform/fs.h"
#include "platform/input.h"
#include "platform/timing.h"
#include "platform/video.h"

CsData cs;
const CsProgram *cs_prog;

static uint8_t buffers[CS_BUFFERS][CS_SCREEN_SIZE];
static Ani ani;
static jmp_buf exit_jump;
static uint64_t timer_base;

enum : int {
    JUMP_HALT = 1,
    JUMP_QUIT = 2,
    ESC_KEY = 27,
};

// One retrace at 70.086 Hz is 800 * 449 / 25.175 MHz = 14368/1007 ms.
enum : int64_t {
    RETRACE_MS_NUM = 14368,
    RETRACE_MS_DEN = 1007,
};

// --- System -------------------------------------------------------------------

uint8_t *cs_screen(void)
{
    return &vid_screen[0][0];
}

uint8_t *cs_buffer(int index)
{
    SDL_assert(index >= 0 && index < CS_BUFFERS);
    return buffers[index];
}

// System.Random: RandSeed := RandSeed * 08088405h + 1, result = high part of
// RandSeed * range.
uint16_t cs_random(uint16_t range)
{
    cs.rand_seed = cs.rand_seed * 0x08088405u + 1;
    return (uint16_t)(((uint64_t)cs.rand_seed * range) >> 32);
}

int cs_frame_count(int anim)
{
    return anim > 0 && anim < cs_prog->anims ? cs_prog->frame_counts[anim] : 0;
}

[[noreturn]] void cs_exit(void)
{
    cs_fade_out(35, cs.pal, 0, 255);
    // Text mode: cutscene_play leaves the port in a black mode 13h instead.
    cs_music_stop();
    while (input_read_key() >= 0) {
    }
    longjmp(exit_jump, JUMP_HALT);
}

[[noreturn]] static void on_quit(void)
{
    longjmp(exit_jump, JUMP_QUIT);
}

// --- Graphics unit ----------------------------------------------------------------

// 1212:08e5 (INTRO): waits for the start of the next vertical retrace.
void cs_wait_retrace(void)
{
    timing_wait_retrace();
}

// 1212:08fe (INTRO): n retraces (a longint count; 0 waits for none).
void cs_wait_frames(int n)
{
    for (int i = 0; i < n; i++)
        cs_wait_retrace();
}

// 11fb:0170 (INTRO): DAC entries lo..hi from pal. The DAC keeps 6 bits.
void cs_set_dac(const uint8_t pal[256][3], int lo, int hi)
{
    for (int c = lo; c <= hi; c++)
        for (int k = 0; k < 3; k++)
            vid_palette[c][k] = pal[c][k] & 63;
}

// 1212:0890 (INTRO): pal[lo..hi] := black, then the DAC after a retrace.
void cs_set_black(uint8_t pal[256][3], int lo, int hi)
{
    for (int c = lo; c <= hi; c++)
        pal[c][0] = pal[c][1] = pal[c][2] = 0;
    cs_wait_retrace();
    cs_set_dac(pal, lo, hi);
}

typedef enum : int { FROM_BLACK, FROM_WHITE } FadeKind;

// One step of the fade loops: entries lo..hi as c * i / n (FROM_BLACK) or
// 63 - (63 - c) * i / n (FROM_WHITE), in 16-bit integers, kept to a byte.
static void fade_step(const uint8_t pal[256][3], int lo, int hi, FadeKind kind, int i, int n)
{
    for (int c = lo; c <= hi; c++) {
        for (int k = 0; k < 3; k++) {
            int v = kind == FROM_BLACK ? (int16_t)(pal[c][k] * i) / n
                                       : 63 - (int16_t)((63 - pal[c][k]) * i) / n;
            vid_palette[c][k] = (uint8_t)v & 63;
        }
    }
}

// The fade loops wait for a retrace before each DAC write; here the step is
// written first and shown by the retrace.
static void fade(int n, const uint8_t pal[256][3], int lo, int hi, FadeKind kind, int from,
                 int to)
{
    if (n == 0)
        return;  // the original divides by zero; no script does this
    int step = from <= to ? 1 : -1;
    for (int i = from;; i += step) {
        fade_step(pal, lo, hi, kind, i, n);
        cs_wait_retrace();
        if (i == to)
            break;
    }
}

// 1212:00bb (INTRO): black to pal in n + 1 retraces, then all 256 entries.
void cs_fade_in(int n, const uint8_t pal[256][3], int lo, int hi)
{
    if (n >= 0)
        fade(n, pal, lo, hi, FROM_BLACK, 0, n);
    cs_set_dac(pal, 0, 255);
}

// 1212:01c2 (INTRO): pal to black. pal itself is left unchanged.
void cs_fade_out(int n, const uint8_t pal[256][3], int lo, int hi)
{
    if (n >= 0)
        fade(n, pal, lo, hi, FROM_BLACK, n, 0);
}

// 1212:02d5 (INTRO)
void cs_white_to_pal(int n, const uint8_t pal[256][3], int lo, int hi)
{
    if (n >= 0)
        fade(n, pal, lo, hi, FROM_WHITE, 0, n);
}

// 1212:065b (INTRO)
void cs_pal_to_white(int n, const uint8_t pal[256][3], int lo, int hi)
{
    if (n >= 0)
        fade(n, pal, lo, hi, FROM_WHITE, n, 0);
}

// 1212:03fd (INTRO): halfway to white (the lightning flash).
void cs_pal_to_half_white(int n, const uint8_t pal[256][3], int lo, int hi)
{
    if (n / 2 <= n)
        fade(n, pal, lo, hi, FROM_WHITE, n, n / 2);
}

// 1212:052c (INTRO)
void cs_half_white_to_pal(int n, const uint8_t pal[256][3], int lo, int hi)
{
    if (n / 2 <= n)
        fade(n, pal, lo, hi, FROM_WHITE, n / 2, n);
}

// 1212:0798 (INTRO): one whitening step i/n, written at once.
void cs_white_step(const uint8_t pal[256][3], int i, int n, int lo, int hi)
{
    fade_step(pal, lo, hi, FROM_WHITE, i, n);
}

// 1212:113a (INTRO): BIOS mode 13h (clears the screen) and 400-line sync.
void cs_mode13(void)
{
    if (cs.skip)
        return;
    SDL_memset(cs_screen(), 0, CS_SCREEN_SIZE);
    vid_set_mode(VID_MODE_13H);
}

// 1212:116c (INTRO): BIOS mode 12h, 640x480x16.
void cs_mode12(void)
{
    if (cs.skip)
        return;
    vid_set_mode(VID_MODE_12H);
}

// --- Music and cues ----------------------------------------------------------------

enum : int { SIM_RATE = 8000 };

static struct {
    bool active;       // a module was started
    bool audio;        // it plays on the audio device (else: simulated)
    xmp_context sim;   // silent sequencer when there is no device
    uint64_t start;    // retrace count when the simulated song started
    uint64_t rendered; // samples the simulation has advanced
    bool sim_ended;
} music;

void cs_music_stop(void)
{
    if (music.audio)
        audio_stop_music();
    if (music.sim) {
        xmp_end_player(music.sim);
        xmp_release_module(music.sim);
        xmp_free_context(music.sim);
    }
    music = (typeof(music)){};
}

static bool sim_start(const char *path)
{
    size_t size = 0;
    uint8_t *data = fs_load(path, &size);
    if (!data)
        return false;
    xmp_context ctx = xmp_create_context();
    bool ok = ctx && xmp_load_module_from_memory(ctx, data, (long)size) == 0;
    SDL_free(data);
    if (ok && xmp_start_player(ctx, SIM_RATE, XMP_FORMAT_MONO | XMP_FORMAT_8BIT) != 0) {
        xmp_release_module(ctx);
        ok = false;
    }
    if (!ok) {
        if (ctx)
            xmp_free_context(ctx);
        return false;
    }
    music.sim = ctx;
    music.start = timing_frames();
    return true;
}

// PlayModGUS (INTRO 1382:0007) / PlayModGUSorSB (1000:0031): <dir>\<name>.MOD.
void cs_music_start(const char *name)
{
    if (cs.skip)
        return;
    cs_music_stop();
    char path[64];
    SDL_snprintf(path, sizeof path, "%s\\%s.MOD", cs_prog->dir, name);
    if (audio_play_music(path, false)) {
        music.audio = true;
    } else if (!sim_start(path)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cut-scene music %s: not found", path);
        return;
    }
    music.active = true;
}

// Brings the simulated sequencer up to the retrace clock.
static void sim_advance(void)
{
    uint64_t due = (timing_frames() - music.start) * (uint64_t)SIM_RATE * RETRACE_MS_NUM
                   / (RETRACE_MS_DEN * 1000);
    int8_t sink[1024];
    while (!music.sim_ended && music.rendered < due) {
        int n = (int)SDL_min((uint64_t)sizeof sink, due - music.rendered);
        if (xmp_play_buffer(music.sim, sink, n, 1) < 0)
            music.sim_ended = true;
        music.rendered += (uint64_t)n;
    }
}

// 1382:0254 (INTRO): the player's (row, order). With no music every cue has
// passed.
static void music_pos(int *row, int *order, bool *ended)
{
    *ended = false;
    if (!music.active) {
        *row = *order = INT16_MAX;
        return;
    }
    int o = 0, r = 0;
    if (music.audio) {
        audio_music_position(&o, &r);
        *ended = audio_music_ended();
    } else {
        sim_advance();
        struct xmp_frame_info fi;
        xmp_get_frame_info(music.sim, &fi);
        o = fi.pos;
        r = fi.row;
        *ended = music.sim_ended;
    }
    *order = o + 1;
    *row = r + 1;
}

// WaitPos (INTRO 119b:0000, CREDITS 10e3:0000, VICTORY 10b4:0000): waits until
// the song reaches (order, row). The Sound Blaster player's end-of-song flag
// stops the wait too; the port raises it when libxmp reaches the end of the
// song, so no wait can hang (with a GUS the original waited forever).
void cs_wait_pos(int row, int order)
{
    if (cs.skip)
        return;
    bool key = false, ended;
    int r, o;
    music_pos(&r, &o, &ended);
    cs.music_end |= ended;
    while ((o < order || (o == order && r < row)) && !cs.music_end && !key) {
        cs_wait_retrace();
        if (cs_prog->keys != CS_KEYS_NONE)
            key = input_key_pressed();
        music_pos(&r, &o, &ended);
        cs.music_end |= ended;
    }
    if (!cs.music_end && !key)
        return;
    switch (cs_prog->keys) {
    case CS_KEYS_SKIP: {
        int k = 0;
        if (key) {
            k = input_read_key();
            if (k == 0)
                input_read_key();  // extended key: its scan code
        }
        if (k == ESC_KEY)
            cs_exit();
        cs.skip = true;
        break;
    }
    case CS_KEYS_QUIT:
        cs_exit();
    case CS_KEYS_NONE:
        break;
    }
}

// --- Timer unit ------------------------------------------------------------------------

static int64_t timer_ms(void)
{
    return (int64_t)(timing_frames() - timer_base) * RETRACE_MS_NUM / RETRACE_MS_DEN;
}

// 1342:0000 (INTRO): clock and deadline back to 0.
void cs_timer_reset(void)
{
    timer_base = timing_frames();
    cs.deadline = 0;
}

// 1342:0016 (INTRO) and the retrace wait its callers do when it was early:
// deadline += ms; if not yet reached, busy-wait for it and then for the next
// retrace. The clock only moves at retraces, so waiting for the first retrace
// at or after the deadline does both. When late, nothing waits (and nothing
// is shown) until the anim has caught up.
void cs_timer_wait(int ms)
{
    cs.deadline += ms;
    cs.early = cs.deadline - timer_ms() >= 1;
    while (cs.deadline > timer_ms())
        cs_wait_retrace();
}

// --- Script helpers -------------------------------------------------------------------

static void load_or_exit(const char *path, uint8_t *dest, int rows)
{
    const PicOptions opt = {
        .rows = rows,
        .palette = cs.target,
        .first_colour = 0,
        .last_colour = 255,
    };
    if (!pic_load_into(path, dest, CS_SCREEN_SIZE, &opt, nullptr, nullptr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cut-scene picture %s: not found", path);
        cs_exit();
    }
}

// LoadPicBuf (INTRO 119b:00e1): <dir>\<name>.PIC into the picture buffer, its
// palette into the target palette.
void cs_load_pic(const char *name)
{
    if (cs.skip)
        return;
    char path[64];
    SDL_snprintf(path, sizeof path, "%s\\%s.PIC", cs_prog->dir, name);
    load_or_exit(path, cs.buf, VID_HEIGHT);
}

// 119b:0152 (INTRO)
void cs_preload(int row, int order, const char *name)
{
    if (cs.skip)
        return;
    cs_load_pic(name);
    cs_wait_pos(row, order);
}

void cs_show_buf(void)
{
    SDL_memcpy(cs_screen(), cs.buf, CS_SCREEN_SIZE);
}

void cs_use_target(void)
{
    SDL_memcpy(cs.pal, cs.target, sizeof cs.pal);
}

void cs_fill_screen(uint8_t colour)
{
    SDL_memset(cs_screen(), colour, CS_SCREEN_SIZE);
}

// Pic (INTRO 119b:018e, VICTORY 10b4:0135, CREDITS 10e3:014e): load, wait for
// the cue, fade the old picture out, show the new one and fade it in.
void cs_pic(int fade_in, int fade_out, int row, int order, const char *name)
{
    if (cs.skip)
        return;
    cs_preload(row, order, name);
    if (cs.skip)
        return;
    if (fade_out > 0)
        cs_fade_out(fade_out, cs.pal, 0, 255);
    cs_show_buf();
    cs_use_target();
    if (fade_in > 0)
        cs_fade_in(fade_in, cs.pal, 0, 255);
}

// 10d9:037f (INTRO)
void cs_fade_out_at(int n, int row, int order)
{
    if (cs.skip || n == 0)
        return;
    cs_wait_pos(row, order);
    cs_fade_out(n, cs.pal, 0, 255);
    cs_set_black(cs.pal, 0, 255);
}

// Strips (INTRO 10d9:00f1, CREDITS 108b:00f2): a 640x480 16-colour picture
// from five strips <dir>\<name>1..5.PIC (640x100, the last 640x80), decoded
// into the picture buffer and written to the four planes. The 16 colours
// land in the DAC entries the attribute controller maps them to. fade_in -1
// is the white flash cut. The original also sets write mode 2 (GC 5), which
// the plane copies don't depend on.
void cs_strips(int fade_in, int in_row, int in_order, int fade_out, int out_row,
               int out_order, const char *dir, const char *name)
{
    static const uint16_t attr[16] = { 0, 1, 2, 3, 4, 5, 0x14, 7,
                                       0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f };  // DS:0002
    if (cs.skip)
        return;
    cs_wait_pos(out_row, out_order);
    if (cs.skip)
        return;
    if (fade_out > 0)
        cs_fade_out(fade_out, cs.pal, 0, 255);
    cs_mode12();
    cs_set_black(cs.pal, 0, 255);
    for (int k = 0; k < 5; k++) {
        char path[64];
        SDL_snprintf(path, sizeof path, "%s\\%s%d.PIC", dir, name, k + 1);
        load_or_exit(path, cs.buf, 100);
        // 8000 bytes per plane = 100 rows; rows past 480 are off-screen.
        vid12_blit_strip(k * 100, SDL_min(100, VID12_HEIGHT - k * 100), cs.buf);
    }
    if (in_order > 0 && in_row > 0)
        cs_wait_pos(in_row, in_order);
    for (int i = 0; i < 16; i++)
        SDL_memcpy(cs.pal[attr[i]], cs.target[i], 3);
    if (fade_in > 0)
        cs_fade_in(fade_in, cs.pal, 0, 255);
    if (fade_in == -1) {
        // BUG? The white target palette is never used: the fades below work
        // on the current one.
        for (int c = 1; c <= 255; c++)
            cs.target[c][0] = cs.target[c][1] = cs.target[c][2] = 0xff;
        cs_fade_in(5, cs.pal, 0, 255);
        cs_pal_to_half_white(5, cs.pal, 0, 255);
        cs_white_to_pal(10, cs.pal, 0, 255);
    }
}

// Shake (INTRO 1116:04ab): `count` times, the picture buffer copied to the
// screen at a random offset within `amp` pixels, then `frames` retraces.
void cs_shake(int amp, int frames, int count)
{
    if (cs.skip || count <= 0)
        return;
    uint8_t *screen = cs_screen();
    for (int i = 1; i <= count; i++) {
        int dx = cs_random((uint16_t)amp);
        int dy = cs_random((uint16_t)amp);
        uint8_t *dst = screen + (amp >> 1) * 321;
        const uint8_t *src = cs.buf + dx + dy * VID_WIDTH;
        for (int y = 0; y < VID_HEIGHT - amp; y++)
            SDL_memcpy(dst + y * VID_WIDTH, src + y * VID_WIDTH, (size_t)(VID_WIDTH - amp));
        cs_wait_frames(frames);
    }
}

// --- Anim player --------------------------------------------------------------------

// 1116:0050 (INTRO): frees the frames.
void cs_anim_close(void)
{
    ani_free(&ani);
}

// 1116:0000 + 1116:0079 + 116c:0010 (INTRO).
void cs_anim_open(int n)
{
    cs_anim_close();
    cs.anim = (int16_t)n;
    cs.frame = 0;
    (void)cs_random(200);  // a variable nothing reads: Random(200) + 100
    char path[64];
    SDL_snprintf(path, sizeof path, "%s\\ANIM%d.ANI", cs_prog->dir, n);
    if (!ani_load(path, &ani)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cut-scene anim %s: not found", path);
        cs_exit();
    }
}

// 1116:009e and 1116:0130 (INTRO). Keyed frames are drawn over frame 1.
void cs_anim_step(int frame, bool keyed)
{
    if (cs.anim == 0)
        return;
    cs.frame++;
    if (cs.frame > cs_frame_count(cs.anim))
        cs.frame = 1;
    if (frame > 0)
        cs.frame = (int16_t)frame;
    if (keyed) {
        ani_draw_frame(&ani, 0, cs.buf, VID_WIDTH, 0);
        ani_draw_frame(&ani, cs.frame - 1, cs.buf, VID_WIDTH, 0);
        cs_show_buf();
    } else {
        ani_draw_frame(&ani, cs.frame - 1, cs_screen(), VID_WIDTH, 0);
    }
}

// Anim (INTRO 1116:01c1, VICTORY 1049:0143): plays ANIM<n> at the cue, one
// frame per `delay` ms. With use_pal its palette (<dir>\PAL<n>.PIC) comes in
// over a fade through black. Except for anims 7, 16 and 18 the original
// releases the picture buffer meanwhile (its contents are not used after).
void cs_anim(bool use_pal, int delay, int row, int order, int n)
{
    if (cs.skip)
        return;
    if (use_pal) {
        char path[64];
        SDL_snprintf(path, sizeof path, "%s\\PAL%d.PIC", cs_prog->dir, n);
        if (!pic_palette_load(path, cs.target))
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cut-scene palette %s: not found", path);
    }
    cs_anim_open(n);
    cs_wait_pos(row, order);
    if (!cs.skip) {
        if (cs.anim == 12)
            cs_wait_frames(18);
        if (use_pal) {
            cs_fade_out(5, cs.pal, 0, 255);
            cs_set_black(cs.pal, 0, 255);
        }
        cs_timer_reset();
        cs_anim_step(0, cs.anim == 7);
        if (use_pal)
            cs_fade_in(5, cs.target, 0, 255);
        int count = cs_frame_count(cs.anim);
        for (int f = 2; f <= count; f++) {
            if (cs.anim == 16 && f < 5)
                cs_shake(3, 0, 1);
            if (cs.anim == 19 && f == 51)
                cs_white_step(cs.target, 4, 5, 0, 255);
            if (cs.anim == 19 && f == 52)
                cs_white_step(cs.target, 3, 5, 0, 255);
            cs_timer_wait(delay);
            cs_anim_step(0, cs.anim == 7);
        }
    }
    cs_anim_close();
    cs_use_target();
}

// --- Runner ---------------------------------------------------------------------------

bool cutscene_from_name(const char *name, Cutscene *out)
{
    static const char *const names[] = { "credits", "intro", "victory" };
    for (int i = 0; i < (int)SDL_arraysize(names); i++) {
        if (name && SDL_strcasecmp(name, names[i]) == 0) {
            *out = (Cutscene)i;
            return true;
        }
    }
    return false;
}

// SAVE\SETUP.SPD byte 0, as the programs read it: 4 (AdLib) and 5 (none) run
// the GUS player silently.
static bool music_silent(void)
{
    size_t size = 0;
    uint8_t *setup = fs_load("SAVE\\SETUP.SPD", &size);
    bool silent = setup && size >= 1 && (setup[0] == 4 || setup[0] == 5);
    SDL_free(setup);
    return silent;
}

bool cutscene_play(Cutscene which)
{
    static const CsProgram *const programs[] = { &cs_credits, &cs_intro, &cs_victory };
    cs_prog = programs[which];
    char *dir = fs_resolve(cs_prog->dir);
    if (!dir) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cut-scene data %s: not found", cs_prog->dir);
        return true;
    }
    SDL_free(dir);

    // The game opens the device in Sound_Init; the cut-scene has its own.
    audio_shutdown();
    audio_init();
    if (music_silent())
        audio_set_music_volume(0);

    cs = (CsData){ .buf = buffers[0] };
    SDL_memset(vid_palette, 0, sizeof vid_palette);
    timer_base = timing_frames();

    volatile bool quit = false;
    timing_set_mouse_hook(nullptr);  // the game's cursor stays off the cut-scene
    timing_set_quit_handler(on_quit);
    int jumped = setjmp(exit_jump);
    if (jumped == 0)
        cs_prog->run();
    else
        quit = jumped == JUMP_QUIT;
    timing_set_quit_handler(nullptr);

    cs_anim_close();
    cs_music_stop();
    audio_set_music_volume(1);
    audio_shutdown();
    vid_set_mode(VID_MODE_13H);
    SDL_memset(cs_screen(), 0, CS_SCREEN_SIZE);
    SDL_memset(vid_palette, 0, sizeof vid_palette);
    while (input_read_key() >= 0) {
    }
    return !quit;
}
