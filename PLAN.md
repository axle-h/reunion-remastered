# Reunion port plan

Goal: a native, modern build of *Reunion* (Amnesty Design / Grandslam, 1994) that
plays identically to the DOS original, then fix its bugs.

Approach: decompile the DOS executable with Ghidra and rewrite it, unit by unit,
in C23 on top of SDL3, built with CMake. The port loads the original game data
from the user's own copy; we never ship the assets.

This file is the only plan: what is done and what is left. Reference
material is in `docs/` (see the README).

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
| Editions | English floppy and the CD's four languages; strict | The port is the floppy's `REUNION.PRG` with the CD's font (docs/versions.md). An import accepts only files whose checksums are known. The CD's import takes all four games, the English one whole and each other language's own files next to it, and the language is a setting (port menu, `reunion.ini`, `--language`): one import, no re-import to switch, about 82 MB. `DATA_DIR`/`REUNION_DATA` still point anywhere without checks. The other languages' program texts come from the builds in the CD's install archives, compiled in by `tools/cd_texts.py` (building needs no copy of the game; the tests that use the ISO need it) |
| Bugs of the original | A bug that changes play gets a switch in `src/game/fixes.h` (all on by default); a harmless one is kept and described where the code reproduces it | With every fix off the port plays like the original, which the golden hashes check |
| Improvements | Changes to the game's design are switches in `src/game/improvements.h` (port menu, Improvements; `[improvements]` in `reunion.ini`), on by default. The Fixes section's "All off (original)" turns them off too | Scripted runs (autopilot, `--replay`, the tests) play with every improvement off, so the golden hashes, the playthrough and the DOS-save tests still prove the original's behaviour; a test of one switches it on |
| Saves | The DOS format unless an improvement needs more: then `save/reunion.<n>`, the DOS blocks plus the port's records behind a magic and a version (`src/game/saveload.h`) | The DOS game never opens that name, where another layout under `spidysav.<n>` would load as garbage; a slot has one file, and the disk screen lists both. The autosave is `save/reunion.auto`, always in the port's format, outside the 12 slots |

## Source

`vendor/Reunion_DOS_EN/GRWAR/REUNION.PRG` (English floppy release; the CD's
differs, see docs/versions.md): Borland
Pascal 7.0, real mode, no packing or overlays, ~240 KB of code in 54
segments (813 functions), plus
`INTRO.PRG`, `CREDITS.PRG` and `VICTORY.PRG`, chained by `start.exe`.
How the port cites the original's addresses: docs/port.md, "Addresses".
The file formats are described in the loaders, `src/assets/*.h`.

## Where we stand (2026-10-14)

The port is complete and the game can be won in it.

- **Ported:** every unit of `REUNION.PRG` and the three cut-scene programs,
  about 37,000 lines of C in `src/`. The game needs only the data files, no
  executable. Real-mode memory, EMS and the Borland run time are gone.
  Every record field the code uses is named; the few nobody reads are
  marked unused.
- **Matches the original:** every unit matched the original routine for
  routine under the differential tests before they were retired. Since
  then, golden state hashes (`tests/golden/`, every fix on and every fix
  off) catch any change in behaviour.
- **Playable start to finish:** the playthrough fixture wins a new game
  with seed 12345 using only clicks, keys and what is on screen (Halt(10),
  2930-03-06). Its recording replays to the same end in the plain binary.
  Seeds 1-240 win too, and so do the CD's German, French and Italian games,
  the program's own texts translated as well (docs/playthrough.md).
- **Port additions:** the F1 port menu (cheats, Sound Blaster/UltraSound
  music, display, skip intro, colony stats, language, improvement and fix
  switches, import, restart, quit), `reunion.ini`, screenshots, headless autopilot,
  `--replay` and soak runs.
