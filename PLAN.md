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
  build mode.
- **Bugs of the original:** 18 are fixed behind switches
  (`src/game/fixes.h`): those that can make the game unwinnable or break
  the economy, and those that unbalance colonies and battles or lose
  information. The CD release fixes four of them; `league_tip` is taken
  from it (docs/versions.md). The rest are harmless and kept, each
  described where the code reproduces it. Of
  the 34 bugs found in the port while playing it, 33 are fixed and one
  turned out to be the original's behaviour; their write-ups are in git
  history (`docs/port-bugs.md`, deleted 2026-10-12).
- **Tests:** all 90 ctest tests pass on the release build, the full
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

**Saves.** An improvement keeps its state in fields the original already
has where it can, so the save stays in the DOS format and still loads in
the original, which then plays it by its own rules. Trade routes (1.1) and
the building table past 1,000 (1.3) have no room there, so they break
with the DOS format:

- **When.** A save is written in the port's own format while
  `trade_routes` or `no_building_limit` is on, or while the game holds
  something the DOS format can't (a route, more than 1,000 buildings,
  e.g. after loading such a save with both switches off). Otherwise it is
  the DOS format, as now.
- **A clean break.** The original takes a slot as used if
  `save\spidysav.<n>` exists and reads it as raw blocks with no checks, so
  a different layout under that name would load garbage or stop on an I/O
  error. The port's format therefore goes in `save/reunion.<n>`, which the
  original never opens: there the slot shows "Empty" and can be saved
  over. Writing a slot in one format deletes the slot's file in the
  other, so a slot has one file. The disk screen lists and loads both.
- **The format.** A magic and a version number, the slot name, the DOS
  blocks in their order with the building table as its true count and
  records, then the routes. The version lets later improvements add to
  it; a newer version than the port knows is listed but refused on load.
- **Loading** a `reunion.<n>` save works whatever the switches say. With
  `no_building_limit` off, the buildings past 1,000 stay but no new one
  is allowed; with `trade_routes` off, routes are kept but not flown.
- **Warning.** The F1 menu's Improvements section shows, while either switch is on:
  "Saves made now can't be loaded by the DOS game." The two switches'
  own descriptions say it too.
- **Test.** Each switch on writes `reunion.<n>` and deletes
  `spidysav.<n>`, and both off does the reverse; a save that still holds
  routes or more than 1,000 buildings is written as `reunion.<n>` with
  both off; save and load in the port format restores everything; an
  unknown version is refused; the DOS-save tests are unchanged.

#### 1.1 Trade routes

Ore is hauled home by hand, group by group, for the whole game. Switch
`trade_routes`: a trade group can be given a route and then flies it on
its own.

- **A route** is a list of up to 6 pickup stops (bodies the player owns)
  and a drop-off, New Earth. A loop: fly to each stop in order, dock and
  load every ore there that fits; then fly to New Earth, dock and unload
  all of it; start again. If the group is full at any stop, it goes
  straight to New Earth, unloads and starts again from the first stop. A
  stop with nothing to load is passed by without docking.
- **Docking takes time.** Loading at a stop and unloading at New Earth
  each take 5 game hours, during which the group stays docked. They are
  counted on the game clock (an hour is 101 frames, about 1.4 s), so they
  pause when the clock stops, follow the clock button, and save and replay
  exactly.
- **Rules stay the game's:** the group needs a hired pilot whose tier
  reaches every stop, a Galleon to leave a system, and travel takes the
  usual time. Loading and unloading use the cargo screen's own booking
  (`Cargo_CargoScreen`, `Cargo_ShipCapacity`) rather than clicks, and
  unloading stops at New Earth's storage: what doesn't fit stays aboard
  and the route goes on.
- **ROUTE button.** A trade group's cockpit toolbar
  (`Satellite_SetupGroupScreenMenu`) gets ROUTE just before PLANET MAIN.
  In orbit that pushes PLANET MAIN and the body's actions to the
  toolbar's second page; in flight ROUTE follows TRANSFER.
- **The editor** is a port screen in the equipment screen's frame
  (`BEOSZTAS`): its header (group name, type, the arrows to the previous
  and next group) and location line as there. Code paints a list over the
  unit grid (Nr and Stop, six rows 10 pixels apart like the grid's),
  copies the texture beside the footer's "Stock:" label over it, and
  writes two footer lines: "Unload at" New Earth, and the route's state
  (not running, flying to stop n, loading at stop n with the hours left,
  or ended and why).
  - Clicking a row selects it, drawn in the cargo screen's highlight
    (`Gfx_DrawTextFC`). The equipment screen's red arrows (`BENYIL`) move
    the selected stop up or down, each shown only when that move is
    possible. While the route runs, the stop it is heading for has its
    number drawn in colour F6, the font having no marker character.
  - Toolbar: BACK TO M.SCREEN, CONTROL PANEL, then ADD STOP, REMOVE STOP
    and START ROUTE (the OK icon) while the route is stopped, and only
    END ROUTE (the ABORT icon) while it runs: a route is ended before it
    is edited. As in the original, a button that can't act isn't shown
    (no REMOVE STOP or START ROUTE without stops).
  - ADD STOP opens the destination screen (`Launch_DestinationScreen`)
    unchanged. A click on a body adds it after the selected stop, or at
    the end, and returns; ABORT returns without one. Refused with a
    message box, as the original refuses a move: not the player's colony,
    out of the pilot's reach (the original's text), already on the route,
    route full.
