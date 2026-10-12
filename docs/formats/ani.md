# `.ANI` animations ("SpidyAnim")

All 98 `.ANI` files in the DOS release use this format: a sequence of
independently allocated frames, each with its own small header and a
delta-RLE pixel stream. There is no frame count, palette, position or timing
in the file; those come from tables in `REUNION.PRG` (or from the separate
intro/credits programs). Decoder: `tools/ani.py`.

| Files | Frames | Size | Played by |
|---|---|---|---|
| `ANIM/MAIN*.ANI` (16; no `MAIN7`) | 3–71 | 44×66, 24×22, 26×24, 100×49, 320×200 | `REUNION.PRG`, main screen and cut-scenes |
| `WAR/WAR/SA*.ANI` (25) | 3–136 | 42×43 … 160×151 | `REUNION.PRG`, space battle |
| `WAR/GRWAR/GRANIM*.ANI` (14) | 3–88 | 58×80 | `REUNION.PRG`, ground battle |
| `INTRO/ANIM*.ANI` (22) | 5–187 | 320×200 | intro program |
| `CREDITS/ANIM*.ANI` (21) | 30–300 | 320×200 | `CREDITS.PRG` |
| `VICTORY/ANIM1.ANI` | 19 | 320×200 | end sequence |

## Layout

The file is just frames back to back until end of file:

| Offset | Size | Contents |
|---|---|---|
| 0 | 2 | `n`: length of the pixel stream (little-endian) |
| 2 | 9 | Magic `SpidyAnim` |
| 11 | 2 | Width |
| 13 | 2 | Height |
| 15 | n | Pixel stream |
| 15 + n | | Next frame |

The loader (`405f:0af8`) reads the word, allocates `n + 13` bytes and reads
the magic, size and stream into that block; the drawing routine reads width and
height at block offsets 9 and 11. The magic is never checked. Every frame in
every file has the same size as frame 1, and every stream ends exactly at its
last byte.

## Pixel stream

Decoded by `3c64:057c` into a linear buffer (no stride): a counter starts at
`width × height`, every opcode subtracts its pixel count, and decoding stops
when the 16-bit counter is exactly 0.

| Byte | Meaning |
|---|---|
| `00–7F` | One literal pixel of that value |
| `80` `lo hi` | Skip a 16-bit count of pixels (leave them unchanged) |
| `81–BF` | Skip `byte & 3Fh` pixels |
| `C0` `lo hi` `v` | Run of a 16-bit count of pixel `v` |
| `C1–FF` `v` | Run of `byte & 3Fh` pixels of value `v` |

A colour offset is added to every written value (literal and run); all
`REUNION.PRG` callers pass `40h`. Literals can therefore only store values
`0–7Fh`, and every file in the release stays within `0–7Fh` (screen colours
`40h–BFh`). Frame 1 of every file covers every pixel; later frames are mostly
skips.

## What a frame is a delta of

The format doesn't say; it depends on how the game draws it:

- **Delta** (previous frame): `3c64:0516` draws frame N alone, onto the screen
  or a buffer that still holds the previous frame. Used for `GRANIM`, the
  `MAIN` main-screen and cut-scene animations, and (judging by the output) all
  intro/credits/victory files.
- **Keyed** (frame 1): `3c64:0496` draws frame 1 and then frame N into a
  buffer every time. Used for `SA*.ANI` (space battle) and `MAIN17.ANI` (disk
  menu). Decoding these as plain deltas leaves stale pixels behind.

`tools/ani.py` picks the mode from the file name (`--mode` overrides). A
statistical guess (fewest literal pixels equal to the reference) was tried and
is unreliable: it picked wrong for `GRANIM1`, `CREDITS/ANIM3` and `INTRO/ANIM3`.

If a frame couldn't be allocated (`MaxAvail` too small), the loader skips it
and stores size 0 / `nil` (never in the port, whose heap always has room). `0516` then reports failure; `0496` falls back to
the nearest earlier loaded frame.

## Palette

None in the file. The animation is drawn over a picture loaded with the same
colour offset `40h`, so ANI value `v` is colour `v` of that picture's palette.
`tools/ani.py` takes the palette from:

