// VGA mode 13h emulation: a 320x200 8-bit framebuffer and a 256-entry palette,
// presented through SDL at the original 4:3 display aspect.
#pragma once

#include <stdint.h>

enum : int {
    VID_WIDTH = 320,
    VID_HEIGHT = 200,
};

// Stands in for video memory at A000:0000.
extern uint8_t vid_screen[VID_HEIGHT][VID_WIDTH];

// VGA DAC palette, 6 bits per component (0..63), as written to port 3C9h.
extern uint8_t vid_palette[256][3];

bool vid_init(const char *title);
void vid_shutdown(void);

// Converts the framebuffer through the palette and shows it. Blocks until the
// next display refresh when vsync is available.
void vid_present(void);

void vid_toggle_fullscreen(void);
