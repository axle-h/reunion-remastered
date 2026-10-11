# Port bugs

Bugs found by playing the port, with root cause and status. Bugs of the
original game itself go in [bugs.md](bugs.md) instead.

Each entry: what was seen, root cause (`file:line` in the port, `seg:off` in
the original), fix, and how it was verified. Status: **open**,
**investigating**, **fixed**, **not a bug** (the original does the same).

## 1. "Choose your hero" voice plays at the main menu

- **Seen:** music-module mode only. The voice plays as soon as the start menu
  appears, not after New Game is clicked.
- **Cause:** not a port bug. The original starts the `choise` track at the
  start menu (`321d:016c` calls `Music.PlayTrack('choise')`) and the voice is
  sample 6 of `ANIM\CHOISE.SPD` (pattern 0, row 3).
- **Fix (deliberate deviation):** the menu plays the module with sample 6
  silenced; the hero-select screen restarts it from the start with the
  sample intact, then it loops as normal.
- **Status:** fixed. `audio_music_mute_sample` / `audio_restart_music`
  (`src/platform/audio.c`) mute sample 6 by zeroing its instrument volume in
  libxmp and restart the module; called from `Title_StartMenu` and before
  `Title_HeroSelect` in `src/game/title.c` (module backend only).
  Verified by `tests/test_audio_mute.c`: over the first 2 s of CHOISE.SPD the
  muted mix is quieter than the reference, and after unmute + restart it
  matches the reference to within 1%.

## 2. Satellite sent at the start isn't taken from the stored satellites

- **Seen:** sending the first satellite to another planet doesn't reduce the
  stored satellite count.
- **Cause:** off-by-one product index in `src/game/satellite.c`. The
  decompile's `Products[k]` is already 0-based from DS:5DAC (Pascal
  `Products[k+1]`), but the port subtracted 1 again. `DeployStockSatellite`
  (`28cd:0000`, `dec ds:5E31`) decremented `Research_Products[1]` (Miner
  droid) instead of `[2]` (Satellite) at satellite.c:53/56, and
  `AddBodyActionItems` (`28cd:06cd`, `cmp ds:5E31`) offered menu 0x46 from the
  droid stock (satellite.c:275). So satellites were never used up and a droid
  went instead. Same mistake in the same file: `CheckStoryLocations`
  (`28cd:0050`, products 0x15/0x1E, satellite.c:65/72), `ColonizeBody`
  (`28cd:02a4`, product 6 and `IdeaTimer[6]`, satellite.c:137),
  `AcquireBody` (`28cd:0350`, product 0xB, satellite.c:159) and the colony
  item in `AddBodyActionItems` (`28cd:08e4`, product 6, satellite.c:305). A
  scan of every `// seg:off` function in src/game for literal
  `Research_Products`/`Game_IdeaTimer` indices against the decompile found no
  other mismatch. The carrier path (Groups slot stock `[di+5D92]`,
  `DeductSatellites`) was right.
- **Fix:** use the decompile's indices as they are (`[2]`, `[0x15]`,
  `[0x1E]`, `[6]`, `[0xB]`), with a comment in satellite.c on the indexing.
- **Verified:** `tests/test_satellite.c` deploys a stock satellite at 1:3 and
  1:5 and checks that Satellite stock goes 2→1→0, droids stay at 7, and menu
  0x46 isn't offered with no satellites left. It fails before the fix. In
  gdb, after a headless new game (frame 1000), `Satellite_DeployStockSatellite`
  at 1:4 took Satellite stock 2→1 and left the Miner droid count at 1.
- **Status:** fixed

## 3. New Earth's moon is never discovered

- **Seen:** the moon of New Earth never appears, so the game can't be won.
- **Cause:** the system view read the wrong table. Planet and moon
  visibility in the original is `Galaxy::PlanetStatus[sys, planet]`
  (DS:47CD + 8*sys + planet: -1 unknown, 0 planet known, 1 its moons
  explored), but Ghidra prints that address as
  `Galaxy::BodyIcon[6] + 7 + sys*8 + planet`, and the port took it literally
  as `Galaxy_BodyIcon[sys-1][planet-1]` (DS:4796, a different table; its
  only real use is `357b:25f4`, copied into the orbit records). Six reads in
  `src/game/planetinfo.c` were wrong: `SetupSystemViewHotspots` (`357b:1ac9`
  planet hotspots, `357b:1f32` moon hotspot count), `DrawOrbitingPlanet`
  (`357b:272d`), `DrawOrbitingMoon` (`357b:2800`), `RenderSystemView`
  (`357b:2c56`/`2c97`, the "unknown moons" label) and
  `UpdateOrbitPositions` (`357b:3043`). New Earth's BodyIcon byte is 0, so
  Apollo (PlanetStatus 1 from the start) was neither drawn nor clickable.
  The sim side (`Sim_ProcessTravellingGroups`, `1b8a:2e4e` sets the status
  to 1 on arrival) and the save/load blocks were already right.
