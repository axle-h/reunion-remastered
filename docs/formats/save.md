# Saved games (`SAVE\SPIDYSAV.<n>`) and `SAVE\INIT`

A save is a raw memory dump of the game's state variables. It has no magic,
version, checksum, compression or obfuscation. Values are little-endian and
records use the unpadded Borland Pascal layouts in `ghidra/types.pas`.
Dumper: `tools/save.py` (JSON, or `--layout` for the block table).

## Code

| Routine | What it does |
|---|---|
| `1a3e:029c` SaveLoadGame(Save: boolean) | Opens `save\spidysav.<slot>` (`Rewrite`/`Reset` with record size 1), then handles the header and 135 blocks in a fixed order. Each block goes through the nested helper `1a3e:0240`, which does one `BlockWrite` or `BlockRead` plus `CheckIO`. Afterwards it calls `405f:134c` and `405f:140a`. |
| `1a3e:0b93` / `1a3e:0b9d` | SaveGame / LoadGame: `SaveLoadGame(true/false)` for slot `DS:7732`. |
| `1a3e:0c40` | Reads the first 20 bytes (the slot name) of `spidysav.1..12` into the slot table `DS:7734^` (12 × 22 bytes: `Used: boolean; Name: string[20]`). It shows `Empty ...` when a file is missing. |
| `1a3e:0bb1` | Loads `save\init`. Main calls it when it sets up a new game (`1000:01a1`). |

Load does not validate the file. A short file stops at the failing `BlockRead`
with I/O error 100 (`CheckIO`), so the state is left half-loaded. Every save is
exactly **41,670 bytes**. Slots are numbered 1..12, so the file name is
`SPIDYSAV.1` .. `SPIDYSAV.12`.

The port appends a 6-byte trailer to saves of games with one of its cheats on
(`src/game/options.h`): `"RNCH"`, a version byte (1) and flags (1 = money
frozen, 2 = ore frozen). The original reads its fixed blocks and ignores the
rest, so such a save still loads in DOS (unfrozen). Saves of games without a
cheat on are byte-for-byte the original format.

`3955` (unit "RecConv") plays no part in saving. It is a sprite scaler (see
below), so no field conversion is applied: every block is a straight memory image.

## `SPIDYSAV.<n>` layout

Address = data address in DS `4a81`. `^XXXX` means the bytes are the target of
the far pointer stored at `DS:XXXX` (a heap or EMS buffer).

