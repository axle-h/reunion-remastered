# Porting notes

## The data segment (`src/game/dseg.h`)

The original's globals are one C global, `DSeg ds`, generated from the
symbols: DS `0000–a543` of `REUNION.PRG` (Ghidra segment `4a81`), field by
field in the original's order.

- Every variable named in `ghidra/symbols.txt` is a field, called
  `Unit_Name` (`Game::Credits` → `ds.Game_Credits`). Names without a unit
  are Borland's own and get `System_`. The game code reaches every global
  by name; the original's DS offsets are only in the comments and the field
  table.
- Records come from `ghidra/types.pas` with the Pascal layout (packed, no
  alignment): they are the game's data formats, the typed constants, saved
  and kept in heap blocks. Pascal arrays become 0-based C arrays: the
  comment gives the Pascal bounds, and the C index is the Pascal index minus
  the lower bound (`Galaxy::SunNames[3]` → `ds.Galaxy_SunNames[2]`).
- `boolean` is `uint8_t`, since the original may store any byte value.
  Strings are `PStrN` / `PString` (length byte + chars). `real` is the
  6-byte `Real48`. `pointer` and `^T` are host pointers (`uint8_t *`, `T *`);
  procedural types (`TProc = procedure;` in `types.pas`) are C function
  pointers. `farptr` marks the few real-mode far pointer values nothing uses
  any more (the Borland heap's variables, interrupt vectors).
- `DSeg` keeps the original's globals in order with no padding: in a dozen
  places the original reads or writes past an array into the next globals
  (alien-talk lines into `Surface::Mode`, race 0/1 records before
  `Races::Races`, index 0 of 1-based tables), and those stay the same
  bytes. Only pointer fields differ in size from the original. Bytes nobody
  named are `_pad_XXXX` arrays.

`dseg_init()` (`src/game/dseg.c`) does what the original's startup does:
DS gets the typed constants, DS `0000–759d`, and the rest is zero. The
values are `ds_initial` in `src/game/dsinit.c`, a C initializer by field
name that the generator reads once from the load image of
`GRWAR\REUNION.PRG` (the data segment starts at file offset
`header paragraphs * 16 + 3A81h * 16`). The far pointers into DS that DOS
relocates (the galaxy and menu tables) are host pointers there
(`&ds.Galaxy_AmnestyBodies[0]`), and the bytes nobody named are in the
`_pad_XXXX` arrays, so every byte matches the original's. The game needs
no executable at run time, only the data files.

The field table (`src/game/dsfields.c`, generated too) lists every field's
original offset, its offset in `DSeg`, its size and whether it holds
pointers. `dseg_orig(offset)` uses it to give the port's byte for an
original DS offset.

### Regenerating

`dseg.h`, `dsfields.c` and `dsinit.c` are generated; don't edit them. After
adding names or types to `symbols.txt` or `types.pas`, run (`dsinit.c` needs
`vendor/Reunion_DOS_EN/GRWAR/REUNION.PRG`, or `--exe`; without it the file
is left as it is):

```sh
python3 -I tools/gen_dseg.py
```

The generator never stops on bad entries. It prints a warning and:

- keeps a symbol whose type it can't parse as an untyped label;
- comments out a symbol that overlaps a bigger (or typed) one;
- skips a record it can't lay out (unknown field type, variant records);
- warns when a record's size or a field's offset disagrees with the
  `{ N bytes }` / `{ +XX }` comments in `types.pas`.

Renaming a symbol renames its field, so code using the old name stops
compiling; that's intended.

## Memory

The port has no emulated address space; the game's state is ordinary C:

| What | Where |
|---|---|
| Data segment | `ds`, a C global (above) |
| Video memory | `vid_screen` (`src/platform/video.c`): the VGA's 64 KB at A000h, of which rows 0..199 are shown; `gfx_screen()` is the drawing target |
| Heap | `System_GetMem`/`System_FreeMem` (`src/game/system.c`): zeroed C blocks, freed by the next `system_init`. The game's memory checks (`MemAvail`/`MaxAvail`) are left out: the port always has enough |

Buffers the original addresses through their 64 KB segment, with 16-bit
offsets that can wrap past the block (drawing buffers, map layers), are
whole segments (`System_GetSegment`); code that relies on the wrap keeps a
`(uint16_t)` offset from the segment's start and says so. One pair keeps the
original's heap layout on purpose: the flight view's backdrop and star
table (`src/game/shipmove.c`), because stars drawn past the backdrop land in
the star table and change the game's random numbers.

There is no EMS: unit Ems (`src/game/ems.c`) keeps the game data and toolbar
blocks on the heap, as on a DOS machine without an EMM, so they never need
mapping. The original's other run-time helpers (`Move`, `FillChar`, unit
Dos) are plain C library calls or gone. The AdLib tune
buffer, which only the port's own player reads, is a C array (docs/audio.md).

The game's state can be serialised in a form that doesn't depend on any of
this (`src/game/state.h`: DS field by field without pointers, the blocks
behind them, the screen and the palette); golden hashes of it
(`tests/test_golden.c`, `tests/golden/`) and the playthrough's hashes
check that changes keep the behaviour.

## Start-up and leaving the game

