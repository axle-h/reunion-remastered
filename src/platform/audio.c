#include "platform/audio.h"

#include <SDL3/SDL.h>
#include <opl3.h>
#include <xmp.h>

#include "platform/autopilot.h"
#include "platform/fs.h"

enum : int {
    CHUNK_FRAMES = 512,
    FRAME_BYTES = 2 * sizeof(int16_t),
    GAIN_ONE = 256,
};

// PIT input clock; the timer source fires every `divisor` of its cycles.
static constexpr uint64_t PIT_HZ = 1193182;

typedef struct {
    uint8_t *data; // owned; freed outside the lock when the slot is reused
    size_t len;
    uint64_t pos;  // 16.16 fixed point, in source samples
    uint32_t step; // source samples per output frame, 16.16
    uint32_t serial;
    bool active;
} Voice;

// Everything below is shared with the device callback and guarded by the
// stream lock (SDL holds it while the callback runs).
static SDL_AudioStream *stream;
static xmp_context music;
static bool music_playing;
// Samples silenced by audio_music_mute_sample, with their original volumes.
enum : int { MAX_MUTABLE = 32, MAX_SUBS = 4 };
static bool muted[MAX_MUTABLE][MAX_SUBS];
static int saved_vol[MAX_MUTABLE][MAX_SUBS];
static bool music_loop;
static bool music_ended;
static int music_gain = GAIN_ONE;
static int sfx_gain = GAIN_ONE;
// The player's volumes (port menu), on top of the game's; they outlive
// audio_shutdown, as does the mixer choice.
static int user_music_gain = GAIN_ONE;
static int user_sfx_gain = GAIN_ONE;
static Voice voices[AUDIO_VOICES];
static uint32_t voice_serial;
// OPL2: generated only once something has been written to it.
static opl3_chip opl;
static bool opl_active;
// Timer source: timer_tick runs whenever timer_pos reaches timer_period.
// Both count PIT cycles times AUDIO_RATE, so one output frame is PIT_HZ.
static void (*timer_tick)(void);
static uint64_t timer_pos;
static uint64_t timer_period;

// The OPL and the timer, interleaved frame by frame so that register writes
// from the tick land at the sample where its interrupt would have fired.
// Without `render` only the timer runs (audio_advance).
static void mix_opl(int32_t *acc, int frames, bool render)
{
    for (int i = 0; i < frames; i++) {
        if (timer_tick) {
            timer_pos += PIT_HZ;
            if (timer_pos >= timer_period) {
                timer_pos -= timer_period;
                timer_tick();
            }
        }
        if (opl_active && render) {
            int16_t s[2];
            OPL3_GenerateResampled(&opl, s);
            acc[2 * i] += s[0] * music_gain / GAIN_ONE * user_music_gain / GAIN_ONE;
            acc[2 * i + 1] += s[1] * music_gain / GAIN_ONE * user_music_gain / GAIN_ONE;
        }
    }
}