- **Fix:** a `planet_status()` helper in planetinfo.c (planetinfo.c:130)
  used at planetinfo.c:932, 985, 1134, 1147, 1214 and 1277.
  tests/test_planetinfo.c placed a Sun planet by setting BodyIcon; it now
  sets PlanetStatus.
- **Verification:** `tests/test_moons.c` (new game: Apollo is the only
  Amnesty moon with a hotspot; Klatoo, Zeus and New Earth are the only
  planets; a satellite carrier arriving at Zeus through
  `Sim_ProcessTravellingGroups` sets its status to 1 and its 5 moons
  appear). It fails on the old code. Headless autopilot run of a new game
  (`random:7:9000:8000` to seed the orbits): the Amnesty map shows three
  planets, New Earth's view shows Apollo, Zeus's shows "unknown moons".
  All 56 tests pass, no warnings.
- **Status:** fixed

## 4. Other planets' moons are known at the start

- **Seen:** the moons of the other planets in the Amnesty system are
  discovered as soon as the game starts. An unvisited planet's moons should
  stay unknown until a ship gets there. Also all seven Amnesty planets are
  on the starmap at the start.
- **Cause:** the same as 3. Zeus's BodyIcon byte is 1, so its five moons
  showed; every BodyIcon byte is 0 or 1 (never negative), so all planets of
  an open system were drawn and clickable instead of only those with
  PlanetStatus >= 0.
