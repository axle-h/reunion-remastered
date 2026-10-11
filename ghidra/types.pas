{ Record types for REUNION.PRG, applied to Ghidra by ApplySymbols.java before
  symbols.txt. Borland Pascal records are unpadded, so fields follow each other
  directly. Unknown bytes are kept as filler fields (_xx) so offsets stay
  exact; the comments give each field's offset. Names are ours.

  Supported: scalar types, string[N], ^T, array[a..b] of T (and a..b, c..d),
  and records declared earlier in this file. }

type
  THotspot = record          { 27 bytes, Mouse::Hotspots }
    X:       integer;        { +00 }
    Y:       byte;           { +02 }
    W:       integer;        { +03 }
    H:       byte;           { +05 }
    Caption: string[18];     { +06  status-line text, shown by 405f:11e8 }
    Repeat_: boolean;        { +19  ? holding the button over it counts as a click (sets DS:8A82) }
    _1A:     byte;           { +1A }
  end;

  TMenuButton = record       { 21 bytes, menu bar at DS:7C51 }
    Caption: string[18];     { +00 }
    Command: integer;        { +13  copied to Game::MenuCommand }
  end;

  TGroupSlot = record        { 10 bytes }
    ShipType: integer;       { ? }
    Weapons:  array[1..4] of integer;   { ? }
  end;

  TCargoSlot = record        { 4 bytes }
    A: integer;              { ? }
    B: integer;              { ? }
  end;

  TGroup = record            { 161 bytes; fleets and planet forces }
    Kind:     byte;          { +00  1 army, 2 trade, 3 secret forces, 4 satellite carrier, 5 planet forces }
    Name:     string[17];    { +01 }
    System:   byte;          { +13  location }
    Planet:   byte;          { +14 }
    Moon:     byte;          { +15  0 = the planet itself }
    Status:   byte;          { +16  ? 1 for a new group; travel state }
    Leg1:     integer;       { +17  ? distance left to leave the system }
    Jump:     integer;       { +19  ? hyperspace hours left }
    Leg2:     integer;       { +1B  ? distance left to arrive }
    Units:    array[1..2, 1..4] of TGroupSlot;   { +1D  ? [1] space fleet, [2] ground forces }
    Cargo:    array[1..6] of TCargoSlot;         { +6D  ? }
    Items:    array[1..13] of integer;           { +85  ? cargo item counts (2583) }
    Storage:  integer;       { +9F  planet forces: storage capacity / 1000 ? }
  end;

  TBuilding = record         { 14 bytes }
    Kind:    byte;           { +00 }
    System:  byte;           { +01 }
    Planet:  byte;           { +02 }
    Moon:    byte;           { +03 }
    X:       byte;           { +04  map position }
    Y:       byte;           { +05 }
    Health:  byte;           { +06  ? 100..159 when built, 0 = destroyed/inactive }
    Output:  byte;           { +07  ? }
    Flag:    byte;           { +08  cleared by 357b:3a05 }
    W9:      integer;        { +09 }
    WB:      integer;        { +0B }
    BD:      byte;           { +0D }
  end;

  TColonySetup = record      { 16 bytes; a colony under construction }
    Active:  boolean;        { +00 }
    System:  byte;           { +01 }
    Planet:  byte;           { +02 }
    Moon:    byte;           { +03 }
    Timers:  array[1..6] of integer;   { +04  hours left per item; building per item at DS:651E + 10*k }
  end;

  { --- Initialised tables (see docs/tables-evidence.md for evidence) --- }

  TBody = record             { 65 bytes; a planet or moon }
    Owner:       byte;       { +00  0 none, 1 player, 2..12 race }
    _01:         byte;
    Surveyed:    boolean;    { +02  needed for "Empty planet, ready for colonisation" }
    _03:         boolean;
    _04:         array[4..5] of byte;
    Colony:      boolean;    { +06  player colony }
    Miner:       shortint;   { +07  miner station / droids (the globals agent saw +0A tested instead; check) }
    Satellite:   byte;       { +08  set to 1 or E2h by Satellite (28cd) }
    _09:         byte;
    _0A:         byte;       { +0A  compared with counts in 1b8a:0cba }
    _0B:         byte;
    InfoLevel:   shortint;   { +0C  how much is known; landing sets 6 }
    Population:  longint;    { +0D  Earth 143000, New Earth 30000 }
    Resistance:  shortint;   { +11  0..69 -> None .. Very difficult }
    TechLevel:   byte;       { +12  0..7 -> Oppressing .. Super tech }
    Attitude:    shortint;   { +13  0..69 -> revolting .. loyal }
    _14:         shortint;   { +14  usually FFh }
    SurfaceType: byte;       { +15  1 Earth-like, 2 Gaseous, 3 Icy, 4 Watery, 5 Tropical, 6 Desert, 7 Swampy, 8 Rocky, 9 Too hot, 10 No atmosphere }
    _16:         byte;
    Diameter:    word;       { +17  tens of km }
    Temperature: integer;    { +19  kelvin }
    Stores:      array[1..8] of longint;   { +1B  1..6 = ores }
    Deposits:    array[1..6] of byte;      { +3B  ore richness }
  end;

  TPubPerson = record        { 27 bytes }
    Name:     string[13];    { +00 }
    Portrait: byte;          { +0E  alien\alien<N> }
    State:    byte;          { +0F }
    Timer:    integer;       { +10  counted down hourly by Sim }
    Deal:     byte;          { +12  index into Pub::Deals; > 10 = Deal-10 with amounts halved }
    SrcX:     integer;       { +13  sprite position in the pub sheet }
    SrcY:     byte;          { +15 }
    X:        integer;       { +16  screen position and hotspot }
    Y:        byte;          { +18 }
    W, H:     byte;          { +19 }
  end;

  TMenu = record             { 6 bytes }
    Count: byte;
    Items: array[1..5] of shortint;   { FFh = unused }
  end;

  TPubDeal = record          { 24 bytes }
    Items:   array[1..4] of byte;     { +00  1..6 ore, >= 10 product id + 10 }
    Amounts: array[1..4] of word;     { +04 }
    Kind:    byte;           { +0C }
    Chance:  byte;           { +0D  Random(100) < Chance }
    Year:    word;           { +0E  date the deal happens }
    Month, Day, Hour: byte;  { +10 }
    Active:  word;           { +13 }
    _15:     array[0..2] of byte;
  end;

  TPubAnim = record          { 20 bytes; ambient pub animation }
    SrcX, SrcY, X, Y, W, H: integer;          { +00 }
    FramesPerRow, FrameCount, Frame: byte;    { +0C }
    MinDelay, RandDelay, FrameDelay: byte;    { +0F  tens of ticks }
    Timer: integer;          { +12 }
  end;

  TMoonList = record         { 9 bytes }
    Count: byte;
    Moons: array[1..8] of byte;   { body indices }
  end;

  TBodyName = record         { 13 bytes }
    Name: string[9];         { +00 }
    Size: byte;              { +0A  icon size on the system map? }
    X, Y: byte;              { +0B  system-map hotspot }
  end;

  TProduct = record          { 53 bytes; inventions/products }
    Name:       string[16];  { +00 }
    State:      integer;     { +11  0 unknown, 1 can be developed, 2 under development, 3 can be analysed, 4 under analysis, 5 done }
    DevTime:    word;        { +13 }
    Progress:   integer;     { +15  /100 = % completed }
    Price:      longint;     { +17 }
    Stock:      integer;     { +1B  pieces in store }
    _1D:        word;
    _1F:        word;        { +1F  50..2000: build time or storage size? }
    _21:        word;
    Produced:   boolean;     { +23 }
    SpaceBuilt: boolean;     { +24  Destroyer and Cruiser only }
    Ores:       array[1..6] of word;   { +25  ore needed }
    Skills:     array[1..4] of byte;   { +31  required Math, Physics, Elect, A.Int }
  end;

  TBuildingType = record     { 63 bytes }
    Name:        string[14]; { +00 }
    NeedProduct: byte;       { +0F  product that must be done }
    _10:         byte;
    Category:    byte;       { +11  1 power, 2 mine, 3 civil, 4 radar, 5 defence, 6 industry, 7 housing, 8 command centre, 9 miner station }
    _12:         array[0..12] of byte;   { 0/1 flags: allowed surface types? }
    Workers:     integer;    { +1F }
    Energy:      longint;    { +21  consumption, kwh }
    Production:  integer;    { +25 }
    Cost:        longint;    { +27  credits }
    _2B:         byte;
    _2C:         byte;
    Width, Height: byte;     { +2D  in tiles }
    Tiles:       array[1..4, 1..4] of byte;   { +2F }
  end;

  TRaceForce = record        { 27 bytes; garrison or fleet }
    Mission:     byte;       { +00  2 stationed, > 1 hostile }
    _01:         byte;
    Active:      boolean;    { +02 }
    _03:         word;
    Timer:       integer;    { +05 }
    NextMission: byte;       { +07 }
    System, Body, Moon: byte;   { +08 }
    Units:       array[1..8] of integer;   { +0B }
  end;

  TRace = record             { 228 bytes }
    Name:         string[13];   { +00 }
    _0E, _0F:     byte;
    Speed:        byte;      { +10  20..95 }
    HomeSystem, HomeBody: byte;  { +11 }
    KnownSystems: array[1..8] of boolean;   { +13  the Intel port reads [1..4] as ship-class and [5..8] as ground-unit weapon flags }
    Status:       shortint;  { +1B  contact state 0/2/4/6 }
    Credits:      longint;   { +1C }
    _20:          array[1..6] of shortint;
    ForceCount:   shortint;  { +26 }
    Forces:       array[1..7] of TRaceForce;   { +27 }
  end;

  TScreenInfo = record       { 25 bytes; Ui::Screens }
    Name: string[8];         { +00  GRAFIKA\<name>.PIC }
    _09:  array[1..16] of byte;
  end;

  TSatStep = record          { 3 bytes }
    DX, DY, Frame: byte;
  end;

  TStarterBuilding = record  { 10 bytes }
    Building, X, Y, W, H: integer;
  end;

  TAdvisorHint = record      { 4 bytes }
    Who, Product: integer;
  end;

  TTypeSlots = record        { 20 bytes }
    Used:   array[1..4] of boolean;
    Layout: array[1..4] of pointer;
  end;

  TCatRange = record         { 2 bytes }
    Lo, Hi: byte;
  end;

  { --- Tables found while naming the data segment (2026-10-11) --- }

  TVecPoint = record         { 6 bytes; a model point or centroid }
    X, Y, Z:     integer;    { +00 }
  end;

  TOrbit = record            { 18 bytes; a body in the system view, PlanetInfo::Orbits }
    RX, RY:      integer;    { +00  orbit radii, from TBodyName.X/Y }
    Angle:       integer;    { +04  tenths of a degree }
    Speed:       word;       { +06  TBodyName.Size }
    X:           integer;    { +08  where it is drawn }
    Y:           word;       { +0A }
    Size:        integer;    { +0C }
    Half:        integer;    { +0E  Size div 2 }
    Icon:        byte;       { +10  Galaxy::BodyIcon }
    Flag:        byte;       { +11  marker: 1 known, owner + 1 alien owner known, 2 the player's }
  end;

  TViewGroup = record        { 6 bytes; Launch::SystemViewTable }
    Kind:        integer;    { +00  group kind (<= 4: the player's group), race + 3 for an alien force, 0 none }
    Index:       integer;    { +02  group number or force index }
    Moon:        integer;    { +04 }
  end;

  TRoomAnim = record         { 7 bytes; Cc::Doors, animations of anim\main }
    Frames: byte;            { +00  frame count (3abd:0c70, 2e4b:0044) }
    W:      integer;         { +01 }
    H:      byte;            { +03 }
    X:      integer;         { +04 }
    Y:      byte;            { +06 }
  end;

  TWarAnim = record          { 5 bytes; Cc::Rooms, animations of war\grwar\granim }
    Frames: byte;            { +00  frame count (3abd:0f19) }
    Speed:  byte;            { +01  retraces per frame }
    Repeat_: byte;           { +02  loops }
    UnitA:  byte;            { +03  unit types that must both be on the field (1537:11f2) }
    UnitB:  byte;            { +04 }
  end;

  TLight = record            { 3 bytes; Command::Lights }
    Ofs:   word;             { +00  pixel offset in the window below the toolbar }
    Count: byte;             { +02  on/off timer (lights 12..19) }
  end;

  TGrEffect = record         { 6 bytes; GrWar::Effects }
    First:  integer;         { +00  first frame in the effect sheet }
    Sheet:  integer;         { +02  gricon2 sheet (attackers' row; defenders add 1 for effects 1..4) }
    Frames: integer;         { +04  frame count; its low byte starts the unit's effect timer (1537:188f) }
  end;

  TSaAnim = record           { 5 bytes; SpWar::SpriteRects, war\war\sa<n>.ani }
    Frames: byte;            { +00 }
    Width:  byte;            { +01 }
    Height: byte;            { +02 }
    X:      byte;            { +03  screen position of the frame }
    Y:      byte;            { +04 }
  end;

  { --- Borland units and system --- }

  Registers = record         { 20 bytes; Dos.Registers, passed to Intr }
    AX, BX, CX, DX, BP, SI, DI, DS, ES, Flags: word;
  end;

  TEmsBlock = record         { 13 bytes; an EMS-backed buffer (4719) }
    State:     byte;         { +00  0 none, 1 heap copy, 2 in EMS }
    Data:      pointer;      { +01  heap copy }
    Size:      word;         { +05 }
    Handle:    word;         { +07 }
    FrameSeg:  word;         { +09  EMS page frame segment }
    PageCount: word;         { +0B }
  end;
