# Typed-constant tables in DS 0000–759D (proposals)

Proposed names and record layouts for the initialised tables in the data
segment. Nothing here has been applied to `ghidra/symbols.txt` yet.

Sources: initial values dumped from `REUNION.PRG` (DS:X is at file offset
19168 + 3A81h·16 + X), the decompiled code, and the manual (commander names
and costs). Confidence: **H** means the fields were checked in code and the
data agrees. **M** means the layout is certain but some field meanings are
guesses. **L** means only the extent and the user are known.

## Conventions

- **Arrays start at 1.** The decompiler folds the lower bound into the
  constant, so it shows `i*27 + 0x11f` for an array whose first element is at
  `013a`. Labels below go on the real first element. The folded base the
  decompiler shows is listed as "folded".
- **Word alignment.** Typed constants larger than one byte start on even
  addresses (`{$A+}`), which leaves odd padding bytes such as `1a9b` and `2489`.
  This shows that each star system's moons, names and bodies are three separate
  constants.
- **Far pointers in the data.** Pointer tables hold `3a81:xxxx`, which is the
  file-relative data segment (Ghidra `4a81`).
- **Namespaces** are named after the main user. Most of these tables are shared
  by several units, so the real owner is unknown.

## Summary

| Start | Size | Count | Name | Conf |
|---|---|---|---|---|
| `013a` | 27 | 10 | `Pub::People` | H |
| `0248–03d9` | 6 | var. | 10 `TMenu` lists, one per pub person | H |
| `03da` | 4 | 10 | `Pub::MenuLists` (far pointers to the above) | H |
| `0424` | 24 | 10 | `Pub::Deals` | H |
| `0514` | 20 | 9 | `Pub::Anims` | H |
| `0d96` | 240 | 11 | `Surface::MapTemplates` (12×20 tiles) | M |
| `1a5c–4715` | 9/13/65 | 8 systems | Star systems: moons, names, bodies | H |
| `4716–481d` | | 8 | System pointer tables, counts, status | H |
| `489c` | 18 | 77 | `Ui::ButtonLabels` | H |
| `4e5f` | 9 | 77 | `Ui::ButtonSounds` (`SOUND\*.SMP`) | M |
| `5114` | 25 | 38 | `Ui::Screens` (`GRAFIKA\*.PIC`) | M |
| `579c–5843` | | 4×3 | Commander price, level, ability and skills | H |
| `5844` | 19 | 4×3 | `Staff::CommanderNames` | H |
| `5928–5bed` | | | Name lists: groups, units, ores, races, systems, disciplines | H |
| `5dac` | 53 | 35 | `Research::Products` | H |
| `6528` | 10 | 6 | `Colony::StarterBuildings` | M |
| `6592` | 63 | 25 | `Surface::Buildings` | H |
| `6bce` | 228 | 11 (2..12) | `Races::Races` | H/M |

---

## 1. Pub (`1fd2`, also `Sim` `1b8a` and the main program)

