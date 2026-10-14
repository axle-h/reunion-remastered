# Porting notes

## Addresses

Comments cite the original's code as `seg:off`, numbered as Ghidra loads
`REUNION.PRG`: file segment + 1000h, so the data segment is `4a81` and
`1b8a:0705` is file segment `0b8a`. The cut-scene programs are numbered the
same way in their own load images (`src/cutscene/cs.h`).

## The data segment (`src/game/dseg.h`)

The original's globals are one C global, `DSeg ds`: DS `0000–a543` of
`REUNION.PRG`, field by field in the original's order, named `Unit_Name`
(`ds.Game_Credits`; Borland's own get `System_`). The game code reaches
every global by name; the original's offsets are only in comments and the
field table.

- Records keep the Pascal layout, packed: they are the game's data formats,
  saved and kept in heap blocks. Pascal arrays become 0-based C arrays; the
  comment gives the Pascal bounds (`[3..9]`), and the C index is the Pascal
  index minus the lower bound.
- `boolean` is `uint8_t`, since the original may store any byte value.
  Strings are `PStrN` / `PString` (length byte + chars), saved and hashed
  byte for byte. Game code uses C strings and converts at the boundary
  with `pstr_get` / `pstr_set` (`src/game/system.h`). The game's few Real
  computations (the cosine table, model normals, taxes) are integer or
  `double` code checked to give the original's results.
- `DSeg` has no padding: in a dozen places the original reads or writes
  past an array into the next globals (alien-talk lines into
  `Surface_Mode`, race 0/1 records before `Races_Races`, index 0 of 1-based
  tables), and those stay the same bytes. Only pointer fields differ in
  size from the original. Bytes nobody named are `_pad_XXXX` arrays.
- Packed means many words and longwords sit at odd addresses. Code reaches
  them through the packed struct (`ds.X`, `rec->X`) or, at a byte offset,
  with `rd16le`/`wr16le`/`rd32le`/`wr32le` (`src/game/le.h`), never through
  an `int16_t *` or `int32_t *`. The `asan` and `trap` presets check
  alignment.

`dseg_init()` (`src/game/dseg.c`) does what the original's startup does:
DS gets the typed constants (`ds_initial`, `src/game/dsinit.c`, DS
`0000–759d`, read once from the executable) and the rest is zero. The
far pointers DOS relocated are host pointers there, so the game needs no
executable at run time, only the data files.

The field table (`src/game/dsfields.c`) lists every field's original
offset, its offset in `DSeg`, its size and whether it holds pointers;
`game/state.c` walks it to serialise the state. `dseg.h`, `dsfields.c`
and `dsinit.c` were generated from the Ghidra symbols while the port was
written; they are ordinary source now, so a field added or renamed in
`dseg.h` needs the same change in `dsfields.c` (and `dsinit.c` for a typed
constant). `test_state` and the golden hashes catch a field missing from
the table.

## Memory

The port has no emulated address space; the game's state is ordinary C:

| What | Where |
|---|---|
| Data segment | `ds`, a C global (above) |
| Video memory | `vid_screen` (`src/platform/video.c`): the VGA's 64 KB at A000h, of which rows 0..199 are shown; `gfx_screen()` is the drawing target |
| Heap | `game_alloc`/`game_free` (`src/game/system.c`), the original's `GetMem`/`FreeMem` without sizes on free: zeroed C blocks, tracked so the next `system_init` frees them (restart and quit `longjmp` past whatever the stack held). The game's memory checks (`MemAvail`/`MaxAvail`) are left out: the port always has enough |

Every other block has the original's size. Buffers the original addresses
through their 64 KB segment, with 16-bit offsets that can wrap or run past
the block, are whole segments (`game_alloc(GAME_SEGMENT)`), and each
allocation says why: the map layers (x or y 0 is below the layer), the
ground battle's unit lists (slot 0 wraps to the segment's end) and field,
the planet frame (sprites at its top edge), and the vector models' buffer
(polygons drawn unclipped). Code that relies on the wrap keeps a
`(uint16_t)` offset from the segment's start and says so. One pair keeps the
original's heap layout on purpose: the flight view's backdrop and star
table (`src/game/shipmove.c`), because stars drawn past the backdrop land in
the star table and change the game's random numbers.