- **Fix / verification:** as 3. At the start the original's data has
  Amnesty PlanetStatus = -2, 0, -2, 0, 1, -3, -3: Klatoo, Zeus and New Earth
  are known (the manual describes exactly these three, with Apollo as New
  Earth's moon), and only New Earth's moons are explored. Klatoo is
  therefore visible at the start in the original as well. The others are
  found by the observatory/satellite events in `ProcessBodyEvents`
  (`1b8a:1ddd`-`2011`, -2/-3/-4 -> 0).
- **Status:** fixed

## 5. Travel between planets takes far too long

- **Seen:** a flight in the satellite carrier (and probably any ship) takes
  much longer than in the original.
- **Cause:** not a port bug. The port's flight times match the original's.
  - `Intel::SetGroupTarget` (`29b9:0e66`, `intel.c:408`) sets the legs in
    game hours. To a moon of the same planet: Leg2 = 80-99. To another planet:
    Leg2 = 150-199. To another system: Leg1 = 200-249, Jump = 1, Leg2 =
    200-249.
  - `Sim::ProcessTravellingGroups` (`1b8a:2b2b`-`2bcb`, `sim.c:1009`/`1013`)
    runs once per game hour. Each run subtracts `StaffExp[1]+1` (the hired
    pilot's experience plus 1) from Leg1 or Leg2, or 1 from Jump.
  - The main loop runs that hour every 101 frames, when `Ticks > 100`
    (`1000:060d`-`0665`, `main_program.c:398`). Frames are paced by the
    retrace only, at 70.086 Hz in mode 13h, so one game hour lasts 1.44 s.
  - Without a pilot, which the satellite carrier doesn't need (speed 1), that
    gives 1.9-2.4 min to a moon, 3.6-4.8 min to another planet and 9.6-12 min
    to another system. A pilot with experience 10/20/40 (`Staff::Level`)
    divides those times by 11/21/41.
  - The time box fast-forwards. The original does the same, with the same
    numbers: a left click or hold gives one hour every 6 frames; a right
    click or hold gives one hour per frame (`1000:0c0a`-`0c4f`), so a trip
    to another planet takes about 2.5 s.
  - There is no Real48 or 16-bit overflow on this path. Every value is a
    small integer word.
- **Fix:** none needed.
- **Verification:**
  - Ground truth: the original `SetGroupTarget` and the leg/status part of
    `ProcessTravellingGroups` ran under unicorn (relocated
    `REUNION.PRG`), one call per hour, for 3 RandSeeds × pilot experience
    0/10/40 × moon/planet/system. Examples: 80/151/445 hours at experience 0
    and 8/14/43 hours at experience 10.
  - `tests/test_travel.c` runs the port's routines on the same 27 cases and
    gets the same legs and hour counts. It also boots the real main loop
    under the autopilot and checks that 1010 frames make exactly 10 game
    hours. It exits 77 without the game data.
  - In-game measurements:
    - A headless satellite carrier flight on screen 0x11 to another planet
      took 166 hours = 18085 frames (101 per hour).
    - Without the autopilot, the paced loop measured 1.441 s per hour.
- **Status:** not a bug

## 6. Black flash when changing screens

- **Seen:** a brief black frame between screens, e.g. from the command
  centre to the planet surface.
- **What the original does:** a picture screen loads with `Gfx::LoadPic`
  (`405f:0bef`, via `LoadPicScreen` `405f:0f23` or e.g. `Surface::EnterSurface`
  `2ef2:004a` for `grafika\designer`) with `fade` set: while it decodes into
  an off-screen buffer it writes the old palette scaled by (40-step)/40 to the
  DAC (`4049:0160`, `rep outsb` to 3C8h/3C9h, no retrace wait) 40 times, so
  the old picture fades out over the decoding time. The prologue at
  `405f:0c97` adds `colourOffset` to `fadeFirst`/`fadeLast`, so the usual
  call (offset 40h, fade 0..BEh) fades DAC **40h..FEh** only; the toolbar and
  status lines (colours 0..3Fh) stay lit. The new picture is then copied to
  A000 while 40h..FEh are black, and `FadeIn` (`405f:00bb`: per step wait
  for retrace start `405f:06e6`, then load the DAC; finally all 256) brings
  it up over 6 retraces.
- **Cause:** two port mistakes in `Gfx_LoadPic` (`src/game/gfx.c:536`), both
  in the inline-asm part the decompile drops: (1) the fade range was not
  shifted by `colourOffset`, so the DAC 0..BEh went black, including the
  toolbar, which stayed black until `FadeInSteps` ended with its full
  `SetPalette` (7 retraces of a black top strip, the "flash"); (2) the 40
  fade steps ran with no retrace in between, so the old picture cut straight
  to black instead of fading. `vid_present` itself, the idle polls and the
  retrace mapping were not involved (traced with gdb on
  `timing_wait_retrace`: the presents in the transition are the toolbar
  reveal, `Surface_RebuildRadar`'s wait and `FadeIn`'s, as in the original).
- **Fix:** `gfx.c:555` shifts the fade range by `colourOffset` as at
  `405f:0c97`. `gfx.c:598` waits for a retrace every 8 fade steps
  (`LOADPIC_FADE_STEPS_PER_RETRACE`), a deliberate stand-in for the
  original's decoding time (about 6 retraces on a 486DX-33): the old picture
  now fades out over 5 retraces. Command centre -> planet surface is now:
  toolbar always lit, picture fades out (5 retraces), black for 4 (as the
  original, whose disk loads made it longer), fades in (5).
- **Verification:** per-frame autopilot dumps of the command centre ->
  planet surface and -> research transitions (before: top 49 rows at
  brightness 0 for 7 frames and a one-frame cut; after: top rows unchanged,
  picture area 100/78/58/38/18/0 %). `tests/test_boot.c` (surface
  transition: toolbar never dark, at least 3 fade-out steps before black;
  fails on the old code) and `tests/test_gfx.c` `load_pic` (fade range
  40h..FEh, 5 retraces, levels per retrace).
- **Status:** fixed

## 7. LoadPic did not clip rows to the picture height

- **Seen:** nothing visible; found while tracing 6.
- **Cause:** `405f:0ccb` clips `rows` to the header height before decoding
  (also missing from the decompile); `Gfx_LoadPic` decoded `rows` rows
  whatever the picture, reading past the file data. This also means the
  `docs/bugs.md` entry for `Surface::EnterSurface` (FANIM5 decoded with 0C0h
  rows overrunning `BufD`) is not a bug of the original: it decodes FANIM5's
  48 rows. The soak test's workaround in `src/game/surface.c` (passing
  `FanimHeight`) is reverted to the original's 0C0h, and the entry is
  removed from `docs/bugs.md`.
- **Fix:** `src/game/gfx.c:557`. The `load_pic` test in `tests/test_gfx.c`
  checks that nothing past the picture is written.
- **Status:** fixed

## 8. Pub dialog never moves to the next question menu

- **Seen:** nothing reported; found by the sweep for wrong-global accesses
  after 3. In the pub dialog the question menu should follow each reply
  (and a reply coded 00 ends the talk); by the code, the port stays on the
  menu the dialog opened with (not seen in play).
- **Cause:** `Pub_ShowReply` passed the question code `DS:776A` as
  `TextFile_DrawReply`'s code out-parameter. The original passes `DS:7764`,
  the question-menu number (`1fd2:075f` `mov di,7764h`); only
  `DrawQuestionMenu` passes `776A` (`1fd2:0a38`). So the reply's two-digit
  code overwrote the question code instead of choosing the next menu, and
  `PUB_MENU == 0` (leave) in `screen_pub_dialog` never came from a reply.
- **Fix:** `src/game/pub.c:199` passes `&MENU_IDX`. `tests/test_pub.c`
  (replies 1..3 of `valasz1.loc`: 7764 gets the line's code, 776A is
  untouched) fails on the old code.
- **Sweep:** 664 port functions checked against their disassembly (~3000 named `ds.` uses, ~780 raw DS constants, 126 constant-index sites, ~40 out-of-range Ghidra forms); no other mismatch.
- **Status:** fixed

## 9. Idea messages were shown in quotes

- **Seen:** found by the differential test of `Sim::ProcessStoryTimers`
  (`tests/test_diff_sim.c`, docs/difftest.md): when an idea timer runs out,
  the idea's line of `kitalal.txt` ("The Mining ...") was framed by `"`
  characters in the port, so its message-log entry differed too.
- **Cause:** `Message_ShowKitalalLine` concatenated `'"' + line + '"'`. The
  original (`34b0:01df`) concatenates the constant at `34b0:01dd`, which is
  the one-character string `' '` (`01 20`), on both sides.
- **Fix:** `src/game/message.c:83` uses `" "`; the comment in
  `src/game/message.h` is corrected. `test_diff_sim` fails on the old code.
- **Status:** fixed

## 10. InputLine stored the edited string's pointer in another form

- **Seen:** nothing visible; found by the differential test of
  `Sim::ProcessAllBodyEvents` (a planet discovery prompts for its name).
- **Cause:** `TextFile_BeginInput` (`398f:0059`) stores the caller's far
  pointer in `DS:7C50`. The port only has a host pointer and rebuilt a
  normalised `seg:000x` pointer, where the original holds what the caller
  passed: `DS:1AEA` (a global) instead of `3DD1:000A`, the same address.
- **Fix:** `host_to_far` in `src/game/textfile.c` returns a DS-relative pointer
  for globals and keeps the normalised form (as `GetMem` returns them) for
  heap blocks. `test_diff_sim` fails on the old code.
- **Status:** fixed

## 11. Hire screen: wrong line for the candidate's last sentence

- **Seen:** found by the differential test of `Hire::DrawCandidateInfo`
  (`tests/test_diff_hire.c`): for a candidate better than the one hired, the
  fourth line of the info box (y = BCh) stayed empty in the port, where the
  original types the candidate's sentence from `text\sz_face.raw`.
- **Cause:** a `sz_face.raw` record (A4h bytes) is `string[40]`,
  `string[81]`, `string[40]`: the original adds 0, 29h and 7Bh
  (`3fab:067c`, `06a4`, `06fc`). `face_text` in `src/game/hire.c` used 52h
  for the third string (three equal strings assumed), i.e. the middle of the
  second one.
- **Fix:** `src/game/hire.c:177` uses 7Bh. `test_diff_hire` fails on the old code.
- **Status:** fixed

## 12. Group panel: stocks and names not redrawn after moving units

- **Seen:** found by the differential test of `Groups::DrawGroupPanel`
  (`tests/test_diff_groups.c`). On the group screen (16h) a click on a ship
  or weapon slot redraws the panel with `full = false`; the stock counts
  under the grid (and the ship names / weapon headers) kept their old values
  in the port.
- **Cause:** `Groups_DrawGroupPanel` put the weapon-header, ship-name and
  stock loops inside `if (full)`. The original (`2b8d:0f06`) only skips the
  two `FillRect`s at y=AFh/B9h when `full` is false and always runs the loops
  from `2b8d:0f2e`.
- **Fix:** `src/game/groups.c:415`: only the two fills depend on `full`.
  `test_diff_groups` fails on the old code.
- **Status:** fixed

## 13. New groups got the type name instead of the default name

- **Seen:** found by the differential test of `Groups::InitGroupOfKind` /
  `InitPlanetForce`: New Earth's planet force is "Planet forces" in the port
  and "New Earth forces" in the original; new groups of a kind got "Army",
  "Trade", ... instead of "1st Army Group", "C&T Trade Co.", ...
- **Cause:** `Groups_InitGroupOfKind` copied `Groups::TypeNames[kind]`
  (`DS:5928`, string[13]); the original (`2b8d:185f`, `imul di,ax,11h; add
  di,595Dh`) copies `Groups::DefaultNames[kind]` (`DS:596E`, string[16]).
  The routine is also near (`RET` at `2b8d:1995`), which symbols.txt does
  not say; the test sets `DiffFn.near`.
- **Fix:** `src/game/groups.c:555`. `test_diff_groups` fails on the old code.
- **Status:** fixed

## 14. Group screen lights drawn from the wrong table entries

- **Seen:** found by the differential test of `ShipMove::AnimateLights`
  (`tests/test_diff_shipmove.c`): the six blinking lights of screen 11h were
  drawn at other places and sizes than in the original.
- **Cause:** the light tables are read at `DS:07D6/07E2/07EE/07FA + 2*i`
  (x, y, w, h; `22ca:1ec4..1f28`); the port's macros used `07D4/07E0/07EC/07F8
  + 2*i`, i.e. the previous entry of each table.
- **Fix:** `src/game/shipmove.c:32`. `test_diff_shipmove` fails on the old
  code.
- **Status:** fixed

## 15. Group screen working variables kept in locals

- **Seen:** nothing visible; found by the differential test of
  `Groups::GroupsScreen`.
- **Cause:** the original keeps the page up / down saved category and the
  slot-button variables in globals (`DS:7892` category, `7894` ship slot,
  `7896` weapon column, `7898` available stock, `78A2` weapons returned;
  `2b8d:02bb..0605`); the port used C locals, so those words differed.
- **Fix:** `src/game/groups.c:22` names them and `Groups_GroupsScreen`
  writes them as the original does (the weapon loop variable keeps its last
  value). `test_diff_groups` fails on the old code.
- **Status:** fixed

## 16. Renaming a group stored the name's pointer in another form

- **Seen:** nothing visible; found by the differential test of
  `Groups::GroupsScreen` (Change name), the same kind of difference as 10.
- **Cause:** the original passes `Groups^[list][group].Name`, a pointer in
  `Game_Groups`' segment (e.g. `E000:1380` with the data in EMS), to
  `TextFile::BeginInput`, which stores it in `DS:7C50`; `host_to_far` rebuilt
  a normalised `E138:0000`.
- **Fix:** `src/game/textfile.c:46`: addresses in the group lists keep
  `Game_Groups`' segment. `test_diff_groups` fails on the old code.
- **Status:** fixed

## 17. "No ships in this unit" box one character narrower

- **Seen:** found by the differential test of `ShipMove::GroupScreen`
  (Launch / Docking with an empty group): the message box and the heap block
  saving its background were 6 pixels narrower.
- **Cause:** the original's text (`22ca:0009`) is `' No ships in this unit !
  '` with a trailing space; the port dropped it.
- **Fix:** `src/game/shipmove.c:140`. `test_diff_shipmove` fails on the old
  code.
- **Status:** fixed

## 18. Attacking alien ships started on the player's side of the space battle

- **Seen:** found by the differential test of `SpWar::InitSpaceFleets`
  (`tests/test_diff_spwar.c`): the ships of a race attacking the planet
  (status 6), and their store ships, started at the left of the battle view
  in the port, at the right in the original.
- **Cause:** the port mirrored the start x (`0A0h - x`) only for defenders.
  The original mirrors every alien ship, forces (`20d4:06ab`) and store ships
  (`20d4:08b8`), whichever side they join; only the player's groups
  (`20d4:0489`) start on the left. The original also keeps the chosen side
  in globals that the port held in locals: the list in `DS:77A6` and a far
  pointer to its count in `DS:77AE`, set per race in `InitSpaceFleets`
  (`20d4:0589`, `07c6`) and `ApplySpaceLosses` (`20d4:1a8b`, `1c8c`).
- **Fix:** `src/game/spwar.c:108` (`add_ship`'s `mirror`, true for all
  alien ships) and `set_side` (`src/game/spwar.c:146`), called where the
  original stores the side. `test_diff_spwar` fails on the old code.
- **Status:** fixed

## 19. Ship info panel counted miner stations wrong and drew them too high

- **Seen:** found by the differential test of `ShipInfo::DrawGroupInfo`
  (`tests/test_diff_shipinfo.c`): for a miner group (kind 2) the "Miner
  stat" count differed and its line was drawn 3 pixels higher than in the
  original.
- **Cause:** two slips in `src/game/shipinfo.c`. The total adds the fourth
  weapon word of the first row's slots, `Units[1, j]` (`26fe:0e7f`, entry
  `j * 0Ah - 2` from `Units`); the port read `Units[j - 1][0]`, slot 1 of
  rows 1..4, i.e. the cargo and item words past the second row. And the
  3-pixel gap after the carrier's satellite lines is added for every kind
  (`26fe:0d76` jumps to the `add [bp-6Ah],3` at `0e3c` when Kind <> 4); the
  port added it only for carriers.