```pascal
type
  TPubPerson = record              { 27 bytes }
    Name:     string[13];          { +00 'Spy-Hunter', 'Stranger', 'Undorling', 'Informer', 'Eran', ... }
    Portrait: byte;                { +0E loads 'alien\alien<N>' (1fd2:02f4, 2ef2:0a9d) }
    State:    byte;                { +0F 0/1/2/3; drawn when 2 or 3 and Timer = 0 }
    Timer:    integer;             { +10 counted down hourly by Sim (1b8a:006e) }
    Deal:     byte;                { +12 index into Pub.Deals; >10 means Deal-10 with amounts halved }
    SrcX:     integer;             { +13 sprite position in the pub sheet (DAT_773a) }
    SrcY:     byte;                { +15 }
    X:        integer;             { +16 screen position; also the hotspot (1fd2:01ef) }
    Y:        byte;                { +18 screen y = Y + 31h }
    W, H:     byte;                { +19, +1A }
  end;

  TMenu = record                   { 6 bytes; also used by the advisor (1991) and alien contact (2e4b, 4 bytes there) }
    Count: byte;
    Items: array[1..5] of shortint;  { $FF = unused }
  end;

  TPubDeal = record                { 24 bytes }
    Items:   array[1..4] of byte;  { +00 1..6 = ore (name at 5a73+9*id), >=10 = product id-10 }
    Amounts: array[1..4] of word;  { +04 "You got N t of <ore>" / "N pieces of <product>s" }
    Kind:    byte;                 { +0C compared with 2/3/4 in Main (1000:1568) }
    Chance:  byte;                 { +0D Random(100) < Chance }
    Year:    word;                 { +0E \ date the deal happens (Sim compares with Sim::Year/Month/Day/Hour) }
    Month, Day, Hour: byte;        { +10..+12 }
    Active:  word;                 { +13 cleared when the deal is resolved }
    Unk15:   array[0..2] of byte;
  end;

  TPubAnim = record                { 20 bytes; ambient animation, 1fd2:0ec1 }
    SrcX, SrcY, X, Y, W, H: integer;   { +00..+0B }
    FramesPerRow, FrameCount, Frame: byte;  { +0C..+0E }
    MinDelay, RandDelay, FrameDelay: byte;  { +0F..+11, in tens of ticks }
    Timer: integer;                { +12 }
  end;
```

```
4a81:013a Pub::People: array[1..10] of TPubPerson   // folded 011f; 1b8a:006e, 1fd2:01ef, Main 1000:1271
4a81:0248 Pub::MenuData                              // 10 consts: array[1..n] of TMenu, n = 5,11,3,5,6,11,7,6,7,6
4a81:03da Pub::MenuLists: array[1..10] of pointer   // folded 03d6; ^array of TMenu per person (1fd2:0788)
4a81:0402 Pub::CurMenu: TMenu                       // copied from MenuLists (1fd2:0788), then extended
4a81:0408 Pub::ItemQty: array[1..12] of byte        // folded 0407; multiplied by ItemPrice (1fd2:05fe)
4a81:0414 Pub::ItemPrice: array[10..13] of longint  // folded 03ec; 10000, 14000, 20000, 7000
4a81:0424 Pub::Deals: array[1..10] of TPubDeal      // folded 040c; Sim 1b8a:006e
4a81:0514 Pub::Anims: array[1..9] of TPubAnim       // folded 0500; 1fd2:0ec1
4a81:00c2 Advisor::Menus: array[1..3] of TMenu      // folded 00bc; 1991:08de
4a81:00d4 Advisor::CurMenu: TMenu
4a81:0c2e Alien::MenuData                           // lists of 4-byte menus (count + 3), 2e4b
4a81:0ce2 Alien::MenuLists: array[1..9] of pointer  // folded 0cde; 2e4b:0780
4a81:0d0a Alien::CurMenu: array[0..3] of byte
```

## 2. Star systems (`1390`, `1b8a`, `22ca`, `29b9`, `357b`, `20d4`)

There are 8 systems: Amnesty, Phoenix, Mirach, Antares, Orionis, Lyrae, Rigel
and Sun. In each, bodies `1..P` are planets and `P+1..P+M` are moons. Each
system has three typed constants, reached through pointer tables at
`4716/4736/4756`.

