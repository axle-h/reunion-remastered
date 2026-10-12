// Unit Music (424a): background music on the port's audio layer.
// Backend 2 (the GUS MOD player) plays ANIM\<name>.SPD through audio.h,
// backend 1 the AdLib tune GRWAR\<name>.PIC (game/adlibmusic.h).
#pragma once

#include <stdbool.h>
#include <stdint.h>

// 424a:000f: 'main1', 'main2' and 'no' set MainThemeOn.
void Music_PlayTrack(const char *name);
// 424a:0095
void Music_StopMusic(void);
// 424a:00b2: chooses ds.Music_Backend from ds.Sound_DeviceType (GUS 1 -> modules,
// SB/AdLib 2, 3, 4, 6 -> AdLib; config.music overrides it) and sets
// Game_MusicTrack.
void Music_InitMusicDevices(void);
// Port addition: the music hardware changed (port menu). Stops the music,
// chooses again as Music_InitMusicDevices does and starts the last track on
// the new hardware. Before Music_InitMusicDevices it does nothing (that
// reads the config itself).
void Music_SwitchDevice(void);
// Port deviation (docs/port-bugs.md #1): while held, CHOISE.SPD plays without
// the "choose your hero" voice, on whatever hardware the music switches to;
// letting go restarts the module with the voice.
void Music_HoldHeroVoice(bool hold);
// 424a:0137: starts the track selected by ds.Game_MusicTrack.
void Music_ApplyMusicTrack(void);
// 424a:017e
void Music_RestartMainTheme(void);
// 424a:0193
void Music_Init(void);
