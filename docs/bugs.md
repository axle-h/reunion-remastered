# Suspected bugs in the original

Found while porting. The port reproduces them faithfully (marked `// BUG?` in
the code) until Phase 7, when each fix goes behind a clear switch.
`grep -rn 'BUG?' src/` lists them in the code.

## Fixed in the port

Switches in `fixes` (`src/game/fixes.h`). All are on in the game;
`REUNION_ORIGINAL=1` turns them off, and the differential tests always run
without them.

| Switch | Original behaviour | Fix |
|---|---|---|
| `find_jade` | Jade (Amnesty 7, status −3) is found only by New Earth's observatory, 1 in 50 per day and at most one find a day galaxy-wide (`sim.c`, `Sim_ProcessBodyEvents`). The Jaanosians must be met before their deadline, 6,000–6,399 h after the start (2928-04-23 at the earliest), or Hunter and Laser cannon are never granted and the game can't be won: about 1 game in 150 | If Jade is still hidden on 2928-04-01 it is found that day, with the observatory's message and name prompt. That leaves 551 h before the earliest deadline; the flight from New Earth takes at most 19 h with the slowest pilot (Leg2 150–199, Bruce Jordan at experience 10), and the meetings, Hunter, Laser cannon, Hyperspace drive and Morgrul wave 1 are all timed from first contact (`intel.c`, `Intel_FirstContact`), so a late contact doesn't shorten them |

## Suspected, not fixed

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