| Offset | Size | Address | Variable | Type |
|---|---|---|---|---|
| 0000 | 20 | 7734^ slot+1 | slot name | `string[19]` (the first 20 bytes of the slot's `string[20]`; the editor allows 16 characters; the default is `Save <n>`) |
| 0014 | 24 | 57CC | Staff::Level | `array[1..4,1..3] of integer` |
| 002C | 12 | 57F0 | Staff::DevSkills (first 12 bytes) | `array[1..3,1..4] of byte` |
| 0038 | 795 | ^7ADE | message log (Message::Init GetMem 31Bh) | `array[1..15] of TMessage` (`Kind: integer; Text: string[50]`; Kind 1/99 drawn highlighted) |
| 0353 | 2 | 7AE2 | message count | `integer` |
| 0355 | 270 | 013A | Pub::People | `array[1..10] of TPubPerson` |
| 0463 | 240 | 0424 | Pub::Deals | `array[1..10] of TPubDeal` |
| 0553 | 12 | 6518 | Races::Hostile | `array[1..12] of boolean` |
| 055F | 12 | 773E..7749 | 12 separate byte flags (pub and story events, Main screen 19h) | `byte` each |
| 056B | 416 | 1A9C | Galaxy::AmnestyNames | `array[1..32] of TBodyName` |
| 070B | 2080 | 1C3C | Galaxy::AmnestyBodies | `array[1..32] of TBody` |
| 0F2B | 156 | 248A | Galaxy::PhoenixNames | `array[1..12] of TBodyName` |
| 0FC7 | 780 | 2526 | Galaxy::PhoenixBodies | `array[1..12] of TBody` |
| 12D3 | 234 | 2860 | Galaxy::MirachNames | `array[1..18] of TBodyName` |
| 13BD | 1170 | 294A | Galaxy::MirachBodies | `array[1..18] of TBody` |
| 184F | 91 | 2DF8 | Galaxy::AntaresNames | `array[1..7] of TBodyName` |
| 18AA | 455 | 2E54 | Galaxy::AntaresBodies | `array[1..7] of TBody` |
| 1A71 | 169 | 3038 | Galaxy::OrionisNames | `array[1..13] of TBodyName` |
| 1B1A | 845 | 30E2 | Galaxy::OrionisBodies | `array[1..13] of TBody` |
| 1E67 | 299 | 3470 | Galaxy::LyraeNames | `array[1..23] of TBodyName` |
| 1F92 | 1495 | 359C | Galaxy::LyraeBodies | `array[1..23] of TBody` |
| 2569 | 65 | 3B86 | Galaxy::RigelNames | `array[1..5] of TBodyName` |
| 25AA | 325 | 3BC8 | Galaxy::RigelBodies | `array[1..5] of TBody` |
| 26EF | 416 | 3D56 | Galaxy::SunNames | `array[1..32] of TBodyName` |
| 288F | 2080 | 3EF6 | Galaxy::SunBodies | `array[1..32] of TBody` |
| 30AF | 64 | 47D6 | Galaxy::PlanetStatus | `array[1..8,1..8] of shortint` |
| 30EF | 8 | 4816 | Galaxy::SystemStatus | `array[1..8] of shortint` |
| 30F7 | 16 | 481E | per-system count of kind-6 buildings (recounted by 357b) | `array[1..8] of integer` |
| 3107 | 2 | 482E | count of kind-13 buildings (357b) | `integer` |
| 3109 | 2 | 7AE4 | Game::CurSystem | `integer` |
| 310B | 16 | 7AE6 | Game::CurPlanet | `array[1..8] of integer` |
| 311B | 16 | 7AF6 | Game::CurMoon | `array[1..8] of integer` |
| 312B | 2 | 9276 | Game::Hero | `integer` |
| 312D | 2 | 9278 | Game::HeroSpriteX | `integer` |
| 312F | 1855 | 5DAC | Research::Products | `array[1..35] of TProduct` |
| 386E | 2 | 95A0 | Game::SelInvention | `integer` |
| 3870 | 2 | 95A2 | Game::CurResearch | `integer` |
| 3872 | 8 | 95A4 | Game::StaffExp | `array[1..4] of word` |
| 387A | 8 | 95AC | Game::DevSkill | `array[1..4] of word` |
| 3882 | 2 | 95B4 | Game::ProductionRate | `word` |
| 3884 | 8 | 95B6 | Game::StaffHired | `array[1..4] of word` |
| 388C | 4 | 95BE | Game::Credits | `longint` |
| 3890 | 24 | 95C6 | Game::HomeStock | `array[1..6] of longint` |
| 38A8 | 8 | 95DE | Game::Year, Month, Day, Hour | `word` each |
| 38B0 | 2 | A21A | Game::ColonySetupCount | `integer` |
| 38B2 | 160 | A21C | Game::ColonySetups | `array[1..10] of TColonySetup` |
| 3952 | 2 | A2C0 | Game::BuildingCount | `integer` |
| 3954 | 14000 | ^A2BC | Game::Buildings^ | `array[1..1000] of TBuilding` |
| 7004 | 2508 | 6BCE | Races::Races | `array[2..12] of TRace` |
| 79D0 | 14 | A2C2..A2CE | Game::ShownGroupCount, GroupCount, PlanetForceCount, SelGroup, SelGroup0, SelGroup1, GroupList | `integer` each |
| 79DE | 10304 | ^A2D0 | Game::Groups^ | `array[0..1,1..32] of TGroup` |
| A21E | 1 | A2D4 | Game::GroupsEnabled | `boolean` |
| A21F | 4 | A2D6 | Game::GroupKindAllowed | `array[1..4] of boolean` |
| A223 | 70 | 91A0 | Game::IdeaTimer | `array[1..35] of integer` |
| A269 | 2 | 91EA | Game::StoryStage | `integer` |
| A26B | 91 | 5D3C..5DA5 | 61 unnamed globals (story/event state, Sim 1b8a and Intel 29b9), saved one by one; see the list below | byte/integer/longint |
| A2C6 | | | end of file (41,670 bytes) | |

The unnamed globals are listed below as DS address and size in bytes, in file order
(each one is its own block, and the 1-byte gaps in DS are not saved):
`5D3C 1, 5D3D 1, 5D3E 1, 5D3F 1, 5D40 2, 5D42 2, 5D44 2, 5D46 2, 5D48 1, 5D4A 2,
5D4C 1, 5D4E 2, 5D50 1, 5D52 2, 5D54 2, 5D56 1, 5D58 2, 5D5A 1, 5D5C 2, 5D5E 1,
5D60 2, 5D62 1, 5D64 2, 5D66 1, 5D67 1, 5D68 1, 5D6A 2, 5D6C 1, 5D6D 1, 5D6E 2,
5D70 1, 5D71 1, 5D72 2, 5D74 1, 5D75 1, 5D76 2, 5D78 1, 5D7A 2, 5D7C 1, 5D7E 2,
5D80 1, 5D82 2, 5D84 1, 5D86 2, 5D88 1, 5D8A 2, 5D8C 1, 5D8E 2, 5D90 1, 5D91 1,
5D92 2, 5D94 1, 5D95 1, 5D96 2, 5D98 1, 5D99 1, 5D9A 1, 5D9C 2, 5D9E 2, 5DA0 2,
5DA2 4`. This answers open question 1 in `tables-evidence.md`: the save treats
them as separate variables. They are not `Research::Products[0]`, because the sizes
don't match the TProduct fields.

`tools/save.py` checks each type's size against the size the game pushes (all
136 match), and checks that the sizes add up to the file length.

