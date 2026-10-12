# Main program and screen state machine (1000:01A1)

Source: the Ghidra decompile of `Main` (one procedure, 2273 lines of C) plus the
screen table at `DS:5114` (`Ui::Screens`, 38 records of 25 bytes, index
1..38 = `Game::CurrentScreen`). `?` marks a guess. Names are from
`ghidra/symbols.txt`. `Game::` here is `Sim::` in `globals-evidence.md`: same variables.

## 1. Startup (before the loop)

1. Unit initialisers, in this order: `System::InitSystem`, Crt, MinMax, **Ems**,
   Gus, SoundDrv, Sound, AdlibMusic, Music, Gfx, RawText, VecLoad, GfxPath,
   Message, Surface, Groups, Intel, Satellite, SaveLoad, GrWar (each `X::Init`).
   Sound's init reads `save\setup.spd` (see segments.md).
2. `Gfx::ScreenSeg := SegA000`, `Randomize`, `Music::InitMusicDevices`
   (AdLib/GUS/SB detection), `405f:1014` (INT 10h mode 13h, then port 3C2h
   tweak: sets the misc-output sync bits), `Mouse::HotspotCount := 0`,
   `Command::SetupCommandHotspots`, `4758:0314` (mouse init: INT 33h, handler
   `4758:0000`, 320x200 range), `4758:040a` (hide cursor).
3. `Title::InitTitleAndMainMenu` (321d:026f): loads charset1/iconmain, picks
   `Game::Hero := Random(2)+1`, shows `grafika\options`; then it tests the
   word `DS:5DAA`: **3 = Halt** (`405f:1933`), **1 = show `grafika\choise` pic and wait for
   click**. `DS:5DAA` (`Game::StartChoice`) has initial value 1 in the file and is set to 1, 2 or 3 by the start menu (`Title::StartMenu`, 321d:0155); previously believed to have no writer in
   REUNION.PRG, so it is presumably patched by the launcher (start.exe) ?. It decides
   new game (other) versus load (2).
4. **Memory check**: `MemAvail < 238,000` bytes (`0x3A1B0`), or `266,000` if
   `SoundDrv::GusReady` -> `405f:1999` prints "Not enough memory! Please run
   REUNION with more conventional memory or EMS (max 256k) !" and Halts. (EMS
   keeps `Groups`/`Buildings`/`Charset` in a mapped page: `4719:02d6` re-maps
   them at the top of every loop pass. The port has neither EMS nor the
   check.)
5. `Title::SetMainMenuPalette`, `Groups::InitPlanetForce`,
   `Music::ApplyMusicTrack`, `Command::LoadCommandGfx`, `3a1b:0019` (build menu
   bar), menu bar flags `7C62=7C63=1`, `7C64=1`, `3a1b:0704`, palette fade-in
   (unless loading, `5DAA==2`).
6. **New-game state set in code**: `CurrentScreen=PrevScreen=1`, `ClockRunning=true`,
   `Year=0xB6F (2927)`, `Month=8`, `Day=13`, `Hour=23`, `Credits=0x1D4C0 (120000)`,
   `ProductionRate=10`, `StoryStage=1`, `CurSystem=1`, groups/log cleared, `DS:5D58 := Random(400)+6000`
   (?), `Cheat::Used/Requested=false`. `405f:134c` draws the date, `SaveLoad::LoadInit`
   reads `save\init` (BuildingCount + Buildings: starter data).
7. If `5DAA == 2` (load): `PrevScreen=1; CurrentScreen=12; ScreenSwitch=true`, i.e. the
   Disk screen opens first (its palette is not faded in: `LoadPic(..., DS:5DAA != 2, ...)`); DiskScreen clears `5DAA` after
   setup. Otherwise the game starts on screen 1 (command centre).