- **Fix:** `src/game/shipinfo.c:419` (gap outside the carrier block) and
  `src/game/shipinfo.c:428` (`Units[0][j - 1]`). `test_diff_shipinfo`
  fails on the old code.
- **Status:** fixed

## 20. Ship list wipe drew the new list's backdrop off by the buffer's offset

- **Seen:** found by the differential test of `ShipInfo::SwitchGroupList`
  (and `ShipInfoScreen`'s "Change mode"): the off-screen copy, and so the
  view wiped in, had the backdrop shifted against the icons.
- **Cause:** the original copies the backdrop to `Buffer`'s segment at offset
  3D40h (`26fe:0710`: `push es; push 3D40h`), ignoring the pointer's offset,
  as the icon and marker routines do. The port added 3D40h to the whole far
  pointer, so with a heap block at a non-zero offset the backdrop landed
  that many bytes later than the icons.
- **Fix:** `src/game/shipinfo.c:266` uses `far_ptr(seg(Buffer), 3D40h)`.
  `test_diff_shipinfo` fails on the old code.
- **Status:** fixed

## 21. Command centre hotspot captions had hyphens

- **Seen:** found by the differential test of `Command::SetupCommandHotspots`
  (`tests/test_diff_command.c`): the captions shown under the pointer read
  "SPACE-LOCAL", "SHIP-INFO" and "PLANET-MAIN".
- **Cause:** the original's constants (`3abd:08be` onwards) are `SPACE LOCAL`,
  `SHIP INFO` and `PLANET MAIN`, with spaces (only `RESEARCH-DESIGN` and
  `INFO-BUY` have hyphens).
- **Fix:** `src/game/command.c:365`; `tests/test_command.c` expects the space.
  `test_diff_command` fails on the old code.
- **Status:** fixed

## 22. The game endings halted with exit code 0, so VICTORY never ran

- **Seen:** found by the differential test of `Command::FadeOutAndHalt` and
  `Command::ShowDeathEnding`: original `Halt(10)` / `Halt(15)`, port `Halt(0)`.
  Winning (screen 36) therefore ended the program instead of playing
  `VICTORY.PRG`, and the death ending (screen 35) did not restart the game.
- **Cause:** `Command_FadeOutAndHalt` and `Command_ShowDeathEnding` called
  `System_Halt(0)`; the original loads AX = 0Ah (`3abd:1a5f`) and 0Fh
  (`3abd:1a3d`) before `Halt`. `docs/screens.md` claimed AX = 0 and is corrected.
- **Fix:** `src/game/command.c:769` and `:777`. `test_diff_command` fails on the
  old code.
- **Status:** fixed

## 23. LoadMap left no trace of its read buffer on the heap

- **Seen:** found by the differential test of `Surface::LoadMap`: the free heap
  block after the call and `System::HeapAllocSize` differed.
- **Cause:** the original (`2ef2:03e7`) reads the map cells into a
  `GetMem(width * height)` buffer and frees it at the end; the port decoded
  from host memory only.
- **Fix:** `src/game/surface.c:204` copies the cells into a `System_GetMem`
  buffer, reads them from there and frees it. `test_diff_surface` fails on the
  old code.
- **Status:** fixed

## 24. ReadTextLine wrote only the wanted line into the caller's string

- **Seen:** found by the differential tests of `Pub::BuildQuestionMenu` and
  `AlienTalk::BuildQuestionMenu`: the question buffers differed past each
  string's length.
- **Cause:** the original (`398f:03ae`) reads every line up to the wanted one
  into the caller's string (`ReadStr` + `ReadLn`) and decodes only the last in
  place, so the bytes past its length keep the raw tail of longer earlier
  lines. The port copied the decoded line only.
- **Fix:** `TextFile_ReadTextLine` (`src/game/textfile.c`) reads the lines the
  same way and then calls `TextFile_DecodeLine`. `test_diff_pub` fails on the
  old code.
- **Status:** fixed

## 25. A message box on the message log screen did not redraw the log unless it logged

- **Seen:** found by the differential test of `Message::ShowMessageNoLog` /
  `Message::MessageBox` (`tests/test_diff_message.c`) on the message log
  screen: the log lines the original redraws stayed as they were.
- **Cause:** the original (`34b0:0715`) jumps over the logging when
  `logIt` is false to `34b0:0870`, the `CurrentScreen = 0Bh` test that
  redraws the log, so the log is redrawn after every box. The port had the
  test inside the logging block.
- **Fix:** `src/game/message.c:226` tests the screen after the block.
- **Status:** fixed

## 26. Toolbar page flag read and toggled in slot 1 instead of DS:7C64

- **Seen:** found by the differential tests of `GfxPath::SetToolbar` and
  `GfxPath::ScrollToolbarNext/Prev` (`tests/test_diff_gfxpath.c`):
  `PageFlagB` (DS:7C63) got the low byte of slot 1's command id, and slot 1's
  id was toggled.
- **Cause:** `SetToolbar(1)` (`3a1b:003a`) copies the byte at `DS:7C64`
  and the scrolls (`3a1b:0625`, `3a1b:06e6`) toggle it. That is the word of slot
  0 of the 21-byte slot records (`7C64 + 21 * slot`, used slots are 1..12);
  the port's `slot_id(1)` is `DS:7C79`.
- **Fix:** `src/game/gfxpath.c:103` and `:199` use `DS:7C64` (as
  `main_program.c` already did).
- **Status:** fixed

## 27. The line editor kept heap string pointers in another form

- **Seen:** nothing visible; found by the differential test of
  `SaveLoad::DiskScreen` (save: the slot name editor), the same kind of
  difference as 10 and 16.
- **Cause:** the original passes `SlotTable + (slot - 1) * 16h + 1` and, in
  `Message::PromptPlanetName`, `DS:4712[sys]^ + (planet - 1) * 13`, far
  pointers that keep the block's segment, to `TextFile::BeginInput`, which
  stores them in `DS:7C50`; `host_to_far` rebuilt normalised pointers.
- **Fix:** `TextFile_SetStringPtr` (`src/game/textfile.c:36`) takes the
  caller's far pointer for the next edit; `src/game/saveload.c:420` and
  `src/game/message.c:313` pass it. `test_diff_saveload` fails on the old
  code.
- **Status:** fixed

## 28. GrWar drew the battlefield at the off-screen buffer's offset

- **Seen:** found by the differential tests of `GrWar::DrawUnitSprite`,
  `DrawEffectSprite`, `DrawProjectile`, `DrawBaseSprites` and `RenderBattle`:
  the sprites landed 8 or 16 bytes away from the original's in the field
  buffer.
- **Cause:** the original addresses the field buffer (DS:7628) as its segment
  with fixed offsets, dropping the offset `GetMem` returned (`1537:08ea`
  `les di,[7628]; push es; push [bp-4]`, likewise `09ab`, `09f5`, `0cb8`
  `push 5410h`, `0dcd` and `11c2` `push 50h`); the port added them to the
  whole pointer (`fp_add(RENDER_BUF, ...)`).
- **Fix:** `render_at()` in `src/game/grwar.c:48` (segment + fixed offset),
  used at grwar.c:328, 343, 350, 381-385, 422 and 501. `test_diff_grwar` fails
  on the old code.
- **Status:** fixed

## 29. GrWar read slot 0 from before the unit list instead of wrapping

- **Seen:** found by the differential test of `GrWar::BattleRound`: an
  attacker in attack mode with target 0 fell back to hold (mode 1) in the
  port and kept attacking in the original, so the units' paths differed.
- **Cause:** the original reaches slot i as list offset + 27h*i - 27h in 16
  bits (`1537:1c99` `imul ax,ax,27h; les di,[7642+4*side]; add di,ax;
  cmp es:[di-26h],0`), so slot 0 (no target, an empty grid cell) wraps to
  the end of the list's segment. The port's `unit()` (`src/game/grwar.c:58`)
  did host pointer arithmetic and read the 39 bytes before the list.
- **Fix:** `unit()` adds the slot offset in 16 bits (`fp_add`).
  `test_diff_grwar` fails on the old code.
- **Status:** fixed

## 30. The ground battle's side-strip hotspot kept an old caption

- **Seen:** found by the differential test of `GrWar::SetupBattleHotspots`:
  `Mouse_Hotspots[4].Caption` (hotspot 5) differed.
- **Cause:** the original clears hotspot 5's caption (`1537:05dd`
  `mov byte [8afe],0`); the port set only its position, size and flags.