| Files | Palette | Evidence |
|---|---|---|
| `INTRO`, `CREDITS`, `VICTORY` `ANIMn` | `PALn.PIC` in the same directory, else the nearest lower `n` | frame 1 equals `PALn.PIC`'s pixels |
| `MAIN8`, `MAIN9` / `MAIN10` / `MAIN13` / `MAIN14` | `PICS/PIC9` / `PIC1` / `PIC10` / `PIC2` | `2e4b:0044` loads `pics\pic<n>` then plays them; frame 1 equals the picture |
| `MAIN15`, `MAIN16` | `GRAFIKA/DEATH2`, `DEATH` | frame 1 matches; caller not found |
| `MAIN17` | `GRAFIKA/DISK.PIC` | `1a3e:0ee5` |
| other `MAIN` | `GRAFIKA/MAIN.PIC` | assumed (main screen) |
| `SA*` | `WAR/SPWAR.PIC` | `20d4` battle setup |
| `GRANIM*` | `WAR/GRWAR.PIC` | `1537:0029` |

`--palette` overrides. Without a match the PNGs use a grey ramp.

## Position and timing

All from tables in the data segment (`4a81`), indexed by the file number:

**`MAIN<n>`**, 7-byte records at `DS:55D7 + 7n`:
`frames: byte; width: word; height: byte; x: word; y: byte`. MAIN1–6 are
44×66 at (83, 49); MAIN11 24×22 at (40, 73); MAIN12 26×24 at (14, 73); MAIN17
100×49 at (166, 150); the rest 320×200 at (0, 0). The table says MAIN13 has 41
frames; the file has 68, so the last 27 are never shown. Entry 7 (14 frames,
44×66) has no file.

- `3abd:0c70` (main-screen idle animations): waits a random 100–299 calls,
  then copies the screen rectangle to a buffer, draws one frame per call into
  it, and copies it back minus a 1-pixel border. At the end it picks another
  animation at random.
- `3abd:0e5d` (cut-scenes, called from `2e4b:0044`): draws the next frame
  straight to the screen, looping; the caller waits `405f:06ff(n)` between
  frames (n = 10, 6 or 2 depending on the scene).

**`SA<n>`**, 5-byte records at `DS:555B + 5n`:
`frames, width, height, x, y` (bytes). Most are about 160×150 at (160, 49).
Frame order comes from `WAR/ANIM/ANIM.DEF`: 25 scripts × 140 steps × 4 bytes
`(anim, frame, sound, delay)`, loaded into the 560-byte buffer at `DS:7E44`.
The player (`20d4`) draws `frame` of `anim` keyed into a buffer, blits it to
(x, y), plays `sound` if non-zero and waits `delay` ticks.

**`GRANIM<n>`**, 5-byte records at `DS:5651 + 5n`:
`frames, delay, loops, ?, ?` (bytes). `3abd:0f19` advances one frame every
`delay` calls, plays the whole animation `loops` times, then asks a callback
(`DS:928E`) for the next animation. Frames are 58×80 in a persistent buffer;
the visible 58×79 below its first row is blitted to (3, 52).

The intro, credits and victory programs aren't part of `REUNION.PRG`, so their
timing is unknown. `tools/ani.py` writes 100 ms per frame (`--delay`).

## Code

| Address | Function |
|---|---|
| `3c64:0010` / `011e` | Load / free `war\war\sa<n>.ani` |
| `3c64:019c` / `028d` | Load / free `anim\main<n>.ani` |
| `3c64:031d` / `0415` | Load / free `war\grwar\granim<n>.ani` |
| `3c64:0496` | Draw frame 1 then frame N (keyed) |
| `3c64:0516` | Draw frame N; returns false if it isn't loaded |
| `3c64:057c` | Frame decoder (`retf 0Ah`: frame, dest, colour offset) |
| `405f:0a78` | Open file (halts with "Cannot Open") |
| `405f:0af8` | Read one size-prefixed chunk into a new heap block |
| `405f:0bd6` | Close file |

Loaded animations live in a 841-byte (`349h`) record pointed to by `DS:7E40`:
a loaded flag, then 140 far pointers and 140 block sizes. Only one animation
is loaded at a time; the loaders do nothing while the flag is set. Animations
can't have more than 140 frames (`CREDITS/ANIM3` has 300, so its player must
be different).

## Output of `tools/ani.py`

For each file, a directory with `NNN.png` per frame (indexed, full frames),
`sheet.png` (all frames in a grid) and `anim.png` (animated PNG). Batch mode
mirrors the source tree under `tools/out/ani/`.