8. **Copy protection is not in the startup path**. It runs from
   `Sim::AdvanceHour` (every game hour, `CheckToken`, segment 24eb) and the loop
   calls `PlanetInfo::RandomizeProtectPage` with chance 1/200 per frame once
   `FrameCount (DS:5DA6, longint with 5DA8) > 30000` and `Ticks >= 100` and
   the clock runs. The manual-word prompts are in 24eb/2693 (see segments.md).
   Keyboard is ignored by the game: `Mouse::Poll` drains and discards keys.

## 2. The main loop (`do { ... } while(true)`; never returns)

One pass of the outer loop handles one *event* (a click, a screen switch, a
hotspot release), not one frame; frames are spent inside the two inner loops.

**Phase A: wait for an event** (skipped if `ScreenSwitch`, `8A83` or `8A88` set).
Inner `do ... while` loop; each iteration is one frame, in this order:

1. If `FrameCount>30000` and `Ticks>99` and clock running: `1/200` chance of
   `RandomizeProtectPage`.
2. `Sound::FreeSampleIfIdle`.
3. `if ClockRunning then Ticks++`.
4. **`if Ticks > 100`**: hourly simulation, in this order: `Sim::AdvanceHour`,
   `ProcessStoryTimers`, `ProgressResearch`, `2e4b:09d3` (alien/story timer ?),
   `ProgressProduction`, (`Hour==0`: `ProcessAllBodyEvents`, daily),
   `GrowStaffExperience`, `UpdateBuildings`, `UpdateColonySetups`,
   `MineAllBodies`, `ProcessTravellingGroups`, `CheckEnemyAttacks`,
   `ProcessPubTimers`; then `Ticks := 0` and `405f:140a` redraws the credits.
5. `Mouse::Poll` (snapshot Buttons/ClickX/ClickY), `Cheat::Apply` if
   `Cheat::Requested` (never set; dead).
6. **Vsync wait**: spin on port 3DAh while bit 3 set, then until bit 3 set
   (returns at the start of vertical retrace).
7. `405f:1c21` palette flash step (`Gfx::FlashStep/FlashCount`), `FrameCount++`.
8. Per-screen animation (only if `ScreenSwitch` false): screen 20 (map scroll
   anim `2ef2:1da3` every `Ticks%10==8` in modes 1/2/6, plus `2ef2:15de`), vec
   model `DAT_7E4E` -> `VecLoad::DrawFrame`, text box `DAT_7C4A` -> `398f:0179`,
   screen 3 `AnimateCurrentResearch`+`AnimateSelectionFrame`, screen 1
   `TickAmbientLights`+`TickPanelDoorAnim`, screen 17 `ShipMove::AnimateGroupView`,
   screen 29 `SpWar::PlayWarAnimScript` (unless `77DA`), screen 32
   `GrWar::BattleTick` or `AnimateResultScreen` (`770B`), screens 7 and 18
   `PlanetInfo::UpdateOrbitPositions`, screen 25 `Pub::UpdateQuestionHover`,
   screen 34 `2e4b:092e`, screen 37 `PlanetList::RefreshList` when `Ticks==0`,
   screen 23 `Pub::UpdatePubAnimations`.