- **Fix:** `src/game/grwar.c:264`. `test_diff_grwar` fails on the old code.
- **Status:** fixed

## 31. Scripted runs hung entering a ground battle

- **Seen:** found by the playthrough fixture's battle drivers
  (`tests/test_play_battle.c`): entering a ground battle the game stopped
  advancing frames, in the fixture as in any unpaced autopilot run
  (`reunion --replay`, soak runs).
- **Cause:** `Sound_AllocWarSndBuf` (`431a:03e8`, called by
  `GrWar_EnterGroundBattle` and `SpWar_EnterSpaceBattle`) waits until the
  war sample still playing has ended. The original spun on the SB status word,
  which the card cleared by itself at the end of the sample; the port spun
  with `SDL_Delay(1)` until `Sound_IsSbPlaying()` turned false
  (`src/game/sound.c:147`). Unpaced autopilot runs keep the audio device
  paused and advance the mixer only from the frame clock, one retrace's worth
  in `timing_wait_retrace` (`src/platform/timing.c` `mix_one_retrace`), so
  while the spin let no retrace pass the voice never ended.
- **Fix:** the spin calls `timing_idle_poll()` like the port's other busy
  loops: every few polls a retrace passes, with the frame clock, the input
  pump and the mixer.
- **Verified:** `tests/test_play_battle.c` (wave 1 fought on the ground,
  the Mirach 3 invasion) reaches screens 19 and 32 and finishes both battles;
  before the fix they hung entering the ground battle.
