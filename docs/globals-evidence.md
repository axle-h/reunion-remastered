# Proposed names for the shared game state (`4a81:8a8a–a2e0` and next candidates)

First pass, from the decompiled C and `globals.csv`. Nothing here is applied
yet. `?` in a comment marks a guess. "Screen N" means a value of
`Sim::CurrentScreen`; the screen table is described at the end.

Namespace: the existing `Sim::Credits` and the date set the convention, so the
shared variables go under `Sim::` below. Low-level buffers that sit in the
block go under their unit (`Mouse`, `Gfx`, `Music`, `Ems`). If the block turns
out to be one variables-only unit, a single namespace such as `Game::` would
fit better.

## 1. `symbols.txt` lines

The record types used here (`THotspot`, `TGroup`, `TBuilding`,
`TColonySetup`, `TBody`) are declared in section 2. They have to exist in the
program before these lines can be applied.

```
# --- Main program, next candidates (outside the shared block) ---
4a81:75ee Main::I: longint                    // FOR loop variable of the main program body (all `i` loops in Main)
4a81:7ae4 Sim::CurSystem: integer             // Selected star system 1..8. Copied from TGroup.System (+13h); indexes the body tables at DS:4752/4732
4a81:7ae6 Sim::CurPlanet: array[1..8] of integer  // Selected planet per system. Copied from TGroup.Planet (+14h)
4a81:7af6 Sim::CurMoon: array[1..8] of integer    // Selected moon per system, 0 = the planet itself. Copied from TGroup.Moon (+15h)
4a81:7b08 Sim::CurBody: ^TBody                // Set by 357b:3253 from CurSystem/CurPlanet/CurMoon: DS:4752[sys]^[body], 65-byte records
4a81:7b0c Sim::CurBodyIsColony: word          // 1 if CurBody^.Owner = 1 and CurBody^.Colony <> 0 (357b:3253) ? boolean stored as word

# --- Mouse (in the shared block) ---
4a81:8a8c Mouse::Hotspots: array[1..66] of THotspot  // Pascal base is DS:8A71 (element 0). 66 records end at 9181, just before CursorState. Hit test 4758:05bc returns 20+index
4a81:918e Mouse::BtnDown: boolean             // Raw button state. Mouse::Poll copies it to Buttons. Never written by Pascal code (callback?)
4a81:918f Mouse::BtnRight: boolean            // ? Copied to DS:8A7A while a button is down (8A7A=1 opens cockpit in 26fe)
4a81:9190 Mouse::BtnOther: boolean            // ? Copied to DS:8A7B while a button is down
4a81:919a Mouse::SavedCursorShape: word       // 357b:29cd saves CursorShape here before changing it
4a81:9602 Mouse::CursorImages: array[0..5] of array[0..255] of byte  // 16x16 cursor bitmaps, CursorShape*256 (47da:065f). Filled by 321d

# --- Research, staff and story ---
4a81:91a0 Sim::IdeaTimer: array[1..35] of integer  // Hours until invention k becomes available (status 0 -> 1). -1 = not scheduled. Decremented by 1b8a:3d07; set by 29b9:136e
4a81:91e6 Cheat::Used: boolean                // Big cheat (1,000,000 credits and boosts) already applied
4a81:91e7 Cheat::Requested: boolean           // Main loop calls Cheat::Apply when set. Never set by Pascal code (keyboard handler?)
4a81:91e8 Sim::BattleFlag: boolean            // ? Cleared at the start of a ground (1537:0200) or space (20d4:01d3) battle and checked during it
4a81:91ea Sim::StoryStage: integer            // 1..6. Raised by story events in Main and 1b8a:3d07. Selects the developer skill caps (DS:57EB + stage*12) and the advisor's lines
4a81:95a0 Sim::SelInvention: integer          // Invention selected on the research screen (screen 3)
4a81:95a2 Sim::CurResearch: integer           // Invention under development, 0 = none. Cleared when it's ready for production
4a81:95a4 Sim::StaffExp: array[1..4] of word  // Experience of the hired pilot, builder, fighter and developer. Grows at random (1b8a:3b83) up to a cap per hired person. Pilot exp+1 = travel speed; the cheat sets 70
4a81:95ac Sim::DevSkill: array[1..4] of word  // Developer's level in the 4 research fields. Compared with each invention's requirements (1b8a:04d8) and shown on screen 3. The cheat sets 6
4a81:95b4 Sim::ProductionRate: word           // Production points per hour subtracted from each item's remaining work (1b8a:0705). Also the divisor for the shop's time estimate. Starts at 10
4a81:95b6 Sim::StaffHired: array[1..4] of word  // Hired candidate (1..3, 0 = none) for 1 pilot, 2 builder, 3 fighter, 4 developer. Name = DS:57F8 + 19*(type*3+n)
4a81:95c6 Sim::HomeStock: array[1..6] of longint  // Resource stocks of New Earth (system 1, planet 5). Other bodies keep theirs in TBody.Stock. Saved as 24 bytes

# --- Screen and input state ---
4a81:95e6 Sim::CurrentScreen: integer         // Index into the 25-byte screen table at DS:50FB (see below). Main dispatches on it. A negative value is negated
4a81:95e8 Sim::PrevScreen: integer            // Screen being left. Main frees its buffers when CurrentScreen <> PrevScreen. Set to CurrentScreen after the switch
4a81:95ea Sim::MenuCommand: integer           // Command id of the clicked menu-bar button (word at DS:7C64 + 21*hotspot). 0 if none. Screens test it (e.g. 0Fh New Group, 28h Hire, 3Bh Colonise)
4a81:95ec Sim::HotspotHit: integer            // Hotspot under the mouse (4758:05bc): 1..12 menu bar, 13 top-right corner, 14 clock (skip to next hour), 20+k = Mouse::Hotspots[k], 999 = forced
4a81:95ee Sim::PrevHotspotHit: integer        // HotspotHit of the previous poll
4a81:95f0 Sim::PressHotspot: integer          // HotspotHit when the button went down
4a81:95f2 Sim::ReleaseHotspot: integer        // HotspotHit when the button went up. The click is cancelled if it differs from PressHotspot
4a81:95f4 Sim::ScreenEntered: boolean         // Set for the first pass through a screen after a switch. Handlers do their setup when it's set
4a81:95f5 Sim::ScreenSwitch: boolean          // Code sets CurrentScreen and this to switch screens without a click
4a81:95f6 Sim::Ticks: word                    // Frame counter. Main advances one game hour when it passes 100 (95 while 8A7A is set). Clicking the clock sets it to the limit
4a81:95f8 Sim::ClockRunning: boolean          // Ticks only advance while set. Cleared by many screens to pause time
4a81:95fe Sim::MusicTrack: integer            // 0 off, 1 "Music 1", 2 "Music 2" (options, 1a3e:0008). Saved in the setup file?
4a81:9600 Sim::SoundFx: integer               // 0 no device, 1 off, 2 on (toggled as 3-x). With 2, menu clicks play per-command samples

# --- Hero ---
4a81:9276 Sim::Hero: integer                  // 1 or 2, chosen at the title screen (321d:026f). Built into picture names (grafika\deathsz<n>)
4a81:9278 Sim::HeroSpriteX: integer           // 45h for hero 1, 1 for hero 2. X offset into the command centre sprite sheet (3abd:163f)
4a81:927a Sim::SatButtons: integer            // Bit mask of the satellite menu buttons shown (28cd:03fb). Redrawn when it changes; -1 forces a redraw
4a81:927c Sim::ShipInfoButtons: integer       // The same for the ship info menu (28cd:0542); -1 forces a redraw

# --- Procedure variables (forward calls between units, set in unit inits) ---
4a81:9272 Blit::CursorHook: pointer           // = 4758:012d (Mouse). Called around blits to hide/restore the software cursor
4a81:927e Groups::UpdateSatMenu: pointer      // = 28cd:03fb (Satellite)
4a81:9282 Groups::IntelHook: pointer          // = 29b9:0b93 (Intel), called with (SelGroup, GroupList)
4a81:9286 Sim::FindBuildingProc: pointer      // = 2ef2:2b1a (Surface) ? called as f(1, moon, planet, system, kind)
4a81:928a Sim::FindPlanetForceProc: pointer   // = 2b8d:1c9f (Groups). Finds the planet-force group of (moon, planet, system)
4a81:928e Sim::GrWarProc: pointer             // = 1537:11f2 (GrWar)
4a81:9292 Sim::GfxPathProc: pointer           // = 3a1b:09a3 (GfxPath)
4a81:9296 Ems::RestoreProc: pointer           // = 4719:0317 (Ems)

# --- Graphics ---
4a81:926e Music::MainThemeOn: boolean         // ? Set when main1/main2 is playing; 424a:017e restarts the theme if clear
4a81:929a Gfx::Palette: array[0..255] of array[0..2] of byte  // Current VGA palette (RGB 0..63). Passed to Gfx::LoadPic and set with 405f:0089
4a81:959a Gfx::ReadPos: word                  // ? Read position in a buffer, advanced by 405f:09f3
4a81:959c Gfx::FlashStep: integer             // Palette flash (alert): counts down 61..0 and writes DAC 3C9h (405f:1c21)
4a81:959e Gfx::FlashCount: integer            // Flashes left; reloads FlashStep
4a81:95fa Gfx::Charset: pointer               // ? = Ems block + 5EF0h (10,368 bytes). grafika\charset1 loaded at +80h (321d:026f); 405f:14c1 draws glyphs from it

# --- Surface buffers ---
4a81:a202 Surface::BufA: pointer              // 24FFh bytes, freed when leaving screen 20 (planet surface)
4a81:a206 Surface::BufB: pointer              // DS:17E4[planetType]*320 bytes
4a81:a20a Surface::BufC: pointer              // F000h bytes. Also freed by Colonise (2841)
4a81:a20e Surface::BufD: pointer              // DS:17FA[planetType]*320 bytes, may be nil

# --- Colonies, buildings, groups ---
4a81:a21a Sim::ColonySetupCount: integer      // Number of used ColonySetups (< 10, checked in 28cd:062b)
4a81:a21c Sim::ColonySetups: array[1..10] of TColonySetup  // Colonies under construction (2841:0060 adds; 1b8a:25b5 counts down and removes). Ends at A2BB
4a81:a2bc Sim::Buildings: ^array[1..1000] of TBuilding  // = Ems block + 2840h. 14,000 bytes saved
4a81:a2c0 Sim::BuildingCount: integer         // < 1000
4a81:a2c2 Sim::ShownGroupCount: integer       // Copy of GroupCount or PlanetForceCount for the list on screen (ship info, 26fe)
4a81:a2c4 Sim::GroupCount: integer            // Groups in list 0 (fleets), max 32
4a81:a2c6 Sim::PlanetForceCount: integer      // Groups in list 1 (planet forces, one per colony). Colonise (2841) adds one
4a81:a2c8 Sim::SelGroup: integer              // Selected group in list GroupList, 0 = none
4a81:a2ca Sim::SelGroup0: integer             // Last selection in list 0
4a81:a2cc Sim::SelGroup1: integer             // Last selection in list 1
4a81:a2ce Sim::GroupList: integer             // 0 = fleets, 1 = planet forces
4a81:a2d0 Sim::Groups: ^array[0..1] of array[1..32] of TGroup  // Start of the Ems block. 2840h bytes saved
4a81:a2d4 Sim::GroupsEnabled: boolean         // "New group" available. Set when invention 4 or 6 is ready
4a81:a2d6 Sim::GroupKindAllowed: array[1..4] of boolean  // 1 army (invention 10), 2 trade (inv. 6), 3 secret forces (story), 4 satellite carrier (inv. 4)

# --- Ems ---
4a81:a2da Sim::MinMemAvail: longint           // Lowest MemAvail seen (3a1b:0864, 357b, 3c64, 3d38)
4a81:a2de Ems::Buffer: pointer                // GetMem(8770h). Conventional-memory copy of the EMS block holding Groups, Buildings and Charset
```

