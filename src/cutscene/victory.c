// VICTORY.PRG: the end sequence after the game is won (docs/cutscenes.md
// section 5). Main block 1000:0161. Keys are ignored.
#include <SDL3/SDL.h>

#include "cutscene/cs.h"

// DS:0002: frames played of ANIM1 (the file has 19).
static const uint8_t frame_counts[2] = { 0, 16 };

// 10b4:01ce: Pic with the picture shown at one cue (still black) and faded
// in at a second.
static void pic_cue(int fade_in, int fade_out, int in_row, int in_order, int row, int order,
                    const char *name)
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
    cs_wait_pos(in_row, in_order);
    if (fade_in > 0)
        cs_fade_in(fade_in, cs.pal, 0, 255);
}

// 10b4:0279: CR1 (in the picture buffer), CR2 and CR3 scroll up through the
// screen one line per retrace, from below to off the top.
static void scroll_credits(int row, int order)
{
    if (cs.skip)
        return;
    uint8_t *pages[3] = { cs.buf, cs_buffer(2), cs_buffer(3) };
    uint8_t *screen = cs_screen();
    cs.buf = pages[1];
    cs_load_pic("CR2");
    cs.buf = pages[2];
    cs_load_pic("CR3");
    cs.buf = pages[0];
    cs_wait_pos(row, order);
    cs_timer_reset();
    for (int phase = 0; phase < 4; phase++) {
        const uint8_t *top = phase > 0 ? pages[phase - 1] : nullptr;
        const uint8_t *bottom = phase < 3 ? pages[phase] : nullptr;
        for (int l = 0; l < VID_HEIGHT; l++) {
            size_t split = (size_t)l * VID_WIDTH, rest = CS_SCREEN_SIZE - split;
            if (top)
                SDL_memcpy(screen, top + split, rest);
            else
                SDL_memset(screen, 0, rest);
            if (bottom)
                SDL_memcpy(screen + rest, bottom, split);
            else
                SDL_memset(screen + rest, 0, split);
            cs_wait_retrace();
        }
    }
    cs_use_target();
}

static void run(void)
{
    cs_music_start("ENDSEQ");
    cs_mode13();
    cs_pic(40, 1, 1, 1, "TX1");
    cs_pic(40, 120, 1, 2, "TX2");
    pic_cue(70, 80, 25, 3, 1, 3, "PAL1");
    cs_anim(false, 74, 30, 3, 1);

    // The soldiers in colour, cut in through a white flash.
    cs_load_pic("RETURN");
    cs_pal_to_half_white(10, cs.pal, 0, 255);
    cs_show_buf();
    cs_use_target();
    cs_white_to_pal(10, cs.pal, 0, 255);

    cs_pic(40, 120, 40, 4, "TX3");
    cs_pic(70, 5, 1, 5, "EARTH");
    cs_pic(70, 5, 1, 6, "END");
    cs_pic(70, 5, 1, 8, "TX4");
    cs_pic(70, 5, 32, 8, "TX5");
    cs_wait_pos(50, 8);
    cs_fade_out(70, cs.pal, 0, 255);

    // The credits pages. Both cues have long passed.
    cs_preload(1, 1, "CR1");
    cs_fill_screen(0);
    cs_set_dac(cs.target, 0, 255);
    scroll_credits(6, 1);
    cs_wait_pos(58, 11);
    cs_exit();
}

const CsProgram cs_victory = {
    .dir = "VICTORY",
    .frame_counts = frame_counts,
    .anims = (int)SDL_arraysize(frame_counts),
    .keys = CS_KEYS_NONE,
    .run = run,
};