```pascal
type
  TMoonList = record               { 9 bytes }
    Count: byte;
    Moons: array[1..8] of byte;    { body indices }
  end;

  TBodyName = record               { 13 bytes }
    Name: string[9];               { 'Amnesty 1', 'Klatoo', 'New Earth', 'Earth', 'Moon 1', ... }
    Size: byte;                    { +0A 20,16,13,9,6,... - icon size on the system map? }
    X, Y: byte;                    { +0B, +0C system-map hotspot (357b:18cc: x = X, y = Y+49) }
  end;

  TBody = record                   { 65 bytes (41h) }
    Owner:       byte;             { +00 0 none, 1 player, 2..12 race (Races) }
    Unk01:       byte;
    Surveyed:    boolean;          { +02 needed for "Empty planet, ready for colonisation" }
    Unk03:       boolean;          { +03 }
    Unk04:       array[4..5] of byte;
    Colony:      boolean;          { +06 player colony ("Colony on") }
    Miner:       shortint;         { +07 miner station / droids ("Miner station on") }
    Satellite:   byte;             { +08 set to 1 or $E2 by 28cd (satellites) }
    Unk09:       byte;
    Unk0A:       byte;             { +0A compared with counts in 1b8a:0cba; cargo needs [6] or [10] }
    Unk0B:       byte;             { +0B 28cd:021f }
    InfoLevel:   shortint;         { +0C how much is known: thresholds 3,5,6,10,20,30,40; landing sets 6 }
    Population:  longint;          { +0D Earth 143000, New Earth 30000 }
    Resistance:  shortint;         { +11 0..69 -> None/Very low/Low/Normal/High/Difficult/Very difficult? }
    TechLevel:   byte;             { +12 0..7 -> Oppressing/Under developed/Poor/Developing/Enhanced/High tech/Super tech }
    Attitude:    shortint;         { +13 0..69 -> revolting men .. loyal people }
    Unk14:       shortint;         { +14 usually $FF }
    SurfaceType: byte;             { +15 0 No data,1 Earth-like,2 Gaseous,3 Icy,4 Watery,5 Tropical,
                                     6 Desert,7 Swampy,8 Rocky,9 Too hot,10 No atmosphere }
    Unk16:       byte;
    Diameter:    word;             { +17 in tens of km: Earth 1287, Jupiter 14254, Moon 347 }
    Temperature: integer;          { +19 kelvin: Earth 295, Venus 753 }
    Stores:      array[1..8] of longint; { +1B cargo storage (2583:0494); 1..6 = ores like Sim ore stock at 95c6 }
    Deposits:    array[1..6] of byte;    { +3B ore richness per ore (357b, 2ef2:0a9d shows /10) }
  end;
```

