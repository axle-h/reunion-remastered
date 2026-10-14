# Full playthrough fixture

Goal: show that the port can be won. A fixture plays a new game to the end
with a policy taken from [walkthrough.md](walkthrough.md) and ends on the
Earth ground-battle win (`Halt(10)`, VICTORY). The game's mechanics are in
the walkthrough; this page is about the fixture, the policy it plays, and
what its runs measured.

Unit abbreviations: H Hunter, SF Starfighter, D Destroyer, C Cruiser, Tr
Trooper, T Battle tank, Air Aircraft, ML Missile launcher. Ores: D Detoxin,
E Energon, K Kremir, L Lepitium, R Raenium, T Texon. "MSG n", "KIT n" and
"PIR n" are line n of `TEXT\MESSAGE.TXT`, `KITALAL.TXT` and `PIRATE.TXT`.

## Rules

- **Input only.** The fixture acts only through simulated mouse moves, clicks
  and key presses (`input_set_mouse`, `input_push_key`). It never writes game
  state; a write barrier (the game's state, `game/state.h`, compared around
  every controller call) enforces this.
- **Player-visible information only** for decisions: the screen shown, its
  hotspots and captions, messages, date, credits, planet and group info the
  game displays. Hidden state (story timers, enemy forces not shown, RNG) may
  be read for logs and assertions, never to decide.
- **No date luck.** One fixed seed for now (`PLAY_SEED`), but every wait is
  on a visible condition with a timeout, never on a frame or date that only
  holds for this seed.
- **Saves.** Not required before battles. Checkpoints for iteration are
  saved and loaded through an API rather than the disk screen. The proof run
  is one continuous game from "new game".
- **No cheats.**

## Proof

The fixture records every input it makes as a plain autopilot script (frame
number + event). Replaying that script with the same seed in the normal
`reunion` binary, without fixture code (`reunion --replay`, `src/main.c`),
must also end in exit code 10.

### Result: seed 12345

`tests/test_play_game.c` plays one game from "new game" with seed 12345
through the 76 steps of acts 1-5 and wins: Halt(10) after 3,504,170
retraces, on 2930-03-06 09:00 (game date), state hash
`b3fe94192bcf1188`. The recording, 3.2 MB of plain autopilot script, is
[tests/play/proof_12345.txt](../tests/play/proof_12345.txt); the fixture
replays it in-process and `reunion --replay` ends the same way, with exit
status 10:

```sh
build/debug/reunion --replay=tests/play/proof_12345.txt vendor/Reunion_DOS_EN
# replay: exit 10 after 3504170 frames, hash b3fe94192bcf1188   (exit status 10)
```

Milestones: Jade found 2927-08-25, Jaanosian contact 2928-03-02, the
Galleon invented 2928-07-26, Mirach 3 taken (the Morgruls eliminated)
2929-04-05, the League's surrender 2929-09-27, Earth 2930-03-06. The Earth
invasion: in space lost 47H 36SF, destroyed 64H 64SF 7D 5C; on the ground
lost 10T 20Air 30ML, destroyed 30Tr 90T 50Air 30ML.

The "as run" dates and losses in the policy below are from earlier
recordings of the same seed, made before the fixes to the original's bugs
(`src/game/fixes.h`) and a few policy changes moved the run by days to
weeks; they show what happened and in what order, not the proof's dates.

`test_play_game` takes about 2 minutes in a release build (ctest `TIMEOUT`
900 s; the play itself ~1.3 minutes, then the two replays) and about 5 in
a debug one, so ctest runs it only in non-Debug builds
(`ctest --test-dir build/release -R test_play_game`).

## Robustness

The same policy, unchanged per seed, from "new game" with `PLAY_SEED`
(release build, one recording each):

| Seed | Won | Seed | Won | Seed | Won | Seed | Won |
|---|---|---|---|---|---|---|---|
| 1 | 2930-03-16 | 16 | 2930-03-02 | 31 | 2930-03-23 | 46 | 2930-03-03 |
| 2 | 2930-03-09 | 17 | 2930-02-21 | 32 | 2930-02-25 | 47 | 2930-02-28 |
| 3 | 2930-02-18 | 18 | 2930-03-05 | 33 | 2930-02-27 | 48 | 2930-02-15 |
| 4 | 2930-03-10 | 19 | 2930-03-02 | 34 | 2930-03-02 | 49 | 2930-02-15 |
| 5 | 2930-02-16 | 20 | 2930-02-14 | 35 | 2930-02-26 | 50 | 2930-02-30 |
| 6 | 2930-03-02 | 21 | 2930-03-02 | 36 | 2930-03-04 | 51 | 2930-02-29 |
| 7 | 2930-03-19 | 22 | 2930-02-28 | 37 | 2930-02-22 | 52 | 2930-03-07 |
| 8 | 2930-03-23 | 23 | 2930-03-19 | 38 | 2930-03-09 | 53 | 2930-03-07 |
| 9 | 2930-02-09 | 24 | 2930-02-13 | 39 | 2930-03-15 | 54 | 2930-03-06 |
| 10 | 2930-02-21 | 25 | 2930-03-13 | 40 | 2930-02-23 | 55 | 2930-03-18 |
| 11 | 2930-03-10 | 26 | 2930-03-23 | 41 | 2930-03-01 | 56 | 2930-03-07 |
| 12 | 2930-02-23 | 27 | 2930-02-15 | 42 | 2930-02-27 | 57 | 2930-02-26 |
| 13 | 2930-03-11 | 28 | 2930-02-23 | 43 | 2930-02-30 | 58 | 2930-02-24 |
| 14 | 2930-03-12 | 29 | 2930-03-02 | 44 | 2930-03-01 | 59 | 2930-02-17 |
| 15 | 2930-02-29 | 30 | 2930-02-16 | 45 | 2930-02-18 | 60 | 2930-03-02 |

All 60 were run on 2026-10-12 with every fix on; each recording replays
to the same end. The wins fall between 2930-02-09 and 03-23; E21 finished
between 2928-05-23 and 07-01. Seeds 61-240 (2026-10-13, not listed) win
too, between 2930-02-14 and 04-07, with E21 finished between 2928-03-28
and 07-08.

Each batch of new seeds so far turned up a rare failure, fixed in the
policy or its drivers: a Mine built twice at midnight that left E21
waiting for a 10th droid (seed 3's old loss at M16), clicks on bodies
that had moved on or hidden behind the sun ("Moving bodies"), a message
read through the mouse cursor, a battle that came up during a step's own
navigation, the Phoenix 1 search losing whole evenings to the haul (M5),
wave 1 lost with 12 Hunters (J8), an alien talk that came up while a
driver read the surface (the game frees its map layers on leaving it;
seed 113), wave 2 attacking during M8's last look at the Home Guard (seed
79), and the Psy radar taking 65 days to find Rigel 2 (seed 207, X4).

## The CD's languages

