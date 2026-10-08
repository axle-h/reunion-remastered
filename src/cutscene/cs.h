// The skeleton shared by the three cut-scene programs (docs/cutscenes.md):
// the same picture/palette/anim/cue units are linked into CREDITS, INTRO and
// VICTORY, only the scene script and a few details differ. Addresses are
// Ghidra addresses in each program's own import (DS = 1e62 INTRO, 1ce7
// CREDITS, 1d3c VICTORY); see ghidra/{intro,credits,victory}.symbols.txt.
//
// The scripts pass cues as (row, order) like the original calls. Positions
// are the GUS player's: both 1-based (order n+1 and row r+1 while row r of
// order index n plays), so (1,1) is the start of the song.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "platform/video.h"

enum : int {
    CS_SCREEN_SIZE = VID_WIDTH * VID_HEIGHT,
    CS_BUFFERS = 4,  // picture buffer + three temporary ones (flyby, scrolls)
};

// Key handling while waiting for a cue (the WaitPos routine of each program).
typedef enum : int {
    CS_KEYS_NONE,  // VICTORY: keys are ignored
    CS_KEYS_SKIP,  // INTRO: a key skips the rest of the block, ESC ends the program
    CS_KEYS_QUIT,  // CREDITS: any key ends the program
} CsKeys;

typedef struct {
    const char *dir;              // data directory and prefix of every file name
    const uint8_t *frame_counts;  // frames played per anim number (DS table)
    int anims;                    // entries in frame_counts
    CsKeys keys;
    void (*run)(void);            // the program's main block; ends in cs_exit()
} CsProgram;

// Mirror of the program's data segment, the variables the shared units use
// (offsets INTRO / CREDITS / VICTORY). The three programs zero them at start.
typedef struct {
    uint8_t pal[256][3];     // 0890 / 0f50 / 07ac  current palette, the fades' source
    uint8_t target[256][3];  // 0b90 / 0706 / 0aac  palette of the last picture loaded
    uint8_t *buf;            // 0e90 / 0702 / 0dac  320x200 off-screen picture buffer
    bool skip;               // 0e98 / -    / 0db4  every routine is a no-op until reset
    bool music_end;          // 074e / 0e48 / -     the SB player's end-of-song flag
    int16_t anim;            // 0702 / 06f0 / 061e  anim number being played (0: none)
    int16_t frame;           // 0704 / 06f2 / 0620  its frame (1-based)
    int32_t deadline;        // 07ba / -    / 06d6  anim pacing deadline, ms after the reset
    bool early;              // 07b4 / -    / 06d0  the last deadline had not passed yet
    uint32_t rand_seed;      // System.RandSeed (never randomised: starts at 0)
} CsData;

extern CsData cs;
extern const CsProgram *cs_prog;

// Programs.
extern const CsProgram cs_credits;
extern const CsProgram cs_intro;
extern const CsProgram cs_victory;

// --- System / port -----------------------------------------------------------
// The exit routine (INTRO 1212:11a3, CREDITS 1117:0f72, VICTORY 111e:0f05):
// FadeOut(35), text mode, music off, keyboard drained, Halt(0).
[[noreturn]] void cs_exit(void);
uint8_t *cs_screen(void);
// Temporary 64000-byte buffers (GetMem(0FD00h) in the original).
uint8_t *cs_buffer(int index);
uint16_t cs_random(uint16_t range);
// Frames per anim as the program's table gives them (may differ from the file).
int cs_frame_count(int anim);

// --- Graphics unit (INTRO 1212, CREDITS 1117, VICTORY 111e) -------------------
void cs_wait_retrace(void);
void cs_wait_frames(int n);
void cs_set_dac(const uint8_t pal[256][3], int lo, int hi);
void cs_set_black(uint8_t pal[256][3], int lo, int hi);
void cs_fade_in(int n, const uint8_t pal[256][3], int lo, int hi);
void cs_fade_out(int n, const uint8_t pal[256][3], int lo, int hi);
void cs_white_to_pal(int n, const uint8_t pal[256][3], int lo, int hi);
void cs_pal_to_white(int n, const uint8_t pal[256][3], int lo, int hi);
void cs_pal_to_half_white(int n, const uint8_t pal[256][3], int lo, int hi);
void cs_half_white_to_pal(int n, const uint8_t pal[256][3], int lo, int hi);
void cs_white_step(const uint8_t pal[256][3], int i, int n, int lo, int hi);
void cs_mode13(void);
void cs_mode12(void);

// --- Music and cues -----------------------------------------------------------
// Starts <dir>\<name>.MOD (once, no loop).
void cs_music_start(const char *name);
void cs_music_stop(void);
void cs_wait_pos(int row, int order);

// --- Timer unit (INTRO 1342, VICTORY 1224): 1 ms clock ------------------------
void cs_timer_reset(void);
// Adds `ms` to the deadline and waits for it (and the next retrace).
void cs_timer_wait(int ms);

// --- Script helpers -------------------------------------------------------------
void cs_load_pic(const char *name);
void cs_preload(int row, int order, const char *name);
void cs_show_buf(void);
void cs_use_target(void);
void cs_pic(int fade_in, int fade_out, int row, int order, const char *name);
void cs_fade_out_at(int n, int row, int order);
void cs_strips(int fade_in, int in_row, int in_order, int fade_out, int out_row,
               int out_order, const char *dir, const char *name);
void cs_shake(int amp, int frames, int count);
void cs_fill_screen(uint8_t colour);

// --- Anim player ------------------------------------------------------------------
// Loads <dir>\ANIM<n>.ANI and makes it current (frame 0).
void cs_anim_open(int n);
void cs_anim_close(void);
// Advances to the next frame (wrapping to 1) or to `frame` if > 0, and draws it:
// keyed into the picture buffer and copied to the screen, or straight onto the
// screen as a delta of the frame shown.
void cs_anim_step(int frame, bool keyed);
void cs_anim(bool use_pal, int delay, int row, int order, int n);
