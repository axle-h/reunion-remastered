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
| `95be` | `Sim::Credits` | longint |
| `95de–95e4` | `Sim::Year/Month/Day/Hour` | Advanced by `Sim::AdvanceHour`; months have 30 days |
| `91ec` | `Gfx::ScreenSeg` | Copy of `SegA000` |
| `8a64–8a8a` | `Mouse::*` | INT 33h registers, buttons, click position |
| `8a8c` | `Mouse::Hotspots` | 27-byte records: x, y, w, h, … (count at `8a8a`) |
| `9182–919e` | `Mouse::*` | Software cursor state |
| `89ac–89b4` | `RawText::*` | Pointers to the loaded `text\sz_*.raw` tables |
| `6564` | (protection) | Longint compared with 25358 by `Protect::CheckToken` |

## Next candidates

These are used everywhere and should be identified next:

| Address | Uses | Clue |
|---|---|---|
| `95e6` | 241 | Compared with small constants (1, 3, 12, 20…): current screen or mode? |
| `7ae4` | 311 | Word; indexed by `75ee` |
| `7b08` | 139 | Far pointer to a record (first byte compared with 1) |
| `95ea`, `95ec` | 74, … | `95ea` is loaded from 21-byte records at `7c64` indexed by `95ec` |
| `95a4–95bc` | ~10 each | Small counters, all boosted by the cheat: staff/ship counts? |
| `a2c4–a2d0` | ~15 users each | Saved by `SaveLoad`: likely core game state |