The same policy plays the CD's German, French and Italian games
(`PLAY_LANGUAGE`, `tests/play/run.h`): the data is an import of the CD,
whose data/ is the English game, or the ISO itself, which the run imports
first. The policy is written against the English texts, so the readers
turn what the game shows into English (`tests/play/lang.h`): a dialogue
or message is the same line of the same file in every language, and a box
read off the screen is matched to its line with a tolerance for misread
cells; the game's own texts (buttons, captions, panels, the messages it
builds) go back through the port's tables of them (`src/game/lang.h`),
whole or piece by piece. Where one translation stands for two English
texts the reading can only be one of them: the drivers compare a reading
with what each translation of the text they want reads as
(`lang_reads_as`), toolbar labels go by the button labels alone, and a
race's name in a message reads as the race. A few checks accept each
game's wording ("Kall 2" for "Kall Fleet 2", a location line without
"system"). The screen reader knows the CD's accented glyphs. Seed 12345,
release build, 2026-10-13:

| Game | Won | Frames | Hash |
|---|---|---|---|
| English (CD import) | 2930-03-06 09:00 | 3,504,170 | `b3fe94192bcf1188`, as on the floppy |
| German | 2930-03-17 03:00 | 3,542,909 | `54595921dacbc0d4` |
| French | 2930-03-03 02:00 | 3,541,306 | `dd17783ba3237a64` |
| Italian | 2930-03-18 22:00 | 3,597,274 | `02c26e6c8446b948` |

The other languages take other paths (their texts and speech take other
times to show), and each recording replays to the same end in-process
and with `reunion --replay --language=...`. ctest runs the three as
`test_play_game_german`, `_french` and `_italian` where `test_play_game`
runs and vendor/ holds the CD's ISO.

## The fixture

Each module's header says how it works; in short:

| Module (`tests/play/`) | What |
|---|---|
| `play.h`, `play.c` | Core: boots the game in-process like a scripted run of `src/main.c` (fixes on, no intro), runs the policy on its own thread with a strict hand-off at every retrace, records the input, enforces the write barrier, checkpoints (`play_checkpoint`, `play_resume`) and replays |
| `run.h`, `run.c` | Runner: a test is a list of steps with a checkpoint before each; the `PLAY_*` variables (`PLAY_RESUME=step`, `PLAY_STOP=step`, `PLAY_SEED`, ...); replays the recording in-process and with `reunion --replay` and checks all three ends match |
| `ui.h`, `ui.c` | Acting: waits, clicks, toolbar, hotspots by caption, navigation, hiring, new game. Every wait services the interrupts (message boxes, rename prompts, story scenes, holograms) |
| `ui_message.h` | Reading the screen (6x8 font text reader, status bar) and the interrupts |
| `lang.h`, `lang.c` | The CD's other languages: what the game shows, in English |
| `ui_office.h`, `ui_planet.h`, `ui_ships.h`, `ui_talk.h`, `ui_battle.h` | Drivers and observers per group of screens: research, shop, log; planet info, surface, droids, colonisation, colony list; ship info, groups, cargo, control panel, destination map; alien talks, pub, advisor; battles |
| `policy.h`, `policy.c` | The policy's shared part: money and ore gates with claims by priority, background goals, standing orders, the dispatch of talks and battles (`pol_try`, `pol_service`) |
| `act1.c` ... `act5.c` | The steps of each act; `tests/test_play_game.c` lists them (one checkpoint per step, e.g. `PLAY_RESUME=E15_station_on_apollo`) |
| `army.h`, `army.c` | The army goal of acts 3 to 5 |

A step rereads what it needs from the screen, so it can run from its
checkpoint knowing only its name; the goals of the steps before it are
registered again. A resumed run therefore goes on differently from the
continuous one (its goals start afresh), and both must pass. Replaying a
recording's prefix is fast enough (about 200,000 retraces/s) that resumes
need no in-memory snapshots, which would also have to carry the audio,
AdLib and port state.

**Speed** (release build, one core). The clock runs 101 retraces per game
hour (`main_program.c`), so two game years are about 1.7 million retraces.
A policy that checks a condition every retrace runs about 22,000
retraces/s (the write barrier is most of it), a sleeping policy or a replay
about 160,000. Unpaced autopilot runs (the fixture, `--replay`, soak runs)
advance the audio without synthesising it (`audio_advance`) and present at
most 60 frames per wall-clock second.

**Game data block.** Groups and Buildings share one 8770h-byte heap block
(`Game_InitGameData`); observers read them through `ui_group_record` and
`ui_building_record`.

## The policy

The policy is a list of steps per act (E act 1, J act 2, M act 3, L act 4,
X act 5), standing orders (S) that run between steps, and the handling of
whatever comes up unasked. A step may start once its precondition holds;
steps that wait for money or a production line run as background goals
while later steps go on. The code is in `act1.c` ... `act5.c`, `army.c`
and `policy.c`; this section keeps the reasons and what the runs showed.
Steps marked *hard* have a story deadline behind them.

### Conventions

- **Waiting.** The clock runs on the idle screen, 11 MESSAGES, which also
  shows group arrivals ("... arrived to ...", logged, no box). Not on
  the command centre: a research that finishes there shows a hologram
  that ends on a press and leaks that press into the room's hotspots.
  The status bar shows no credits during the first game hour.
- **Money.** Every purchase is gated on credits ≥ cost + reserve. The
  reserve is 20,000 until the Jaanosian contact and 50,000 from then on;
  it covers the dialogue payments, which the game takes without a balance
  check (talk 3, the Spy, bartender, informer, Treasonable, Bounty-Hunter),
  and negative credits block shop, hire and build. A goal waiting for
  money claims it: lower goals spend only what credits − reserve − the
  claims above them leave. The money order is `PolPriority` in
  `policy.h`. A short purchase buys what it can now and the rest daily.
- **The shop** takes ore only from New Earth's stock and silently cuts the
  quantity to what credits and ore allow, so the driver checks "Bought
  items:" after OK,BUY. Each product builds one unit at a time at its build
  time per unit, all products in parallel, and nothing builds while the
  quantity editor is open. A big order of one unit type is limited by build
  hours, not only money: 90 Battle tanks are 4,500 h of tank line.
- **Travel.** The one hired pilot flies every group; each group in flight
  covers pilot experience + 1 legs per hour. A trip is 80–99 legs to
  another body around the same planet, 150–199 to another planet, 200–249
  + 1 h jump + 200–249 to another system. Without a pilot only carriers
  move, at 1 leg/h; with Jack at hire 4–10 h.
- **Not shown anywhere:** a colony's food, hospital and housing capacity,
  power totals and intel number. The policy uses visible building counts,
  population, the mood word, a building's "Status : Active/Passive" and
  the colony warning boxes.
- **Interrupts.** Talks, battles and story scenes come up at any hour, also
  in the middle of a driver sequence; the sequence is left there
  (`pol_try`), the screen handled (`pol_service`) and the sequence run
  again later from what the screens show. A story scene ends on the
  command centre. An attack set in the same hour as a talk or scene
  replaces it, and the talk is lost; before meeting 3 no hostile forces
  exist, so the Jaanosian chain is safe. Never press keys during scenes:
  any key sets `Gfx_KeyAbort` for good and later scenes skip.