```
4a81:1a3a Galaxy::SurfaceMap: array[1..11] of integer      // folded 1a38; SurfaceType -> map template (1,8,2,7,10,3,9,5,4,6,11)
4a81:1a50 Galaxy::SurfaceColonisable: array[1..11] of boolean  // folded 1a4f; 1390:0047, 357b
4a81:1a5c Galaxy::AmnestyMoons: array[1..7] of TMoonList
4a81:1a9c Galaxy::AmnestyNames: array[1..32] of TBodyName
4a81:1c3c Galaxy::AmnestyBodies: array[1..32] of TBody
4a81:245c Galaxy::PhoenixMoons: array[1..5] of TMoonList
4a81:248a Galaxy::PhoenixNames: array[1..12] of TBodyName
4a81:2526 Galaxy::PhoenixBodies: array[1..12] of TBody
4a81:2832 Galaxy::MirachMoons: array[1..5] of TMoonList
4a81:2860 Galaxy::MirachNames: array[1..18] of TBodyName
4a81:294a Galaxy::MirachBodies: array[1..18] of TBody
4a81:2ddc Galaxy::AntaresMoons: array[1..3] of TMoonList
4a81:2df8 Galaxy::AntaresNames: array[1..7] of TBodyName
4a81:2e54 Galaxy::AntaresBodies: array[1..7] of TBody
4a81:301c Galaxy::OrionisMoons: array[1..3] of TMoonList
4a81:3038 Galaxy::OrionisNames: array[1..13] of TBodyName
4a81:30e2 Galaxy::OrionisBodies: array[1..13] of TBody
4a81:3430 Galaxy::LyraeMoons: array[1..7] of TMoonList
4a81:3470 Galaxy::LyraeNames: array[1..23] of TBodyName
4a81:359c Galaxy::LyraeBodies: array[1..23] of TBody
4a81:3b74 Galaxy::RigelMoons: array[1..2] of TMoonList
4a81:3b86 Galaxy::RigelNames: array[1..5] of TBodyName
4a81:3bc8 Galaxy::RigelBodies: array[1..5] of TBody
4a81:3d0e Galaxy::SunMoons: array[1..8] of TMoonList
4a81:3d56 Galaxy::SunNames: array[1..32] of TBodyName      // Mercury..Neptune, Moon, Phobos, ... Triton, Nereid
4a81:3ef6 Galaxy::SunBodies: array[1..32] of TBody
4a81:4716 Galaxy::Names: array[1..8] of pointer           // folded 4712; -> <Sys>Names (34b0:0bbe, 357b)
4a81:4736 Galaxy::MoonLists: array[1..8] of pointer       // folded 4732; -> <Sys>Moons (1390:02b6)
4a81:4756 Galaxy::Bodies: array[1..8] of pointer          // folded 4752; -> <Sys>Bodies (41 uses)
4a81:4776 Galaxy::PlanetCount: array[1..8] of integer     // folded 4774; 7,5,5,3,3,7,2,8
4a81:4786 Galaxy::MoonCount: array[1..8] of integer       // 25,7,13,4,10,16,3,24 (P+M = array sizes above)
4a81:4796 Galaxy::BodyIcon: array[1..8,1..8] of byte      // folded 478d; copied into the planet-list hotspot (357b:23db). Meaning?
4a81:47d6 Galaxy::PlanetStatus: array[1..8,1..8] of shortint  // folded 47cd; -1 unknown, 0 known, 1, -2/-3/-4 events (1b8a:0cba)
4a81:4816 Galaxy::SystemStatus: array[1..8] of shortint   // folded 4815; 1 open, 0 known, -1 unknown (cheat sets -1 -> 0)
```

Every system has the same layout: `Moons` is followed by an alignment byte,
then `Names` and `Bodies`. Bodies end exactly where the next system's `Moons`
begins, and `SunBodies` ends at `4716`. The moon lists match the names, for
example Phoenix planets 3, 4 and 5 own moons {6,7}, {8..11} and {12}. The
owners match the race table: Jade has owner 2 (Jaanosian home 1/7) and Earth
has owner 12 (Earthlings home 8/3).

The copy protection stores its own values inside this data. `2971/2973`
(MirachBodies[1].Stores), `2b3e` and `3119/311b` (OrionisBodies[1].Stores[8])
are read by `264a`, `2693` and `2e4b`. `1482`, inside MapTemplates, is read by
`2693`. They should be labelled separately as protection variables.

## 3. Surface (`2ef2`, main program)

```
4a81:0d96 Surface::MapTemplates: array[1..11, 1..12, 0..19] of byte  // folded 0c92 (2ef2:03e7: t*0F0h + row*14h + col)
4a81:17e6 Surface::ViewY1: array[1..11] of integer   // folded 17e4; Main 1000:0660 (*140h)
4a81:17fc Surface::ViewY2: array[1..11] of integer   // folded 17fa
4a81:1812 Surface::ViewX1: array[1..11] of integer   // folded 1810 (values 50h..F0h)
4a81:1828 Surface::ViewX2: array[1..11] of integer   // folded 1826
4a81:1841 Surface::CategoryRows: array[1..9, 1..3] of shortint  // folded 183e; info-panel row for Production/Workers/Energy by building Category (2ef2:0a9d)
4a81:185c Gfx::HeroCharset: string    // 'ABC..Z a..z ?![]'"+-:;., 1234567890 ' (75 chars, string[255]), 321d
4a81:195c Sat::AnimPath: array[1..74] of record dx, dy, frame: byte end  // folded 1959; 34b0:0953 'grafika\sat'
```