There is no EMS: the game data and toolbar blocks (`Game_InitGameData`,
`GfxPath_Init`) stay on the heap, as on a DOS machine without an EMM, so they
never need mapping, and unit Ems is folded into its callers. Their
`TEmsBlock` variables keep the original's state 1 (a heap copy). The
original's other run-time helpers (`Move`, `FillChar`, unit Dos) are plain
C library calls or gone. The AdLib tune buffer, which only the port's own
player reads, is a C array (`src/game/adlibmusic.c`), so the heap is the
same whatever music the game started with.

The game's state can be serialised in a form that doesn't depend on any of
this (`src/game/state.h`: DS field by field without pointers, the blocks
behind them, the screen and the palette); golden hashes of it
(`tests/test_golden.c`, `tests/golden/`) and the playthrough's hashes
check that changes keep the behaviour.

Before the move, while the port still kept the original's real-mode
address space, differential tests ran the original's routines under
[Unicorn](https://www.unicorn-engine.org/) (an x86 emulator) on the port's
memory and compared everything they left behind, until every unit matched.
They can't run on native memory and were removed; the tag `last-real-mode`
has them and their harness (`tests/diff/`).

## Start-up and leaving the game

`src/main.c` finds the game data (README, "Running"), showing the import
screen when there is none, and plays the cut-scenes. Each boot then picks
the language's folder (`fs_set_language_dir`), the language of the
program's own texts (`Lang_Set`, `src/game/lang.h`) and the CD's or the
floppy's font for the data's edition (`gfx_set_cd_charset`), and runs `vid_clear`, `dseg_init`, `system_init`, the unit initialisers in
Main's order (`Game_InitUnits`) and `Game_Main`, the main program body
(`src/game/main_program.c`). The original never returns from
Main: it ends in `Halt`, often deep inside a screen. The port unwinds with
`longjmp` back to `main()` instead:

- `System_Halt` / `RunError` run the exit hook (the original's only
  ExitProc, `Gfx_ExitHandler`: it stops the music and frees the toolbar),
  then call `system_exit_handler` with the exit code. Code 15 (start.exe:
  restart) boots again; code 10 (start.exe: VICTORY) plays the victory
  cut-scene.
- A run-time error (`System_RunError`: the original's code, the port's
  routine and, for file errors, the path) and the data loaders' "Cannot
  Open" are fatal errors (`system_fatal`): reported on stderr and, when a
  player is watching, in a message box, instead of the original's text-mode
  error screen. Scripted runs (autopilot, `--replay`, tests, the offscreen
  and dummy video drivers) never show the box, so a failure can't block
  them. Tests catch run-time errors with `system_runerror_handler`. The
  original's "Not enough memory" error is gone: the port doesn't emulate the
  DOS memory limit, and a failing C allocator is a plain "Out of memory".
- Closing the window sets a quit request that `timing_wait_retrace()` turns
  into a call of the quit handler at the next retrace. Every waiting loop of
  the original waits for retrace somewhere, so no loop needs its own check.
  Loops that only poll the mouse get a retrace every few `Mouse_Update`
  calls (`timing_idle_poll`).

All game state is in DS, heap blocks and video memory, which the next boot
rebuilds, so nothing left on the abandoned C stack matters.

## Deliberate deviations

Besides the fixes (`src/game/fixes.h`), the improvements
(`src/game/improvements.h`) and the port menu, the port departs from
the original in a few places, each commented in the code:

- **The hero voice.** The original starts `ANIM\CHOISE.SPD` at the start
  menu (`321d:016c`), so its "choose your hero" voice (sample 6) plays
  before New game is clicked. The port plays the module there with that
  sample muted and restarts it with the voice on the hero-select screen
  (`Music_HoldHeroVoice`, `src/game/music.c`; `tests/test_audio_mute.c`).
- **LoadPic's fade-out** waits for a retrace every 8 of its 40 steps
  (`LOADPIC_FADE_STEPS_PER_RETRACE`, `src/game/gfx.c`), standing in for
  the decoding time that paced the original.
- **Busy waits on hardware** (a sound effect finishing, the mouse) call
  `timing_idle_poll`, so they see retraces and scripted runs, whose audio
  follows the frame clock, can't spin for ever.
- **Writes the port has no memory for** are dropped where the code says so:
  a nil `LossPtr` in `SpWar_ApplySpaceLosses` (the original wrote to
  0000:0000), and a footprint at uninitialised coordinates in
  `Surface_RegisterBuildingAnims`.
- **No copy protection** (`src/game/protect.h`).
- **The CD's French and Italian accents** draw with the font sheet's own
  glyphs, as the install archives' programs do; the CD's executable draws
  them all as é (`cd_glyph`, `src/game/gfx.c`).
- **The texts compiled into the program** are the language's in the CD's
  German, French and Italian games: the CD runs them with the English
  program, the port takes the texts of each language's own build of it
  from the install archives (docs/versions.md, `src/game/lang.h`).
- **Without `SAVE\SETUP.SPD`** `Sound_Init` takes the floppy's Sound
  Blaster, where the original takes a GUS. The import leaves the file out,
  since the CD's switches the sound off.

## Port additions

- **Port menu** (F1, `src/platform/menu.h`): cheats, music card, effects
  and volumes, display, skip intro, colony stats, Detoxin rate, language,
  the improvement and fix switches, and System: import game data, restart
  and quit. The game
  waits inside `timing_wait_retrace` while it is open. The platform layer
  knows nothing of the game: display and volumes apply at once, the music
  switch is made at once by `src/main.c`, and everything that touches
  game state is carried out by `Options_Frame` at the main loop's next
  frame (`src/game/options.h`, which also describes the cheats). Scripted
  runs keep F1 as an ordinary key.
- **The player's folder** (`src/platform/playerdir.h`), the SDL pref
  directory (`~/.local/share/Reunion/Reunion/` on Linux): `reunion.ini`,
  `save/`, `screenshots/` and the imported game in `data/`.
- **Settings** (`src/platform/config.h`): `reunion.ini`. A fix or an
  improvement it doesn't list is on. Tests and scripted runs never read or
  write it: they play the original's game with every fix on and every
  improvement off. Saves carry no settings.
- **Improvements** (`src/game/improvements.h`): changes to the game's
  design, each a switch in the port menu like the fixes, kept in fields
  the DOS saves already have where they can. Trade routes
  (`src/game/routes.h`) and buildings past 1,000 (No building limit,
  `Game_ReserveBuildings`) can't: a save holding them is in the port's own
  format, `save/reunion.<n>` (`src/game/saveload.h`); otherwise saves are
  byte for byte the original's. The route editor is the port's own screen
  39, with toolbar commands past the original's 77
  (`src/game/portcmd.h`). Keyboard play (`src/game/keyplay.h`) changes no
  screen: an input hook turns keys into the mouse the game reads.
- **Importing the game data** (`src/platform/import.h`): the player's copy
  (a folder, a ZIP or an ISO image, `src/platform/source.h`) is checked
  against the list of files the port loads (`src/platform/gamedata.h`)
  and copied to `data/`, with the DOS game's saves. The import screen
  (`src/platform/importscreen.h`) asks for it when no game data is found,
  and the port menu's System section imports another copy. Debug builds
  warn when the game loads a file that isn't on the list.
- **Editions and languages.** The English floppy release, and the European
  CD's four games, whose font differs (docs/versions.md):
  `gfx_set_cd_charset` (`src/game/gfx.h`) picks it. An import of the CD
  has all four: data/ the English game, data/DEU, FRA and ITA each
  language's own files, read first in that language
  (`src/platform/import.h`, `fs_set_language_dir`). The language is a
  setting (port menu, Game; `--language`) used from the next restart.
- **Music cards.** None, Sound Blaster (the AdLib FM tunes, through the
  original's player on Nuked OPL3) or UltraSound (the `ANIM\*.SPD`
  ProTracker modules, through libxmp), switchable at any time
  (`Music_SwitchDevice`). The cut-scenes only have modules, which the
  original played on both cards.
- **Colony stats** (`src/game/colonystats.h`), switched in the menu's Game
  section like the other additions to the game's screens. Golden runs
  switch these off, since they add hotspots to the state stream.
- **Detoxin rate** (`Surface_DerrickRate`, `src/game/surface.h`): the
  droids screen's Detoxin row shows the T/PTP of the body's active
  Derricks (the original shows `--`, as droids mine no Detoxin), and a
  Derrick's info adds their Total. Golden runs switch it off too.
- **Headless runs**: autopilot scripts, `--replay` and soak runs
  (`src/platform/autopilot.h`, README), and the playthrough fixture
  (docs/playthrough.md).
