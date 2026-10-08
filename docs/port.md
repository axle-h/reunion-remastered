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
| Heap | from `MEM_HEAP_SEG` (after the stack) to `A000:0000`, managed by `System_GetMem`/`System_FreeMem` (`src/game/system.c`) exactly as Borland's System unit does |
| EMS | page frame `E000:0000` (4 × 16 KB), driver header at `C800:0000` behind INT 67h's vector; `src/game/emm.c` emulates a LIM 4.0 driver with 256 KB, which unit Ems (`src/game/ems.c`) uses for the game data and toolbar blocks as on a DOS machine with EMS |

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

## Options panel (port addition)

The disk operations toolbar (screen 0Ch) has a sixth button, OPTIONS, using
command id 3Dh: the original's tables have an entry for it with a blank label
and no use. `Options_Init` (`src/game/options.c`) fills in its label,
icon and sound and adds it to the screen's button list at boot. Its icon,
number 44h (one past ICON.ALL's 68), is the port's own
(`src/game/options_icon.c`, made from a picture by `tools/icon_png.py`);
`GfxPath_DrawIcon` draws it from there.

Clicking it swaps the toolbar for BACK TO M.SCREEN and BACK TO DISK OPS (the
unused command 3Fh, with RETREAT's arrow) and shows a panel in place of the
save slot list (`src/game/saveload.c`) with the playtest cheats. The music and
effects buttons still work.

- Freeze money / freeze ore hold `Game_Credits` / New Earth's six ore
  stocks (`Game_HomeStock`, what production draws on; colonies' stores are
  left alone) at their value when switched on. Every frame of the main loop
  puts them back, so purchases cost nothing (and income is lost while
  frozen). Unfreezing leaves the stock where it is.
- Add 100000 credits / add 10000 ore each raise the stock (and the frozen
  value) once per click, for purchases larger than the balance.

The main loop hands a non-repeating hotspot to the screen on every frame the
button is held (that is how dragging across the slot list selects), so a
toggle waits for the button to be released.

Cheats belong to a game. Every boot starts with them off, and a save of a
game with one on carries their state in a trailer after the original
blocks (`docs/formats/save.md`), so loading it brings them back. Loading a
save without the trailer turns them off.
