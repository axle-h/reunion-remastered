# Data segment (`4a81`)

What's known about the game's global variables. Named variables are in
`ghidra/symbols.txt`; this page covers the layout and what's still open.

Regenerate the raw data with:

```sh
~/ghidra/support/analyzeHeadless ghidra/project reunion -process REUNION.PRG \
    -noanalysis -readOnly -scriptPath ghidra/scripts \
    -postScript GlobalsReport.java /tmp/globals.csv 4a81
tools/globals_map.py /tmp/globals.csv
```

That's 9,213 references from code to 1,355 distinct globals (first pass).

## Layout

| Range | Contents |
|---|---|
| `0000–759d` | Typed constants (initialised variables) of all units. Includes large game tables, e.g. 27-byte records at `011f` used by `Sim` |
| `759e–75ed` | `System` variables |
| `75ee–8a88` | Each unit's own variables, unit by unit in code-segment order: main program `75ee`, `1390` `760e`, `1430` `7618`, `1537` `7628`, … `3d38` `7e48`, `405f` `89b8`, sound units `89be–8a4a`, `Ems` `8a4c`, `Xms` `8a60`, `Mouse` `8a64` |
| `8a8a–a2e0` | Mostly **shared game state**, used by most units. Probably a unit with only variables (no code). Low-level units' buffers are mixed in (`Mouse` cursor state `9182–919e`, `Blit` `9270`, `9c02`) |
| `a2e2–a2f4` | `Crt` variables |
| `a2f6–a543` | `System.Input`, `System.Output`, saved vectors |
| `a544–` | Stack and heap |

Within the per-unit area the main program's variables are interleaved with
each unit's. Those are probably variables declared in a unit's interface and
used by the main program.

## Known variables

| Address | Name | Notes |
|---|---|---|
| `95be` | `Game::Credits` | longint |
| `95de–95e4` | `Game::Year/Month/Day/Hour` | Advanced by `Sim::AdvanceHour`; months have 30 days |
| `91ec` | `Gfx::ScreenSeg` | Copy of `SegA000` |
| `8a64–8a8a` | `Mouse::*` | INT 33h registers, buttons, click position |
| `8a8c` | `Mouse::Hotspots` | 27-byte records: x, y, w, h, … (count at `8a8a`) |
| `9182–919e` | `Mouse::*` | Software cursor state |
| `89ac–89b4` | `RawText::*` | Pointers to the loaded `text\sz_*.raw` tables |
| `6564` | (protection) | Longint compared with 25358 by `Protect::CheckToken` |

## Detailed notes

All variables and tables below are named in `ghidra/symbols.txt`, with record
types in `ghidra/types.pas`. The evidence behind them (code addresses,
initial values, manual cross-checks) is in:

- [globals-evidence.md](globals-evidence.md): shared game state and
  screen/input state (`Game::*`), plus the screen table at `DS:50FB`
- [tables-evidence.md](tables-evidence.md): initialised tables (galaxy,
  products, building types, races, staff, pub, name lists)

The proposals were written with `Sim::` for shared state; in `symbols.txt` that
became `Game::`, and the building-type record became `TBuildingType` (the
14-byte placed building is `TBuilding`). The `Surface` view tables at
`17E6–1828` are tile-sheet sizes, not view coordinates.

## Open questions

- Most `TGroup` fields, `TBody` bytes `+01/+03/+09..+0B`, the `TProduct`
  and `TBuildingType` unknowns
- What fills `9C02–A201`
- `91E7`, `918E–9190` are only written outside the Pascal code, probably by
  the keyboard or mouse interrupt handler