// Mixes `frames` into `out`; with out == nullptr only advances everything
// the game can observe (module position, timer, which voices still play).
static void mix_chunk(int16_t *out, int frames)
{
    int32_t acc[CHUNK_FRAMES * 2] = {};

    if (music_playing) {
        int16_t mod[CHUNK_FRAMES * 2];
        // loop 0 = forever, 1 = play the song once.
        if (xmp_play_buffer(music, mod, frames * FRAME_BYTES, music_loop ? 0 : 1) < 0) {
            music_playing = false;
            music_ended = true;
        } else {
            for (int i = 0; i < frames * 2; i++)
                acc[i] = mod[i] * music_gain / GAIN_ONE * user_music_gain / GAIN_ONE;
        }
    }

    if (opl_active || timer_tick)
        mix_opl(acc, frames, out != nullptr);

    for (int v = 0; v < AUDIO_VOICES; v++) {
        Voice *vc = &voices[v];
        if (!out && vc->active) {
            // As the loop below: stops at the first frame past the end.
            uint64_t last = vc->pos + (uint64_t)vc->step * (uint64_t)(frames - 1);
            if ((size_t)(last >> 16) >= vc->len)
                vc->active = false;
            else
                vc->pos += (uint64_t)vc->step * (uint64_t)frames;
            continue;
        }
        for (int i = 0; vc->active && i < frames; i++) {
            size_t idx = (size_t)(vc->pos >> 16);
            if (idx >= vc->len) {
                vc->active = false;
                break;
            }
            // Linear interpolation, roughly what the GUS did in hardware.
            int s0 = vc->data[idx] - 128;
            int s1 = idx + 1 < vc->len ? vc->data[idx + 1] - 128 : s0;
            int frac = (int)(vc->pos & 0xffff);
            int s = s0 * 256 + (s1 - s0) * frac / 256;
            s = s * sfx_gain / GAIN_ONE * user_sfx_gain / GAIN_ONE;
            acc[2 * i] += s;
            acc[2 * i + 1] += s;
            vc->pos += vc->step;
        }
    }

    for (int i = 0; out && i < frames * 2; i++)
        out[i] = (int16_t)SDL_clamp(acc[i], INT16_MIN, INT16_MAX);
}

static void mix_locked(int16_t *out, int frames)
{
    while (frames > 0) {
        int n = SDL_min(frames, CHUNK_FRAMES);
        mix_chunk(out, n);
        if (out)
            out += n * 2;
        frames -= n;
    }
}

static void SDLCALL feed(void *userdata, SDL_AudioStream *s, int additional, int total)
{
    (void)userdata;
    (void)total;
    int16_t buf[CHUNK_FRAMES * 2];
    int frames = (additional + FRAME_BYTES - 1) / FRAME_BYTES;
    while (frames > 0) {
        int n = SDL_min(frames, CHUNK_FRAMES);
        mix_locked(buf, n);
        SDL_PutAudioStreamData(s, buf, n * FRAME_BYTES);
        frames -= n;
    }
}

bool audio_init(void)
{
    // Scripted runs are never heard (see below), and the game takes another
    // path without a sound device: the dummy device makes them replay the
    // same on machines without one. SDL_AUDIO_DRIVER still wins.
    if (autopilot_active())
        SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, "dummy", SDL_HINT_DEFAULT);
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Audio: %s", SDL_GetError());
        return false;
    }
    const SDL_AudioSpec spec = { .format = SDL_AUDIO_S16, .channels = 2, .freq = AUDIO_RATE };
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed, nullptr);
    if (!stream) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Audio device: %s", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    OPL3_Reset(&opl, AUDIO_RATE);
    opl_active = false;
    timer_tick = nullptr;
    // Scripted runs mix from the frame clock only (timing.c), so they replay
    // exactly: the device must not pull audio on its own in real time.
    if (!autopilot_active())
        SDL_ResumeAudioStreamDevice(stream);
    return true;
}

void audio_shutdown(void)
{
    if (!stream)
        return;
    // Destroying the stream stops the callback, so no locking from here on.
    SDL_DestroyAudioStream(stream);
    stream = nullptr;
    if (music)
        xmp_free_context(music);
    music = nullptr;
    music_playing = false;
    timer_tick = nullptr;
    opl_active = false;
    for (int v = 0; v < AUDIO_VOICES; v++) {
        SDL_free(voices[v].data);
        voices[v] = (Voice){};
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

bool audio_play_music(const char *dos_path, bool loop)
{
    if (!stream)
        return false;

    size_t size = 0;
    uint8_t *data = fs_load(dos_path, &size);
    if (!data) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Music %s: not found", dos_path);
        return false;
    }
    // Load into a fresh context outside the lock so the mixer never waits on
    // file parsing; libxmp copies what it needs from the buffer.
    xmp_context ctx = xmp_create_context();
    bool ok = ctx && xmp_load_module_from_memory(ctx, data, (long)size) == 0
              && xmp_start_player(ctx, AUDIO_RATE, 0) == 0;
    SDL_free(data);
    if (!ok) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Music %s: not a module", dos_path);
        if (ctx)
            xmp_free_context(ctx);
        return false;
    }

    SDL_LockAudioStream(stream);
    xmp_context old = music;
    music = ctx;
    SDL_zeroa(muted);
    music_loop = loop;
    music_playing = true;
    music_ended = false;
    SDL_UnlockAudioStream(stream);

    if (old)
        xmp_free_context(old);
    return true;
}