- **Moving bodies.** The planets and moons move on to their next orbit
  position at each game hour, and the map's hotspots with them, so a click
  aimed just before the hour can open another body. Every open is checked
  on screen: planet info by its NAME line, a surface by the clock showing
  the same hour before and after the click (the surface names no body),
  a launch by the group's location line ("Fly to ... Mirach 3"). A planet
  behind the sun is not drawn and a click on it does nothing; it comes out
  within hours, so the click is tried hourly.

### Act 1: Economy (new game to Jade, E1–E23)

Start: 120,000 credits, New Earth 30,000 people, tax Normal, 2 droids
working and 1 in stock, no staff; New Earth's ore stock starts at 0.

- **E1, E2. Builder and developer first** (William Forbs, Thomas Dempsey).
  Research is the critical path, and Jaanosian meeting 1 only counts down
  while a developer is present.
- **E3. Tax New Earth Very difficult.** Its target attitude there is 30
  (10 for the tax, +30 for Leisure centre, Church and Park, −10 above
  30,000 people), right at the line below which the population ceiling
  halves. Observed: "people like you" → "sympathetic" on day 3 →
  "neutral men" by days 6–20, and there it stays from 30,000 to 50,000
  people and, with the Stadium, to 76,000. The one fall seen was the
  Stadium missing past 50,000 (−20): "people hate you" in two days. So the
  tax stays Very difficult, S3 computes it from what is built, and the
  Stadium (E9) comes before 50,000.
- **E4–E8. Satellite, Miner droid, a third mine, Derricks, satellites.**
  New Earth starts with one Derrick (~2.45 t/h of Detoxin), enough for the
  first two satellites (D74 in ~30 h). The satellite on Apollo survives;
  the one on Klatoo is destroyed by design, and MSG 1 follows in 5–14 h.
  The fourth Derrick came 30–65 days in, yet E15's Detoxin was in stock
  before E15's money every time.
- **E9. Vehicle plant, Stadium** before 50,000 people (no Stadium there is
  −20 attitude, 40–50% of the income). From 40,000 people on E9 goes
  before S2 in the money order, and S2 builds no housing past 45,000 until
  the Stadium works. Observed: 40,000 people 2927-09-19, Stadium 10-13,
  50,000 in late October.
- **E10, E11, E13, E17. Satellite carrier, Miner station, Transfer ship,
  Control Centre**, each as its idea (KIT) appears. Control Centre needs
  Physics 4 and stalls at "Completed: 50%" without it, hence E12.
- **E12, E14. Courses.** A course costs (Random(10) + experience) × 1,000,
  quoted before paying (Dempsey 29,000–37,000 before his first, 70,000 and
  more after). The policy pays when the money left after higher claims
  covers the quote, else declines and asks the next day; a skill under the
  research screen, or a quote of 60,000 or more, says the course is done.
  Research and idea timers stop while the developer is away. Jack Benson
  (pilot) is hired before the carriers fly: travel speed is ×(exp + 1).
- **E15. Miner station on Apollo** (*hard*: it unlocks Control Centre,
  which gates colonies). The station is equipment in a Trade group's "Mine"
  column, not cargo; Apollo must be unowned, without a colony and at info
  level > 9. Observed: ores in stock, money the gate; station and ship in
  60 h, MSG 4 on arrival.
- **E16. Carriers survey Amnesty.** Zeus's and Amnesty 2's moons are listed
  only after a group arrived at the planet, so fly there before Penelope
  and Mir. Carriers reveal status −2 planets (1/5 per transit day), never
  Jade. Observed: the observatory and Trade 1 found Amnesty 1–3 within the
  first month, so no shuttling was needed.
- **E18–E20. Colonies Apollo, Penelope, Mir**, each colony's core (S2's
  needs, S4 power, Vehicle plant, Spaceport) before the next founding;
  Park, Church, Leisure centre and Mines (their droids need a Spaceport
  and a trade group, S7) from act 2. Penelope comes before Mir: it is the
  only early Texon source, and Texon is the first ore short in act 3. Mir
  needs a Medicine plant and a second Hospital before colony day 10. As
  run: Apollo 2927-11-30, Penelope 2928-02-12, Mir 03-28; on 08-01 Apollo
  had 68,000 people, Penelope 52,000, Mir 40,000.
- **E21. New Earth to 9 Mines and 20 Derricks**, lowest priority. A body
  takes at most 9 droids, so a 10th Mine gets none; E21, the colonies'
  droids (S6) and their loading count Mines up to 9. A 10th Mine once came
  from a build counted as failed because midnight's taxes outweighed its
  price, and E21 then waited for its droid for good: no Derrick after 5,
  and M16 (Space station) never got its Detoxin. The build driver counts
  the buildings instead.
- **E22. Jade** (*hard*). Only New Earth's observatory finds it (status
  −3); keep it standing. The port's fix reveals Jade on 2928-04-01 at the
  latest (`fixes.find_jade`), so a timeout at 2928-04-02 is a port bug.
  Observed with seed 12345: 2927-09-08, 09-16, 09-25, 10-13, 12-18 and
  2928-01-21 in runs whose input differed. The observatory's roll uses
  the game's one random generator, which the policy's own actions advance,
  so the date is no fixed property of the seed.
- **E23. Stand-by.** Trade 1 waits in orbit at New Earth for Jade, hauling
  meanwhile.

### Act 2: Jaanosians (Jade contact to the Galleon, J1–J13)

- **J1. Contact.** Everything after the contact is a fixed offset from it
  (`Intel_FirstContact`: meeting 1 650–719 developer hours later, meeting 2
  a further 720–769 h, the S.O.S. 2,590–2,807 h after the contact). Before
  it only the pre-contact deadline runs (6,000–6,399 h from the start, so
  2928-04-23 at the earliest). A contact on the day Jade is found would
  bring wave 1 with no colony mining yet, so the policy launches once the
  status bar shows 2928-03-01, whatever day Jade came.
- **J2. Communicator** (*hard*) before meeting 1; without it each meeting
  becomes MSG 6 and is lost for good, and meeting 3 is the only source of
  Hunter and Laser cannon. As run: ready 77 h after the contact.
- **J3.** No university for Dempsey until meeting 1 (it counts down only
  while he is present).
- **J4–J6. Meetings 1–3** (talks 2, 3, 4; the paths are in `ui_talk.c`).
  Talk 3 costs 16,000 unchecked. As run: meeting 1 at contact + 674 h,
  meeting 2 + 1,402 h, the S.O.S. + 2,718 h. From 700 h after meeting 2 the
  policy claims J7's and J8's 150,600 credits; income ran far ahead (616,000
  at the S.O.S.).
- **J7. Kyle Douglas** (*hard*): a space attack needs a war commander, and
  damage scales with his experience. Hired 18 h after the S.O.S., course
  2–3 days later.
