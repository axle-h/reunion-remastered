# Reunion port plan

Goal: a native, modern build of *Reunion* (Amnesty Design / Grandslam, 1994) that
plays identically to the DOS original, then fix its bugs.

Approach: decompile the DOS executable with Ghidra and rewrite it, unit by unit,
in C23 on top of SDL3, built with CMake. The port loads the original game data
from the user's own copy; we never ship the assets.

This file is the only plan: what is done, what is left, and every known bug
that is still open. Reference material is in `docs/` (see the README).

Keep it lean. When you finish an item, delete it here (git history keeps
it), move any lasting choice into Decisions, and update "Where we stand" in
a line. Don't leave "done" notes, finished checklists or write-ups behind.

## Decisions

| Topic | Choice | Why |
|---|---|---|
| Language | C23 | Fixed-underlying-type enums (`enum : uint8_t`) mirror Pascal byte enums/records exactly; `static_assert`, `bool`, `nullptr`, `constexpr`, binary literals without headers or workarounds. GCC 14+ / Clang 18+. On Windows build with clang-cl or MinGW, as MSVC's C23 support is incomplete |
| Platform | SDL3 | SDL2 is maintenance-only (Fedora already ships SDL2 as `sdl2-compat` on top of SDL3). SDL3's audio streams make feeding our own mixer simple, and its logical presentation does integer-scaled 320×200 out of the box |
| Music | libxmp (modules), Nuked OPL (AdLib) | The UltraSound music is ProTracker modules played as-is; the Sound Blaster music is the original's AdLib player ported (`src/game/adlib.h`) |
| Build | CMake (≥ 3.25) with presets | Uses system SDL3/libxmp if found, otherwise fetches them with `FetchContent` |
| VCS | git | `vendor/` (the game data) is git-ignored |
| Memory | Native C | The real-mode memory model (`mem[]`, far pointers, the Borland heap, EMS) let ported code match the original byte for byte; once every unit matched it was replaced by C globals, typed pointers and C allocations, and the Borland run time went with it (docs/port.md) |
| Verification | Golden state hashes and the playthrough | Differential tests under Unicorn checked every unit against the original until the move to native memory (tag `last-real-mode`); golden hashes of the layout-independent state (fixes on and off) and the playthrough's proof guard behaviour since |
| Distribution | The binary without the art; the player brings their copy | Data taken from the executables stays compiled in (typed constants, texts, price tables, cut-scene tables, the AdLib player). Pictures, animations, modules, samples and anything else that can be read from the player's copy is never bundled. The first launch imports the player's copy (README, "Running") |
| Player's folder | One `SDL_GetPrefPath("Reunion", "Reunion")` folder for everything | `reunion.ini`, `save/`, `screenshots/` and the imported game in `data/` |
| Editions | English floppy and the English folder of the CD; strict | The port is the floppy's `REUNION.PRG`. The CD's English data differs only in its font sheet (docs/versions.md). An import accepts only files whose checksums are known. `DATA_DIR`/`REUNION_DATA` still point anywhere without checks. The CD's other three languages are a possible later addition |

## Source

`vendor/Reunion_DOS_EN/GRWAR/REUNION.PRG` (English floppy release; the CD's
differs, see docs/versions.md): Borland
Pascal 7.0, real mode, no packing or overlays, ~240 KB of code in 54
segments (813 functions), plus
`INTRO.PRG`, `CREDITS.PRG` and `VICTORY.PRG`, chained by `start.exe`.
How the port cites the original's addresses: docs/port.md, "Addresses".
The file formats are described in the loaders, `src/assets/*.h`.

## Where we stand (2026-10-12)

The port is complete and the game can be won in it.

- **Ported:** every unit of `REUNION.PRG` and the three cut-scene programs,
  about 37,000 lines of C in `src/`. The game needs only the data files, no
  executable. Real-mode memory, EMS and the Borland run time are gone.
- **Matches the original:** every unit matched the original routine for
  routine under the differential tests before they were retired. Since
  then, golden state hashes (`tests/golden/`, every fix on and every fix
  off) catch any change in behaviour.
- **Playable start to finish:** the playthrough fixture wins a new game
  with seed 12345 using only clicks, keys and what is on screen (Halt(10),
  2930-02-26). Its recording replays to the same end in the plain binary.
  Six of seven seeds win (docs/playthrough.md).