bool audio_music_mute_sample(int sample, bool mute)
{
    if (!stream)
        return false;
    bool ok = false;
    SDL_LockAudioStream(stream);
    struct xmp_module_info mi;
    if (music) {
        xmp_get_module_info(music, &mi);
        // Samples are numbered from 1, as in the tracker; in a ProTracker
        // module sample n is instrument n. Silencing it is done through the
        // instrument volume, saving the original to restore on unmute.
        if (sample >= 1 && sample <= mi.mod->ins && sample <= MAX_MUTABLE) {
            struct xmp_instrument *xi = &mi.mod->xxi[sample - 1];
            for (int k = 0; k < xi->nsm && k < MAX_SUBS; k++) {
                if (mute && !muted[sample - 1][k]) {
                    saved_vol[sample - 1][k] = xi->sub[k].vol;
                    muted[sample - 1][k] = true;
                    xi->sub[k].vol = 0;
                } else if (!mute && muted[sample - 1][k]) {
                    xi->sub[k].vol = saved_vol[sample - 1][k];
                    muted[sample - 1][k] = false;
                }
            }
            ok = true;
        }
    }
    SDL_UnlockAudioStream(stream);
    return ok;
}

bool audio_restart_music(void)
{
    if (!stream)
        return false;
    SDL_LockAudioStream(stream);
    bool ok = music != nullptr;
    if (ok) {
        xmp_restart_module(music);
        music_playing = true;
        music_ended = false;
    }
    SDL_UnlockAudioStream(stream);
    return ok;
}

void audio_stop_music(void)
{
    if (!stream)
        return;
    SDL_LockAudioStream(stream);
    xmp_context old = music;
    music = nullptr;
    music_playing = false;
    music_ended = false;
    SDL_UnlockAudioStream(stream);
    if (old)
        xmp_free_context(old);
}

bool audio_music_playing(void)
{
    if (!stream)
        return false;
    SDL_LockAudioStream(stream);
    bool playing = music_playing;
    SDL_UnlockAudioStream(stream);
    return playing;
}

bool audio_music_position(int *order, int *row)
{
    if (!stream)
        return false;
    SDL_LockAudioStream(stream);
    bool ok = music != nullptr;
    if (ok) {
        struct xmp_frame_info fi;
        xmp_get_frame_info(music, &fi);
        if (order)
            *order = fi.pos;
        if (row)
            *row = fi.row;
    }
    SDL_UnlockAudioStream(stream);
    return ok;
}

bool audio_music_ended(void)
{
    if (!stream)
        return false;
    SDL_LockAudioStream(stream);
    bool ended = music_ended;
    if (music && !ended) {
        struct xmp_frame_info fi;
        xmp_get_frame_info(music, &fi);
        ended = fi.loop_count > 0;
    }
    SDL_UnlockAudioStream(stream);
    return ended;
}

int audio_play_sfx(const uint8_t *pcm, size_t len, int rate)
{
    if (!stream || !pcm || len == 0 || rate <= 0)
        return -1;
    uint8_t *copy = SDL_malloc(len);
    if (!copy)
        return -1;
    SDL_memcpy(copy, pcm, len);

    SDL_LockAudioStream(stream);
    int slot = 0;
    for (int v = 0; v < AUDIO_VOICES; v++) {
        if (!voices[v].active) {
            slot = v;
            break;
        }
        if (voices[v].serial - voices[slot].serial > UINT32_MAX / 2)
            slot = v; // older (serials wrap around)
    }
    uint8_t *old = voices[slot].data;
    voices[slot] = (Voice){
        .data = copy,
        .len = len,
        .step = (uint32_t)(((uint64_t)rate << 16) / AUDIO_RATE),
        .serial = voice_serial++,
        .active = true,
    };
    SDL_UnlockAudioStream(stream);

    SDL_free(old);
    return slot;
}