- **J8. First army** (*hard*): 14 Hunters with 2 lasers each in "Home
  Guard", docked at New Earth (a docked Army group defends). 12H won wave 1
  199/200 and 14H 200/200 ("Battles"), and a lost space battle lands the
  troopers on a New Earth with no tanks: game over. As run with 12:
  complete 323 h after the S.O.S. (240 h of Hunter line).
- **J9, J10. The base falls; Radio.** Docking at Jade after the fall gives
  MSG 13 (the radio). The arrival is also the first contact with the
  Morgruls, who hold Jade from then on; their force there had no mission
  and left Trade 1 alone. A late Radio only delays the Phoenix S.O.S.,
  which waits for it.
- **J11. Russel** (Jade's moon; E90 R80 T40): the base's fall clears the
  Jaanosians' moons, so it can be colonised. Its satellite flies on a
  carrier group of its own ("Survey 2"): Survey 1's satellite count is
  E16's progress. Founded 28 days after the fall.
- **J12, J13. Hyperspace drive, Galleon.** Dempsey takes Phisics.& electr.
  right after the fall (before it, story stage 1 caps his skills). MSG 30
  came at fall + 360 h, the Galleon was docked at fall + 741 h
  (2928-08-05).

Wave 1 can come before J13: as run the ATTENTION box came at fall + 666 h,
the attack 20 h later; S8 sent the trade and carrier groups away, and Home
Guard's 12 Hunters beat the 6 Hunters losing none (0–2 in other runs).

### Act 3: Morgruls (the Galleon to Mirach 3, M1–M18)

One goal, `M8_army` (`army.c`), runs the army from M2 to M18: it keeps
every production line the target force needs busy and fills Home Guard,
docked at New Earth, from the stores; the target grows with the step (M8,
M12, M18). A pass of all the goals takes about 3 game days of clicks by
act 3 (every screen switch runs the clock), so nothing may rely on a goal
reacting within a day. M6 and M7 run before M5: the Battle tank they
unlock is the act's gate.

- **M2. Twin lasers and Troopers.** Twin laser gun takes 3 h of research;
  the army goal fits the twins as the line makes them (waiting for the
  refit held M3 back 24 days in one run). Troopers are never bought.
- **M3, M4. Phoenix and the Kalls.** Arriving at a sun puts the group at
  the last planet found, Phoenix 3, so the explore box, the Kall contact,
  MSG 15 and talk 5 came together; 10 Starfighters come free.
- **M5. Phoenix 1** (status −2) is found by a group in flight in Phoenix at
  midnight; each hidden planet rolls 1 in 5 a day and the first that hits
  is found (Phoenix 4 is the other). Launched only when found in orbit, the
  Galleon was in flight at one midnight in three or four and found nothing
  in 30 days in one run. From 18:00 it is now kept in flight until 0:00,
  and an evening flight starts no earlier than 21:00: a 3-5 h flight
  started on landing came down again before midnight. The shuttle runs
  between goals, and also while Trade 1 flies (a haul's round trip took
  16 h and skipped whole evenings). The search goes on while M8-M10 run;
  M11 waits for the find before it needs the Galleon. Over 60 seeds
  Phoenix 1 came 1 to 47 days after M5.
- **M6, M7. Phelonians, Battle tank.** Phoenix 5 is status −3 too: only
  the Informer finds it (−500), and his Phelonian item is replaced once
  Antares is explored, so M6 must precede L5. The Phelonian deal halves
  New Earth's Energon (107,000 t to 41,000 t as run, which cost nothing
  that mattered). In talk 7 the "pleasure doing business" row is also
  offered right after "What will you give in return?", where it sells
  Energon instead: click the rows in order.
- **M8. Wave 2 force: 24H (2L+1T), 16SF (4T), 20T (3L)** in Home Guard.
  The ground reserve can't sit in the shop's stores, which no battle
  counts: the tanks ride in Home Guard's ground row (a docked Army group
  defends, and after a lost space battle its units join the planet force).
  Money is no gate (~850,000 credits at the act's start); the Twin laser
  line is: about 81 twins by the earliest wave 2 against 108 wanted, so
  twins go to the Hunters first, then the Starfighters, and the M8 tanks
  get theirs for M12.
- **M9, M12. Waves 2 and 3.** As run: wave 2 40 ships against 25, lost
  2H 2SF; wave 3 30H 24SF against 30, lost 3H 2SF.
- **M10. Dempsey's courses.** His caps are 6/7/7/5 at story stage 2 and
  7/7/7/7 at stage 3. Courses add: Maths & physics. +2/+1/0/0, Phisics.&
  electr. 0/+2/+1/0, Electr and maths. +1/0/+2/0, Artificial int.
  0/0/0/+2. So Artificial int. (A4: Spy satellite, Missile) and Electr and
  maths. (M6 E7: Destroyer) go at stage 2, as soon as Twin laser gun and
  Starfighter are done, and only Maths & physics. (M7: Space station) waits
  for stage 3 (wave 2 beaten).
- **M11. The drifting ship.** After MSG 17 the policy waits until the
  Phoenix 5 zoom lists "Kall Fleet 2" (allied forces are listed), then
  flies to Phoenix 1. The original fires the scene when any group arrives
  anywhere while Phoenix 1 is selected; the port needs the arrival there
  (`fixes.drifting_ship`). "All right." in talk 6 delays wave 3 by 250 h;
  afterwards the Kalls' fleet 2 (15H 10SF, 10Tr 35T) sits at Mirach 3 as
  an ally and joins the invasion there. The planets move each hour, so a
  zoom click can land on Phoenix 3, which lists the fleet before it
  leaves; talk 6 then came too early, the Kalls turned hostile and
  attacked New Earth in act 4. The check now also needs the zoom to name
  Phoenix 5.
- **M13. Spy.** The Spy's answer gives Mirach's coordinates; Mirach appears
  in the system list with no box.
- **M15. Explorer 1** on Mirach 2's first moon (the destination map names
  moons "Moon 1", ...). Arriving at Mirach's sun put the Galleon at Mirach
  3, the Morgrul home; nothing attacked a trade group there.
- **M16. Space station, Destroyers.** One station is enough: each
  product's queue is capped at the stations owned, but units build one at
  a time anyway, so the army goal queues the next Destroyer when one is
  done. Detoxin was no gate with 20 Derricks (a run with 5 failed here;
  see "Robustness").
- **M17.** The spy satellite on Mirach 3 gave "alien activity" after 6
  days and the colony box after 8.
- **M18. Invade Mirach 3** with Strike (Home Guard renamed: waves 1–3 are
  spent and wave 4 is 8,000–8,499 h after wave 3, so New Earth keeps no
  army meanwhile). No code timer starts with the invasion; the real
  deadline is wave 4. The tank line set the date (90 tanks by March), the
  Twin line came last (302 twins). The ship target went up to 40H 40SF 6D
  because the waves' survivors had gone home to Mirach 3 ("Battles").

### Act 4: The League (Mirach 3 to the League's surrender, L1–L13)

