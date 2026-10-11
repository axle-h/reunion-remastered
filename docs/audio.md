# Audio

`src/platform/audio.{h,c}` replaces the game's sound drivers with one SDL3
audio stream (44.1 kHz, stereo, S16) opened with `SDL_OpenAudioDeviceStream`.
Its callback mixes, per chunk of 512 frames:

- **Music**: `ANIM\*.SPD` are plain 4-channel ProTracker modules
  ([formats/spd.md](formats/spd.md)), played with
  [libxmp](https://github.com/libxmp/libxmp) (pinned in `CMakeLists.txt`,
  system package used when present). `audio_play_music` loads into a new
  libxmp context outside the lock and swaps it in, so the mixer never waits on
  parsing. Looping or play-once is libxmp's loop count.
- **AdLib music**: an emulated OPL2,
  [Nuked OPL3](https://github.com/nukeykt/Nuked-OPL3) (LGPL-2.1, pinned
  commit in `CMakeLists.txt`) in its OPL2-compatible mode, resampled from
  49,716 Hz with `OPL3_GenerateResampled`, mixed at the music volume. It is
  only run once something has written to it. Registers are written with
  `audio_opl_write`, through Nuked's write buffer: each write lands 2 chip
  samples (40 us) after the previous one, about the delay loops the
  original runs after every write. Applying them at once broke the music:
  the player restarts a note with a key off and then a key on a few writes
  later, which the envelope generator then never saw, so percussive
  instruments sounded only once and most channels went quiet. The player ticks from `audio_set_timer`, a stand-in for
  a timer interrupt: the mixer counts PIT cycles per output frame and calls
  the tick between two samples, so OPL writes land where the interrupt
  would have fired, whatever the buffer size or frame rate.
- **Sound effects**: up to 8 voices of 8-bit unsigned mono PCM at their own
  rate (the `.SMP` files, [formats/smp.md](formats/smp.md), are 5000 to 20000
  Hz), resampled with linear interpolation (the GUS interpolated too; the SB
  did not). The data is copied; when all voices are busy the oldest is cut.
  The API takes raw PCM + rate, so it doesn't depend on the `.SMP` loader.

Shared state is guarded by the stream lock (`SDL_LockAudioStream`), which SDL
also holds during the callback. Memory is only freed outside the callback.
The timer callback runs under that lock; game code that changes what it
reads (the AdLib player starting or stopping) takes it with `audio_lock`, so
the tick never sees a half-started tune, as on the single-CPU original.
`audio_mix()` runs the same mixer directly, for tests (`tests/test_audio.c`
runs on `SDL_AUDIO_DRIVER=dummy` with the device paused).

## What it replaces

| Original | Port |
|---|---|
| `43d5` GUS driver: GF1 registers, PIT timer, MOD player (`43d5:2a26`), `.SMP` upload to GUS DRAM | libxmp + the voice mixer |
| `431a` + `474f`/SB path: `.SMP` through the Sound Blaster driver | the voice mixer |
| `SAVE\SETUP.SPD` card type / port / IRQ / DMA | not needed; SDL picks the device |
| `428e` AdLib (OPL2) player for `GRWAR\*.PIC`, `4265` loader | `game/adlib.c`, `game/adlibmusic.c` + the emulated OPL2 |

`Music::InitMusicDevices` (`424a:00b2`) picks the music from the card type in
`SAVE\SETUP.SPD`: modules on a GUS (type 1), AdLib tunes for the Sound
Blaster family and AdLib (2, 3, 4, 6), nothing otherwise. The shipped
`SETUP.SPD` says 2, so the AdLib tunes are the default. The port menu's
music setting (`config.music`, `docs/port.md`) overrides the choice for the
music only (sound effects keep following the setup file). The menu names
the choices after the cards, Sound Blaster (the AdLib tunes) and
UltraSound (the modules); `music = adlib` in the file still reads as Sound
Blaster. The cut-scenes only have modules, which the original played on
both cards; they play them unless the music is None.

`ANIM\NO.SPD` is the "no music" track: a one-pattern module whose only
event is a speed command (7.68 s of silence), though it still carries
MAIN1's samples. `MAIN1` and `FAILURE` fade in from near silence.

## AdLib music

12 of the 14 `GRWAR\*.PIC` files are tunes (`ALFA1` is the Sound Blaster
driver, see [sound-original.md](sound-original.md); `ALFA2` isn't a tune
either). `AdlibMusic::LoadMusicFile`
(`4265:000c`) reads one into a zeroed 31,700-byte buffer, reverses the bytes
and adds `2Fh` to each. Layout of the result:

| Offset | Contents |
|---|---|
| `000h` | 51 order bytes (pattern numbers), `FFh` ends the list (and loops to 0) |
| `033h` | 128 instruments × 12 bytes: `20h`, `40h`, `60h`, `80h`, `E0h` values as (op A, op B) pairs at +0, +2, +4, +6, +9; `C0h` at +8; high nibble of +11 = constant added to the `A0h` value |
| `633h` | patterns: 64 rows × 9 channels × 2 bytes (`480h` bytes) |

Op A is the carrier, op B the modulator. Cell byte 0: note 1..97 (note *n*
uses entry *n*−1 of the player's F-number/block table; entry 0 is silent),
`7Fh` key off, `80h` = byte 1 is an instrument number (no effect then), 0 =
nothing. Byte 1: `01h` pattern break; `1x`/`2x` slide the F-number's low
byte up/down by *x*+1; `Ax`/`Bx` set op A/B level to 4*x*; `Cx` sets op A
(and op B if the instrument is additive); anything else sets the speed to
*x*+1 ticks per row. The player (`428e`) runs from INT 1Ch at 18.2 Hz with
speed 2 by default; it rewrites changed frequencies and levels on every tick.

The port keeps the player's code-segment variables as a byte-exact image
(`428e:0743..` plus the first 140h bytes of `431a`), because of two overruns
it reproduces:

- Key off (`7Fh`) writes 156h bytes past the channel frequency table, into
  `431a:00BD..` (Sound's already-run driver loader). The OPL never sees a key
  off; notes ring until the next note on that channel.
- Notes above 97 index past the frequency table. Only junk past the end of
  a file has such notes, but `FAILURE` plays one such partial pattern
  (order 4).

Other quirks kept as is: the level scaling turns KSL 1 into 3 and vice
versa, the slides don't carry into the F-number's high bits, and an unknown
effect is a speed change. `ScanAdlibPorts` (OPL timer test with VGA retrace
waits) is not ported; the OPL is "found" at 388h whenever audio works.

The original allocates the tune buffer on its heap when it finds an AdLib.
The port keeps it outside the heap (`tune_buf` in
`game/adlibmusic.c`; `AdlibMusic_TuneBuf` stays nil), so the heap is the
same whatever music the game started with and the port menu can switch to
the AdLib tunes at any time. The original's heap with an AdLib was 31,700 bytes
smaller; the only effect is a little more room for the game.

`tests/test_adlib.c` checks the loader, plays every tune for a few seconds
and checks the output is audible, and that stopping silences it.