## 4. Research products (`2d66`, `32b4`, `1b8a`, `1991`)

```pascal
type
  TProduct = record                { 53 bytes (35h) }
    Name:      string[16];         { +00 'Nuclear gen', 'Miner droid', ... 'Missile launcher' (16 chars) }
    State:     integer;            { +11 0 unknown, 1 can be developed, 2 under development,
                                     3 can be analysed, 4 under analysis, 5 done (2d66:03e9) }
    DevTime:   word;               { +13 140..10000 - research effort? }
    Progress:  integer;            { +15 shown as Progress/100 "% completed"; initially 10000 }
    Price:     longint;            { +17 Cruiser 340000, Galleon 47000, Satellite 780 }
    Stock:     integer;            { +1B pieces in store (Sim 1b8a:006e adds to it) }
    Unk1D:     word;               { always 0 }
    Unk1F:     word;               { 50..2000 - build time or storage size? }
    Unk21:     word;               { always 0 }
    Produced:  boolean;            { +23 0 for the "built, not produced" items (Control Centre, Radio, Hyperspace drv, shields...) }
    SpaceBuilt: boolean;           { +24 1 only for Destroyer and Cruiser }
    Ores:      array[1..6] of word;{ +25 ore needed: Detoxin, Energon, Kremir, Lepitium, Raenium, Texon }
    Skills:    array[1..4] of byte;{ +31 required Math, Physics, Elect, A.Int (labels at 5b4e) }
  end;
```

```
4a81:5dac Research::Products: array[1..35] of TProduct   // folded 5d77; fields seen as DAT_5d88(+11) / 5d8c(+15) / 5d92(+1B) + i*35h
4a81:64f0 Advisor::Hints: array[1..9] of record Who, Product: integer end  // folded 64ec; 1991:02c9 checks Products[Product].State
4a81:6bc6 Research::ShipProduct: array[1..4] of byte     // folded 6bc5; Hunter 10, Starfighter 16, Destroyer 23, Cruiser 33
4a81:6bca Research::GroundProduct: array[1..4] of byte   // folded 6bc9; Trooper 17, Battle tank 21, Aircraft 26, Missile launcher 32
4a81:0808 Cargo::Items: array[1..13] of shortint         // folded 0807; product ids, -1 unused (2583:0494)
4a81:0816 Cargo::ItemSpace: array[1..13] of longint      // folded 0812; storage units per item
4a81:084a Cargo::UnitSpace: array[1..4] of longint       // folded 0846; 500, 2000, 1000, 8000
```

`5d40–5d76` lie below element 1 and are all zero. The many `DAT_4a81_5d4x/5d5x`
references in `1b8a` are probably separate variables. Some may be accesses at
a negative index. Leave them unnamed until checked.

## 5. Buildings (`2ef2`, `2841`, main program)

```pascal
type
  TBuilding = record               { 63 bytes (3Fh) }
    Name:       string[14];        { +00 'Command centre', 'Nuclear Gen.', ... 'Miner station' }
    NeedProduct: byte;             { +0F product that must be Done (Main 1000:1019); Ioncannon 29, Energy shield 35 }
    Unk10:      byte;
    Category:   byte;              { +11 1 power, 2 mine/derrick, 3 civil, 4 radar, 5 defence, 6 industry/farm,
                                     7 housing, 8 command centre, 9 miner station; 2 or 9 shows "Production : mine" }
    Unk12:      array[0..12] of byte;  { 0/1 flags - allowed surface types? }
    Workers:    integer;           { +1F "Workers :" }
    Energy:     longint;           { +21 "Energy : n kwh" (consumption) }
    Production: integer;           { +25 "Production : n kwh" (generators: Nuclear 3000, Windtrap 1200) }
    Cost:       longint;           { +27 "Cost :" - Command centre 100000 (manual: 100,000 credits) }
    Unk2B:      byte;              { $FF or 1..7 }
    Unk2C:      byte;              { always 10 }
    Width, Height: byte;           { +2D, +2E in tiles (used to centre it) }
    Tiles:      array[1..4, 1..4] of byte;  { +2F tile numbers }
  end;
```

