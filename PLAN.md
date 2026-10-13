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
| Editions | English floppy and the English folder of the CD; strict | The port is the floppy's `REUNION.PRG`. The CD's English data differs only in its font sheet (docs/versions.md). An import accepts only files whose checksums are known. `DATA_DIR`/`REUNION_DATA` still point anywhere without checks. The CD's other three languages are planned (Remaining work, 1) |
| Bugs of the original | A bug that changes play gets a switch in `src/game/fixes.h` (all on by default); a harmless one is kept and described where the code reproduces it | With every fix off the port plays like the original, which the golden hashes check |

## Source

`vendor/Reunion_DOS_EN/GRWAR/REUNION.PRG` (English floppy release; the CD's
differs, see docs/versions.md): Borland
Pascal 7.0, real mode, no packing or overlays, ~240 KB of code in 54
segments (813 functions), plus
`INTRO.PRG`, `CREDITS.PRG` and `VICTORY.PRG`, chained by `start.exe`.
How the port cites the original's addresses: docs/port.md, "Addresses".
The file formats are described in the loaders, `src/assets/*.h`.

## Where we stand (2026-10-13)

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
  2930-03-06). Its recording replays to the same end in the plain binary.
  Seeds 1-240 win too (docs/playthrough.md).
- **Port additions:** the F1 port menu (cheats, Sound Blaster/UltraSound
  music, display, skip intro, colony stats, fix switches, import, restart,
  quit), `reunion.ini`, screenshots, headless autopilot, `--replay` and soak
  runs.
- **Ships without the game data:** release builds have none built in. The
  first start imports the player's copy (a folder, a ZIP or the CD's ISO;
  the floppy and CD editions) into the player's folder, checked against
  the list of files the port loads.
- **Bugs of the original:** 18 are fixed behind switches
  (`src/game/fixes.h`): those that can make the game unwinnable or break
  the economy, and those that unbalance colonies and battles or lose
  information. The CD release fixes four of them; `league_tip` is taken
  from it (docs/versions.md). The rest are harmless and kept, each
  described where the code reproduces it. Of
  the 34 bugs found in the port while playing it, 33 are fixed and one
  turned out to be the original's behaviour; their write-ups are in git
  history (`docs/port-bugs.md`, deleted 2026-10-12).
- **Tests:** all 87 ctest tests pass on the release build, the full
  playthrough (`test_play_game`), the golden proof and the two DOS saves
  (`golden_load_DOS_1`, `_2`: loaded, saved back byte for byte, played on)
  included.

Milestones from the original plan: Ghidra database named (done; the
Ghidra project and the Python asset tools were retired 2026-10-12), assets
viewable (done, `tools/`), title screen (done), every screen playable (done),
full game playable start to finish (done), verified against the original
(mostly done, see 5 below).

## Remaining work

In rough priority order.

### 1. The CD's other languages

The CD has complete French, German and Italian games next to the English
one (`REUNION.DEU`, `.FRA`, `.ITA`; docs/versions.md, "The languages"). Add
them as editions of the CD, so a player with the CD can play in any of the
four.

