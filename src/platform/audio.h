// Sound output, replacing the original GUS / Sound Blaster / AdLib hardware:
// one SDL audio stream mixing ProTracker music (ANIM\*.SPD, via libxmp), an
// emulated OPL2 (Nuked OPL3) for the AdLib tunes, and up to AUDIO_VOICES
// sound effects. All functions are safe to call from any thread
// and are no-ops (returning false / -1) when audio_init() failed.
#pragma once

#include <stddef.h>
#include <stdint.h>

enum : int {
    AUDIO_RATE = 44100,
    AUDIO_VOICES = 8,
};

// Opens the default playback device (initialising SDL's audio subsystem).
// Returns false if there is no usable device; the game then runs silent.
bool audio_init(void);
void audio_shutdown(void);

// Starts a module (e.g. "ANIM\\MAIN1.SPD"), replacing the current one.
// With loop false the music stops at the end of the song.
bool audio_play_music(const char *dos_path, bool loop);
void audio_stop_music(void);
// Silences (or restores) sample `sample` (1-based) of the current module;
// the mute is dropped when another module starts. False if there is none.
bool audio_music_mute_sample(int sample, bool mute);
// Restarts the current module from the beginning (keeping its loop mode).
bool audio_restart_music(void);
bool audio_music_playing(void);

// Plays 8-bit unsigned mono PCM (80h = silence) recorded at `rate` Hz. The
// data is copied. Returns the voice number, or -1. When all voices are busy
// the oldest one is cut off.
// Current module position (pattern order and row) as of the last mixed
// buffer. Returns false if no module has been started (or it was stopped);
// after a non-looping song ends the final position is kept. Either pointer
// may be null.
bool audio_music_position(int *order, int *row);
// True once the song has reached its end (non-looping), or has wrapped at
// least once (looping).
bool audio_music_ended(void);

int audio_play_sfx(const uint8_t *pcm, size_t len, int rate);
void audio_stop_sfx(int voice);
bool audio_sfx_playing(int voice);

// Stops music and all sound effects.
void audio_stop_all(void);

// Volumes from 0 (mute) to 1 (full, the default).
void audio_set_music_volume(float volume);
void audio_set_sfx_volume(float volume);

// The player's own volumes (0..1), applied on top of the game's; effects
// at 0 still play (silently), so the game's waits on them are unchanged.
// They survive audio_shutdown.
void audio_set_user_volumes(float music, float sfx);
// Modules started from now on play through libxmp's Amiga 500 (Paula)
// mixer with full stereo separation. Survives audio_shutdown.
void audio_set_amiga_mixer(bool on);
// True if the current module plays through the Paula mixer (tests).
bool audio_music_amiga_mixer(void);

// Pauses or resumes the output device; playback positions are kept.
void audio_set_paused(bool paused);

// OPL2 (AdLib) chip, emulated in OPL2-compatible mode and mixed at the music
// volume. Writes take effect from the mixer's current position on, 40 us
// apart (about the card's write delay; back-to-back writes would hide a key
// off between them).
void audio_opl_write(uint8_t reg, uint8_t value);

// A timer interrupt driven by the mixer: `tick` runs on the audio thread,
// with the audio lock held, every `divisor` cycles of the 1,193,182 Hz PIT
// clock (65536 = the BIOS's 18.2 Hz, as for INT 1Ch), interleaved with the
// OPL output by sample position. nullptr removes it.
void audio_set_timer(void (*tick)(void), uint32_t divisor);

// Holds the mixer (and so the timer callback) off while the caller changes
// state the callback reads. Recursive.
void audio_lock(void);
void audio_unlock(void);

// Renders `frames` stereo frames of interleaved S16 at AUDIO_RATE, advancing
// music and voices exactly like the device callback. For tests.
void audio_mix(int16_t *out, int frames);
// Advances `frames` like audio_mix without producing the sound: the module
// position, the timer (and so the AdLib player) and the sound effects'
// play state move on exactly as there, but the OPL is not synthesised.
// Unpaced autopilot runs, which discard the sound, use it (timing.c).
void audio_advance(int frames);
