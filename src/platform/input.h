// Mouse and keyboard as the original game saw them: the INT 33h callback
// state and the Borland Crt ReadKey/KeyPressed key queue.
#pragma once

#include <stdint.h>

// Mouse position in game pixels, clamped to the driver range set at startup
// (x 0..318, y 0..198), and the callback's button bytes (0 or 1).
typedef struct {
    int x, y;
    uint8_t left, right, middle;
} InputMouse;

// Starts SDL text input on the video window. Call after vid_init.
void input_init(void);

// Processes pending SDL events. Handles quit, Alt+Enter, F12 (screenshot) and
// mouse/keyboard state.
void input_pump(void);

bool input_quit_requested(void);
// Asks the game to quit, as closing the window does.
void input_request_quit(void);
// Clears the quit request, the key queue and the mouse buttons (restart).
void input_reset(void);

// Sets the mouse state as if the user moved/clicked (test autopilot). The
// state holds until the next real mouse event or the next call.
void input_set_mouse(int x, int y, uint8_t left, uint8_t right);
// Queues a Crt.ReadKey code (0 then a scan code for extended keys).
void input_push_key(int code);

InputMouse input_mouse(void);

// Crt.KeyPressed.
bool input_key_pressed(void);

// Crt.ReadKey without the wait: returns the next code, or -1 if none. Extended
// keys give 0 followed by the BIOS scan code on the next call.
int input_read_key(void);