- **Port additions:** the F1 port menu (cheats, Sound Blaster/UltraSound
  music, display, skip intro, colony stats, fix switches, import, restart,
  quit), `reunion.ini`, screenshots, headless autopilot, `--replay` and soak
  runs.
- **Ships without the game data:** release builds have none built in. The
  first start imports the player's copy (a folder, a ZIP or the CD's ISO;
  the floppy and CD editions) into the player's folder, checked against
  the list of files the port loads.
- **Bug fixing has started:** 11 bugs of the original that can make the
  game unwinnable or break the economy are fixed behind switches
  (`src/game/fixes.h`). Of the 34 bugs found in the port while playing it, 33
  are fixed and one turned out to be the original's behaviour; their
  write-ups are in git history (`docs/port-bugs.md`, deleted 2026-10-12).
- **Tests:** all 85 ctest tests pass on the release build, the full
  playthrough (`test_play_game`) and the golden proof included.

Milestones from the original plan: Ghidra database named (done; the
Ghidra project and the Python asset tools were retired 2026-10-12), assets
viewable (done, `tools/`), title screen (done), every screen playable (done),
full game playable start to finish (done), verified against the original
(mostly done, see 1 below).

## Remaining work

In rough priority order.

### 1. Verification gaps

- **Saves from the DOS game.** Loading original saves works (checked on
  2026-10-12 with a scratch harness: docs/versions.md, "Saves"), but no test
  covers it. Copy `~/dos/c/REUNION/SAVE/SPIDYSAV.1` and `.2` into
  `tests/golden/saves/` and check that they load and save back unchanged
  (as `test_golden`'s `load_save` does). Still to do: load the port's saves
  in DOSBox-X.
- **Whole-game side-by-side.** The differential tests compared single
  routines, and `test_sim` has spot checks against a DOSBox-X memory dump.
  Nothing has compared a longer run (timing per game hour, a battle) with
  the original from the same save.
- **Original crashes on modern machines.** The suspects are all in the
  DOS sound drivers and timer, which the port replaces, so there is
  nothing to fix on the port's side; reproducing a crash in DOSBox-X would
  only confirm the cause:
  - the UltraSound module player (`43d5`) reprograms the PIT to about
    1 kHz with its own INT 08h handler, which never chains to the BIOS's,
    so the BIOS tick, INT 1Ch and the DOS clock stop while it plays;
  - UltraSound detection scans 200h–2A0h, but the player hard-codes base
    220h (where a Sound Blaster usually sits; DOSBox's UltraSound default
    is 240h), and a failed detection still writes to ports 0–7 (the DMA
    controller) and 20h (the PIC);
  - AdLib detection (`428e`) waits for VGA retrace edges on each candidate
    port and hangs where port 3DAh doesn't toggle;
  - the Sound Blaster driver is a raw real-mode blob (`GRWAR\ALFA1.PIC`),
    and the game spins on its status word, hanging if the driver never
    clears it.

### 2. Playthrough robustness

Seed 3 loses at M16: New Earth stops at 5 Derricks (seed 12345 has 20), so
the Detoxin for the Space station never gathers in time. Why step E21 built
no more Derricks is not known yet (docs/playthrough.md, "Robustness").

### 3. Bugs of the original still open

All are marked `// BUG?` in the code (`grep -rn 'BUG?' src/`). The port
reproduces them. For each one that changes play, decide whether it gets a
switch in `src/game/fixes.h`, next to the existing ones.

**The CD's `REUNION.PRG` may fix some.** It was rebuilt on 1994-08-15 and is
said to fix stability problems (docs/versions.md). Its functions are the
same as the floppy's, but its data segment moved, so a plain diff shows
almost half the bytes changed. Compare it function by function, with
relocations and DS offsets masked out (a 16-byte-window match already pairs
most code up), list the routines whose logic changed, and give real fixes a
switch here.

**Change play (candidates for a fix switch):**

| Where | What |
|---|---|
| `main_program.c`, after a ground battle | When an enemy wins a ground battle at one of your colonies, `ClaimBodyForRace` gets its arguments in the wrong order: (system, planet, moon, race) instead of (owner, system, planet, moon). The colony is purged but not taken over. Instead, the body at (planet, moon, race) is cleared and handed to owner = the system number. In system 1 that owner is the player |
| `spwar.c` `add_store_ships` | Every garrison ship from a body's stores gets id C9h, but `ApplySpaceLosses` counts survivors by 200 + moon. Garrison losses after a space battle are booked against the wrong body |
| `spwar.c` `SpWar_ApplySpaceLosses` | Garrison losses are added to whichever side `LossPtr` was left pointing at. When it was nil, the original wrote to 0000:0000 (the interrupt table); the port skips that write |
| `intel.c` `Intel_ClaimBodyForRace` (owners 8 and 12) | Every pass writes `Stores[1]`, so these races' bodies only get a garrison in the first slot (the last of the eight draws) |
| `planetinfo.c` `PlanetInfo_SetupSystemViewHotspots` | The sun's hotspot sets orbit record 8's flag from `Bodies[sys][-1]` |
| `launch.c`, GROUND WAR | The race state is indexed with `Owner` read as a signed char |
| `sim.c` `Sim_UpdateColonySetups` | After a removal, the entry moved into its slot waits until the next hour |
| `grwar.c` `GrWar_FindPath` | Tests cell (a,b) instead of (c,d), so (c,d) is marked only when (a,b) is out of range |
| `vec3d.c` model 24 | Run-time error 200 if one of the centroids has x = 0. Check the shipped models; if none has, this is harmless |
| `sim.c`, mines (docs/walkthrough.md) | Demolishing a mine never lowers the droid count. Bodies with only a miner station share one spare storage record |

**Visible but cosmetic:**

| Where | What |
|---|---|
| `sound.c` `PlayWarSnd` / `PlayGrSnd` | Share one "last loaded" key, so ground sound *n* right after war sound *n* replays the war sample |
| `gfxpath.c` `AddToolbarIcon` | Clears slot `slot` 13 − `slot` times instead of the slots after it |
| `shipinfo.c` "Change mode" | On list 0 it plays the error sound and still switches to list 1 |
| `intel.c` spy reports | The "Information on" line is built and then overwritten (three places) |
| `planetinfo.c` alien planet info | Race record bytes 13h..1Ah are read as "has this resource" flags |
| `research.c` selection frame | Draws without hiding the mouse cursor |
| `surface.c` build | A full building table plays the 'x' sound and skips the 'buildin' branch |
| `command.c` room animation | Calls `StartMainAnim` where `StartGrAnim` is meant |
| `message.c` | A box with more than 6 lines overflows its line array (the port drops the extra text). The log count can go negative |
| `pub.c`, `alientalk.c` | Their line-clearing loops reuse the previous text's line count |
| `blit.c` fill | An odd count fills count + 2 bytes |
| `credits.c`, `cutscene.c` | A white palette is set up and never used |
| `adlib.c` | KSL bits swapped, key-off never reaches the OPL, slide up without carry. This is how the original's music sounds; leave it |

**Harmless, or already handled by the port:** `textfile.c` `BeginInput`
(255-character limit into 60 bytes; edited strings are at most 19);
`gfx.c` text wider than 255 (never happens); `main_program.c` clearing a
fifth `DevSkill` (it is `ProductionRate`, set just after); `advisor.c`
uninitialised first pass (the port zeroes it); `surface.c`
`RegisterBuildingAnims` marking at uninitialised coordinates (the port
skips it); the unreachable money cheat (`26eb`, the port has its own); the
GUS and AdLib detection and timer bugs (the port replaces the drivers;
see 1);
the copy protection (removed, `src/game/protect.h`).

### 4. Quality of life

Remappable keys, and saving anywhere.

### 5. Reverse-engineering loose ends

These matter only for reading the code; nothing depends on them:

- record fields that are still `_xx` fillers or marked `?` in
  `src/game/dseg.h`: `TGroup`'s travel and cargo fields, and bytes of
  `TBody`, `TProduct`, `TBuilding` and `TRace` (`TRace.Status` 0/2/4/6,
  what `TBody.Stores[7..8]` hold);
- DS bytes nobody references, `_pad_XXXX` in `src/game/dseg.h`: the
  1,535 bytes at `9C03–A201`, `A212–A219`, and a few small gaps in the
  typed constants.

### 6. Licence and publication

The game is still under copyright. Decided (2026-10-12): release builds
ship without the art and carry the data taken from the executables
(`src/game/dsinit.c`, the game's typed constants and texts; the cut-scene
tables; the AdLib player's image in `src/game/adlib.c`). The player
supplies the rest (the import, README). Still open: whether to publish the repository itself.
Until then keep it private, or publish only the code (no assets, no raw
decompiler dumps).
