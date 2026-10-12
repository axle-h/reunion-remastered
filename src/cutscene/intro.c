// INTRO.PRG: the story intro, eleven MOD blocks of mode 12h stills, mode 13h
// pictures and anims. Main block 1000:0285.
// A key skips the rest of the current block (some blocks share the skip
// flag, so a key in INTRO1 also skips INTRO2-4); ESC ends the program.
#include <SDL3/SDL.h>

#include "cutscene/cs.h"

// DS:0041 + 7n: frames played per anim (the file may hold more).
static const uint8_t frame_counts[25] = {
    0, 6, 0, 26, 42, 16, 51, 31, 28, 0, 5, 13, 102,
    187, 91, 120, 18, 123, 85, 52, 29, 42, 17, 47, 75,
};

// DS:0023: frame order of the scripted anim 11 (frames 2..35).
static const uint8_t monitor_frames[34] = {
    2, 3, 2, 3, 2, 3, 2, 4, 1, 4, 1, 1, 3, 3, 2, 1, 4,
    5, 3, 4, 5, 6, 7, 8, 9, 10, 2, 6, 9, 2, 4, 3, 9, 10,
};

static void strips(int fade_in, int in_row, int in_order, int fade_out, int out_row,
                   int out_order, const char *name)
{
    cs_strips(fade_in, in_row, in_order, fade_out, out_row, out_order, "INTROP", name);
}

// Start of a MOD block (the flag reset is not done before every block).
static void new_block(void)
{
    cs.skip = false;
    cs.music_end = false;
}

// ScrollPic (119b:0227): the new picture slides down from the top over the
// old one, one line per retrace.
static void scroll_pic(int row, int order, const char *name)
{
    if (cs.skip)
        return;
    uint8_t *old = cs.buf, *next = cs_buffer(1), *screen = cs_screen();
    cs.buf = next;
    cs_preload(row, order, name);
    cs.buf = old;
    cs_timer_reset();
    for (int l = VID_HEIGHT - 1; l >= 0; l--) {
        size_t split = (size_t)l * VID_WIDTH;
        SDL_memcpy(screen, next + split, CS_SCREEN_SIZE - split);
        SDL_memcpy(screen + CS_SCREEN_SIZE - split, old, split);
        cs_wait_retrace();
    }
    cs_use_target();
}

static void copy_rect(uint8_t *dst, const uint8_t *src, int w, int h)
{
    for (int y = 0; y < h; y++)
        SDL_memcpy(dst + y * VID_WIDTH, src + y * VID_WIDTH, (size_t)w);
}

// 1630:00f6: copies a rectangle, leaving pixels of colour `key` out.
static void copy_rect_keyed(uint8_t *dst, const uint8_t *src, int w, int h, uint8_t key)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (src[y * VID_WIDTH + x] != key)
                dst[y * VID_WIDTH + x] = src[y * VID_WIDTH + x];
}

// Flyby (119b:0366): the star field (the picture buffer) pans 2 px per frame
// with wrap-around while the planet and then the ship move in from the left,
// 168 frames at 14 ms. A start-up benchmark picks the fast variant (separate
// planet and ship layers) on any machine that runs the port; slower machines
// got the single picture PLANET2.
static void flyby(int row, int order)
{
    if (cs.skip)
        return;
    const bool fast = true;
    uint8_t *stars = cs.buf, *compose = cs_buffer(1), *planet = cs_buffer(2),
            *ship = cs_buffer(3);
    cs.buf = planet;
    cs_load_pic(fast ? "PLANET" : "PLANET2");
    if (fast) {
        cs.buf = ship;
        cs_load_pic("SHIP");
    }
    cs.buf = stars;
    cs_wait_pos(row, order);
    cs_timer_reset();
    if (!cs.skip) {
        for (int i = 0; i <= 167; i++) {
            int x = (i * 2) % VID_WIDTH + 1;
            int planet_w = SDL_max((i * 2 - 80) * 5 / 4, 0);
            int ship_w = SDL_max((i * 2 - 176) * 2, 0);
            copy_rect(compose, stars + VID_WIDTH - x, x, VID_HEIGHT);
            copy_rect(compose + x, stars, VID_WIDTH - x, VID_HEIGHT);
            if (planet_w > 0)
                copy_rect_keyed(compose, planet + VID_WIDTH - planet_w, planet_w, VID_HEIGHT, 0);
            if (fast && ship_w > 0)
                copy_rect_keyed(compose, ship + VID_WIDTH - ship_w, ship_w, VID_HEIGHT, 0);
            SDL_memcpy(cs_screen(), compose, CS_SCREEN_SIZE);
            cs_timer_wait(14);
        }
    }
    cs_use_target();
}

