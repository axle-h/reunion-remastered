// VGA mode 13h emulation: a 320x200 8-bit framebuffer and a 256-entry palette,
// presented through SDL at the original 4:3 display aspect.
#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_render.h>
#include <stdint.h>

enum : int {
    VID_WIDTH = 320,
    VID_HEIGHT = 200,
};

// The 320x200 framebuffer: the first 64,000 bytes of the 64 KB of video
// memory (segment A000h) it points to.
extern uint8_t (*vid_screen)[VID_WIDTH];

// Zeroes video memory (all 64 KB).
void vid_clear(void);

// VGA DAC palette, 6 bits per component (0..63), as written to port 3C9h.
extern uint8_t vid_palette[256][3];

// Mode 12h (640x480, 16 colours; planar in hardware). Kept as a chunky
// framebuffer with one byte per pixel (0..15). Pixel values go through the
// attribute controller table vid_attr to a DAC entry of vid_palette; the
// cut-scenes leave it at the VGA default (0-5, 14h, 7, 38h-3Fh) and so
// carry their picture palette in those DAC entries.
enum : int {
    VID12_WIDTH = 640,
    VID12_HEIGHT = 480,
};

typedef enum : int {
    VID_MODE_13H,
    VID_MODE_12H,
} VidMode;

extern uint8_t vid12_screen[VID12_HEIGHT][VID12_WIDTH];
extern uint8_t vid_attr[16];

// Switches mode and clears the new mode's framebuffer. Safe before vid_init.
void vid_set_mode(VidMode mode);
VidMode vid_mode(void);
void vid_reset_attr(void);

// Copies `rows` rows of 640 pixels (values 0..15, stride 640) to row `y`.
// Returns false if the strip does not fit.
bool vid12_blit_strip(int y, int rows, const uint8_t *pixels);

// Emulates a CPU byte write in write mode 2 at byte offset `offset` (y * 80 +
// x / 8): bits of `bitmask` (bit 7 = leftmost) select the 8 pixels, and in
// each plane of `planemask` (map mask, bit n = plane n) the pixel's bit n
// becomes bit n of `color`.
void vid12_planar_write(int offset, uint8_t color, uint8_t bitmask, uint8_t planemask);

bool vid_init(const char *title);
void vid_shutdown(void);

// Converts the framebuffer through the palette and shows it (no display
// vsync; timing.c paces frames).
void vid_present(void);

// Alt+Enter: between the window and the last full-screen choice (desktop
// by default); updates the config.
void vid_toggle_fullscreen(void);
// Applies config.display and the window or full-screen size.
void vid_apply_display(void);
// Window events from the event loops: a window resized by hand keeps its
// size in config.window_width / _height.
void vid_window_event(const SDL_Event *e);
// Window sizes for the port menu: 320x240 times 2, 3, ... up to what fits
// the display (at least one). Returns the count.
int vid_window_sizes(int sizes[][2], int max);
// The display's full-screen modes, largest first, one per size and refresh
// rate. Returns the count.
int vid_fullscreen_modes(SDL_DisplayMode *modes, int max);
// Drawn over every presented frame, in render output pixels with no
// logical presentation (the port menu). nullptr: none.
void vid_set_overlay(void (*draw)(SDL_Renderer *));
SDL_Window *vid_window(void);
// True when a player can see the window: one is open on a video driver
// other than offscreen and dummy.
bool vid_visible(void);

// Saves the current frame (framebuffer through the palette, at its native
// size) as an 8-bit indexed image: PNG if `path` ends in ".png", else BMP.
bool vid_save_image(const char *path);

// Developer screenshot (F12): saves the frame as a BMP in
// SDL_GetPrefPath("Reunion", "Reunion")/screenshots/.
void vid_screenshot(void);

// For mapping window coordinates to the 320x200 game space (input.c).
SDL_Renderer *vid_renderer(void);