The army goal fills two Army groups docked at New Earth, Strike and Home
Guard, tier by tier: a tier's units only once every group holds the tier
before with its weapons (more units would take weapons the tier before
still lacks). Units a group holds beyond its target go back to the stores
for the other. Aircraft carry 4 Twins each, so the Twin line is queued 10
deep and gates the act. Each home world taken becomes a colony of the
player with its own planet force; the ship-info screen then shows the
planet-forces list, where NEW UNIT is missing, so the drivers switch back
first. The box reader skips pixels under the mouse cursor, so a partly
hidden glyph can read as another ("Superhova"); act 4's waits allow one
character in eight to differ (`pol_seen_like`).

- **L1, L2. Julie-Anne Roxford, Klaatoo**, replacing Jack and Dempsey.
  Julie at experience 90 reaches a moon of the same planet within the
  hour. Klaatoo starts at 5/7/6/5, below Aircraft's M6, so his courses
  come first: Maths & physics., Electr and maths., Artificial int. twice
  (8/8/8/9 by 2929-04-23). Phisics.& electr. isn't needed.
- **L3. Pirate Leader** (optional): the Informer's answer also unmasks the
  bartender's League tip. No pirate deals were taken.
- **L4. League tip** (*hard*, before the Lisonian wave): a carrier
  explores Orionis (carriers make no contact), then the bartender's tip
  (−5,000).
- **L5, L6. Antares and the Erans.** The colony icon needs intel 30, so a
  satellite first; the nova warning's 50 h count runs only while a
  finished Observatory stands there. After talk 9 every group leaves
  Antares at once: the supernova comes 560–569 h later and deletes
  everything there. As run: 563 h.
- **L7. Home guard before the League contact.** Winning in space is the
  only defence against the Lisonian wave: a 40-tank reserve loses the
  landing 0/10 to its 5 aircraft, and aircraft only come with L11. Strike
  (40H 40SF 6D, then 50H 50SF 8D) stays docked at New Earth until the wave
  is beaten.
- **L8. Spy satellites** on Orionis 3, Lyrae 5 and 7 stop at "alien
  activity" until the contact. Lyrae 3 (Undorlings) is allied after L10
  and never invaded.
- **L9, L10. Contact, Eran's job.** One trip meets all four races; MSG 28
  comes with the first. Asking the Eran for the Undorlings gives MSG 40 and
  44 136 h later (Undorlings allied, Ion cannon, Aircraft).
- **L11, L13. Aircraft and the invasions.** Orionis 3 and Lyrae 5 with 40H
  40SF 6D, 30T 20Air; Lyrae 7 with 50H 50SF 8D, 30T 30Air, as the
  Lisonian wave's troops had gone home there ("Battles"). Home Guard kept
  40T 10Air at New Earth meanwhile. The Twin line set the dates; Lyrae 7
  fell on 2929-10-01 in that run.

### Act 5: Earth (X1–X9)

The army goal fills Home Guard first (40T 10Air 10ML), then Strike (60H
60SF 12D 10C, 40T 20Air 40ML). The Missile line is the act's gate (~340
missiles of 10 h: 250 for the launchers, 60 for the Cruisers), queued 16
deep from the act's start; the Cruiser line (200 h each) ended a month
before it.

- **X1. Rigel 1.** A system's explored flag is only ever set, so no group
  needs to stay in Rigel.
- **X3. 10 Cruisers and 50 launchers.** Earth's fleet beats 6 and 8
  Cruisers 0/10 and loses to 10 (20/20): no Cruiser fallback. 40 launchers
  are the least that wins the ground every time, and each costs the gate
  line 5 missiles. One Space station was enough.
- **X4. Rigel 2** is found by the Psy-radar (1/10 a day once Mental radar
  is done); as run 1.4–16 days. Talk 10 has one option per state.
- **X5.** A carrier explores the Sun (no contact) and puts a spy
  satellite on Earth.
- **X6. Home guard for Earth wave 1: no warships**, ground 40T 10Air 10ML.
  A home fleet of 20SF 4D 2C or 50SF 10D 4C loses 0/10, so every warship
  goes to Strike; 5 launchers turn the ground battle (40T 10Air 0/20, 40T
  10Air 5ML 20/20). The wave only comes if Earth stands 1,000 h after the
  contact.
- **X7–X9. Contact and invasion.** Strike's arrival brings MSG 38 and sets
  the Syonians hostile (nothing of theirs came). The Bounty-Hunter (X8) is
  not needed. GROUND WAR as soon as the intel box comes.

### Standing orders

They run between steps, at least once a game day and after every box
(`policy.c`). What the runs taught:

- **S1. Map on Amnesty.** Only matters with `fixes.mine_all_systems` off;
  the policy keeps it.
- **S2. Colony growth.** Housing and hospitals are kept ahead of the
  population: on New Earth while 10,000 + 7,000 × Buildings < 1.6 × pop (up
  to 12 Buildings, a ceiling of ~95,000 that keeps it under the −20 at
  100,000) and 20,000 + 20,000 × Hospitals < 1.6 × pop (up to 5). Each new
  Building also adds 800–1,300 people at once. Shortages (within 3,000
  people of a capacity, or a box) come before the lead and before the
  Stadium: once the lead stopped at 45,000 for the Stadium, food ran short
  and New Earth starved for two weeks. Observed: 30,200 people at the
  start, 44,000 on 2927-10-01, 68,000 on 2928-01-01, 78,000 on 02-28.
- **S3. Tax** is computed daily from what is built, as the colony update
  does: the highest level (at most Very difficult) whose target attitude
  is 30 or more. The mood word catches the penalties only boxes show. The
  earlier rules, which lowered the tax on a bad mood word, sent New Earth
  from Very difficult to None in two days at 50,000 people without a
  Stadium and kept it there for 12, with no income at all. One trap:
  unpowered leisure buildings count as missing, so the tax falls and the
  generator's money never comes; hence S4's place in the money order.
  Colonies: High from the founding, one step up with each of Park and
  Church, Very difficult within weeks.
- **S4. Power.** A generator before anything else when a finished building
  reads Passive, and ahead of the shortage when a Derrick's (or in a
  colony a Vehicle plant's or Spaceport's) "Energy" is below 60% of its
  nominal value. The colonies' Mines draw 1,100 kWh each, so a Desert
  colony with 9 Mines runs on 5–6 Windtraps; before S4 looked ahead,
  Apollo's Mines switched its leisure buildings off now and then and the
  tax fell two steps.
- **S6, S7. Hauling and droids.** One round trip a daily pass: unload at
  New Earth, read its stock and storage on the cargo screen, go to the
  colony that waits for droids or was visited longest ago, put back what
  New Earth had no room for, and load the ores New Earth has least of,
  each up to its room. Loading Raenium first regardless filled the ship
  with it and no Texon came. New Earth's storage (10,000 t per Mine, 5,000
  per Derrick: 190,000 t with 9 and 20) is the limit: every ore was near it
  by the end of July of the first year. Trade 1 grew to 3 Transfer ships
  and 4 Trade ships in act 2 (9,500 t a trip) and 10 Trade ships in act 3
  (21,500 t); with 4, New Earth's Energon fell to 7,000 t in one act-3 run.
  A droid takes 500 t; a colony builds its next Mine only when every
  finished one has its droid, so a colony gained about one droid a week.
  Each visit puts the colony's stocked droids to work (Penelope once kept
  one in stock for good).
