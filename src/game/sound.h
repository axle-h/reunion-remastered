// Unit Sound (431a): sound effects on the port's audio layer. The original
// kept one SB/GUS "voice" plus a few preloaded buffers; here the buffers are
// decoded .SMP files and the voice is one audio.h voice. The SB driver's
// status word (ds.Sound_SbStatus, nonzero = playing) is kept up to date so
// code polling it behaves as before.
#pragma once

#include <stdint.h>

// 431a:09c1: reads SAVE\SETUP.SPD (device type + 3 words), opens the audio
// device and sets ds.Sound_Enabled / ds.Game_SoundFx. A missing setup file
// selects device type 2, the floppy's Sound Blaster (the original took 1, a
// GUS); an imported game has none (platform/import.h).
void Sound_Init(void);
// True once Sound_Init opened an audio device (stands in for GUS/SB detection).
bool Sound_DeviceOpen(void);

// 431a:0192: ds.Sound_SbStatus <> 0. Refreshes the status word first.
bool Sound_IsSbPlaying(void);
// Same as above under the name used in the docs.
bool Sound_IsPlaying(void);

// 431a:01a8 / 431a:01e6: stop the voice and drop the loaded sample.
void Sound_StopAndFreeSample(void);
void Sound_FreeSampleIfIdle(void);
// 431a:0209: name is a DOS path. Loads the .SMP (from offset,
// size 0 = whole file) as the current sample; the file offset/size are only
// ever 0 in the game.
void Sound_LoadSample(const char *name, uint16_t offset, uint16_t size);
// 431a:031a: PlaySfx then StartSample.
void Sound_PlayButtonSfx(const char *name);
// 431a:0353: loads SOUND\<name>.SMP (does NOT start it; see StartSample).
void Sound_PlaySfx(const char *name);
// 431a:03a8: starts the loaded sample.
void Sound_StartSample(void);
// 431a:03e8 / 431a:0424: war-sound buffer. Alloc waits for playback to end.
void Sound_AllocWarSndBuf(void);
void Sound_FreeWarSndBuf(void);
// 431a:0468: SOUND2\WARSND<n>.SMP. 431a:05ca: SOUND3\GRSND<n>.SMP.
void Sound_PlayWarSnd(int16_t n);
void Sound_PlayGrSnd(int16_t n);
// 431a:059e
void Sound_StopSbVoice(void);
// 431a:0712 / 431a:08aa / 431a:0903: SELECT1..3.SMP, index 1..3.
void Sound_PreloadSelectSfx(void);
void Sound_FreeSelectSfx(void);
void Sound_PlaySelectSfx(int32_t index);
