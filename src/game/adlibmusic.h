// Unit AdlibMusic (4265): loads the AdLib tunes (GRWAR\<name>.PIC) and hands
// them to the 428e player (game/adlib.h).
#pragma once

#include <stdint.h>

// 4265:000c: name is a Pascal string. Reads GRWAR\<name>.PIC into the tune
// buffer (the port's own, not on the emulated heap) and undoes the obfuscation: reverse the bytes, add
// 2Fh to each. Files over 31,700 bytes are ignored (the buffer keeps the old
// tune). Does nothing without an AdLib.
void AdlibMusic_LoadMusicFile(const uint8_t *name);
// 4265:01d6: stop, load, start.
void AdlibMusic_PlayMusic(const uint8_t *name);
// 4265:022d
void AdlibMusic_StopAdlib(void);
// 4265:0242: detects the AdLib (the original also allocated the tune buffer
// on its heap the first time).
void AdlibMusic_InitMusic(void);
// The tune buffer, for tests.
const uint8_t *AdlibMusic_Tune(void);
// 4265:027c
void AdlibMusic_Init(void);
// 4265:0290 / 4265:0296: 428e StartPlayer (with TuneBuf) / StopPlayer.
void AdlibMusic_StartAdlibTune(void);
void AdlibMusic_StopAdlibTune(void);