- **What differs.** All four folders share one `REUNION.PRG` and
  `CHARSET1.PIC`, so the texts compiled into the executable (`dsinit.c`)
  are English in every language. Each other language changes about 160
  files: `TEXT\`, the speech in `SOUND\`, and stills with text in
  `INTRO\`, `INTROP\`, `GRAFIKA\`, `WAR\` and `VICTORY\`. `VICTORY.PRG`
  differs per language at the same size: find what changed (probably
  its texts) and compile in each language's version, as the English one
  is.
- **The font.** The CD's executable maps 87 characters (`ÄÖÜäöüß` among
  them) and its sheet has 20 accented glyphs; the port keeps the floppy's
  78 (`gfx_set_cd_charset`). Take the CD's character table for every
  language, and its text drawing's one change: character 1 is drawn as `ß`
  (docs/versions.md). Check how French and Italian text reaches the
  accented glyphs the table doesn't name.
- **Import.** The import takes `REUNION.ENG` only (`source.c`). Let it take
  any of the four language folders (checksums for each language's files in
  `gamefiles.c`), and decide between importing one language (asked on
  import) or all four with a language switch in the F1 menu and
  `reunion.ini`. Update the Editions decision above.
- **Test.** Import each language from the ISO, check its texts and font
  draw, and run the playthrough on each. Its waits read the screen, so
  any that match English text need a per-language form.

### 2. Reverse-engineering loose ends

These matter only for reading the code; nothing depends on them:

- record fields that are still `_xx` fillers or marked `?` in
  `src/game/dseg.h`: `TGroup`'s travel and cargo fields, and bytes of
  `TBody`, `TProduct`, `TBuilding` and `TRace` (`TRace.Status` 0/2/4/6,
  what `TBody.Stores[7..8]` hold; `TRace.KnownSystems` is the race's
  weapon flags, ship classes then ground units);
- DS bytes nobody references, `_pad_XXXX` in `src/game/dseg.h`: the
  1,535 bytes at `9C03–A201`, `A212–A219`, and a few small gaps in the
  typed constants.

### 3. Licence and publication

The game is still under copyright. Decided (2026-10-12): release builds
ship without the art and carry the data taken from the executables
(`src/game/dsinit.c`, the game's typed constants and texts; the cut-scene
tables; the AdLib player's image in `src/game/adlib.c`). The player
supplies the rest (the import, README). Still open: whether to publish the repository itself.
Until then keep it private, or publish only the code (no assets, no raw
decompiler dumps).

### 4. Improvements

Changes to the game's design rather than its bugs: quality of life first,
then mechanics that don't do what the game seems to mean. Each one is a
switch like the fixes, in its own struct (`src/game/improvements.h`, an
Improvements section in the port menu, `[improvements]` in `reunion.ini`),
on by default. The Fixes section's "All off (original)" turns them off
too. The golden hashes, the playthrough and the DOS-save tests run with
every improvement off, so the proof of the original's behaviour stands;
each improvement gets its own test with it on.

**Saves.** An improvement keeps its state in fields the original already
has where it can, so the save stays in the DOS format and still loads in
the original, which then plays it by its own rules. Trade routes (4.3) and
the building table past 1,000 (4.5) have no room there, so they break
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

#### 4.1 More Miner stations on a mining-only body

A body with only a Miner station is barely worth the station. It mines
with one "droid" (`TBody._0A` = 1) in 30% of hours (`Sim_MineBody`), so
deposit / 10 × 0.3 t per hour per ore: 2.1 t/h of Kalium on Gallilei (K7),
where a colony's 9 mines on the same deposit make 63 t/h. Its 10,000 t
would take about 200 days to fill.

Switch `more_stations`:

- **Adding.** A body the player owns with no colony and 1-4 stations
  gets an "Add Miner station" action in the body menu when a trade group
  there carries one (the same check as the first station's icon in
  `Satellite_SetupGroupScreenMenu`). It takes one station from the group
  (`Satellite_TakeTradeShips`), adds 1 to `_0A` and places a Miner station
  building (19h) as the first one does (`Satellite_HandleBodyActions`). At
  5 the action goes away. A Galleon carries 5 stations, so one trip can
  fill a body.
- **Storage.** 10,000 t per ore per station, so 50,000 t at 5
  (`Groups_PlanetStorage` without a planet force returns 10 × `_0A`
  instead of `fixes.station_storage`'s 10; the improvement uses that path
  whatever the fix's switch says).
- **Output.** Each station mines as one droid does, deposit × stations /
  10 t per hour per ore. **Recommendation: drop the 30% roll** for
  station-only bodies while the switch is on. With the roll, 5 stations
  on K7 give 10.5 t/h, still a sixth of a colony; without it they give
  35 t/h, about half a full colony for ~48,500 credits of stations and
  no colony to run. (Dropping the roll changes the random stream, which is
  why the golden runs pin the improvement off.)
- **Display.** Planet info shows "*n* Miner stations" for such a body
  instead of "*n* Miner Droid"; the droids screen's output column already
  uses `_0A`.
- **Later.** Founding a colony on the body keeps the station buildings,
  each a mine slot with 10,000 t, as the single station is today. Check
  what the colony founding does with `_0A` (the stations would become that
  many droids). A body taken by an enemy loses its stations, as today.
- **Test.** Add stations up to 5 and check the sixth is refused, the
  storage is 10,000 t per station and the output is deposit × n / 10 every
  hour; with the switch off nothing changes. Check that 5 station buildings
  fit on the smallest mining-only surface (`fixes.autoplace_when_full`
  adds them unplaced if not). Then update docs/walkthrough.md's Storage and
  Mining notes.

#### 4.2 Build the same building again

Placing many copies of one building (the guide builds 20, then 60
Derricks on New Earth) means going back to the build menu for each one.
Switch `repeat_build`: placing a building with the **right** button
places it as the left button does, then keeps the build mode (2) with the
same kind selected, so the next right click places another. A left click
places as today and ends the mode. The repeat stops by itself when money
runs out or the spot is taken (`Surface_UpdateModeForCell` drops to mode
1 as now). Check first what a right click does on the surface today.

#### 4.3 Trade routes

Ore is hauled home by hand, group by group, for the whole game. Switch
`trade_routes`: a trade group can be given a route and then flies it on
its own.

- **A route** is a list of up to 6 pickup stops (bodies the player owns)
  and a drop-off, New Earth. A loop: fly to each stop in order and load
  every ore there that fits; then fly to New Earth and unload all of it;
  start again. If the group is full at any stop, it goes straight to New
  Earth, unloads and starts again from the first stop. A stop with
  nothing to load is passed by.
- **The UI.** A "Route" button on a trade group's screen opens a route
  editor: the stop list with Add (picks a body as the destination
  screen, `Launch_DestinationScreen`, does), Remove and Up/Down, then
  Start and Stop. The groups list marks a group on a route. Decide
  whether the editor is a game screen in the original's style or a port
  panel like the F1 menu (simpler, and the keyboard work in 4.4 gets it
  for free).
- **Rules stay the game's:** the group needs a hired pilot whose tier
  reaches every stop, a Galleon to leave a system, and travel takes the
  usual time. Loading and unloading use the cargo screen's own booking
  (`Cargo_CargoScreen`, `Cargo_ShipCapacity`) rather than clicks, and
  unloading stops at New Earth's storage: what doesn't fit stays aboard
  and the route goes on.
- **It stops** when the player gives the group any other order, the
  group is disbanded or loses a battle, a stop is lost to an enemy, or
  the pilot can no longer reach a stop; a message line says which and
  why.
- **Where it lives.** Routes are stepped where groups arrive
  (`Sim_ProcessTravellingGroups`). The DOS records have no room for them,
  so they are saved in the port's format (Saves, above); a route names
  its group by index and must follow it when the list shifts after a
  disband.
- **Test.** A route of two stops runs several loops; a full group goes
  home early; New Earth full; each stop condition; save and load keep the
  route.

#### 4.4 Playing with the keyboard only

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

#### 4.5 No building limit

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

#### 4.6 Smaller candidates

Each needs a short spec before work starts.

| Idea | Why |
|---|---|
| Autosave before every battle and alien conversation, into a slot of its own | The walkthrough's standing order is to save before each, as results are random. The original has 12 slots; the autosave must not take one of the player's |
| A storage-full message, once per body and ore | Production stops without a word when a store is full (Storage, docs/walkthrough.md); players find out by noticing the numbers stand still |
| Saving anywhere | |
| Universities that count | A University only matters for population (the 80k cap, the mood above 70k); it has no effect on courses and a second one does nothing (docs/walkthrough.md). E.g. each extra University cuts course time; needs a design |

### 5. Checks against the DOS game

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