9. **Hit test**: `PrevHotspotHit := HotspotHit`; `4758:05bc(HotspotHit)`
   (below); if the menu bar is in its second state (`7C63==0`) ids 1..6 map to 7..12;
   on change, the hint line is drawn (`405f:11e8`, text from `DS:7C51+0x15*n` for
   menu buttons, or the Mouse hotspot's label string); `8A82` = "a button": the
   menu toggle, a menu button with a command, or a Mouse hotspot with byte `+19h`
   set (`THotspot.Repeat_`, a name the wrong way round: set means it does *not*
   repeat).
10. Loop ends when (`1000:089d-08f6`): `ScreenSwitch`, or `8A80` (quit flag ?), or
    (button down over a hotspot, and either `8A82` clear or the button state just
    changed), or (button up, the state just changed, `8A82` clear, and the hotspot
    the same as on the previous frame), or (`8A87` cursor-hidden flag and no
    right/other button). Effectively: a press on a hotspot, a release, or a screen switch.

**Clicks and handler passes.** Every end of phase A is one pass of the screen's
handler with `HotspotHit` set. A button (`8A82`) ends it once, on the press. Any
other hotspot, the clock button (0Eh) among them, ends it on the press, again on
every frame the button stays down over it (auto-repeat), and once more on the
release over it. So a click held n frames runs such a handler n+1 times, unless
the handler's own waits swallow the frames (cargo's 4-frame row flash: a 4-frame
click moves two loads of ore). Measured in the port with the playthrough fixture
on the group grid (screen 22, hotspots set with `+19h` = 0): a press held 1, 4,
12 or 40 frames and released in place added 2, 5, 13 or 41 ships; slid off to an
empty point before the release, 1, 4, 12 or 40. This is the original's loop (the
condition above, transcribed in `main_program.c` `wait_for_event`), not a port
bug. The fixture's `UI_PRESS` (tests/play/ui.h) presses until the loop has
polled the press (one frame when it waits for input) and slides off.

**Hit-test hotspot ids** (`4758:05bc`): `0` none; `1..12` menu-bar buttons
(top strip, command id at word `DS:7C64+0x15*n`, label at `7C51+0x15*n`); `0xD`
menu-bar toggle (x>=0x120,y<=0x20); `0xE` clock/fast-forward button (x 0xDA..,
y 0x22..0x2D); `>=0x15` = `Mouse::Hotspots[id-0x14]` (27-byte records, handler-defined,
Main compares `HotspotHit==0x15` for the main play area).

**After Phase A**: if `ScreenSwitch`, `HotspotHit:=0; ScreenEntered:=true`.
`PressHotspot := HotspotHit`.

**Phase B: held button** (when `8A83` set, which is set when the previous pass
pressed a "pushable" button): loops frames exactly like steps 2-8 above (with the
hourly block skipped on screen 29 and `ProgressProduction`/`2e4b:09d3` in swapped
order) while `Mouse::Buttons` is down and no screen switch. On release,
`ReleaseHotspot` is hit-tested; if it differs from `PressHotspot` then
`HotspotHit := 0` (cancel: click must release on the same hotspot).

**Post event**:
- `8A80` set -> `405f:1933` fade to black and **Halt** (quit).
- `HotspotHit==0xD` (menu toggle) -> button sound + `3a1b:0586/0643` slide-animate the bar
  (`7C63` flips).
- `HotspotHit==0xE` (clock button) -> `Ticks := max(Ticks, 100)` (95 if right button
  held `8A7A`), so the next frame runs an hour: **fast-forward = one hour per click/repeat**.
- `MenuCommand := 0`, `ScreenEntered:=false` unless `ScreenSwitch`.
- If a menu button (1..12) was hit: `MenuCommand := word[7C64+0x15*hit]`; then for
  `i = 1..38`: if byte `+0B` of `Screens[i]` (a shortint) equals `MenuCommand`,
  `CurrentScreen := i; ScreenEntered := true` (**last match wins**, so cmd 7 goes to screen 9, not 6).
- If `ScreenEntered or ScreenSwitch`: `398f:011a` (close any text box). If `ScreenEntered`:
  `Mouse::HotspotCount := 0`.
- **Buffer freeing**: for each screen `s`, if `CurrentScreen != s && PrevScreen == s
  && ScreenEntered` free the buffers `s` allocated (command centre `E600h/4C40h/5280h/5B00h` +
  room anim; faces `SzFace` 7B0h; research 6901h; miner 8980h; info `VecLoad` buffers + AF00h + `SzTalal`; star map
  6900h/BCC0h/24000 (shared with 18); planet-info `SzFaj`; disk 108h + anim 11 + five bufs;
  ship info BCC0h+5000h; group view 9100h+BCC0h+1E1Bh; unit assign 3C00h (+2x333h
  unless going to 32); planet surface 820h, map, `Surface::BufA..D`, 2A30h, 2x1518h;
  group 2300h; pub AF00h+64000; advisor 131h; pub-dialog 500h; space battle
  ~12 buffers; ground battle 2x333h+2860h; screen 33 and 34 `RestartMainTheme`; colony list 4B0h).
  Returning to screen 7 from 19/29/32 re-applies `Music::ApplyMusicTrack`.