- **New toolbar commands** take ids past the original's 78
  (`Ui_ButtonLabels` and `GfxPath_CommandIcon` are DS arrays of that
  size), with their labels and icons in a port table that `GfxPath`
  draws from.
- **Icons** are built at run time from the player's `ICON.ALL`, so no
  pixels of the original are compiled in (Decisions, Distribution). ADD
  STOP and REMOVE STOP are PLANET INFO with its "?" erased and a + in the
  "?"'s greens or a − in END TALK's reds drawn by code. ROUTE is PLANET
  INFO's planet and ADD MINER STATION's small planet on a loop drawn in
  those greens. START and END ROUTE use OK and ABORT, as OK, BUILD IT and
  ABORT MOVE do.
- **The groups list** gives a group on a route a green slot square (red
  stays the selected group's), and the info panel adds "Route: to stop
  n/m".
- **Notifications.** A route's traffic is silent: arriving, docking,
  loading and unloading add no log entry, sample or flash, where a
  group's arrival otherwise does all three
  (`Message_AddMessageLogEntry`), and the log holds only 15 entries.
- **It stops** when the player gives the group any other order, the
  group is disbanded or loses a battle, a stop is lost to an enemy, or
  the pilot can no longer reach a stop. The editor's footer says which
  and why, and one log entry is added (sample and flash, no box).
- **Where it lives.** Routes are stepped where groups arrive
  (`Sim_ProcessTravellingGroups`) and their docking hours each game hour
  after it. The DOS records have no room for them, so they are saved in
  the port's format (Saves, above), the hours left included; a route
  names its group by index and must follow it when the list shifts after
  a disband. New texts (captions, footer lines, refusals, log entries)
  go in `lang_texts.c` with German, French and Italian.
- **Test.** A route of two stops runs several loops, each stop and New
  Earth docked for 5 hours; a full group goes home early; New Earth full;
  each stop condition, with its one log entry and none for routine
  traffic; save and load mid-loading keep the route and its hours. The
  editor is built through clicks on the playthrough fixture: add, move,
  remove, start, end, and the refusals.

#### 1.2 Playing with the keyboard only

The original is played with the mouse; the keyboard is used only for
typing names. Switch `keyboard_play`: every screen can be played with
keys alone, and the keys can be remapped (`[keys]` in `reunion.ini`).

- **Hotspots.** Every screen registers its clickable areas
  (`Mouse_SetHotspot`, up to 66, with a caption). Tab and Shift+Tab move
  between them (in screen order, top to bottom), with a frame drawn
  around the current one and its caption shown; Enter is a left click and
  Space a right click on it.
- **Grids and maps.** Where a click lands on a cell rather than a hotspot
  (the surface map, the star map, the cargo rows, the battle map), the
  arrow keys move the cursor one cell and Enter/Space click there.
- **Shortcuts** for the main screens and for leaving one (Escape), and
  for the game date (speed), shown in the port menu's help.
- **Test.** Drive a screen of each kind (a menu, the surface, the star
  map, a battle) with key events alone.

#### 1.3 No building limit

All colonies share one table of 1,000 buildings (`Game_Buildings`, 14
bytes each, in the game data block from 2840h up to the charset at 5EF0h).
When it is full a new building is refused (`Surface_AddBuilding`,
`Surface_BuildOnCurrentPlanet`; `fixes.building_limit` keeps it from
being paid for), and so is every building a new colony would get. A large
late game runs into it.

Switch `no_building_limit`:

- **The table.** Move it out of the game data block into a heap array
  that grows as buildings are added; records 1-1,000 stay where they are
  so the data block keeps its layout. Indices are `int16_t` everywhere
  (the surface's building layer holds `uint16_t`), so the real cap is
  32,767; refuse at that as the fix refuses at 1,000. Every `< 1000` /
  `> 999` test goes through one limit function.
- **Saves** are in the port's format while the switch is on (Saves,
  above), with the whole table. The DOS format can't hold more than
  1,000: its block is 14,000 bytes, and a larger `Game_BuildingCount`
  would make the original read past the table into the charset.
- **Check** every loop over the table for a fixed bound, and the code
  that holds a building index in a byte. Deleting a building
  (`Surface_DeleteBuilding`) shifts the later records down; the side file
  needs nothing for that, as it is written from the table.
- **Test.** Build past 1,000, save, load, and check every building and the
  count came back; with the switch off, the 1,001st is refused.

#### 1.4 Smaller candidates

Each needs a short spec before work starts.

| Idea | Why |
|---|---|
| Autosave before every battle and alien conversation, into a slot of its own | The walkthrough's standing order is to save before each, as results are random. The original has 12 slots; the autosave must not take one of the player's |
| A storage-full message, once per body and ore | Production stops without a word when a store is full (Storage, docs/walkthrough.md); players find out by noticing the numbers stand still. Trade routes (1.1) leave ore aboard silently when New Earth is full, so it covers them too |
| Saving anywhere | |
| Universities that count | A University only matters for population (the 80k cap, the mood above 70k); it has no effect on courses and a second one does nothing (docs/walkthrough.md). E.g. each extra University cuts course time; needs a design |

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
