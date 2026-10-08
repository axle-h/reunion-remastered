// Unit Music (424a): background music on the port's audio layer.
// Backend 2 (the GUS MOD player) plays ANIM\<name>.SPD through audio.h,
// backend 1 the AdLib tune GRWAR\<name>.PIC (game/adlibmusic.h).
#pragma once

#include <stdint.h>

// 424a:000f: name is a Pascal string; 'main1', 'main2' and 'no' set MainThemeOn.
void Music_PlayTrack(const uint8_t *name);
// 424a:0095
void Music_StopMusic(void);
// 424a:00b2: chooses ds.Music_Backend from ds.Sound_DeviceType (GUS 1 -> modules,
// SB/AdLib 2, 3, 4, 6 -> AdLib; REUNION_MUSIC=mod|adlib overrides) and sets
// Game_MusicTrack. The AdLib path needs the heap (system_init).
void Music_InitMusicDevices(void);
// 424a:0137: starts the track selected by ds.Game_MusicTrack.
void Music_ApplyMusicTrack(void);
// 424a:017e
void Music_RestartMainTheme(void);
// 424a:0193
void Music_Init(void);