`src/main.c` runs `fs_init`, `vid_clear`, `dseg_init`, `system_init`, the unit
initialisers in Main's order (`Game_InitUnits`) and then `Game_Main`, the main
program body (`src/game/main_program.c`). The original never returns from
Main: it ends in `Halt`, often deep inside a screen. The port unwinds with
`longjmp` back to `main()` instead:

- `System_Halt` / `RunError` run the ExitProc chain, then call
  `system_exit_handler` with the exit code. Code 15 (start.exe: restart)
  boots again; code 10 (start.exe: VICTORY) will play the
  victory cut-scene.
- Closing the window sets a quit request that `timing_wait_retrace()` turns
  into a call of the quit handler at the next retrace. Every waiting loop of
  the original waits for retrace somewhere, so no loop needs its own check.
  Loops that only poll the mouse get a retrace every few `Mouse_Update`
  calls (`timing_idle_poll`).

All game state is in DS, heap blocks and video memory, which the next boot
rebuilds, so nothing left on the abandoned C stack matters.

## Port menu (port addition)

F1 opens a menu drawn over the game (`src/platform/menu.c`): Cheats, Sound,
Video, Game, Fixes, Restart and Quit. Scripted runs keep F1 as an ordinary
key, so they never open it. `timing_wait_retrace` runs the menu when F1 was
pressed, so the game is held wherever it was waiting for the retrace; the
menu pumps its own events and draws with SDL's 8x8 debug font in a 320x240
space over the presented frame (`vid_set_overlay`). On closing it drops the
keys and buttons it saw, so none reach the game.

Every setting is kept in `reunion.ini` in the SDL pref directory
(`~/.local/share/Reunion/Reunion/` on Linux; `src/platform/config.c`),
rewritten on every change. Scripted runs and tests never read or write it,
and without it the game behaves as before the menu: music from
`SAVE\SETUP.SPD`, every fix on. `REUNION_ORIGINAL` still takes precedence
over the file.

The platform layer knows nothing of the game. Display and volumes apply at
once, and so does the music (below). Everything else that touches game
state is only asked for (`MenuHooks.changed` → `Options_Request`,
`src/game/options.c`) and carried out by `Options_Frame` at the main loop's
next frame, since the menu may have been opened in the middle of anything:

- **Cheats.** Freeze money / freeze ore hold `Game_Credits` / New Earth's six
  ore stocks (`Game_HomeStock`, what production draws on; colonies' stores
  are left alone). Every frame of the main loop puts them back, so purchases
  cost nothing (and income is lost while frozen). The value held is taken
  when a freeze is switched on and again at boot and after a load, so a
  freeze outlasting a game holds the new game's stock. Unfreezing leaves the
  stock where it is. Add 100000 credits / add 10000 ore each raise the stock
  (and the frozen value) once per click, during a game only. Saves carry no
  cheat state: they are byte-for-byte the original format.
- **Sound.** Music None, Sound Blaster or UltraSound, the cards of the
  original setup program. Sound Blaster plays the AdLib (FM) tunes in the
  game, UltraSound the `ANIM\*.SPD` modules. The change is made at once,
  wherever the menu was opened (`menu_changed` in `src/main.c`). In the
  game, title screen included, it stops the music and starts the last track
  again on the new hardware (`Music_SwitchDevice`); the start menu keeps
  CHOISE.SPD's hero voice held back on any hardware (`Music_HoldHeroVoice`,
  [port-bugs.md](port-bugs.md) #1). The AdLib tune buffer is not on the
  heap (see Memory), so any switch works the same whatever
  the game started with. The cut-scenes only have modules, which the
  original played on both cards (on a Sound Blaster through its own software
  mixer): None silences them (`cutscene_music_changed`), the other two sound
  alike, as the menu's help line says. Effects on/off and both volumes are
  the mixer's own gains on top of the game's (`audio_set_user_volumes`):
  muted effects still play silently, so the game's waits for them are
  unchanged.
- **Video.** Window (in 320x240 steps that fit the display, or the size the
  window was dragged to), full screen at a chosen display mode, or desktop
  full screen. Alt+Enter toggles between the window and the last full-screen
  choice.
- **Game.** Skip intro, and the port's additions to the game's screens,
  on by default. The game reads `config` when it draws, so no request is
  needed. Golden runs (`tests/test_golden.c`) switch them off, since they
  add hotspots to the state stream.
  - **Colony stats** (`src/game/colonystats.c`): the POP row of your
    colony's planet info screen becomes hotspot 4, "COLONY STATS", which
    shows an overlay (`PlanetInfo_AlienMode` 4) with what the next daily
    colony update will decide. `ColonyStats_Compute` repeats
    `Sim_ProcessBodyEvents` without its chances and writes no state.
    `test_colonystats` checks it against the real update.
- **Fixes.** One switch per fix in `game/fixes.h`, listed in `Fixes_List`
  (a new fix needs a line there; `test_options` checks), plus all on / all
  off. With `REUNION_ORIGINAL=1` they show off and can't be changed.
- **Restart** goes back to the title screen without the cut-scenes (in a
  cut-scene, on to the game), through the same unwinding as a restart
  request of the original. **Quit** closes the game as closing the window
  does.
