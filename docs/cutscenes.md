# Cut-scene programs: CREDITS, INTRO, VICTORY

`GRWAR\CREDITS.PRG`, `INTRO.PRG` and `VICTORY.PRG` are separate Borland Pascal 7 real-mode programs (57-64 KB each), launched by `start.exe`. They share one skeleton (same units, same helper set); only the scene script differs. Evidence comes from Ghidra decompiles (own throwaway projects, DS = `1e62` INTRO, `1ce7` CREDITS, `1d3c` VICTORY; the DS is the constant loaded by the `System` init at the entry's first call, not the SS in the MZ header).

## 1. Launcher (`start.exe`, 2 KB Pascal)

```
Exec('grwar\credits.prg','');   Exec('grwar\intro.prg','');
repeat Exec('grwar\reunion.prg','sxca') until DosExitCode <> 15;   { 15 = restart game }
if DosExitCode = 10 then Exec('grwar\victory.prg','');
Halt(0)
```

- Order is CREDITS (publisher logo, title, credits roll), then INTRO, then the game. VICTORY runs only when REUNION.PRG exits with code 10.
- Exit codes of the cut-scenes are ignored. All three always end with `Halt(0)` (ESC and normal end both reach the same exit routine; its "error" argument comes from a variable that is never set).
- `sxca` is REUNION's command-line token. The cut-scenes read no command line.

## 2. Shared technical notes

**Video modes (no VESA, no 640x480x256).**
- Mode 13h (320x200x256) via `Intr($10)` AX=0013h, then `out 3C2h,(in 3CCh & 3Fh) | 40h` (400-line sync polarity, same as REUNION's `SetVideoMode13`). All animations and 320x200 pictures use it.
- Mode 12h (640x480x16, planar) is used only for the "strip" pictures (INTROP\*, CREDITS\A1-A5). The 640x480 image is stored as 5 PIC strips (4 x 640x100 + 1 x 640x80, colour indices 0-15). Each strip is decoded to a heap buffer, then written to the four planes with `out 3C4h,0102h/0202h/0402h/0802h` (map mask), with write mode 2 set via GC register 5. The 16 colours are mapped through the standard EGA attribute table (0,1,2,3,4,5,14h,7,38h-3Fh), so DAC entries 0-7 and 38h-3Fh carry the picture palette.
- Mode 03h on exit. Switching 13h to 12h and back happens between scenes with the screen black.
- The port needs 320x200 indexed, plus a 640x480 16-colour still (a 640x480 indexed surface is fine; letterbox/scale as for the rest of the game).

**Palette.**
- 6-bit VGA DAC. PIC 8-bit palettes are shifted down by 2.
- Program variables: current palette (768 bytes) and target palette (768 bytes). A picture load fills the target; the display step copies it to current.
- All fades are done in the DAC per vertical retrace (poll 3DAh bit 3): N steps = N+1 frames at about 70 Hz in mode 13h.
- Ops, with N steps, over entries 0-255:

| Op | Effect |
|---|---|
| FadeIn(N) | `c*i/N`, i = 0..N |
| FadeOut(N) | `c*(N-i)/N` |
| WhiteToPal(N) | `63-(63-c)*i/N` |
| PalToWhite(N) | reverse |
| PalToHalfWhite(N) / HalfWhiteToPal(N) | k runs N..N/2 or N/2..N (lightning flash) |
| WhiteStep(i,N) | one frame of whitening |
| `Strips` with fade-in -1 | all entries 1-255 set to white, then FadeIn(5), PalToHalfWhite(5), WhiteToPal(10) (flash cut) |

- Screens are blitted as a 64000-byte copy from an off-screen buffer.

**Anim playback (`.ANI`).**
- The whole file is loaded first (frame sizes in a per-program table at the start of DS). The table's frame count is what plays and can be lower than the file's: CREDITS ANIM3 165 of 300 (starting at frame 31, frames 1-30 are blank), ANIM4 41 of 42, VICTORY ANIM1 16 of 19.
- Frame 1 is drawn, then frames 2..N are drawn as deltas on the current screen, one per tick.
- `usePal=1`: load the palette of `<prog>\PAL<n>.PIC`, FadeOut(5) and black the current palette, draw frame 1, then FadeIn(5) of the new palette. `usePal=0`: reuse the current palette.
- Special cases in INTRO and VICTORY (same code): anim 12 first waits 18 frames; anim 16 does `Shake(3,0,1)` for frames <5; anim 19 whitens the palette at frames 51 and 52 (WhiteStep 4/5, 3/5). The "scripted" variant (anim 11) draws frames in the order given by a byte table at DS:23h (monitor blink).
- `Shake(amp,frames,count)` copies the buffer to the screen offset by `Random(amp)` in x and y, `count` times.
- `ScrollPic`: new picture scrolls vertically over the old one, 200 frames (1 line per vsync).
- Full-screen transitions never use the mouse.

**Timing.** Music and picture cues are driven by the module player position `(row, order)`.
- `WaitPos(row, order)` blocks until `(order,row) >= (cue)`. Both are 1-based in the GUS player: while row r of order index n plays it reports `(n+1, r+1)`, so `(1,1)` is the song start and `(7,48)` in a 7-order song is its last pattern.
- Position comes from the MOD player's variables (GUS player: ISR copies row `[11E0]` and order `[11E2]`; the SB player updates the same pair, with a 1-row offset).
- Order restarts at 0 for each loaded MOD (one MOD per scene).
- Frame pacing inside Anim (INTRO, VICTORY): a cumulative deadline in ms, `deadline += delay`, then wait until the 1 ms counter reaches it. The counter is the INT 1Ch hook (18.2 Hz BIOS ticks x 55 ms when no GUS, or the GUS 1 kHz PIT ISR). If already late, the frame is drawn at once. Otherwise it waits for vsync, then draws.
- CREDITS paces Anim frames with DOS `GetTime` (INT 21h/2Ch) in centiseconds when it uses the SB player: mark, then wait until `delay` cs has elapsed (effectively 55 ms granularity), then a retrace. With the GUS player it waits 2 retraces plus 1 instead, whatever the delay.
- A startup benchmark counts to 500,000 and measures elapsed time (INTRO only). If it took >= 331 ms the "flyby" scene uses the single-image fallback `planet2`; a modern machine takes the fast path (`planet` + `ship` layers).

**Input and abort.**
- Keys are only looked at inside `WaitPos`, and only while it actually waits; each program differs:
  - INTRO: a key (BIOS buffer head != tail) is consumed with INT 16h. ESC (0x011B) calls the exit routine. Any other key sets the skip flag. Every call checks the flag and becomes a no-op, so the rest of the block is skipped (a few calls are unguarded, e.g. a `Strips` already past its first cue still fades its picture in, and the 40-frame waits between `MON1-3`).
  - CREDITS: any key calls the exit routine (not consumed; the exit drains the buffer).
  - VICTORY: keys are ignored; it cannot be skipped.
- INTRO resets the flag before REBEL, BASE1, BASE2, TUNNEL and INTRO5 only, so a key in INTRO1 skips INTRO1-4 (their `PlayMod` calls too: INTRO1's music plays on) and a key in INTRO5 skips INTRO5-7.
- Exit routine: FadeOut(35), mode 3, stop music, drain the keyboard buffer, restore INT 1Ch, `Halt(0)`.
- The script is wrapped in `do ... while(true)` but always ends in the exit routine. No loop in practice.

**Sound.** MODs are standard 4-channel `M.K.` modules (one per scene, loaded fresh, about 180-420 KB each, see below). Device from `SAVE\SETUP.SPD` byte 0 (same file as REUNION):
- 1 = GUS: the GUS MOD player (unit about 90% instruction-identical to REUNION's `43d5`, hard-coded ports 220h/32xh, 1 kHz PIT ISR).
- 4/5 (AdLib/none): GUS code is initialised in the same way and `silent` is set.
- 2/3/6 (Sound Blaster): a software-mixing MOD player with object-oriented design, present only in the cut-scenes. SB detection scans ports 210h-280h. Mixing rate is 15,500 Hz (INTRO), 16,000 Hz (CREDITS), 18,000 Hz (VICTORY). Messages: "Soundblaster not found", "SbNoDma", "SB-Pro" in DS.
- AdLib music (`GRWAR\*.PIC`) is not used by the cut-scenes.
- INTRO and VICTORY also abort early with "Not enough memory to run the intro" if free memory is under about 500 KB.
- Port: one MOD player for all devices; expose `(order,row)` (ticks of the module sequencer) to the cue system. If music is disabled, the cues need a fallback (see section 6).

**Code shared with REUNION.PRG** (instruction-sequence match, mnemonics only):
- Borland `Crt` unit: 100%.
- GUS MOD player: about 90% (different build; sizes 10.9 KB vs 12.1 KB).
- EMS unit: about 89%.
- `System` runtime: about 69% (smaller RTL).
- `Dos`/`Intr`: about 52%.
- The picture loader and palette code are a trimmed copy of the graphics core (`405f`, about 57%): the same `SpidyGfx` decoder, and the same `SetVideoMode13` sequence.
- Everything else (script unit, Anim and Strips players, SB MOD player, timer unit) is cut-scene-only. The port's PIC/ANI decoders (`tools/pic.py`, `tools/ani.py`) already cover all the data.

**Localisation.** All visible text is baked into pictures or animation frames (for example `ATTACK.PIC`: "THE REBELS TAKE CONTROL OF PLANET EARTH."). The programs contain no text strings except error messages.

## 3. CREDITS.PRG

Files: `CREDITS\ANIM3/4/10-28.ANI`, `PAL3.PIC`, `PAL4.PIC` (raw 768-byte palettes), `PRESENTS`, `GAMEBY`, `REUNION`, `PAL10` (320x200), `A1-A5` (strips, 640x480), `STEAL2.MOD`. Unreferenced: `STEAL3.MOD`, `CHARMED.PIC`, `GRANDSLA.PIC` (the Grandslam logo is `ANIM3`).

Script (Anim = `usePal,row,order,n`; cues are `(order,row)`):

1. Mode 13h, `STEAL2.MOD` starts.
2. ANIM3 (300 frames, `PAL3`): animated Grandslam logo. Delay pattern 0/2 cs alternating. Wait `(2,32)`.
3. `PRESENTS.PIC`: flash in via white (WhiteToPal 20). `GAMEBY.PIC` at `(3,1)` (fade 70/5).
4. Strips `A1-A5` (mode 12h), fade-in 70 at `(4,1)`, fade-out 5 at `(3,43)`. ANIM4 (42 frames, `PAL4`, delay 5 cs) at `(4,45)`.
5. PalToHalfWhite(4), `REUNION.PIC` title via WhiteToPal(10) (mode 13h again).
6. `PAL10.PIC` (alien face) fades in at `(8,1)`. Then the credits roll: ANIM10..27 in pairs (ANIM28 is not played), each 30 frames of zooming text ("PROGRAMMED BY ...") over the face, 3 cs per frame (with a GUS: 3 retraces per frame), cues from `(9,22)` to `(18,23)`, one anim per cue.
7. Wait `(20,1)`, flash PalToWhite(5) / HalfWhiteToPal(5), wait `(37,63)`, exit.

## 4. INTRO.PRG

Files: `INTRO\ANIM1/3-8/10-16/18-24.ANI` (20 files, 320x200, 5-187 frames), `PAL*.PIC` and scene pictures (320x200), `INTROP\NxM.PIC` strips (110 files: 21 images x 5 strips, 640x80/100), MODs `INTRO1-7`, `REBEL`, `BASE1`, `BASE2`, `TUNNEL`.

Strip names: image `Nx` -> `INTROP\NxM.PIC`, M = 1..5 (`1x`..`21x`, plus `10_2x`). `Strips(fadeIn, inRow, inOrder, fadeOut, outRow, outOrder, name)`: wait for the out-cue, fade the previous picture out (N frames), switch to mode 12h if needed, decode, wait for the in-cue, snap in the 16-colour palette, FadeIn(N).

Block summary (`Pic(fadeIn,fadeOut,row,order,name)`: preload, wait the cue, fade the old picture out, show, fade in; `Anim(usePal,delay_ms,row,order,n)`):

| MOD | Content |
|---|---|
| `INTRO1` | `PAL20` (alien face), ANIM20, ANIM21 (face animation), fade out 30 at `(2,16)`, mode 12h strips `1x` (in `(2,30)`), `2x` (`(3,54)`), `3x` (`(4,36)`), `4x` (`(6,56)`); wait `(7,48)` |
| `INTRO2` | `5x` (`(2,1)`), `6x` (`(3,32)`); wait `(5,32)` |
| `INTRO3` | `7x` (300-frame slow fade), `8x`, `9x`, `10x`, `10_2x`, `11x`; wait `(7,14)` |
| `INTRO4` | `12x`..`14x`, `15x` with the white-flash cut; fade out 70 at `(8,40)`; wait `(8,60)` |
| `REBEL` | Mode 13h. `STARS`; "flyby" (`Flyby(1,6)`, see below); `PAL12`, ANIM12 (155 ms), ANIM13 (187 frames, 41 ms); `BASE` (`(6,24)`), `PAL15`, ANIM15 (42 ms), ANIM14 (50 ms); wait `(10,6)`, FadeOut 180 |
| `BASE1` | `BASE0` slow fade-in (255 frames) at `(1,10)`, `ScrollPic` to `BASE` at `(1,32)`; `PAL1`, ANIM1 twice, Shake x15, `BASE` with two lightning flashes (PalToWhite/HalfWhiteToPal 2) at `(5,20)` and `(5,24)`; `FOLYOSO` (corridor) `(5,40)`; Shake x20; ANIM3 (`PAL3`), `MUSZER`, `TRACE`, ANIM4/5/6/7/8 with `usePal`; flash; wait `(14,38)` |
| `BASE2` | `PAL11`, Shake at `(2,54)` and `(3,22)`; `PAL10` + ANIM10; `PAL11`; scripted ANIM11 (120 ms), `MON1`, `MON2`, `MON3` (40 frames each); wait `(7,6)` |
| `TUNNEL` | `PAL16`, ANIM16, ANIM23, ANIM24, ANIM18, ANIM19 (42 ms); palette pulse (WhiteStep x3); `ATTACK` (rebels take Earth) via WhiteToPal(100) at `(6,20)`; fade out 100 at `(6,64)` |
| `INTRO5` | Mode 12h: `16x`, `17x` (white-flash cut), `18x`; wait `(8,63)` |
| `INTRO6` | `19x`, `20x`, `21x`; wait `(4,24)` |
| `INTRO7` | Mode 13h: `PAL22`, ANIM22 (70 ms); `LEADER` (fade-in 140), `GABOR` (the protagonist, arms raised under Earth); fade out 70 at `(7,63)`, exit |

**Flyby (`Flyby(row,order)`):** 168 frames at 14 ms. `STARS` pans at 2 px/frame with wrap; the planet picture (keyed on colour 0) slides in from the left at 2.5 px/frame after frame 40; the ship picture at 4 px/frame after frame 88 (only in the fast variant). The stars move right as well (the view pans left).

Memory: five 64000-byte buffers (screen buffer, two layer buffers, strip buffer) and the loaded ANI.

## 5. VICTORY.PRG

Files: `VICTORY\ANIM1.ANI` (19 frames in the file, 16 played, 320x200), `PAL1`, `TX1`-`TX5` (text screens), `RETURN`, `EARTH`, `END`, `CR1`-`CR3` (credits pages), `ENDSEQ.MOD`. VICTORY uses the 1 ms timer (as INTRO), has no strip images, and the same anim code. Sample rate 18,000 Hz for the SB player.

Script (cues `(order,row)`):

1. Mode 13h, `ENDSEQ.MOD`.
2. `TX1` ("CONGRATULATIONS!") fade 40/1 at `(1,1)`; `TX2` ("YOU PROVED YOURSELF AS A BRILLIANT STRATEGIST, GOOD POLITICIAN AND AN EXCELLENT LEADER.") at `(2,1)`, fade 40/120.
3. `PAL1` (silhouettes, shown black first, fade-in 70 at `(3,25)`), ANIM1 (19 frames, 74 ms, cue `(3,30)`).
4. `RETURN` (the three soldiers in colour): preload, flash PalToHalfWhite(10) / WhiteToPal(10) around the cut.
5. `TX3` `(4,40)`, `EARTH` `(5,1)`, `END` ("THE END", `(6,1)`), `TX4`, `TX5` at `(8,1)`/`(8,32)`; wait `(8,50)`.
6. FadeOut 70. `CR1`, then `CR2` and `CR3` scroll up vertically (200 frames each, 1 line per vsync, `ScrollPic`) as the credits pages ("PROGRAMMED BY: ISTVAN KISS", ...).
7. Wait `(11,58)`, exit.

## 6. Port

`src/cutscene/`: `cutscene.c` holds the shared units (palette ops, WaitPos, Pic, Strips, Anim, Shake, exit) and the runner `cutscene_play`; `credits.c`, `intro.c` and `victory.c` hold each program's script and its own routines (ScrollPic, Flyby, the scripted anim, PicCue, ScrollCredits). Routine names and addresses: `ghidra/{credits,intro,victory}.symbols.txt`. `main.c` runs CREDITS and INTRO before the game (unless Skip intro is on in the port menu or an autopilot script runs) and VICTORY on exit code 10; `--cutscene=NAME` plays one and exits.

- Configuration: the GUS one (1-based positions without the SB row correction, the 1 ms timer, the CREDITS retrace pacing). SETUP.SPD device 4/5 plays the music silently, as the originals did.
- Music: libxmp through `platform/audio` (`audio_music_position`). Without an audio device a silent libxmp sequencer stepped by the retrace clock gives the same cue timeline.
- The 1 ms timer is derived from the retrace count, so anim pacing is identical in real time and in unpaced autopilot runs; the anim deadline wait ends at the first retrace after the deadline.
- When libxmp reaches the end of a song the wait ends (as the SB player's end flag did; with a GUS a cue past the end hung).
- Not ported: the memory check, the EMS buffer handling, the sound-device error paths, the slow-machine flyby (`PLANET2`).
- Tests: `tests/test_cutscene_{credits,intro,victory}.c` play each program headless (frame counts, sample frames, keys, window close) and check the DS frame tables against the PRGs.