## 2. Record layouts

```pascal
type
  { Mouse hotspot, 27 bytes. Base DS:8A71, used from index 1 (DS:8A8C). }
  THotspot = record
    X:       integer;      { +00 }
    Y:       byte;         { +02 }
    W:       integer;      { +03 }
    H:       byte;         { +05 }
    Caption: string[18];   { +06  status-line text, shown by 405f:11e8 }
    Repeat_: boolean;      { +19  ? hovering with the button held counts as a click (sets DS:8A82) }
    Unknown: byte;         { +1A }
  end;

  { Menu-bar button. Base DS:7C51, used from index 1 (DS:7C66). 12 buttons. Outside the block, but
    MenuCommand comes from it. }
  TMenuButton = record
    Caption: string[18];   { +00  shown at DS:7C51 + 21*id }
    Command: integer;      { +13  copied to Sim::MenuCommand }
  end;

  { Group (fleet or planet force), 161 bytes. Sim::Groups^[list, i]. }
  TGroupSlot = record      { 10 bytes }
    ShipType: integer;     { ? }
    Weapons:  array[1..4] of integer;   { ? weapon counts or types }
  end;
  TGroup = record
    Kind:     byte;        { +00  1 army, 2 trade, 3 secret forces, 4 sat carrier, 5 planet forces (default names at DS:595D + 17*Kind) }
    Name:     string[17];  { +01 }
    System:   byte;        { +13  location: copied to CurSystem }
    Planet:   byte;        { +14 }
    Moon:     byte;        { +15  0 = the planet itself }
    Status:   byte;        { +16  1 for a new group; values 1, 2, 7 and > 3 tested. Landed/orbit/travelling ? }
    Leg1:     integer;     { +17  ? distance left to leave the system, reduced by StaffExp[1]+1 per hour }
    Jump:     integer;     { +19  ? hyperspace hours left, reduced by 1 }
    Leg2:     integer;     { +1B  ? distance left to arrive, reduced by StaffExp[1]+1 }
    Units:    array[1..2, 1..4] of TGroupSlot;  { +1D  [1] space fleet, [2] ground forces ?; 4 ship types per group (manual) }
    Cargo:    array[1..6] of record A, B: integer end;  { +6D  ? }
    Items:    array[1..13] of integer;  { +85  ? 13 kinds looped over by the cargo screen (2583) }
    Storage:  integer;     { +9F  planet forces only: storage capacity / 1000 ? (1b8a:2178, 2583) }
  end;

  { Building on a planet surface, 14 bytes. Filled by 2ef2:2463. }
  TBuilding = record
    Kind:    byte;         { +00 }
    System:  byte;         { +01 }
    Planet:  byte;         { +02 }
    Moon:    byte;         { +03 }
    X:       byte;         { +04  map position }
    Y:       byte;         { +05 }
    Health:  byte;         { +06  ? 100..159 when built; 0 = destroyed/inactive ? }
    Output:  byte;         { +07  ? suitability(DS:6566[kind*63 + bodyClass]) * 40 + Random(80) }
    Flag:    byte;         { +08  cleared by 357b:3a05 }
    W9:      integer;      { +09 }
    WB:      integer;      { +0B }
    BD:      byte;         { +0D }
  end;

  { Colony under construction, 16 bytes. }
  TColonySetup = record
    Active:  boolean;      { +00  0 when created }
    System:  byte;         { +01 }
    Planet:  byte;         { +02 }
    Moon:    byte;         { +03 }
    Timers:  array[1..6] of integer;  { +04  hours left per item. Colonise toggles them 0/1; at 0 the item's building
                                        (word at DS:651E + 10*k) is placed by 2ef2:29b1. The record is removed when all are 0 }
  end;

  { Planet or moon, 65 bytes. Outside the block, but CurBody points to one. Per system: DS:4752[sys]^[index];
    moons map through the byte table DS:4732[sys]^[planet*9 + moon - 9]. }
  TBody = record
    Owner:   byte;         { +00  1 = player }
    { +01..05 ? }
    Colony:  boolean;      { +06  ? <> 0 for a colony }
    { +07..09 ? }
    Miner:   boolean;      { +0A  ? the alternative to Colony in 2ef2 checks: miner station ? }
    { +0B..14 ? }
    Class:   shortint;     { +15  index into DS:1A38 (planet type), DS:6566 (building suitability) }
    Sub:     shortint;     { +16 }
    { +17..1A ? }
    Stock:   array[1..6] of longint;  { +1B  resource stocks (HomeStock for New Earth) }
    { +33..3A ? }
    Rich:    byte;         { +3B  ? mining yield; Rich div 10 added per mining hour (1b8a:2178) }
    { +3C..40 ? }
  end;
```