- `CurrentScreen<0` -> negated (negative = "enter without reloading"?).
- If no switch: re-read `MenuCommand` for menu hotspots and play the button sound
  (`Ui::ButtonSounds[cmd]` if `SoundFx==2`, else `x`).
- **Screen entry** (`ScreenEntered`): clear hint line; if `Screens[cur]+0A` -> `3a1b:0019` reload
  menu bar buttons; if `+09` -> `Gfx::ShowGrafikaPic(Screens[cur].Name)`; `HotspotHit:=0`;
  `ScreenSwitch:=false`.
- **Dispatch** (section 3): a chain of `if CurrentScreen==N and not ScreenSwitch`; the
  handler is called every pass, with `ScreenEntered` true on the first. Handlers
  set `ClockRunning`.
- Last statement: `if ScreenEntered then PrevScreen := CurrentScreen`.

### Switching screens
Two ways, both end with `ScreenEntered` true for one pass: (a) menu button whose
command matches `Screens[i].+0B`; (b) code sets `CurrentScreen := n; ScreenSwitch := true`
(handlers do this; the rest of the pass skips handlers, the next pass sets
`ScreenEntered`). `PrevScreen` is therefore the screen *before* the one entered
while `ScreenEntered` is true (handlers read it, e.g. planet surface from 1 or 17/22/16).

## 3. Screen table

Columns: id (dec/hex), table name (picture `grafika\<name>.pic` if the pic flag is set), `pic`=`+09`,
`mb`=`+0A` reload menu bar, `cmd` = `+0B` menu command that opens it (255 = none, only code),
handler (called from Main). Clock: state set by Main before the handler (T running, F
stopped, - not touched).

