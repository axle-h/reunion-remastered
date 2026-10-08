// CREDITS.PRG: the Grandslam logo, the title and the credits roll over the
// alien face (docs/cutscenes.md section 3). Main block 1000:0389. Any key
// while waiting for a cue ends the program.
#include <SDL3/SDL.h>

#include "assets/pic.h"
#include "cutscene/cs.h"

// DS:001b + 7n: frames played per anim. ANIM3 has 300 in the file, of which
// frames 31-165 are shown; ANIM28 is never played.
static const uint8_t frame_counts[29] = {
    0, 10, 30, 165, 41, 29, 13, 14, 34, 42, 30, 30, 30, 30, 30,
    30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 0,
};

// Anim (1000:0105). Unlike INTRO's: no skip flag, ANIM3 starts at frame 31,
// ANIM4 switches back to mode 13h, and frames are paced by the retrace
// (GUS player: 2 retraces, then 1 more; with a Sound Blaster DOS GetTime
// gave 0/2 cs for ANIM3, 5 cs for ANIM4 and 3 cs otherwise instead).
static void anim(bool use_pal, int row, int order, int n)
{
    // The picture buffer is released here and allocated again at the end.
    if (use_pal) {
        char path[64];
        SDL_snprintf(path, sizeof path, "CREDITS\\PAL%d.PIC", n);
        if (!pic_palette_load(path, cs.target))
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cut-scene palette %s: not found", path);
    }
    cs_anim_open(n);
    if (cs.anim == 3)
        cs.frame = 30;
    cs_wait_pos(row, order);
    if (use_pal) {
        cs_fade_out(5, cs.pal, 0, 255);
        cs_set_black(cs.pal, 0, 255);
    }
    if (n == 4)
        cs_mode13();
    if (use_pal)
        cs_fade_in(5, cs.target, 0, 255);
    cs_anim_step(0, false);
    int count = cs_frame_count(cs.anim);
    for (int f = cs.frame + 1; f <= count; f++) {
        cs_wait_frames(2);
        cs_wait_retrace();
        cs_anim_step(0, false);
    }
    cs_anim_close();
    cs_use_target();
}

static void run(void)
{
    cs_music_start("STEAL2");
    cs_mode13();

    // The Grandslam logo, then "presents" flashing in through white.
    anim(true, 1, 1, 3);
    cs_wait_pos(32, 2);
    cs_load_pic("PRESENTS");
    cs_fade_out(5, cs.pal, 1, 255);
    cs_fill_screen(1);
    cs_show_buf();
    // BUG? Whitened and at once overwritten by the picture's palette.
    for (int c = 1; c <= 255; c++)
        cs.pal[c][0] = cs.pal[c][1] = cs.pal[c][2] = 0xff;
    cs_use_target();
    cs_white_to_pal(20, cs.pal, 1, 255);
    cs_pic(70, 5, 1, 3, "GAMEBY");

    // The 640x480 picture, the ship anim and the title.
    cs_strips(70, 1, 4, 5, 43, 3, "CREDITS", "A");
    anim(true, 45, 4, 4);
    cs_pal_to_half_white(4, cs.pal, 0, 255);
    cs_load_pic("REUNION");
    cs_show_buf();
    cs_use_target();
    cs_white_to_pal(10, cs.pal, 0, 255);

    // The credits roll over the alien face.
    cs_pic(70, 195, 1, 8, "PAL10");
    static const uint8_t roll[][2] = {  // row, order of ANIM10..27
        { 22, 9 }, { 53, 9 }, { 23, 10 }, { 54, 10 }, { 22, 11 }, { 54, 11 },
        { 22, 12 }, { 55, 12 }, { 23, 13 }, { 55, 13 }, { 23, 14 }, { 55, 14 },
        { 56, 15 }, { 28, 16 }, { 58, 16 }, { 23, 17 }, { 55, 17 }, { 23, 18 },
    };
    for (int i = 0; i < (int)SDL_arraysize(roll); i++)
        anim(false, roll[i][0], roll[i][1], 10 + i);

    cs_load_pic("PAL10");
    cs_wait_pos(1, 20);
    cs_pal_to_white(5, cs.pal, 0, 255);
    cs_show_buf();
    cs_use_target();
    cs_half_white_to_pal(5, cs.pal, 0, 255);
    cs_wait_pos(63, 37);
    cs_exit();
}

const CsProgram cs_credits = {
    .dir = "CREDITS",
    .frame_counts = frame_counts,
    .anims = (int)SDL_arraysize(frame_counts),
    .keys = CS_KEYS_QUIT,
    .run = run,
};
