# Suspected bugs in the original

Found while porting. The port reproduces them faithfully (marked `// BUG?` in
the code) until Phase 7, when each fix goes behind a clear switch.
`grep -rn 'BUG?' src/` lists them in the code.

## Fixed in the port

Switches in `fixes` (`src/game/fixes.h`). All are on in the game; the port
menu (F1, Fixes) switches each one or all of them (the golden hashes,
`tests/test_golden.c`, check all on and all off).

| Switch | Original behaviour | Fix |
|---|---|---|
| `find_jade` | Jade (Amnesty 7, status −3) is found only by New Earth's observatory, 1 in 50 per day and at most one find a day galaxy-wide (`sim.c`, `Sim_ProcessBodyEvents`). The Jaanosians must be met before their deadline, 6,000–6,399 h after the start (2928-04-23 at the earliest), or Hunter and Laser cannon are never granted and the game can't be won: about 1 game in 150 | If Jade is still hidden on 2928-04-01 it is found that day, with the observatory's message and name prompt. That leaves 551 h before the earliest deadline; the flight from New Earth takes at most 19 h with the slowest pilot (Leg2 150–199, Bruce Jordan at experience 10), and the meetings, Hunter, Laser cannon, Hyperspace drive and Morgrul wave 1 are all timed from first contact (`intel.c`, `Intel_FirstContact`), so a late contact doesn't shorten them |
| `mine_all_systems` | The hourly mining pass (`sim.c`, `Sim_MineBody`) walks every open system's planets and moons but reads each body from the selected system's table (`Game_CurSystem`, the system the map shows). Only that system's colonies and miner droids mine; the others stop, and a selected-system body whose index also exists in another open system is mined once per such system, against that system's storage cap | Each body is read from its own system's table, so every colony and droid-mined body mines once an hour whatever the map shows |
| `meteorite_droids` | A meteorite that destroys a colony building lowers the miner droid count of `Game_CurBody`, the selected body, not of the colony it hit (`sim.c`, `Sim_ProcessBodyEvents`) | The hit colony loses the droid |
| `build_on_moons` | Construction speed uses the body's kind-16h output (body bytes 4..5), but `Sim_UpdateBuildings` reads it from the planet, not the moon a building is on, whenever that system's selected body is a planet | The building's own body |
| `derricks_on_moons` | A Derrick on a moon puts its ore into the planet's store whenever the selected system's selected body is a planet (`Sim_UpdateBuildings`) | The moon's store |
| `drifting_ship` | The drifting Morgrul ship (story flag DS:5D71) is found when any group arrives anywhere while the selected body is Phoenix 1, and missed when a group arrives at Phoenix 1 while another body is selected (`Sim_ProcessTravellingGroups`) | Found when a group arrives at Phoenix 1, whatever is selected |
| `disband_list_count` | `Intel_DisbandGroup` closes up the list it disbands from only up to `Game_ShownGroupCount`, the count of the list the ship info screen last showed. When a revolt, a dying colony, a lost battle or system 4's annihilation disbands groups of the other list, the callers' loops wipe every later group of that list or duplicate one and lose the last | The disbanded list's own count |
| `annihilate_leaves_screen` | System 4's annihilation (a story timer, `Intel_AnnihilateSystem4`) deletes buildings and groups and clears the selected group while the group, ship, cargo or surface screens may still hold their indices. `Intel_DestroyBody` leaves those screens first; this does not | Leaves them first, as `Intel_DestroyBody` does |
| `autoplace_when_full` | `Surface_AutoPlaceBuilding` (a new colony's command centre and starter buildings) drops the building if the player is watching that colony's surface and no spot is free; on any other screen it is added unplaced | Added unplaced in both cases |
| `away_staff_hidden` | A staff member at the university is left off the command centre (`Command_DrawStaffIcon`, `Command_SetupCommandHotspots`), but the builders' door (`Command_TickPanelDoorAnim`) redraws the builder each time it slides, and the window animation (`Command_TickRoomAnim`) redraws the kind-3 staff member each frame, without checking. They reappear until the screen is next entered | Neither redraws a staff member who is at the university |

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
