// Mouse and keyboard as the original game saw them: the INT 33h callback
// state and the Borland Crt ReadKey/KeyPressed key queue.
#pragma once

#include <SDL3/SDL_events.h>
#include <stdint.h>

// Mouse position in game pixels, clamped to the driver range set at startup
// (x 0..318, y 0..198), and the callback's button bytes (0 or 1).
typedef struct {
    int x, y;
    uint8_t left, right, middle;
} InputMouse;

// Starts SDL text input on the video window. Call after vid_init.
void input_init(void);

// Processes pending SDL events. Handles quit, Alt+Enter, F1 (port menu),
// F12 (screenshot) and mouse/keyboard state.
void input_pump(void);

bool input_quit_requested(void);
// Asks the game to quit, as closing the window does.
void input_request_quit(void);
// Asks the game to restart (port menu): timing_wait_retrace calls the
// restart handler.
void input_request_restart(void);
bool input_restart_requested(void);
// True once per F1 press (not in scripted runs): open the port menu.
bool input_take_menu_request(void);
// Clears the quit and restart requests, the key queue and the mouse
// buttons (restart).
void input_reset(void);
// Drops the key queue and the mouse buttons and re-reads the pointer
// position, so nothing done in the port menu reaches the game.
void input_release(void);

// Sets the mouse state as if the user moved/clicked (test autopilot). The
// state holds until the next real mouse event or the next call.
void input_set_mouse(int x, int y, uint8_t left, uint8_t right);
// Queues a Crt.ReadKey code (0 then a scan code for extended keys).
void input_push_key(int code);

InputMouse input_mouse(void);

// Is a Shift key held? (Shift+click, game/improvements.h repeat_build.)
bool input_shift(void);
// Sets the Shift state as if the user pressed or released it (test
// autopilot). It holds until the next real key event or the next call.
void input_set_shift(bool held);

// Sees every key press and release before the game does, after the
// port's own keys (Alt+Enter, F1, F12); returns true when it took the key,
// which then reaches neither the key queue nor, for a press, as typed text
// (game/keyplay.h). nullptr: none.
void input_set_key_hook(bool (*hook)(const SDL_KeyboardEvent *e));
// The mouse was moved or clicked since the last call (a real one, not
// input_set_mouse).
bool input_take_mouse_moved(void);

// Crt.KeyPressed.
bool input_key_pressed(void);

// Crt.ReadKey without the wait: returns the next code, or -1 if none. Extended
// keys give 0 followed by the BIOS scan code on the next call.
int input_read_key(void);
