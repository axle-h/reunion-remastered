// The web build's imports from the page (platform/web/library.js). Calls
// marked async return to the browser and resume (Asyncify).
#pragma once
#ifdef __EMSCRIPTEN__
// Async: returns the time (ms) of the browser's next animation frame, or the
// time after ms if none comes.
double web_wait_frame(int ms);
// Async: shows the click-to-play screen over the canvas until the player
// clicks it (or presses Enter or Space).
void web_click_to_play(void);
// Async: mounts the player's folder (platform/playerdir.h) from the browser's
// storage, where every change to it is kept.
void web_open_storage(void);
// Async: the browser's file picker, for a ZIP or ISO or a folder. Returns
// where the files chosen were copied, to free with SDL_free, or nullptr if
// the player cancelled.
char *web_choose_import(bool folder);
#endif