```
4a81:6592 Surface::Buildings: array[1..25] of TBuilding  // folded 6553; 2ef2:0a9d, 2841, Main
4a81:6528 Colony::StarterBuildings: array[1..6] of record Building, X, Y, W, H: integer end  // folded 651e; Nuclear, Windtrap, Builder Plant, Mine, Radar, Building (2841)
4a81:6518 Races::Hostile: array[1..12] of boolean        // folded 6517; indexed by body Owner (1390:0047)
4a81:6564 Protect::PageIndex: longint                    // already in globals.md; indexes the table below
4a81:6568 Protect::PageCodes: array[1..21] of integer    // folded 6566 (264a:034a)
4a81:6bba Protect::Key1: longint                         // 6bba..6bc1, 24eb/264a/2841/2e4b
4a81:6bbe Protect::Key2: longint
```

## 6. Races (`29b9`, `20d4`, `1430`/`1537`, `1b8a`)

Index 2..12, matching the race names at `5ab2` where 1 is Humans, the player.
Every loop runs from `2` to `0xc` (29b9:03d2).

```pascal
type
  TRaceForce = record              { 27 bytes; garrison or fleet }
    Mission:  byte;                { +00 2 = stationed; >1 means hostile (29b9:03d2) }
    Unk01:    byte;                { 14h..64h }
    Active:   boolean;             { +02 }
    Unk03:    word;
    Timer:    integer;             { +05 Random(100 - Race.Speed) + 5 (29b9:0316) }
    NextMission: byte;             { +07 copied to Mission (29b9:0379) }
    System, Body, Moon: byte;      { +08..+0A location, compared with the current battle (1430, 1537) }
    Units:    array[1..8] of integer;  { +0B unit counts }
  end;

  TRace = record                   { 228 bytes (E4h) }
    Name:       string[13];        { 'Jaanosians', 'The Morgruls', ... 'Earthlings' }
    Unk0E, Unk0F: byte;
    Speed:      byte;              { +10 20..95 }
    HomeSystem, HomeBody: byte;    { +11, +12 Jaanosians 1/7 (Jade), Earthlings 8/3 (Earth) }
    KnownSystems: array[1..8] of boolean;  { +13 }
    Status:     shortint;          { +1B contact state 0/2/4/6 (29b9:0000 sets 2/4/6; Main 1000:2301) }
    Credits:    longint;           { +1C 400000 for all }
    Unk20:      array[1..6] of shortint;   { all $FF }
    ForceCount: shortint;          { +26 }
    Forces:     array[1..7] of TRaceForce; { +27 }
  end;
```

```
4a81:6bce Races::Races: array[2..12] of TRace   // folded 6a06; Status at 6a21, ForceCount at 6a2c, Force fields at 6a12..6a1c + k*1Bh
```

## 7. Commanders (`3fab`, `1991`, `3abd`, `1b8a`)

Index `[Category, Choice]`. Category is 1 Pilots, 2 Builders, 3 Fighters or
4 Developers, and there are 3 choices per category.

```
4a81:579c Staff::Price: array[1..4,1..3] of longint     // folded 578c; 4500, 25000, 120000 for the pilots
4a81:57cc Staff::Level: array[1..4,1..3] of integer     // folded 57c4; 10/20/40 "scholarly level", saved by 1a3e
4a81:57e4 Staff::Ability: array[1..4,1..3] of byte     // folded 57e0; 30/70/90
4a81:57f0 Staff::DevSkills: array[0..6,1..3,1..4] of byte  // folded 57eb; developer Math/Physics/Elect/A.Int per training grade (DAT_91ea), compared with research needs at 95ac..95b2; first 12 bytes saved
4a81:5844 Staff::Names: array[1..4,1..3] of string[18]  // 'Bruce Jordan'..'Klaatoo' (manual pp. "Commanders"); also seen as 57f8/580b/586a/58dc + n*13h
4a81:5780 Staff::FacePics: array[1..4] of string[6]     // folded 5779; 'faces1'..'faces4' (1991:0355)
```