- **Status:** fixed

## 32. Two refusal boxes were blank in optimised builds

- **Seen:** found by the playthrough fixture: in a release build
  `tests/test_play_ships.c` read an empty box where " The satellite carriers
  can only land | on your main planet" belongs (docking a carrier at Zeus);
  debug builds showed the text.
- **Cause:** `PSTR` (`src/game/system.h`) is a compound literal, which lives
  only to the end of its block, and every if or else branch is a block. The
  docking refusals in `ShipMove` (`src/game/shipmove.c`) and the "This can be
  built on your colonies" family in `Shop_DrawNotProducible`
  (`src/game/shop.c`) chose the text in branches and drew it after them, so
  the pointer was dangling; at -O2 the stack slot was reused and the box drew
  nothing. GCC warned only for the shop.
- **Fix:** both choose the text in one conditional expression in the block
  that uses it; `PSTR`'s comment says why.
- **Verified:** `test_play_ships` passes in the release build (it failed
  before) and in the debug build.
- **Status:** fixed

## 33. The alien menu lists were declared one entry short

- **Seen:** found by the sanitizer build (`cmake --preset asan`):
  `tests/test_play_talk.c` reported `index 9 out of bounds for type
  'uint32_t [9]'` at `AlienTalk_BuildQuestionMenu`
  (`src/game/alientalk.c:275`) when talking to race 10.
- **Cause:** `2e4b:0780` reads the race's menu list from `DS:0cde + race *
  4`, so races 1..10 use DS:0CE2..0D09. `Alien_MenuLists` in
  `src/game/dseg.h` had 9 entries and a 4-byte pad after it; race 10's list
  was the pad. The bytes were right (the pad holds the original's pointer to
  DS:0CCA), so the game behaved, but indexing past the array is undefined
  and an optimiser may assume it does not happen.
- **Fix:** the array has 10 entries and no pad.
- **Verified:** `test_play_talk` passes in the sanitizer build with no
  report; the playthrough proof (`tests/play/proof_12345.txt`) replays to the
  same hash.
- **Status:** fixed