void audio_stop_sfx(int voice)
{
    if (!stream || voice < 0 || voice >= AUDIO_VOICES)
        return;
    SDL_LockAudioStream(stream);
    voices[voice].active = false;
    SDL_UnlockAudioStream(stream);
}

bool audio_sfx_playing(int voice)
{
    if (!stream || voice < 0 || voice >= AUDIO_VOICES)
        return false;
    SDL_LockAudioStream(stream);
    bool playing = voices[voice].active;
    SDL_UnlockAudioStream(stream);
    return playing;
}

void audio_stop_all(void)
{
    audio_stop_music();
    if (!stream)
        return;
    SDL_LockAudioStream(stream);
    for (int v = 0; v < AUDIO_VOICES; v++)
        voices[v].active = false;
    SDL_UnlockAudioStream(stream);
}

static int to_gain(float volume)
{
    return (int)(SDL_clamp(volume, 0.0f, 1.0f) * GAIN_ONE + 0.5f);
}

void audio_set_music_volume(float volume)
{
    if (!stream)
        return;
    SDL_LockAudioStream(stream);
    music_gain = to_gain(volume);
    SDL_UnlockAudioStream(stream);
}

void audio_set_sfx_volume(float volume)
{
    if (!stream)
        return;
    SDL_LockAudioStream(stream);
    sfx_gain = to_gain(volume);
    SDL_UnlockAudioStream(stream);
}

void audio_set_user_volumes(float music, float sfx)
{
    if (stream)
        SDL_LockAudioStream(stream);
    user_music_gain = to_gain(music);
    user_sfx_gain = to_gain(sfx);
    if (stream)
        SDL_UnlockAudioStream(stream);
}

void audio_set_paused(bool paused)
{
    if (!stream)
        return;
    if (paused)
        SDL_PauseAudioStreamDevice(stream);
    else
        SDL_ResumeAudioStreamDevice(stream);
}

void audio_lock(void)
{
    if (stream)
        SDL_LockAudioStream(stream);
}

void audio_unlock(void)
{
    if (stream)
        SDL_UnlockAudioStream(stream);
}

void audio_opl_write(uint8_t reg, uint8_t value)
{
    if (!stream)
        return;
    // The lock is recursive, so this also works from the timer callback.
    // Buffered: Nuked applies each write 2 chip samples (40 us) after the
    // previous one, like the delay loops after every write on the real
    // card. Applied all at once, a key off followed by a key on (how the
    // player restarts a note) is never seen by the envelope generator, and
    // notes on percussive instruments don't sound again.
    SDL_LockAudioStream(stream);
    OPL3_WriteRegBuffered(&opl, reg, value);
    opl_active = true;
    SDL_UnlockAudioStream(stream);
}

void audio_set_timer(void (*tick)(void), uint32_t divisor)
{
    if (!stream)
        return;
    SDL_LockAudioStream(stream);
    timer_tick = tick;
    timer_period = (uint64_t)(divisor ? divisor : 65536) * AUDIO_RATE;
    timer_pos = 0;
    SDL_UnlockAudioStream(stream);
}

void audio_mix(int16_t *out, int frames)
{
    if (!stream) {
        SDL_memset(out, 0, (size_t)frames * FRAME_BYTES);
        return;
    }
    SDL_LockAudioStream(stream);
    mix_locked(out, frames);
    SDL_UnlockAudioStream(stream);
}

void audio_advance(int frames)
{
    if (!stream)
        return;
    SDL_LockAudioStream(stream);
    mix_locked(nullptr, frames);
    SDL_UnlockAudioStream(stream);
}