| id | hex | name | pic/mb | cmd | handler (addr name) | unit | clock | reached from |
|---|---|---|---|---|---|---|---|---|
| 1 | 01 | main | 0/1 | 24 | `3abd:005c Command::HandleCommandCentre` | Command | T | start; menu cmd 24; back from 33, 17, Planet exit |
| 2 | 02 | faces | 1/1 | 14 | `3fab:0032 Hire::HireScreen` | Hire | T | command centre door (hotspots 0x14-0x20 in 3abd) / cmd 14 |
| 3 | 03 | research | 0/1 | 2 | `2d66:0029 Research::ResearchScreen` | Research | T | cmd 2 |
| 4 | 04 | miner | 0/1 | 8 | `2527:001f FUN_2527_001f` (droids/miner ?) | 2527 | T | cmd 8; planet surface (Main sets 4 at ~line 930) |
| 5 | 05 | info | 0/1 | 23 | `32b4:0057 FUN_32b4_0057` (shop/production) | 32b4 | T on entry | cmd 23 |
| 6 | 06 | ship | 1/1 | 7 | none (unreachable: cmd 7 maps to 9) | - | - | dead |
| 7 | 07 | csillag | 0/0 | 34 | `18e0:00e8 Launch::MapScreen` | Launch | T | cmd 34; return from battles, planet exit (cmd 0x2E) |
| 8 | 08 | bolygo | 0/0 | 9 | `1413:000a FUN_1413_000a` | 1413 | T | cmd 9 |
| 9 | 09 | keresked | 1/1 | 7 | none | - | - | cmd 7 (dead end ?) |
| 10 | 0A | (empty) | 0/1 | 6 | `264a:01b8 FUN_264a_01b8` (credits text, on entry only) | 264a | F | cmd 6 |
| 11 | 0B | uzenet | 0/1 | 4 | inline: `3a1b:0932`, `Message::DrawMessageLog`, palette (on entry) | Message | T | cmd 4 |
| 12 | 0C | disk | 0/0 | 1 | `1a3e:103b SaveLoad::DiskScreen` | SaveLoad | F | cmd 1; start when loading |
| 13 | 0D | transfer | 1/1 | 3 | `2583:0029 Cargo::CargoScreen` | Cargo | T | cmd 3 |
| 14 | 0E | - | 0/1 | none | none | - | - | unused |
| 15 | 0F | - | 0/1 | none | none | - | - | unused |
| 16 | 10 | (empty) | 0/0 | 13 | `26fe:001b ShipInfo::ShipInfoScreen` | ShipInfo | T | cmd 13; from planet surface cmd 0x2B |
| 17 | 11 | MUSZI | 0/0 | 16 | `22ca:0105 ShipMove::GroupScreen` | ShipMove | T | cmd 16 |
| 18 | 12 | (none) | 0/0 | none | `18e0:07ec Launch::DestinationScreen` | Launch | F | from group screens |
| 19 | 13 | grbeoszt | 0/0 | none | `1430:000f GrWarUnits::UnitAssignScreen` | GrWarUnits | F | after space battle win (29) or before ground battle |
| 20 | 14 | planmain | 0/0 | 41 | inline in Main + `2ef2:*` (map `2ef2:15de`, redraw `2ef2:2c52`...) + `Satellite::HandleBodyActions` | 2ef2/Surface | T | cmd 41; from command centre (sets New Earth: system 1 planet 5) |
| 21 | 15 | beoszt | 0/1 | none | `2b8d:0773 Groups::NewGroupScreen` | Groups | F | from group lists |
| 22 | 16 | beosztas | 0/0 | 49 | `2b8d:0002 Groups::GroupsScreen` | Groups | T | cmd 49; planet-surface cmd 0x42 |
| 23 | 17 | (empty) | 0/1 | 45 | inline: `Pub::LoadPubGraphics`, `SetupPubHotspots`; hotspot 0x15..0x1E -> 25 | Pub | T | cmd 45 |
| 24 | 18 | duma | 0/1 | none | `1991:0000 Advisor::AdvisorScreen` | Advisor | - | command centre |
| 25 | 19 | (none) | 0/1 | none | inline (pub question menu, `Pub::BuildQuestionMenu`, `UpdateTopicAvailability`) | Pub | F | click person in screen 23 |
| 26,27 | 1A,1B | - | 1/1 | none | none | - | - | unused |
| 28 | 1C | KOLONIZ | 0/1 | none | `2841:0060 FUN_2841_0060` (colonise) | 2841 | F | planet surface etc. |
| 29 | 1D | SPWAR | 0/1 | none | inline + `SpWar::EnterSpaceBattle`... (see 4) | SpWar | F | enemy encounter; ends -> 7 or 19 |
| 30,31 | 1E,1F | - | 0/0 | none | none | - | - | unused |
| 32 | 20 | GRWAR | 0/1 | none | inline + `GrWar::EnterGroundBattle`, `BattleTick` | GrWar | F | from 19 |
| 33 | 21 | (none) | 0/1 | none | inline: plays `atv<n>` music by `DS:78C8`, then -> screen 1 with `PrevScreen=0x21` | Main | F | story event interstitial |
| 34 | 22 | (none) | 0/1 | none | inline + `2e4b:*` (alien contact) | 2e4b | F | story events |
| 35 | 23 | (none) | 0/0 | none | `Command::ShowDeathEnding` (3abd:17dd) | Command | - | ground battle lost at Earth (system 1, planet 5, moon 0) |
| 36 | 24 | (none) | 0/0 | none | `Command::FadeOutAndHalt` (3abd:1a47) | Command | - | ground battle won against body owner `0xC` |
| 37 | 25 | colinfo | 1/1 | 74 | inline `PlanetList::*` (1390) | PlanetList | - | cmd 74 |
| 38 | 26 | disk | 0/0 | none | none (id 12 is the live disk screen) | - | - | unused duplicate |