## 8. Name lists

| Start | Type | Contents | Folded |
|---|---|---|---|
| `5928` | `array[1..5] of string[13]` | Group types: Army, Trade, Pirate, Carrier, Planet forces | `591a` (*0Eh) |
| `596e` | `array[1..5] of string[16]` | Default group names: 1st Army Group … New Earth forces | `595d` (*11h) |
| `59c4` | `array[1..4] of string[11]` | Satellite, Spy Sat, Spy Ship, Solar sat | |
| `59f4` | `array[1..4] of string[9]` | Hunter, Fighter, Destroyer, Cruiser | |
| `5a1c` | `array[1..5] of string[8]` | Trooper, Tank, Aircraft, Missile, Base | `5a13` (*9) |
| `5a4a` | `array[1..5] of string[9]` | Rebellious … Excellent | |
| `5a7c` | `array[1..6] of string[8]` | Ores: Detoxin, Energon, Kremir, Lepitium, Raenium, Texon | `5a73` (*9) |
| `5ab2` | `array[1..12] of string[9]` | Races: Humans, Jaanosian … Earthling | |
| `5b2a` | `array[1..4] of string[8]` | Army, Trade, Pirate, Sat carr | |
| `5b4e` | `array[1..4] of string[7]` | Math, Physics, Elect, A.Int | `5b46` (*8) |
| `5b6e` | `array[1..8] of string[8]` | Systems: Amnesty … Sun | `5b65` (*9) |
| `5bb6` | `array[1..8] of string[6]` | Short system names: Amnes, Phnx … | |
| `5bee` | `string` (255) | Font charset (`405f` uses `5cee/5cf0` just after it) | |

```
4a81:5928 Groups::TypeNames: array[1..5] of string[13]
4a81:596e Groups::DefaultNames: array[1..5] of string[16]
4a81:59c4 Names::Satellites: array[1..4] of string[11]
4a81:59f4 Names::ShipClasses: array[1..4] of string[9]
4a81:5a1c Names::GroundUnits: array[1..5] of string[8]
4a81:5a4a Names::Ratings: array[1..5] of string[9]
4a81:5a7c Names::Ores: array[1..6] of string[8]
4a81:5ab2 Names::Races: array[1..12] of string[9]
4a81:5b2a Names::GroupKinds: array[1..4] of string[8]
4a81:5b4e Names::Disciplines: array[1..4] of string[7]
4a81:5b6e Names::Systems: array[1..8] of string[8]
4a81:5bb6 Names::SystemsShort: array[1..8] of string[6]
4a81:5bee Gfx::Charset: string
```

## 9. User interface (`3a1b`, `3abd`, main program)

```
4a81:489c Ui::ButtonLabels: array[1..77] of string[17]   // folded 488a; 'DISK OPERATIONS' .. 'ALIEN PLANETS'; index from hotspot records at 7c64
4a81:4e5f Ui::ButtonSounds: array[1..77] of string[8]    // folded 4e56; Main passes 95ea*9+4e56 to 431a:031a; 'disk','maincpu' = SOUND\DISK.SMP, MAINCPU.SMP
4a81:5114 Ui::Screens: array[1..38] of TScreenInfo       // folded 50fb; Name string[8] ('main','faces','research'.. 'colinfo' = GRAFIKA\*.PIC) + 16 bytes; +9/+A/+B read with 95e6 (Main 1000:0733)
4a81:4834 Conv::Procs: array[1..16] of pointer           // folded 4830; far procs in 3955 (3955:030e)
4a81:4875 Ui::RowOffset: array[1..20] of byte            // folded 4874; 3a1b:0586 (*140h)
4a81:55de Cc::Doors: array[1..?] of 7-byte records       // folded 55d7; 3abd:0c70 (frames, w:word, h, x:word, y)
4a81:5656 Cc::Rooms: array[1..?] of 5-byte records       // folded 5651; 3abd:0c05
4a81:5560 SpWar::SpriteRects: array[1..?] of 5-byte records // folded 555b; 20d4:0a85
```

