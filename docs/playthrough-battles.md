# Playthrough battles (thread E)

How the three battle screens work in the port, what the player controls on
each, the tactic the fixture uses, and the forces the policy
([playthrough-policy.md](playthrough-policy.md)) must bring to win each scripted
battle. The drivers are in `tests/play/ui_battle.c`; the measurements come from
`tests/test_play_battle.c` (see [Measuring](#7-measuring)).

Unit abbreviations: H Hunter, SF Starfighter, D Destroyer, C Cruiser, Tr
Trooper, T Battle tank, Air Aircraft, ML Missile launcher.

## 1. How battles start

| Start | Code | Chain |
|---|---|---|
| Enemy wave at New Earth | A race force at 1/5/0 with `NextMission` 2 counts `Timer` down each hour; at 0 `Sim_CheckEnemyAttacks` (sim.c:1263-1328) sets the battle body (DS:7798/779A/779C), DS:7788 = colony attacked, DS:778B = 0, shows "Your colony is under attack by ..." (or "Your ships around ... is under attack") and switches to 29 | 29; **lost** space battle → 19 → 32; won → 7 |
| Own invasion | Star map: own Army group selected at a body whose owner is not allied and whose intel level is ≥ 40 → GROUND WAR (0x47) (launch.c:133, 207-238); needs a fighter hired, Kyle Douglas or MACH | 29 with DS:7788 = 1, DS:778B = 1; **won** space battle → 19 → 32; lost → 7 |
| Own space attack | ATTACK icon 0x37 on an alien fleet entry (launch.c:192-205) | 29 only |

Screen 29 → 19 is decided in main_program.c:1239-1244. A ground battle lost
at New Earth goes to screen 35 (death ending, `Halt(15)`); won against race 12
goes to 36 (`Halt(10)`) (main_program.c:1437-1446, 1308-1312).

Every attack opens with a modal message box; so do the story messages after
a won battle (MSG 16, 19, " Your forces have eliminated ...").
The shared waits close them (`ui_handle_interrupts`, tests/play/ui_message.h).

## 2. Space battle (screen 29, `spwar.c`)

**The player controls nothing but RETREAT (0x3E).** The fight is automatic.
RETREAT ends it as a loss (`RETREATED` makes `SpWar_PlayerWon` false,
spwar.c:701). When a side has no ships left the toolbar switches to END BATTLE
(0x41) over `war\spvict` / `war\splost` with the loss table.

- **Fleets** (`SpWar_InitSpaceFleets`): the player's side is every Army,
  Pirate and planet-force group of the player at the battle *planet* (any
  moon) with status 1, 2 or 7, plus forces of allied races (status 6). The
  enemy side is every hostile (status 2) race force at the planet, plus the
  ships stored on the planet and all its moons. Up to 500 ships per side.
- **Ships** have hit points by class (`SpWar_ClassA`): H 800, SF 2,000,
  D 5,000, C 12,000.
- **Damage per hit.** Enemy ships do the class value `SpWar_ClassB`: 11, 68,
  374, 840. Player ships do their slot's pooled weapons (laser 2, twin 7,
  missile 40, plasma 200) ÷ ships in the slot, × (fighter experience + 200)
  / 220, or × 2/3 without a fighter. A fully equipped Hunter (2L+1T) does 11,
  SF (4T+1M) 68, D 374, C 840: the same as the enemy's. Kyle Douglas is hired
  at experience 20 (factor 1.00) and grows to 70 (1.23); MACH starts at 40 and
  grows to 90 (1.32).
- **Each step** (every 4th retrace) every living ship fires at a random
  living enemy and hits 11% of the time. Hunters and Starfighters do a third
  of their damage to Destroyers and Cruisers. A small ship hitting a small ship
  also kills it outright with chance (table DS:07CA): H→H 4%, H→SF 2%,
  SF→H 7%, SF→SF 4%. Big ships have no instant kills: Destroyers and Cruisers
  decide battles against each other, and only they can kill big ships.
- **Result.** A side with no ships loses. A fifth of the ships destroyed
  survive, on both sides (`SpWar_ApplySpaceLosses`); surviving enemy forces go
  home.

So the outcome depends only on the fleets brought; the fixture waits for END
BATTLE and clicks it (`ui_space_battle`).

## 3. Unit assignment (screen 19, `grwarunits.c`)

`GrWarUnits_CollectAvailableForces` pools the ground units of the player's
groups at the battle *body* (planet and moon must match: the Army group's
second unit row, `Units[1]`, so **Army groups carry their ground units into
an invasion**; New Earth's planet force when defending) and of allied forces
there. The enemy's side is every hostile force at the body plus the body's
ground stores; all of it goes into slots.

The player's units are pre-filled into slots in type order (troopers, tanks,
aircraft, launchers), at most 30 Tr, 30 T, 10 Air or 5 ML per slot and 20
slots; what does not fit stays in the pool and survives. On the screen:

- "Remove unit" (left square of a box) returns the slot to the pool and
  closes the gap.
- "Add/sub" (the number) adds one unit from the pool (left button) or takes
  one off (right button).
- The toolbar icons ADD TROOPER/TANK/AIRCRAFT/LAUNCHER (0x0C, 0x13, 0x2C,
  0x24) add a new slot of that type with min(pool, cap) units.
- OK (0x0B) starts the ground battle; CANCEL ATTACK (0x2A, own invasions
  only, DS:778B) goes back to the star map.

**Slot order is the battle formation**: slot i stands at column
(i − 1) / 9 + 2 (slots 1–9 at the rear, x = 2; 10–18 at x = 3; 19–20 at x = 4)
and row (i − 1) mod 9, centred (grwar.c:171-187). `ui_assign_units(order)`
removes every slot and adds them back type by type, rear to front.

## 4. Ground battle (screen 32, `grwar.c`)

A 16 × 9 tile field (64, 52)–(319, 195) on screen, one tile 16 pixels. The
player's slots stand from x = 2 rightwards, the enemy's from x = 13 leftwards;
each side's base (the colony) is not a fighting unit. `GrWar_BattleRound`
runs once per retrace, also while the mouse button is held.

- **Enemy behaviour:** every enemy stack attacks the nearest player stack
  (mode 3), paths towards it and stops as soon as it is in weapon range;
  when its target dies it picks the nearest again.
- **Player stacks** start holding (mode 1) and **fire at any enemy in range
  whatever their order** (the highest-numbered one in range, which gives
  natural focus fire). Orders: click a stack (left panel: name, "Pcs", "Str"),
  then **Move** and a tile, or **Attack** and an enemy stack. An attacking
  stack paths towards its target until in range and prefers it as target;
  when the target dies the stack holds again.
- **Ranges** (from sprite pixel positions, grwar.c:530-587): Trooper adjacent
  (Chebyshev < 2); Tank Manhattan ≤ 2; Aircraft Chebyshev ≤ 2 (so it hits
  on the diagonal (2, 2) where a tank cannot answer); Missile launcher shells
  at squared distance 4..16 (2..4 tiles); next to an enemy it only scratches.
- **Damage per hit** = stack count × average attack × target weakness ÷ 500,
  clamped to **1..15** units. Attack per unit: Tr 2 (1L), T 13 (3L+1T),
  Air 28 (4T); weakness: Tr 40, T 20, Air 5, ML 10. Launcher shells do count × 45
  × weakness ÷ 180 (15 against tanks and troopers, 6 against aircraft) when
  they land on the target's tile. Cooldowns (retraces): Tr 30, T 20, Air 10,
  ML 60. Speed: one pixel per 7/6/4/10 retraces (a tile in 112 retraces for
  a tank).
- **Stun.** A stack that is hit plays the hit animation for about 10 retraces
  and **cannot fire while any animation plays** (grwar.c:714). Aircraft fire
  every ~11 retraces, so one aircraft stack keeps its target permanently
  stunned: a 30-tank stack facing 5 enemy aircraft on the diagonal dies without
  firing a shot. Troopers stun too (1 damage minimum, every 30 retraces).
  Every hit also counts at least 1. This is why aircraft decide ground
  battles, and why tanks alone lose to tanks plus troopers.
- **Result.** A side with no units left loses; RETREAT (0x48) is a loss. Then
  END BATTLE (0x41) over `war\grvict` / `war\grlost` with the loss table.
  Losses scale the units in the groups and forces at the body; unassigned
  units survive.

The ground battle uses the random generator only for which side moves first
in a round and for ties, so with the same forces it plays almost the same way
every time; small changes in formation or numbers, on the other hand, can
turn it round (Earth: 30T 20Air 30ML wins 10/10, 40T 20Air 30ML 8/10).

## 5. Tactic

Measured over seeds with the forces below (Kyle at hire, experience 20):

| Ground battle (enemy) | Own | HOLD | COUNTER | CHARGE |
|---|---|---|---|---|
| Earth 1 landing (30Tr 30T 30Air) | 20T 10ML | 6/6, lost 6.8T | 6/6, lost 6.8T | 6/6, lost 20T 5ML |
| Lisonian landing (30Tr 15T 5Air) | 30T 10Air | 6/6, lost 0.3T 5.2Air | 6/6, lost 3.5Air | 5/6 |
| Earth (30Tr 90T 50Air 30ML) | 60T 20Air 50ML | 10/10 | 10/10 | 10/10 |
| Mirach 3 (61Tr 40T) | 60T | 7/10 | 7/10 | 7/10 |

(An older ENGAGE tactic, which sent every idle stack forward once in
contact, lost 20T against Earth 1 where holding lost 7.)

The enemy always comes to the player, so **holding** is sound: the player's
stacks keep their formation and fire first at whatever enters range, while
charging breaks the line and spreads fire. The weak spot of holding is an
enemy that hits from where it cannot be answered (aircraft on the diagonal,
launchers at 3–4 tiles). **COUNTER** (`UI_TACTIC_COUNTER`, the default) holds
and only sends a tank or aircraft stack against an enemy stack that reaches
it while nothing is in its own reach. It is never worse than holding in the
runs above and loses less where aircraft are involved. Only on-screen
information is used: the stacks' sprites (type and pixel position) and the
selection panel.

**Formation** (`ui_assign_units`): launchers at the rear, then troopers,
tanks, and aircraft in front (order "mrta"). Aircraft are the best front
line (weakness 5: tanks do 3 per hit to them) and lock what attacks them;
launchers in the rear shell at 2–4 tiles over the front. Over ten ground
configurations, this order and "launchers, troopers, aircraft, tanks" won the
most (8 of 10 configurations each); the game's pre-fill won 7.

**Composition matters more than tactics.** Against aircraft, tanks are close
to useless (stun-lock), and against tanks plus troopers, tank-only forces are
a coin flip: Mirach 3 is won 10/10 by 30T + 10Air with no losses, but only
6/10 by 60 tanks.

## 6. Force sizes for the scripted battles

Win rates over seeds 1–10 (Mirach and Earth 1–20) with the policy's force
and alternatives. "Space" is the space battle, "ground" the landing. Losses
are means per run.

| Battle | Enemy | Policy force | Measured | Recommendation |
|---|---|---|---|---|
| Morgrul wave 1 (J8) | 6H (+3Tr) | 12H (2L), fallback 8H | 12H: 10/10, lose 1.8H; 10H: 10/10; **8H: 7/10** | ≥ 12H. A lost space battle lands the troopers on a New Earth with no ground units (no tanks yet): game over. **Fallback 8H is insufficient.** |
| Morgrul wave 2 (M8) | 25H (+10Tr) | 24H, 16SF (4T), 20T | space 10/10, lose 2.5H 0.9SF; ground (20T vs 10Tr) 10/10 | as planned |
| Morgrul wave 3 (M12) | 20H 10SF (+15Tr 2T) | 30H, 24SF, 30T | space 10/10, lose 3.9H 2.6SF; ground (30T) 10/10 | as planned |
| Mirach 3 (M18) | 20H 20SF 3D; 61Tr 40T | 20H 30SF 5D, 60T; fallback 40T | space 30/30; **ground 60T: 6/10, 40T: 4/10**; 75T 9/10; 90T 20/20 (lose ~29T); 60T + 30Tr 9/10 | **≥ 90 tanks** (or 60T + 30Tr). Plan and fallback are insufficient. |
| Lisonian wave (L7/L12) | 20H 20SF 3D; 30Tr 15T 5Air | 30H 30SF 5D, 40T | space 10/10, lose 10.7H 4.7SF; **ground 40T: 0/10** (5 aircraft stun-lock the tanks); 40T 10Air 10/10. Without Plasma: 30H 30SF 5D 10/10 (lose ~14H 4SF), 40H 40SF 6D 10/10 (lose ~9H 2.5SF); ground alone 40T 10Air 10/10, 30T 10Air 9/10, 40T 5Air 9/20, 60T 5Air 13/20, 60T 0/20 | Space as planned; the 40-tank reserve cannot hold a landing. Aircraft (≥ 10) come only with L11, after this wave, so the space win is the only defence: keep the margin (or add SF). |
| Orionis 3 (L13) | 30H 23SF 3D; 90Tr 35T | 30H 30SF 5D, 60T 20Air | space 10/10, lose 15.7H 5.8SF; ground 10/10, lose 3Air; 40T 4/10; 30T 10Air 10/10. Without Plasma: 30H 30SF 5D 10/10 (lose ~22H 12SF), 40H 40SF 6D 20/20 (lose ~13H 4SF); ground 40T 20Air 20/20, 40T 15Air 20/20 (lose 3Air), **40T 10Air 17/20**, 30T 10-20Air 20/20 | 40H 40SF 6D, 30T 20Air |
| Lyrae 5 (L13) | 30H 25SF 3D; 52Tr 10T | same | space 10/10; ground 10/10. Without Plasma: 40H 40SF 6D 20/20 (lose ~7H 2SF); ground 40T 10-30Air 20/20 (lose 1Air) | as Orionis 3 |
| Lyrae 7 (L13) | 40H 30SF 2D 1C; 70Tr 40T 10Air | 50H 50SF 8D, 60T 20Air | space 10/10, lose 15.1H 5.6SF; ground 10/10, lose 1T 8.8Air; 40T 20Air 10/10; **60T 10Air 4/10**, 40T 10Air 2/10 | the Lisonian wave's troops come back: next row |
| Lyrae 7 after the Lisonian wave (`lyrae7-back`) | the wave's fifth of ships and all its troops home: 44H 34SF 3D 1C; 100Tr 55T 15Air | 50H 50SF 8D, 30T 30Air (no Plasma) | space 30/30 (lose ~24H 10SF); 40H 40SF 6D 13/20; 60H 60SF 8D 10/10. Ground 30T 30Air 20/20 (lose 9-20Air), 40T 30Air 30/30 (lose 1T 9-22Air), 60T 30Air 20/20, 60T 25Air 19/20, 40T 25Air 15/20, 80T 20Air 18/20, 40T 20Air 11/20, 80T 15Air 9/20 | **≥ 30 aircraft**, 50H 50SF 8D |
| Earth 1 wave (X6) | 80SF 12D 3C; 30Tr 30T 30Air | 20SF 4D 2C, 40T 20ML | **space 0/10** (kills 1.7 SF); 50SF 10D 4C also 0/10; 50H 80SF 15D 6C 10/10. Ground 40T 20ML: 10/10, no losses; 20T 10ML 10/10. 20 seeds: 40T 10Air 0/20, 40T 20Air 0/20, 40T 30Air 8/20; 40T 10Air 5ML, 40T 10Air 10ML, 40T 10ML, 40T 20ML, 20T 10ML 20/20 | **Home guard ships are wasted**; the ground reserve wins alone, but only with launchers. Keep 40T 10Air 10ML (act 4's Home Guard and 10 launchers) and no warships at New Earth, or a fleet of ≥ 50H 80SF 15D 6C. |
| Earth (X3/X9) | 80H 80SF 7D 5C; 30Tr 90T 50Air 30ML | 50H 50SF 10D 6C; 60T 20Air 50ML | **space 6C: 0/10**, 8C: 0/10; 10C: 20/20 (lose 40H 39SF 4D); 12C 10/10; 60H 60SF 12D 10C 10/10. Ground plan 10/10 but loses all tanks and aircraft and 30 ML. With Plasma, 10 seeds: 50H 50SF 10D 10C 10/10 (lose 40H 40SF 1-8D 0-1C), 50H 50SF 12D 10C 10/10, 60H 60SF 12D 10C 10/10 (lose ~48H 40SF 0-2D), 40H 50SF 10D 10C 9/10, 50H 50SF 10D 8C 3/10. Ground (`earth-ground` needs one own ship to carry the troops): 40T 20-40Air with 0-20ML 0/10; 40T 30Air 30ML 6/10, 40T 20Air 30ML 9/10, 30T 20Air 30ML 10/10; 20 seeds: 40T 20Air 40ML (lose 10T 20Air 30ML), 40T 20Air 50ML, 40T 10Air 50ML, 30T 20-30Air 40ML, 60T 20Air 50ML 20/20 | **≥ 10 Cruisers**: the policy takes 60H 60SF 12D 10C. Ground 40T 20Air 40ML (≥ 40 launchers win every time; each costs the Missile line 5 missiles). |

**As played** (thread F, the whole-game fixture, seed 12345; Kyle after his
course; one proof run and runs resumed from its checkpoints):

| Battle | Own | Enemy | Result |
|---|---|---|---|
| Morgrul wave 2 | 24H (2L+1T) 16SF (4T); 19T at home | 25H (10Tr) | space won, lost 2–4H 0–2SF |
| Morgrul wave 3 | 30H 24SF; 30+T at home | 20H 10SF (15Tr 2T) | space won, lost 3–11H 2–4SF |
| Mirach 3 | 40H 40SF 6D (+ Kall fleet 2: 15H 10SF) | 55 ships | space won, lost 21–30H 18–20SF 3D (the Kalls' included) |
| Mirach 3, ground | 90T (3L+1T) + the Kalls' 10Tr 35T | 89Tr 42T | won, lost 14–69T |
| Lisonian wave (New Earth) | Strike docked, 40–50H 40–50SF 6–8D (no Plasma); 70–80T at home | 20H 20SF 3D (30Tr 15T 5Air) | space won, lost 3–8H 1–5SF |
| Orionis 3 | 47–50H 49–50SF 8D | 30H 23SF 3D (56 ships) | space won, lost 4–11H 1–5SF |
| Orionis 3, ground | 30–40T 20–30Air | 90Tr 35T | won, lost 3–6Air |
| Lyrae 5 | 41–50H 45–50SF 8D | 42 ships | space won, lost 2–7H 0–4SF |
| Lyrae 5, ground | 30–40T 20–28Air | 52Tr 10T | won, lost 1Air |
| Lyrae 7 | 50H 50SF 8D | 81 ships | space won, lost 16–21H 3–5SF (the grids; the loss table is wrong, below) |
| Lyrae 7, ground | 30–40T 30Air | 100Tr 55T 15Air | won, lost 0–1T 10–19Air |
| Earth | 60H 60SF 12D 10C (Plasma) | 80H 80SF 7D 5C (172 ships) | space won, lost 39–48H 20–42SF |
| Earth, ground | 40T 20Air 40ML | 30Tr 90T 50Air 30ML | won, lost 10T 20Air 30ML: VICTORY |

At Mirach 3 the enemy is more than §6 measured: the waves' survivors go
home there (a fifth of the destroyed survive, `SpWar_ApplySpaceLosses`),
so the space side was 55 ships and the ground 89Tr 42T, not 20H 20SF 3D
and 61Tr 40T. Allied forces at the battle body fight on the player's
side in both battles (status 6: `SpWar_InitSpaceFleets`,
`GrWarUnits_CollectAvailableForces`), so the Kalls' fleet 2, sent to
Mirach 3 by talk 6, joins the invasion. 30H 30SF 5D with them also won
(lost 31H 19SF 3D; ground 125T 10Tr lost 69T), but with little to spare
in space; the three Morgrul Destroyers, which only Destroyers hurt much,
did most of the damage.

At Lyrae 7 it is the same: the Lisonian wave, beaten in space at New
Earth, takes the fifth of its ships that survive and all its troops home
(its ground units never land), so Lyrae 7 had 81 ships and 100Tr 55T
15Air, not the measured 73 and 70Tr 40T 10Air (`lyrae7-back` measures
that). Its space battle's loss table, read off the result picture,
showed "lost 50–58H 29–33SF 2D 1C, destroyed 4H 4SF" for a won battle
in which the grid lost 21H 5SF: `SpWar_ApplySpaceLosses` adds the losses
of the ships stored on the planet and its moons to the side its pointer
was left at by the last race of the force loop with status 2 or 6
(allied Undorlings, race 8: the player's side), as the original does
(the code's "BUG?"). The policy reads the grids for the forces. Each home
world taken becomes a colony of the player (`Intel_ClaimBodyForPlayer`).

The runs use Kyle Douglas at hire (experience 20). By act 5 his factor has
grown to about 1.23 (MACH 1.32), which helps the player's space damage only.

**Ground reserve at New Earth.** A space battle lost at New Earth disbands
the Army groups there into the planet force and loses trade and carrier
groups (intel.c:181-196); the ground battle that follows decides the game.
Against waves 1–3 tanks suffice (wave 1 needs the space win, see above);
against anything with aircraft, bring aircraft or launchers.

## 7. Driver API

`tests/play/ui_battle.h`:

| Function | What it does |
|---|---|
| `ui_invade(system, planet, group)` | Star map: GALACTIC MAP if needed, the system in the list, the planet (zoom), the group entry, GROUND WAR; waits for screen 29 |
| `ui_space_battle(&losses)` | Waits for END BATTLE, reads the result, clicks END BATTLE; returns won. Never retreats |
| `ui_assign_units(order)` | Reorders the slots rear to front by type (nullptr keeps the pre-fill), OK |
| `ui_ground_battle(tactic, &losses)` | Fights with HOLD / COUNTER / CHARGE until END BATTLE, clicks it; returns won. Never retreats |

Observers and the on-screen element each one reads:

| Observer | Memory | On screen |
|---|---|---|
| `ui_toolbar_has_command(cmd)` (ui.h) | toolbar slots DS:7C51/7C64 | the toolbar buttons |
| `ui_battle_over()` | END BATTLE in the toolbar, DS:77DA / 770B | END BATTLE and the result picture |
| `ui_battle_won()` | `SpWar_PlayerWon` / `GrWar_PlayerWon` | `spvict`/`splost`, `grvict`/`grlost` picture |
| `ui_battle_losses()` | DS:77B6.., 77C6.. / 76EA.., 76FA.. | "Your loss" / "Enemy loss" table on the result picture |
| `ui_space_ships(own)` | live ship counts | ship dots in the battle view (own fly on the left) |
| `ui_ground_stacks(own, out)` | GrWar unit lists (type, tile, step, count) | the unit sprites (type and position); the count is "Pcs" in the left panel when the stack is selected. The tactic decides on type and position only |
| `ui_assign_slots(out)`, `ui_assign_pool(type)` | unit list DS:7646, pool DS:7676 | the slot boxes (icon, count) and the pool line under them |

The fixture logs the fleets' starting sizes (DS:77B2/77B4) for information
only.

The message boxes on the way (an attack's box comes before screen 29) are
closed by `ui_wait_screen` and the other shared waits (tests/play/ui.h).

## 8. Measuring

`tests/test_play_battle.c` puts the game into the situation the game's own
code creates, with `play_test_setup` (so these runs are not proofs):

- **Defence**: the attacking race at war, its force at New Earth with one
  hour left on `Timer` (the state after the ATTENTION box), an Army group
  "Home Guard" docked at New Earth with the space units, New Earth's planet
  force with the ground units. The attack, its message box and the battle
  chain then run as in the game.
- **Invasion**: Hyperspace drive researched (system list), the system
  explored, the race at war, the target's intel at 40, the waves the race has
  already sent removed from its forces, an Army group "Strike" in orbit with
  space and ground units. The fixture then clicks GROUND WAR on the star map.
- `*-ground` scenarios take the other side's ships away (defence: no Home
  Guard; invasion: no enemy ships) to measure the ground battle alone.
- `lyrae7-back`: Lyrae 7 with the Lisonian wave (force 1) home again, a
  fifth of its ships and all its troops (beaten in space at New Earth).
- `kit=noplasma`: Destroyers without the Plasma gun, as before Rigel 1
  (act 5): the "without Plasma" figures of §6.

Run `build/e/test_play_battle DATA` for the checks (wave 1 won in space,
wave 1 beaten on the ground by 10 tanks, Mirach 3 invaded with 90 tanks,
seed 12345), or
`build/e/test_play_battle DATA NAME [hold|counter|charge] [FIRST LAST]
[own=H,SF,D,C/Tr,T,Air,ML] [order=mrta|keep] [kit=noplasma]` for a scenario over seeds
(order letters: r trooper, t tank, a aircraft, m launcher, rear to front).

## 9. Port bug found

`Sound_AllocWarSndBuf` (src/game/sound.c) waited for the sound card voice to
finish with `SDL_Delay(1)` and no retrace. Scripted runs mix audio from the
frame clock only (timing.c), so after OK, ATTACK (whose click sound is still
playing when screen 32 opens) the game hung for ever in
`GrWar_EnterGroundBattle`. The original's spin ends when the card finishes the
sample. Fixed with `timing_idle_poll()`, the port's way for busy waits.