## `INIT` layout

`1a3e:0bb1` reads two blocks, and they are the same as save offsets 3952..7003:

| Offset | Size | Variable |
|---|---|---|
| 0 | 2 | Game::BuildingCount (23 in the shipped file) |
| 2 | 14000 | Game::Buildings^ `array[1..1000] of TBuilding` |

Total 14,002 bytes, consumed exactly. The 23 buildings are New Earth's starting
colony (System 1, Planet 5, Moon 0): Kind 1 = Command centre, 2 Nuclear Gen.,
4 Mine, 7 Building ×4, 17 Radar, and others (names from
`Surface::BuildingTypes`). The other new-game values are not in INIT. Main sets
them in code: Credits 120000, date 2927-08-13 23:00, StoryStage 1,
ProductionRate 10. Galaxy, products and races start from the EXE's initialised data.

Oddities in INIT:
- Slots 24..1000 are not zeroed: 2,674 non-zero bytes of stale memory remain
  (they're not record-aligned, and slot 24 starts with Kind 1Dh). The game ignores
  them because it only uses entries `1..BuildingCount`.
- Every starting building has `Health` (+06) = 0 and `BD` (+0D) = 100. Byte
  +07 (`Output`) varies from 54 to 241. Either the comment on TBuilding.Health is
  wrong, or 0 means "intact". Code in 357b counts buildings with +06 = 0.

## Segment 3955: sprite scaler, not a record converter

`3955:030e` ScaleSprite(Src, Dst: pointer; Size: integer) shrinks a 32×32 block
from a 320-byte-wide buffer to Size×Size (Size is clamped to 16..32) in a
320-wide buffer. A Bresenham accumulator drops source rows, and each kept row is
copied by `RowProcs[Size]`. That table is at `DS:47F0 + 4*Size`, so it is
`array[16..32] of pointer` at `DS:4830`, not `array[1..16]` at 4834. Its 17 entries
are unrolled `movsb`/`movsw` procs at `3955:0000` (width 16; Ghidra hadn't
found it), 0034, 0068, 009a, 00ca, 00f8, 0124, 0150, 017d, 01aa, 01dd, 0215, 024a,
027c, 02ab, 02d3 and 02f4 (width 32, a plain 32-byte copy). Each one samples a fixed
subset of the 32 source bytes and returns with `RETF 8`. The callers are
`357b:270b` and `357b:27e9`, which draw scaled planet and moon icons on the system map.