## 10. Smaller tables (L, for completeness)

```
4a81:08d8 Groups::SlotLayouts      // 4 variable-length blobs (at 08d8, 0976, 0a14, 0ab2): count bytes + {string, x:word, y:byte, x2:word, y2:byte, 2 bytes} entries: Laser/Twin/Miss/Plasm/Hunter.., Sloop/Trade ship.., Trooper/Tank.., Sat/SypSt..
4a81:0b50 Groups::TypeSlots: array[1..5] of record Used: array[1..4] of boolean; Layout: array[1..4] of pointer end  // folded 0b3b/0b3c (20 bytes); 2b8d, 26fe
4a81:0bb4 Groups::CatRange: array[1..5] of record Lo, Hi: byte end  // folded 0bb2
4a81:0bbe Groups::SlotA: array[1..5,1..4] of byte  // folded 0bb9
4a81:0bd2 Groups::SlotB: array[1..5,1..4] of byte  // folded 0bcd
4a81:05d8 SpWar::ClassA: array[1..4] of longint    // folded 05d4; 800, 2000, 5000, 12000
4a81:05e8 SpWar::ClassB: array[1..4] of longint    // folded 05e4; 11, 68, 374, 840
4a81:07b8 SpWar::ClassBytes                        // 07b8..07cb read as 07b7/07bb/07bf/07c3 + class, 20d4
4a81:085a Protect::WordCodes: array[1..14] of integer  // folded 0858; 2693:0342
4a81:5cfe Gus::RateTable: array[1..32] of word     // folded 5cfc; 592, 296, 197, ... (sound driver)
```

## Open questions

1. **Constant indices.** Are the many `DAT_4a81_5d4x–5d76` references (`1b8a`,
   `29b9`) separate variables, or `Products` accessed with an offset index?
2. **TBody unknown bytes.** `+01/+03/+04/+05/+09/+0A/+0B/+14/+16` are
   unexplained, and so are the bucket labels for `+11` and `+13`. The labels
   could be swapped: `357b:18cc` reads `+13` first, for your own colonies.
3. **Stores.** What are `Stores[7..8]`? The ore stock elsewhere has 6 entries.
4. **TProduct unknown fields.** `+13` and `+1F` are unexplained. `Progress`
   starts at 10000, so it may count down rather than up.
5. **TBuilding unknown fields.** `Unk12[0..12]`, `+2B` and `+2C` are
   unexplained.
6. **TRace unknown fields.** The meaning of `Status` (0/2/4/6), `Unk20` (all
   $FF, perhaps per ore) and `TRaceForce.Units[1..8]` (possibly 4 ship classes
   plus 4 ground units).
7. **The `1a38–1a39` gap.** These 2 bytes sit between `Sat::AnimPath` and
   `SurfaceMap`. `SurfaceMap` element 11 and the colonisable flags may be
   offset by one.
8. **Undecoded ranges.** `0000–00c1` and `0108–0139` (ground war and save
   slot screen positions), `0d0e–0d95`, `481e–482f` (saved), and `0d0e+` read
   by `2e4b`/Main.
9. **Symbol format.** `symbols.txt` only takes existing types, so the record
   types need support in `ApplySymbols.java` (or Ghidra structures created
   first) before the typed lines above can be applied. Until then, use name
   plus comment.
