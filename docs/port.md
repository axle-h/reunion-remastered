# Porting notes

## The data segment mirror (`src/game/dseg.h`)

The first pass of the port keeps the original's global data exactly where the
DOS program had it, so the port's state can be compared byte for byte with
DS dumps from DOSBox-X. All globals live in one struct, `DSeg ds`, that covers
DS `0000–a543` (Ghidra segment `4a81`):

- Every variable named in `ghidra/symbols.txt` is a field at its original
  offset, called `Unit_Name` (`Game::Credits` → `ds.Game_Credits`). Names
  without a unit are Borland's own and get `System_`.
- Records come from `ghidra/types.pas` with the Pascal layout (packed, no
  alignment). Pascal arrays become 0-based C arrays: the comment gives the
  Pascal bounds, and the C index is the Pascal index minus the lower bound
  (`Galaxy::SunNames[3]` → `ds.Galaxy_SunNames[2]`).
- `boolean` is `uint8_t`, since the original may store any byte value.
  Strings are `PStrN` / `PString` (length byte + chars). Pointers are
  `uint32_t` far pointer *values* (segment:offset), not C pointers. `real` is
  the 6-byte `Real48`.
- Bytes nobody has named yet are `_pad_XXXX` arrays; untyped labels are a
  single `uint8_t`. Every record field and every DS field has a
  `static_assert` on its offset, so a layout mistake fails the build.

`dseg_init()` (in `src/game/dseg.c`) does what the original's startup does:
it copies the typed constants, DS `0000–759d`, from the load image of the
player's `GRWAR\REUNION.PRG` and leaves the rest zeroed. The data segment
starts at file offset `header paragraphs * 16 + 3A81h * 16`.

### Regenerating

`dseg.h` is generated; don't edit it. After adding names or types to
`symbols.txt` or `types.pas`, run:

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

## Memory model

The game keeps DOS far pointers (`segment:offset`) in its data: to globals,
to video memory, to heap blocks and to procedures. So that ported code can
use them unchanged, `src/game/mem.h` emulates the real-mode address space as
one 1 MB (+64 KB) array, `mem[]`, with everything at its original address:

| What | Where |
|---|---|
| Data segment | `MEM_DS_SEG` = load segment + 3A81h; `ds` is a `DSeg` view of it |
| Video memory | `A000:0000`; `vid_screen` points there after `mem_init()` |
| Heap | from `MEM_HEAP_SEG` (after the stack) to `A000:0000`, managed by `System_GetMem`/`System_FreeMem` (`src/game/system.c`) exactly as Borland's System unit does; if it reaches video memory it goes on above it to `F000:0000`. `System_MemAvail`/`MaxAvail` always report 640 KB, so the game's memory checks pass |

There is no EMS: unit Ems (`src/game/ems.c`) keeps the game data and toolbar
blocks on the heap, as on a DOS machine without an EMM.

One exception: the AdLib tune buffer, which only the port's own player
reads, is a C array rather than a heap block (docs/audio.md), so the heap
doesn't depend on the music chosen at start-up.

`far_host(p)` turns a stored far pointer into a host pointer, and
`ghidra_host(seg, off)` does the same for Ghidra's segment numbering.
Far *code* pointers (procedure variables such as `Game::GrWarProc`) will need
a table mapping original entry points to C functions.

`MEM_PSP_SEG` is provisional. To diff memory against the original in
DOSBox-X, set it to the PSP segment the original gets there.

Start-up order: `mem_init()`, then `fs_init()`, then `dseg_init()`, then
`system_init()` (heap variables).

## Start-up and leaving the game

`src/main.c` runs `mem_init`, `fs_init`, `dseg_init`, `system_init`, the unit
initialisers in Main's order (`Game_InitUnits`) and then `Game_Main`, the main
program body (`src/game/main_program.c`). The original never returns from
Main: it ends in `Halt`, often deep inside a screen. The port unwinds with
`longjmp` back to `main()` instead:

- `System_Halt` / `RunError` run the ExitProc chain, then call
  `system_exit_handler` with the exit code. Code 15 (start.exe: restart)
  boots again from `mem_init`; code 10 (start.exe: VICTORY) will play the
  victory cut-scene.
- Closing the window sets a quit request that `timing_wait_retrace()` turns
  into a call of the quit handler at the next retrace. Every waiting loop of
  the original waits for retrace somewhere, so no loop needs its own check.
  Loops that only poll the mouse get a retrace every few `Mouse_Update`
  calls (`timing_idle_poll`).

All game state is in the emulated memory, which the next boot rebuilds, so
nothing left on the abandoned C stack matters.

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
  emulated heap (see Memory model), so any switch works the same whatever
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
- **Game.** Skip intro.
- **Fixes.** One switch per fix in `game/fixes.h`, listed in `Fixes_List`
  (a new fix needs a line there; `test_options` checks), plus all on / all
  off. With `REUNION_ORIGINAL=1` they show off and can't be changed.
- **Restart** goes back to the title screen without the cut-scenes (in a
  cut-scene, on to the game), through the same unwinding as a restart
  request of the original. **Quit** closes the game as closing the window
  does.