## Screen table (`DS:50FB`, 25-byte records, index = `CurrentScreen`)

`+00` picture name `string[8]` (shown from `grafika\` if `+09` is set), `+0A`
reload the menu bar, `+0B` menu command that opens the screen (shortint, -1 =
none), `+0C` button count, `+0D..` the menu-bar command ids.

| # | Name | Notes |
|---|---|---|
| 1 | `main` | Command centre (3abd) |
| 2 | `faces` | Hiring staff (3fab) |
| 3 | `research` | Research and design (2d66) |
| 4 | `miner` | |
| 5 | `info` | Shop / production (32b4) |
| 6 | `ship` | Ship info (26fe) |
| 7 | `csillag` | Star map (357b) |
| 8 | `bolygo` | Planet info |
| 9 | `keresked` | Trade |
| 11 | `uzenet` | Messages |
| 12, 38 | `disk` | Save/load and options (1a3e) |
| 13 | `transfer` | Cargo transfer (2583) |
| 17 | `MUSZI` | Cockpit (22ca; *műszer* = instrument) |
| 19 | `grbeoszt` | Ground war unit assignment |
| 20 | `planmain` | Planet surface (2ef2) |
| 21 | `beoszt` | New group |
| 22 | `beosztas` | Group screen (2b8d) |
| 23, 25 | | Pub (1fd2) |
| 24 | `duma` | Advisor (1991) |
| 28 | `KOLONIZ` | Colonise (2841) |
| 29 | `SPWAR` | Space battle (20d4) |
| 32 | `GRWAR` | Ground battle (1537) |
| 34 | | Alien contact (2e4b) |
| 37 | `colinfo` | Colony list (1390) |

## Still open

- `9184`, `919c` (Mouse) and `9c02–a201`: `Blit::CursorBackground` uses 2
  bytes at `9c02`, but nothing else is referenced until `a202`. That's
  1,536 bytes, either a larger buffer or unused variables.
- `a212–a219`: never referenced.
- The meaning of `TGroup.Status`, `TGroup.Units` and the two cargo arrays.
- `91e7`, `918e–9190`: written outside Pascal code, probably by an interrupt
  or mouse callback.
- Invention records (outside the block): 53 bytes, `DS:5D77 + 35h*(k-1)`, 35
  entries. Name at `+00`, status word at `+11` (0 unknown, 1 idea, 2/4
  developing, 3, 5 ready), progress at `+15`, remaining work at `+21`, and
  requirements per research field at name byte `+2Fh+k`.
