# Suspected bugs in the original

Found while porting. The port reproduces them faithfully (marked `// BUG?` in
the code) until Phase 7, when each fix goes behind a clear switch.
`grep -rn 'BUG?' src/` lists them in the code.

| Where | What |
|---|---|
| `431a` `PlayWarSnd` / `PlayGrSnd` | Both use the same "last loaded sample" key, so ground sound *n* right after war sound *n* replays the war sample |
| `43d5` sound driver | Hard-coded GUS base 220h; failed GUS detection still writes to ports 0–2; timer ISR never chains to INT 08h (see sound-original.md). Not reproduced: the port replaces the drivers |
| `428e` AdLib detection | Waits on VGA retrace and can hang without VGA. Not reproduced |
| `26eb` money cheat | Unreachable: nothing ever sets its trigger flag (DS:91E7) |
| `3a1b` `AddToolbarIcon` | Clears the wrong toolbar slots |
| `398f` `BeginInput` | Backs up 60 bytes but stores with a 255-character limit |
| `34b0` `MessageBox` | Overflows its line array past 6 lines |
| `34b0` message log | The log count can go negative |
| `26fe` ShipInfo "Change mode" | On list 0 it plays the error sound but still switches to list 1 |
| `2ef2` `RegisterBuildingAnims` | Marks the extra footprint of building kind 19h at uninitialised coordinates when no spot was found. The port skips that marking (it cannot reproduce leftover stack contents) |
| Copy protection (`2693`) | An armed but never-asked date check scribbled random bytes; the port auto-answers it instead (protection removed) |