- **Ships without the game data:** release builds have none built in. The
  first start imports the player's copy (a folder, a ZIP or the CD's ISO;
  the floppy release, and the CD with its four languages) into the
  player's folder, checked against the list of files the port loads.
- **Improvements** (`src/game/improvements.h`): More Miner stations, up
  to four on a body, mining every hour and kept by a colony founded there;
  Repeat build, Shift+click on the surface places a building and stays in
  build mode; Trade routes, a trade group hauls ore home on its own along
  up to six stops (`src/game/routes.h`), edited on a screen of the port's;
  Keyboard play, every screen played with keys that drive the mouse
  (`src/game/keyplay.h`), remappable in `[keys]`; No building limit, the
  building table leaves the game data block past 1,000 and grows up to
  32,766 (`Game_ReserveBuildings`); Autosave, one rolling save beside the
  12 slots, written as a battle or alien talk starts (the hour's start),
  weekly and by F11 (`src/game/autosave.h`); Storage full message, one
  log entry when a body's production stops on a full store
  (`src/game/storefull.h`); Universities count, a course needs a
  powered University and each one lets one more staff member be away
  (`src/game/courses.h`).
- **Bugs of the original:** 18 are fixed behind switches
  (`src/game/fixes.h`): those that can make the game unwinnable or break
  the economy, and those that unbalance colonies and battles or lose
  information. The CD release fixes four of them; `league_tip` is taken
  from it (docs/versions.md). The rest are harmless and kept, each
  described where the code reproduces it. Of
  the 34 bugs found in the port while playing it, 33 are fixed and one
  turned out to be the original's behaviour; their write-ups are in git
  history (`docs/port-bugs.md`, deleted 2026-10-12).
- **Tests:** all 98 ctest tests pass on the release build, the full
  playthrough (`test_play_game`, and `_german`, `_french`, `_italian` from
  the CD's ISO), the golden proof and the two DOS saves
  (`golden_load_DOS_1`, `_2`: loaded, saved back byte for byte, played on)
  included.

Milestones from the original plan: Ghidra database named (done; the
Ghidra project and the Python asset tools were retired 2026-10-12), assets
viewable (done, `tools/`), title screen (done), every screen playable (done),
full game playable start to finish (done), verified against the original
(mostly done, see 2 below).

## Remaining work

In rough priority order.

### 1. Improvements

Changes to the game's design rather than its bugs: quality of life first,
then mechanics that don't do what the game seems to mean. Each one is a
switch in `src/game/improvements.h` (see Decisions), with its own test with
it on.

### 2. Checks against the DOS game

Both need DOSBox-X driven by a script, too much work for now.

- **The port's saves in the original.** The DOS saves load in the port
  (`golden_load_DOS_*`); the other way round is untested. Load a port save
  (e.g. `tests/golden/saves/X01_rigel1.sav` as `SPIDYSAV.3`) in DOSBox-X by
  hand and play on. A save holds only data records (no pointers), so a
  difference would be a value the original never writes.
- **Whole-game side-by-side.** The differential tests compared single
  routines, and `test_sim` has spot checks against a DOSBox-X memory dump.
  Nothing has compared a longer run (timing per game hour, a battle) with
  the original from the same save.

### 3. Publication

Last, once the work above is done. The repository is to go public.
Decided (2026-10-12): release builds ship without the art and carry the
data taken from the executables (`src/game/dsinit.c`, the game's typed
constants and texts; the cut-scene tables; the AdLib player's image in
`src/game/adlib.c`). The player supplies the rest (the import, README).
Until it is published the repository stays private.

- **A last scan for copyrighted content.** Nothing of the game beyond what
  the Decisions allow: no pictures, animations, modules, samples or other
  files of the player's copy, no raw decompiler dumps, nothing of them in
  `tests/`, `tools/` or the git history.
- **The README for players only:** what the port is, getting and running
  it, importing their copy, the port menu and `reunion.ini`. Developer
  material (building from source, tests, the docs) moves out of it.