Menu command -> screen (cmd: screen): 1:12, 2:3, 3:13, 4:11, 6:10, 7:9, 8:4,
9:8, 13:16, 14:2, 16:17, 23:5, 24:1, 34:7, 41:20, 45:23, 49:22, 74:37.
Every pic/menu-bar byte is read from the table; the menu-bar command ids of each
screen start at `+0D` (`+0C` = count).

## 4. Battle chains and endings

- Space battle (29): on entry sets system/planet/moon from `DS:7798/779A/779C`, plays `space`
  music, `SpWar::EnterSpaceBattle`. MenuCommand `0x3E` = retreat, `0x41` = end
  (battle over): on win/loss calls `29b9:03d2/0487`, then screen 7, or 19 (ground
  assignment) when `DS:7788` set and result fits; sets story flags (`StoryStage` 3, 4).
- Ground battle (32): `MenuCommand 0x48` = retreat, `0x41` = end -> screen 7. If the
  player lost and the body is Earth (1,5,0) -> **35 (death ending)**. If won and the
  body's owner is race 12 (`0xC`) -> **36 (final ending)**; owner 3 -> `StoryStage=5`;
  owners 7-10 all eliminated -> `StoryStage=6`.
- **Game end**: no victory screen exists in REUNION.PRG. Both endings play the
  music track `failure` (`1000:0181`, same string for both; odd for a win ?), then:
  35 = `ShowDeathEnding` (death pics `grafika\deathsz<Hero>`/`death<Hero>`, animation, then
  `System::Halt(15)`, 3abd:1a3d); 36 = `FadeOutAndHalt` (fade palette, `Halt(10)`,
  3abd:1a5f). Process exit returns to the launcher (`start.exe`), which tells
  the cases apart by the exit code: 10 runs `VICTORY.PRG`, 15 the game again
  (`src/main.c`).
  Quit by `8A80` also fades and Halts.
  `DS:8A80` is cleared in `Mouse::Poll`; its setter was not found (outside Pascal code, mouse callback ?).

## 5. Timing

- Pacing is **vsync only**: each frame waits for the VGA retrace (port 3DAh bit 3,
  start of retrace), mode 13h = ~70.09 Hz. No PIT/timer is used by the loop.
- `Game::Ticks` counts **frames while `ClockRunning`**. The hour fires when `Ticks > 100`
  (checked after the increment, so every 101 running frames, ~1.44 s at 70 Hz).
  That gives ~34.6 s per game day, ~17 min per 30-day month.
- Time stops (ticks do not increase) on the screens with clock F above (disk, battles, pub
  dialog, destination, new group, colonise, credits, alien contact). Screen 24/37 inherit the
  last value (advisor and colony list do not touch it).
- **Fast-forward**: the clock button (hotspot 0xE) forces `Ticks` to 100 (95 with right button),
  so each press/repeat produces one hour on the next frame.
- Sim runs inside the frame loop, so a slow hour (many groups) costs extra frame time; on a fast machine the frame is exactly one retrace.
  The port should run the loop at 70 Hz (or count 101 frames/hour at any fixed rate;
  the original speed is 70.09 frames/s).
- Animations (research, door, ambient lights, orbits, surface) advance once per frame
  with `Ticks%10` style phases, i.e. also locked to 70 Hz. `FrameCount (DS:5DA6/5DA8)` counts
  every frame (all screens, including paused) and only feeds the protection check.
- Phase B (button held) and phase A both loop at one vsync per iteration; a click is detected
  at frame granularity (event = press, release, or switch).