- **S8. Wave evacuation.** On an ATTENTION box naming New Earth (or the
  Morgruls' first-arrival box, which names no body), every trade and
  carrier group at New Earth **or Apollo** leaves for Penelope: the battle
  matches system and planet only, so New Earth's moons count, and a lost
  space battle there loses those groups. Over when a space battle has been
  fought after the box, or 120 h after it. Wave 1's box came 10–31 h
  before the attack.
- **S9. Defence floor.** The army goal holds the step's target. After wave
  3 nothing is due for 8,000 h, so M18 takes everything to Mirach; in act 4
  Home Guard holds a ground reserve (40T 10Air, which beats the Lisonian
  landing on its own) and no warships while Strike is away.
- **S11. Research queue.** When the developer is idle: the step's product,
  then Communicator, Radio, Hyperspace drive, Galleon, Starfighter, Twin
  laser gun, Spy satellite, Missile, Destroyer, Space station, Aircraft,
  Plasma gun, Missile launcher, Mental radar, Trooper. Clicking a running
  project pauses it, and starting another stops it.

## Economy measured

Fully equipped unit cost (hull plus the full loadout, walkthrough "Weapons
and loadouts"):

| Unit | Credits | D | E | K | L | R | T | Build h (hull) |
|---|---|---|---|---|---|---|---|---|
| Hunter, 2 Laser + 1 Twin | 11,450 | 635 | 2,020 | 473 | 509 | 352 | 0 | 20 |
| Hunter, 2 Laser (before Twin) | 8,800 | 550 | 1,945 | 438 | 484 | 234 | 0 | 20 |
| Starfighter, 4 Twin + 1 Missile | 22,000 | 1,870 | 2,080 | 706 | 640 | 672 | 815 | 30 |
| Destroyer, 2 Twin + 4 Missile + 1 Plasma | 60,300 | 8,120 | 13,885 | 2,522 | 8,585 | 3,155 | 5,030 | 67 |
| Cruiser, 6 Missile + 3 Plasma | 373,000 | 11,110 | 19,080 | 13,205 | 7,970 | 3,477 | 11,560 | 200 |
| Trooper, 1 Laser | 6,050 | 1,050 | 1,005 | 76 | 350 | 208 | 236 | 35 |
| Battle tank, 3 Laser + 1 Twin | 13,300 | 1,535 | 1,395 | 149 | 165 | 1,132 | 150 | 50 |
| Aircraft, 4 Twin + 1 Missile | 30,600 | 1,080 | 2,805 | 1,655 | 650 | 752 | 500 | 35 |
| Missile launcher, 5 Missile | 41,000 | 830 | 760 | 645 | 298 | 490 | 1,500 | 50 |

Mining output per droid per hour is the planet's deposit as stored (planet
info shows it /10): New Earth E30 K20 L30 gives 3/2/3 t per droid-hour.
Detoxin comes only from Derricks, each 35% × deposit/10 per hour: about
2.45 t/h per Derrick on New Earth. A colony's income is pop × tax ×
development × (attitude + 40) / 200,000 a day: New Earth at the start
about 3,200.

The forces planned before the battles were measured (50H 50SF 10D 10C, 110
tanks, 20 Aircraft, 70 ML) cost about 11 M credits before losses. Build
hours gate as much as money: the tank line needs 5,500 h for 110 tanks, the
Cruiser line 2,000 h, the Missile and Twin laser lines ~4,200 h and ~6,000 h
over the game, so those lines must never sit idle. From act 2 on, money
never waited in the runs.

Measured in act 1 (seed 12345, an earlier recording; New Earth at
Very difficult throughout; credits as the daily S3 read saw them, the
income from spans without purchases; the policy spends down to R + claims,
so the credits say little):

| Date | New Earth pop | Mood | Credits | Income/day |
|---|---|---|---|---|
| 2927-08-14 | 30,198 | people like you | 36,620 | ~3,100 (Aug) |
| 2927-09-01 | 36,476 | neutral men | 21,862 | ~3,400 (Sep) |
| 2927-10-01 | 44,092 | neutral men | 27,622 | ~4,700 (Oct) |
| 2927-11-01 | 53,895 | sympathetic | 55,867 | ~6,300 (Nov) |
| 2927-12-01 | 61,939 | neutral men | 47,562 | ~6,600 (Dec) |
| 2928-01-01 | 68,301 | neutral men | 132,748 | ~6,800 (Jan) |
| 2928-02-01 | 73,806 | neutral men | 65,708 | ~9,400 (Feb) |
| 2928-02-28 | 77,991 | neutral men | 60,557 | |

The run was held in act 1 to 2928-03-01 for this (Jade came on 2927-09-08
there); the continuous run's act 1 ends at Jade. About 1.1 M credits came in
from 2927-08-13 to 2928-03-01: the staff (110,000), courses (~60,000), New
Earth's growth (S2: ~12 Buildings, 4 Hospitals, Food resources, Vehicle
plant, Stadium, generators, Derricks) and the three colonies (142,000,
167,000, 142,000 + cores) took it. The colonies were founded on 2927-11-28
(Apollo), 2928-01-12 (Penelope) and 2928-02-15 (Mir); none mines yet (no
droids, S7). The pre-run estimate for act 1 (0.94 M, 6.1k/day at the end,
with Jade on 2928-04-02) matches.

Measured through act 2 (an earlier recording of seed 12345, one game from
"new game": Jade 2927-09-08, contact 2928-03-02; populations and the income,
the credits' step at midnight, were read off the game's memory for this
table only; every colony's mood "neutral men" or better, New Earth at Very
difficult, the colonies Very difficult from their Park and Church on, S3):

| Date | New Earth | Apollo | Penelope | Mir | Credits | Income/day |
|---|---|---|---|---|---|---|
| 2928-01-01 | 67,456 | 11,824 | | | 43,704 | ~7,700 (Jan) |
| 2928-02-01 | 73,088 | 20,862 | | | 151,722 | ~9,300 (Feb) |
| 2928-03-01 | 77,813 | 30,405 | 5,745 | | 73,682 | ~11,300 (Mar) |
| 2928-04-01 | 81,651 | 39,674 | 14,132 | 4,151 | 93,914 | ~13,900 (Apr) |
| 2928-05-01 | 84,688 | 48,723 | 23,654 | 11,103 | 129,926 | ~17,500 (May) |
| 2928-06-01 | 87,039 | 57,499 | 32,553 | 21,561 | 303,508 | ~20,300 (Jun) |
| 2928-07-01 | 88,829 | 63,041 | 43,234 | 31,045 | 532,862 | ~22,700 (Jul) |
| 2928-08-01 | 90,177 | 67,516 | 52,194 | 39,794 | 981,887 | ~24,500 (Aug) |
| 2928-08-05 | 90,329 | 68,034 | 53,092 | 40,736 | 854,482 | 24,852 |

