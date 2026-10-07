# `WAR/ANIM/ANIM.DEF`

Scripts for the animated background props of the space battle screen (segment
`20d4`). 14000 bytes = **25 scripts of 560 bytes**, each 140 entries of 4
bytes. Dumper: `tools/animdef.py` -> `tools/out/animdef.json`.

Loader `20d4:0985(n)`: seeks to `(n-1)*230h` and reads one script into a 560-byte
buffer (`DAT_7e44`). A random script 1-25 is picked (the "not yet played"
set at `DS:77F6` ensures each plays once before repeats) and played at
screen rect x 0A0h-13Fh, y 31h-C7h; after finishing it re-rolls.

## Entry (4 bytes)

| Byte | Meaning |
|---|---|
| 0 | war-anim id (0 = nothing drawn). `Anim::LoadWarAnim(id)`; per-anim 5-byte info table at `DS:555B + id*5` (screen offset bytes at +1..+4) |
| 1 | frame number for `Anim::DrawAnimKeyed` |
| 2 | sound id for `Sound::PlayWarSnd` (0 = none) |
| 3 | delay in ticks until the next entry (the game sets the countdown `DS:7796` from it); **FFh = end of script** (the entry's other bytes are ignored) |

The per-tick routine (`20d4:0a85`) advances to the next entry, and on an end
marker either restarts at entry 1 or switches script. The first entry of a
script often has delay 0 (immediate next). Statistics: 965 entries in 25
scripts (6-140 each); script n uses anim id n almost exclusively (script
24 uses 140 entries with no marker before the buffer ends and a final
zero entry carrying the FFh delay).