// AnimScripted (1116:0391): anim 11, the monitor, frames in the order of
// monitor_frames, each drawn over frame 1.
static void anim_scripted(bool use_pal, int delay, int row, int order, int n)
{
    if (cs.skip)
        return;
    if (use_pal)
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "anim_scripted: use_pal is not ported");
    cs_anim_open(n);
    cs_wait_pos(row, order);
    if (!cs.skip) {
        cs_timer_reset();
        cs_anim_step(0, cs.anim == 7);
        for (int i = 2; i <= 35; i++) {
            cs_timer_wait(delay);
            cs_anim_step(monitor_frames[i - 2], true);
        }
    }
    cs_anim_close();
    cs_use_target();
}

static void run(void)
{
    // 1353:02ac allocates the picture buffer (EMS if possible); the memory
    // check ("Not enough memory to run the intro") and the sound device
    // errors are left out.
    cs_music_start("INTRO1");

    // INTRO1: the alien face, then the first stills.
    cs_mode13();
    cs_pic(70, 5, 1, 1, "PAL20");
    cs_anim(false, 84, 15, 1, 20);
    cs_anim(false, 84, 3, 2, 21);
    cs_fade_out_at(30, 16, 2);
    cs_mode12();
    strips(100, 30, 2, 10, 1, 1, "1X");
    strips(100, 54, 3, 70, 8, 3, "2X");
    strips(100, 36, 4, 70, 25, 4, "3X");
    strips(100, 56, 6, 140, 2, 6, "4X");
    cs_wait_pos(48, 7);

    cs_music_start("INTRO2");
    strips(100, 1, 2, 30, 1, 0, "5X");
    strips(100, 32, 3, 70, 15, 3, "6X");
    cs_wait_pos(32, 5);

    cs_music_start("INTRO3");
    strips(300, 18, 1, 30, 1, 0, "7X");
    strips(5, 1, 3, 70, 40, 2, "8X");
    strips(5, 1, 4, 70, 50, 3, "9X");
    strips(5, 1, 5, 70, 50, 4, "10X");
    strips(100, 32, 5, 70, 20, 5, "10_2X");
    strips(100, 58, 5, 70, 45, 5, "11X");
    cs_wait_pos(14, 7);

    cs_music_start("INTRO4");
    strips(20, 1, 2, 30, 1, 0, "12X");
    strips(100, 1, 4, 70, 16, 3, "13X");
    strips(100, 1, 6, 70, 40, 5, "14X");
    strips(-1, 1, 8, 70, 12, 7, "15X");
    cs_fade_out_at(70, 40, 8);
    cs_wait_pos(60, 8);

    // REBEL: stars, the fly-by, the rebel base.
    new_block();
    cs_mode13();
    cs_music_start("REBEL");
    cs_pic(5, 5, 1, 1, "STARS");
    flyby(6, 1);
    cs_pic(5, 5, 64, 2, "PAL12");
    cs_anim(false, 155, 16, 3, 12);
    cs_anim(false, 41, 56, 4, 13);
    cs_pic(5, 5, 24, 6, "BASE");
    cs_pic(40, 99, 46, 6, "PAL15");
    cs_anim(false, 42, 8, 8, 15);
    cs_anim(false, 50, 5, 9, 14);
    cs_wait_pos(6, 10);
    if (!cs.skip)
        cs_fade_out(180, cs.pal, 0, 255);

    // BASE1
    new_block();
    cs_mode13();
    cs_music_start("BASE1");
    cs_pic(255, 5, 10, 1, "BASE0");
    scroll_pic(32, 1, "BASE");
    cs_pic(5, 5, 1, 2, "PAL1");
    cs_pic(5, 5, 1, 3, "BASE");
    cs_pic(5, 5, 10, 3, "PAL11X");
    cs_pic(5, 5, 1, 4, "PAL1");
    cs_anim(false, 42, 55, 4, 1);
    cs_anim(false, 42, 63, 4, 1);
    cs_preload(1, 5, "PAL1");
    cs_shake(3, 1, 15);
    cs_pic(5, 5, 12, 5, "BASE");
    for (int flash = 0; flash < 2; flash++) {
        cs_wait_pos(flash == 0 ? 20 : 24, 5);
        if (!cs.skip)
            cs_pal_to_white(2, cs.pal, 0, 255);
        if (!cs.skip)
            cs_half_white_to_pal(2, cs.pal, 0, 255);
    }
    cs_pic(5, 5, 40, 5, "FOLYOSO");
    cs_wait_pos(41, 5);
    cs_shake(3, 1, 20);
    cs_pic(5, 5, 1, 6, "BASE");
    cs_anim(true, 84, 1, 7, 3);
    cs_pic(5, 5, 32, 7, "MUSZER");
    cs_pic(5, 5, 1, 8, "BASE");
    cs_pic(5, 5, 1, 9, "TRACE");
    cs_anim(true, 70, 10, 9, 4);
    cs_pic(5, 5, 10, 10, "PAL5");
    cs_anim(false, 64, 30, 10, 5);
    cs_pic(5, 5, 1, 11, "BASE");
    cs_anim(true, 70, 5, 12, 6);
    cs_anim(true, 74, 1, 13, 7);
    cs_anim(true, 70, 36, 13, 8);
    if (!cs.skip)
        cs_pal_to_half_white(10, cs.pal, 0, 255);
    if (!cs.skip)
        cs_white_to_pal(70, cs.pal, 0, 255);
    cs_wait_pos(38, 14);

    // BASE2: the shaking base, the monitor.
    new_block();
    cs_mode13();
    cs_music_start("BASE2");
    cs_pic(5, 5, 32, 2, "PAL11");
    cs_wait_pos(54, 2);
    cs_shake(3, 1, 15);
    cs_wait_pos(22, 3);
    cs_shake(3, 1, 15);
    cs_pic(5, 5, 1, 4, "PAL10");
    cs_anim(false, 42, 39, 4, 10);
    cs_pic(5, 5, 1, 5, "PAL11");
    cs_wait_pos(22, 5);
    cs_shake(3, 1, 15);
    anim_scripted(false, 120, 28, 6, 11);
    cs_pic(0, 0, 0, 0, "MON1");
    cs_wait_frames(40);
    cs_pic(0, 0, 0, 0, "MON2");
    cs_wait_frames(40);
    cs_pic(0, 0, 0, 0, "MON3");
    cs_wait_pos(6, 7);

    // TUNNEL: the attack on Earth.
    new_block();
    cs_mode13();
    cs_music_start("TUNNEL");
    cs_pic(70, 5, 1, 1, "PAL16");
    cs_anim(false, 42, 1, 3, 16);
    cs_anim(false, 42, 4, 4, 23);
    cs_anim(false, 42, 4, 4, 24);
    cs_anim(false, 42, 2, 5, 18);
    cs_anim(false, 42, 45, 5, 19);
    if (!cs.skip) {
        for (int i = 2; i >= 0; i--) {
            cs_white_step(cs.target, i, 5, 0, 255);
            cs_wait_retrace();
        }
    }
    cs_load_pic("ATTACK");
    cs_show_buf();
    cs_wait_pos(20, 6);
    cs_use_target();
    if (!cs.skip)
        cs_white_to_pal(100, cs.pal, 0, 255);
    cs_wait_pos(64, 6);
    if (!cs.skip)
        cs_fade_out(100, cs.pal, 0, 255);

    // INTRO5, INTRO6: stills.
    new_block();
    cs_mode12();
    cs_music_start("INTRO5");
    strips(100, 32, 1, 0, 0, 0, "16X");
    strips(-1, 1, 6, 70, 34, 5, "17X");
    strips(70, 1, 8, 70, 32, 7, "18X");
    cs_fade_out_at(70, 40, 8);
    cs_wait_pos(63, 8);

    cs_music_start("INTRO6");
    strips(10, 1, 2, 0, 0, 0, "19X");
    strips(20, 32, 2, 20, 24, 2, "20X");
    strips(70, 40, 3, 70, 10, 3, "21X");
    cs_fade_out_at(70, 16, 4);
    cs_wait_pos(24, 4);

    // INTRO7: the leader and Gabor.
    cs_music_start("INTRO7");
    cs_mode13();
    cs_pic(70, 5, 1, 1, "PAL22");
    cs_anim(false, 70, 63, 1, 22);
    cs_fade_out_at(70, 1, 3);
    if (!cs.skip)
        cs_fill_screen(0);
    cs_pic(140, 5, 18, 3, "LEADER");
    cs_pic(70, 5, 1, 7, "GABOR");
    cs_fade_out_at(70, 63, 7);
    cs_exit();
}

const CsProgram cs_intro = {
    .dir = "INTRO",
    .frame_counts = frame_counts,
    .anims = (int)SDL_arraysize(frame_counts),
    .keys = CS_KEYS_SKIP,
    .run = run,
};