Russel (founded 08-02) had 4,087 people at the end. 3.65 M came in from Jade
(E23, 2927-09-16) to the Galleon, 2.59 M of it after the contact; the
pre-run estimate has 1.65 M and 19,600/day from the contact to the fall, the
run 1.94 M and ~22,000/day. From May on the goals bought far less than came
in (the J8 claim of 150,600 never held anything back), so the credits piled
up: 616,000 at the S.O.S., 854,000 at the end (Russel's founding, 167,000,
among the last buys). New Earth's housing stop (S2, ~95,000 ceiling) held it
at 90,000; the colonies carry the growth. Ore is no gate either: New Earth's
storage (190,000 t per ore) was nearly full of every ore by August (S6),
Texon included. Act 3 can spend from the start: M8's ~0.75 M is in hand, and
its lines (Twin laser, Battle tank), not money, set the pace.

Measured through act 3 (an earlier recording, from the Galleon to Mirach 3;
read off the game's memory for this table only; every colony "neutral
men" or better at Very difficult):

| Date | New Earth | Apollo | Penelope | Mir | Russel | Credits | Income/day |
|---|---|---|---|---|---|---|---|
| 2928-08-05 | 90,329 | 68,034 | 53,092 | 40,736 | 4,087 | 854,482 | 24,852 |
| 2928-09-01 | 91,183 | 72,709 | 59,312 | 48,363 | 9,234 | 731,326 | ~29,200 (Sep) |
| 2928-10-01 | 91,926 | 77,499 | 65,681 | 54,120 | 18,193 | 1,156,031 | ~32,300 (Oct) |
| 2928-11-01 | 92,473 | 81,398 | 71,569 | 58,929 | 27,418 | 1,482,734 | ~34,400 (Nov) |
| 2928-12-01 | 92,874 | 84,488 | 76,562 | 63,409 | 36,655 | 2,055,322 | ~36,800 (Dec) |
| 2929-01-01 | 93,167 | 86,885 | 80,650 | 66,813 | 45,818 | 2,348,397 | ~36,900 (Jan) |
| 2929-02-01 | 93,380 | 88,714 | 83,902 | 71,061 | 54,575 | 3,079,374 | ~42,300 (Feb) |
| 2929-03-01 | 93,537 | 90,093 | 86,434 | 74,707 | 61,689 | 4,131,221 | ~43,300 (Mar) |
| 2929-04-04 | 93,658 | 91,206 | 88,537 | 77,797 | 66,863 | 5,480,208 | 43,872 |

8.47 M came in over the act's 239 days and 3.84 M went out: the army (Home
Guard grown to 40H 40SF 6D and 90 tanks with their weapons, ~2.9 M at the
shop's prices), Dempsey's three courses (229,000), the Space station, the
colonies' growth (Universities above 60,000 people, act 3) and 6 more
Trade ships. Money never waited: the army goal ran each line at its depth
from the day its product was done. The lines set the pace: Battle tank
from 2928-08-11 (90 by March, ~50 h each with the passes' gaps), then the
Twin laser line, which the M18 force (302 twins) kept busy to 2929-04-03.
Ores: Energon fell from 107,000 t to 41,000 t at the Phelonian deal and
never below it again (the 10 Trade ships); Detoxin fell from 145,000 t to
41,000 t (the station's 60,000 and 6 Destroyers' 45,000) and was back at
64,000 t in March, so 20 Derricks did. The continuous run took 22
days longer than runs resumed at J13 (Mirach 2929-03-09 to 03-16): their
goals start afresh and the passes fall differently.

Measured through act 4 (an earlier recording, from Mirach
3 to the League's surrender; read off the game's memory for this table
only):

| Date | New Earth | Apollo | Penelope | Mir | Russel | Credits | Income/day |
|---|---|---|---|---|---|---|---|
| 2929-04-01 | 93,649 | 91,139 | 88,596 | 78,081 | 58,669 | 5,689,510 | 45,362 |
| 2929-05-01 | 93,732 | 91,893 | 90,002 | 81,866 | 59,810 | 5,415,881 | 47,208 |
| 2929-06-01 | 93,792 | 92,449 | 91,050 | 84,855 | 59,944 | 6,554,547 | 48,606 |
| 2929-07-01 | 93,832 | 92,857 | 91,827 | 87,168 | 59,181 | 7,572,113 | 49,121 |
| 2929-08-01 | 93,862 | 93,155 | 92,401 | 88,926 | 58,400 | 8,482,232 | 50,163 |
| 2929-09-01 | 93,892 | 93,371 | 92,821 | 90,249 | 59,501 | 9,622,057 | 50,685 |
| 2929-10-01 | 93,906 | 93,532 | 93,129 | 91,235 | 60,121 | 11,098,736 | 53,516 |

6.41 M came in over the act's 186 days and 1.06 M went out: Julie and
Klaatoo (370,000) and their five courses (~370,000), the Antares colony
(178,000 with its Observatory and carrier), the tips, and the army's
lines: 50 Aircraft, ~100 tanks' worth of the lost and the new, and the
ships lost at the invasions. Money never waited; the lines did. The
Twin laser line ran the whole act (the aircraft's 4 each, 200 for 50),
and when Strike brings few tanks home from Mirach the tank line for
Home Guard's 40 is a second gate (a run whose Strike came home with 29
tanks took Orionis 3 two weeks later). Russel's attitude swung between
21 and 40 and its population stayed at ~60,000: its ground is full, S4
wants a generator there before anything else (a Vehicle plant gets 60%
or less of its energy) and finds no room for it, so its Food resources
never come and it starves now and then ("Your people are starving");
New Earth, Apollo, Penelope and Mir stayed at 30.

Measured through act 5 (an earlier recording, from the League's surrender to
Earth; read off the game's memory for this table only):

| Date | New Earth | Apollo | Penelope | Mir | Russel | Credits | Income/day |
|---|---|---|---|---|---|---|---|
| 2929-10-02 | 93,906 | 93,536 | 93,138 | 91,263 | 60,190 | 10,988,850 | 53,564 |
| 2929-11-02 | 93,906 | 93,648 | 93,359 | 91,985 | 59,552 | 10,363,619 | 53,783 |
| 2929-12-02 | 93,906 | 93,731 | 93,522 | 92,515 | 58,780 | 10,462,808 | 54,366 |
| 2930-01-02 | 93,906 | 93,791 | 93,639 | 92,904 | 58,919 | 10,453,562 | |
| 2930-02-02 | 93,906 | 93,832 | 93,725 | 93,189 | 60,249 | 10,969,183 | |
| 2930-03-04 | 93,906 | 93,864 | 93,789 | 93,409 | 60,110 | 12,651,161 | 56,051 |

About 8.3 M came in over the act's 151 days and 6.7 M went out, nearly
all of it the army: 10 Cruisers (3.4 M), 50 Missile launchers (1.3 M),
~340 Missiles (1.0 M), 42 Plasma guns (0.2 M), and Strike's ships beyond
act 4's (27H 14SF 4D). Money never waited; the lines did.
The Missile line set the date: its ~340 missiles (250 for the launchers,
60 for the Cruisers) at 10 h each kept it busy from the act's first day
to 2930-02-30, a month after the 10th Cruiser (02-02) and the 40th
launcher for Strike (01-28). The Cruiser line (one queued per Space
station) idled for a pass of the goals after each Cruiser, which a
second station would have saved, but it was never the gate. Ores held:
New Earth's Texon, which the Missiles (300 each) and Plasma guns (2,300)
draw, fell from 112,000 t to 72,000 t by early November and was back
at 122,000 t by March (S6). The forces at the end: Strike 60H 60SF 12D
10C, 40T 20Air 40ML; Home Guard 40T 10Air 10ML.

## Battles

The battle mechanics are in [walkthrough.md](walkthrough.md), "War". The
fixture never retreats: it waits for END BATTLE in space
(`ui_space_battle`), orders the ground formation (`ui_assign_units`) and
fights the ground battle with a tactic (`ui_ground_battle`, `ui_battle.h`).

### Tactic

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

### Force sizes

Win rates over seeds 1–10 (Mirach and Earth 1–20) with the policy's force
and alternatives. "Space" is the space battle, "ground" the landing. Losses
are means per run.

| Battle | Enemy | Policy force | Measured | Recommendation |
|---|---|---|---|---|
| Morgrul wave 1 (J8) | 6H (+3Tr) | 14H (2L) | 200 seeds: **12H 199/200**, lose 1.9H; 14H 200/200, lose 1.4H; 16H 200/200. 10 seeds: 10H 10/10; **8H: 7/10** | 14H. A lost space battle lands the troopers on a New Earth with no ground units (no tanks yet): game over. |
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

**As played** (the whole-game fixture, seed 12345, Kyle after his course;
earlier recordings and runs resumed from their checkpoints):

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

At Mirach 3 the enemy is more than measured above: the waves' survivors go
home there (a fifth of the destroyed survive, `SpWar_ApplySpaceLosses`),
so the space side was 55 ships and the ground 89Tr 42T, not 20H 20SF 3D
and 61Tr 40T. Allied forces at the battle body fight on the player's
side in both battles (status 6: `SpWar_InitSpaceFleets`,
`GrWarUnits_CollectAvailableForces`), so the Kalls' fleet 2, sent to
Mirach 3 by talk 6, joins the invasion. 30H 30SF 5D with them also won
(lost 31H 19SF 3D; ground 125T 10Tr lost 69T), but with little to spare
in space; the three Morgrul Destroyers, which only Destroyers hurt much,
did most of the damage.

At Lyrae 7 it is the same: the Lisonian wave, beaten in space at New Earth,
takes the fifth of its ships that survive and all its troops home (its
ground units never land), so Lyrae 7 had 81 ships and 100Tr 55T 15Air, not
the measured 73 and 70Tr 40T 10Air (`lyrae7-back` measures that). Its space
battle's loss table, read off the result picture, showed "lost 50–58H
29–33SF 2D 1C, destroyed 4H 4SF" for a won battle in which the grid lost 21H
5SF: `SpWar_ApplySpaceLosses` adds the losses of the ships stored on the
planet and its moons to the side its pointer was left at by the last race of
the force loop with status 2 or 6 (allied Undorlings, race 8: the player's
side), as the original does; `fixes.garrison_losses` now books them on the
body owner's side. The policy
reads the grids for the forces. Each home world taken becomes a colony of
the player (`Intel_ClaimBodyForPlayer`).

The runs use Kyle Douglas at hire (experience 20). By act 5 his factor has
grown to about 1.23 (MACH 1.32), which helps the player's space damage only.

**Ground reserve at New Earth.** A space battle lost at New Earth disbands
the Army groups there into the planet force and loses trade and carrier
groups; the ground battle that follows decides the game. Against waves 1–3
tanks suffice (wave 1 needs the space win); against anything with aircraft,
bring aircraft or launchers.

### Measuring

`tests/test_play_battle.c` puts the game into the situation the game's own
code creates (`play_test_setup`, so these runs are not proofs): a defence
starts with the attacking force at New Earth one hour before its attack,
an invasion with the target explored, at war and at intel 40, and an Army
group "Strike" in orbit. `*-ground` scenarios take the other side's ships
away to measure the ground battle alone; `lyrae7-back` is Lyrae 7 with the
Lisonian wave home again; `kit=noplasma` gives Destroyers no Plasma gun, as
before Rigel 1. The command line is in the file's header comment (for
example `build/release/test_play_battle vendor/Reunion_DOS_EN mirach
counter 1 10`); `order=` takes the formation rear to front in letters (r
trooper, t tank, a aircraft, m launcher).

## Risks

- **Story deadlines.** A missed Jaanosian meeting is lost for good (J2).
  Antares deletes everything there 560–569 h after talk 9 (L6). Earth's
  first wave comes 1,000–1,199 h after the contact unless Earth falls
  first (X6). The Mirach invasion has no deadline but Morgrul wave 4.
- **Battles are random**, but the outcome depends only on the forces
  brought: space battles roll per shot, ground battles only for move order
  and ties, so the same forces fight almost the same way while small
  changes in numbers or formation can turn them. A lost ground defence of
  New Earth is game over, so the ground reserve is the real safety net. A
  lost space defence loses the trade and carrier groups there and merges
  the Army units into the planet force. After a lost invasion the
  survivors go home and the units in reserve survive: rebuild to target
  +25% and retry.
- **Random finds.** Phoenix 1 (M5) and the Psy-radar's Rigel 2 (X4, 1/10
  a day; it waits up to 200 days, as 60 failed with p ≈ 0.2%) depend on
  daily rolls.
  Spy prices and reports are fixed; pirate deals succeed with their chance
  (40–100%) and each window comes once, so the policy never depends on
  one.
- **Code against the guide.**
  - A satellite carrier needs no pilot to move, but covers only 1 leg/h
    without one. Founding a colony needs Control Centre (which needs a
    Miner station placed once), not a Miner station at the colony.
  - The Tractor beam (from the wave 2 space win) is never used by any
    code; a ground win against wave 2 also sets story stage 3.
  - Pirate-type groups can't be created (nothing enables them).
  - Ion cannon and Energy shield have no combat effect.
  - Army groups carry their ground units into an invasion (the group's
    second unit row). A group's slot takes up to 1,000 units, each weapon
    column up to its capacity × the units, so the Mirach strike (40H 40SF
    6D 90T) fits one Army group.
  - Docking at Jade while Morgrul force 1 sits there, or at Mirach 3 with
    a trade group, triggers nothing.
  - Waves arrive 20–29 h (inside a system) or 50–69 h (between systems)
    after launch and attack Random(100 − race speed) + 5 h later; S8
    relies on that gap of at least 5 h.
