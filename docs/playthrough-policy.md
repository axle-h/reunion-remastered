# Playthrough policy

The policy the full-playthrough fixture ([playthrough.md](playthrough.md))
follows from "new game" to the Earth ground win (`Halt(10)`). Sources: the
guide as condensed in [walkthrough.md](walkthrough.md), the screen map in
[screens.md](screens.md) and the port in `src/game/`. Decoded texts are in
`tools/out/text/`: "MSG n" is line n of `MESSAGE.TXT`, "KIT n" of
`KITALAL.TXT`, "PIR n" of `PIRATE.TXT`.

Line numbers are for the working tree when this file was written: it has
uncommitted spike changes, so `main_program.c` lines past 390 are 3 higher
than in commit df789a6.

Markings: **[src]** checked in the code, **[plan]** a number chosen here
(the guide's vague count made concrete), **[guide]** untested play advice.

Contents: [0 Conventions](#0-conventions) · [1 Policy list](#1-policy-list) ·
[2 Standing orders](#2-standing-orders) · [3 Interrupt catalogue](#3-interrupt-catalogue) ·
[4 Visible signals](#4-visible-signals-for-story-triggers) · [5 Open questions and risks](#5-open-questions-and-risks)

## 0. Conventions

### Time and waiting

- h = game hour (101 running frames, [screens.md](screens.md) §5). The date
  `Y-M-D-H` is drawn at x=224 on the status bar, credits at x=144 (gfx.c:761, 778).
- `wait_until(pred, timeout)` lets the clock run on the **idle screen**:
  screen 11 MESSAGES (cmd 4), with the selected system set to Amnesty (S1).
  Screen 11 keeps the clock running and redraws the log, so group arrivals
  ("&lt;group&gt; arrived to ...", sim.c:1036-1047, log only, no box) are readable.
  Do not idle on screen 1: a research that finishes there shows a hologram
  that ends on a press and leaks that press into the command-centre hotspots
  (sim.c:353-354, command.c:291-302).
- Fast-forward: clicking the clock button (hotspot 0xE) gives one hour per
  click on screens whose clock runs (main_program.c:1832-1835). Allowed but
  not required.
- `T(x)` is the date on which visible event x was seen. The fixture records it
  when the event's box appears.
- Timeouts are sized from the code's ranges plus margin. A timeout never
  triggers an action that assumes the hidden event happened; it triggers the
  step's failure branch.

### Money and ore gates

- Every purchase is gated on **credits ≥ cost + reserve**. Reserve (R) is
  20,000 in act 1 and 50,000 from act 2 on; it covers dialogue payments, which
  are **not** balance-checked (`add_credits`, main_program.c:132-135).
- The shop takes ore only from New Earth's stock and silently cuts the
  quantity to what credits and ore allow (shop.c:262-266, 435-449). After
  "OK,BUY" the driver reads "Bought items:" in the queue panel and compares it
  with the request. A short order is a failure: haul ore (S6) and buy again.
- Each product's queue builds one unit at a time at `Time` h per unit; all
  products build in parallel (sim.c:369-396, ProductionRate 10 fixed,
  main_program.c:255).

Fully equipped unit cost (hull plus the loadout from walkthrough.md "Weapons
and loadouts"), computed from `Research::Products` **[src]**:

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

Ores are D Detoxin, E Energon, K Kremir, L Lepitium, R Raenium, T Texon.
Mining output per droid per hour is the planet's displayed deposit (planet
info shows raw/10) and the raw values are tens: New Earth E30 K20 L30, so
3/2/3 t per droid-hour (sim.c:921-940). Detoxin comes only from Derricks,
each 35% × deposit/10 per hour (sim.c:869-883): about 2.45 t/h per Derrick on
New Earth. **New Earth's ore stock starts at 0** (main_program.c:214-215).

**Short-money fallback (applies to every "buy n" and "build n"):** buy
`k = min(n, floor((credits − R) / unit cost))`. If `k ≥ 1`, buy k now and
repeat each game day until n is reached. If `k = 0`, wait (S-orders keep
running) with the step's timeout. Steps marked *hard* (a story deadline
depends on them) take priority over every other purchase in the step list.

### UI operations

Screen, command and refusal details are from the code; the drivers own the
pixel positions.

| Operation | Where | Success signal | Refusals (box text, unlogged) |
|---|---|---|---|
| `hire(name)` | Screen 2 (cmd 14): category cmd 50–53, candidate frame 0x15–0x17, cmd 40 HIRE MAN | Reply "OK, I will work for you."; info line "I work for you" (hire.c:122-141, 219) | "You don't have enough credits to hire me.", "I'm not skilled enough for the job." (only upgrades, hire.c:39-46) |
| `university(who, course)` | Command-centre staff hotspot 1–4 → screen 24. "Go to university?" → (developer) "What course shall I take?" → course → "It will be N credits" → "I will pay." | "All right, I'll go."; later box "Your advisor has returned from the university" (sim.c:1359) | "I don't think I could become more skilled...", "I cannot leave my work behind right now, sorry" (someone already away), "You don't have enough cash to pay the costs." (advisor.c:79-124) |
| `set_tax(colony, level)` | Screen 8 of the colony: INCREASE TAX 0x26 / DECREASE TAX 0x27, one step per click | Tax text at (0x61,0x55) reads the level (planetinfo.c:311) | – |
| `build(colony, kind, n)` | Screen 20: Invention up/down 0x17/0x18 to pick the kind, Build 0x1E, click a terrain cell | Cost deducted; building shown "under construction" then "Status : Active" (surface.c:362-423) | " You have no builder! ", " This building needs a builder plant! ", " ... vehicle plant! ", "&lt;builder&gt; is not able to construct this building" (main_program.c:739-761); credits flash = no money |
| `research(product)` | Screen 3 (cmd 2): click the product icon. **Clicking a running project pauses it** | Panel "Under development"/"Under analysis", "Completed: n%" rising; box "The &lt;name&gt; is ready for production" (sim.c:321) | % stops rising = skills short (sim.c:306-315) |
| `buy(item, n)` | Screen 5 (cmd 23): select, BUY ITEM 18, editor (+1/−1/+10/−10), 30 OK,BUY. The editor starts at the current queue: set it to queued + n | Queue panel "Bought items" | Silent cut by credits/ore; " You can build only one Destroyer or Cruiser on a space station" (shop.c:268-273) |
| `add_droid(body)` | Screen 4: ADD DROIDS 25 | Droid count +1 (droids.c:59-73) | Nothing happens if stock 0, droids ≥ mines+stations, or ≥ 9 |
| `new_group(type)` | Screen 16 (cmd 13): NEW UNIT 15 → screen 21 "Change type" → 60 OK,CREATE IT | Group listed on screen 16 | Type missing until unlocked: Army after Hunter, Trade after Transfer ship, Carrier after Satellite carrier; Pirate never (shipinfo.c:146, sim.c:344-352) |
| `equip(group, unit, n)` | Screen 22: ship slots 0x1C–0x20 (left +1, right −1), weapons 0x21–0x34, page 0x19/0x1A. **Only while docked at a colony** (groups.c:454) | Counts in the grid | Grid dim = not docked |
| `cargo(group, item, ±n)` | Screen 13 via screen 17 "Transfer": ores left 100 t / right all, items one per click | "Space: used/cap", "Storage space on planet" (cargo.c:309-318) | " You need a spaceport to transfer items " for non-ore items (cargo.c:113, 157) |
| `dock / take_off(group)` | Screen 17 Docking/Launch toggle | Panel "In the dock of" / "In orbit" (groups.c:329-334) | " You haven't got a pilot! ", gaseous planet, "satellite carriers can only land on your main planet" (shipmove.c:128-144, 190, 208) |
| `launch(group, dest)` | Screen 17 Move → screen 18: system list (0x15–0x1C), right-click planet = destination, left-click = zoom to moons, sun 0x27 = explore system | Panel "Fly to &lt;sys&gt; &lt;body&gt;" | " You haven't hired a pilot! ", " No ships in this unit! ", " The ships are on ground! ", " No hyperdrive in this unit! ", " Your pilot is unable to undertake such a long voyage!" (launch.c:245-305) |
| `deploy_satellite(kind, body)` | Body toolbar on screens 8/20/17: 0x46 stock satellite, 0x14 carrier satellite, 0x43 spy satellite (carrier), 0x44 spy ship, 10 Miner station (trade group carrying one) (satellite.c:273-307) | ~5 s animation; object list line "Satellite"/"Spy satellite"/"Miner Station" (planetinfo.c:395-462) | "| You lost the contact with your satellite ! |" (normal satellite on alien body, satellite.c:113) |
| `found_colony(body, starters)` | COLONIZATION icon 5 on screens 8/20/17 → screen 28, starter hotspots 0x18–0x1D, 59 OK,BUILD IT | Box "The building of a colony started on X" (sim.c:698-701) | "You haven't got a builder!", "You builder not able to build this colony" (colonize.c:65-69); credits flash |
| `invade(group)` | Star-map zoom, select own Army group at the target: GROUND WAR icon 0x47 | Screen 29 | " You haven't got a battle advisor! ", " Your fighter not skilled to lead a ground battle " (launch.c:213-226) |
| `pub_ask(person, path)` | Screen 23 (cmd 45): click person hotspot 0x14+k → screen 25, click question rows until a `00` reply | Reply text; effects apply on the click | No exit button: every path must be clicked to its end |
| `answer(text)` | Screen 34 or 25: hover the question's first row, y = 129 + 9·row, wait 1 frame, click; click again to continue | Reply drawn | – |
| `name_prompt_accept()` | Rename box "The new planet's name in &lt;sys&gt; system?" | Box closes | Closes only on **Enter** with a non-empty name; never type (textfile.c:152-174) |
| `select_amnesty()` | Command centre PLANET MAIN (hotspot 0x20 / cmd 41) | Forces system 1, New Earth (main_program.c:592-596) | – |

Colony info that is **not** shown anywhere: food, hospital and housing
capacity, power totals and the intel number. The policy uses the counts of
visible buildings, population, the mood word, building "Status :
Active/Passive", and the colony warning boxes.

## 1. Policy list

Steps run in order inside an act; a step may start once its precondition
holds, so steps marked "‖" run in parallel with the next ones. Standing
orders (§2) and interrupts (§3) run between steps.

### Act 1: Economy (2927-08-13 to Jade contact; 23 steps)

Start: 120,000 credits, New Earth (NE) 30,000 people, tax Normal, 2 droids
working, 1 in stock; no staff (main_program.c:240-262; walkthrough
"Starting position").

**E1. Hire builder.** Pre: screen 1, credits ≥ 70,000. Do: `hire(William Forbs)`.
Done: "I work for you". Timeout: none. Fail: none expected (start money).
Building needs a hired builder (" You have no builder! ", main_program.c:741).

**E2. Hire developer.** Pre: credits ≥ 15,000. Do: `hire(Thomas Dempsey)`.
Done: as E1. Earlier than the guide's order: research is the critical path,
and Jaanosian meeting 1 only counts down while a developer is present
(sim.c:1407-1408).

**E3. Tax New Earth.** Do: `set_tax(New Earth, Very difficult)`. Done: tax
text. Then S3 governs it.

**E4. Research Satellite.** Pre: screen 3 shows Satellite "Can be developed"
(state at start, DS:5D77 table). Do: `research(Satellite)`. Done: box "The
Satellite is ready for production". Timeout: 20 h (dev 140 / exp 20 = 7 h).
‖ then `research(Miner droid)` ("Can be analysed"; 10 h, timeout 30 h).

**E5. Third mine.** Pre: credits ≥ 6,000 + R. Do: `build(New Earth, Mine, 1)`;
when "Status : Active", `add_droid(New Earth)`. Done: droid count 3.
Timeout: 72 h.

**E6. First Derricks.** Pre: credits ≥ 4 × 7,000 + R. Do: `build(New Earth,
Derrick, 4)` **[plan]**, with S4 (power) checked after each. Done: 4 Active.
Timeout: 120 h. Short money: fallback rule.

**E7. Two satellites.** Pre: Satellite done; NE stock ≥ D74 E62 K24 L10;
credits ≥ 1,560. Do: `buy(Satellite, 2)`. Done: shop "Stores: 2". Timeout:
200 h (Detoxin at ~10 t/h with 4 Derricks). Fail: build 2 more Derricks.

**E8. Deploy satellites.** Pre: 2 in stock. Do:
`deploy_satellite(stock, Apollo)` (New Earth's moon; survives,
satellite.c:49-52), then `deploy_satellite(stock, Klatoo)` (destroyed by
design, satellite.c:53-57). Done: Apollo object list shows "Satellite"; within
5–14 h scene 9 with MSG 1 "Your satellites were destroyed!..." (sim.c:1402-1406,
main_program.c:1507-1510). Timeout for MSG 1: 30 h. Fail: deploy a third on
Zeus.

**E9. Vehicle plant, Stadium.** Pre: credits ≥ 35,000 + R, then ≥ 36,000 + R.
Do: `build(New Earth, Vehicle plant, 1)`, then `build(New Earth, Stadium, 1)`
(needs Elanor or better, Builder and Vehicle plant). Done: both Active.
Timeout: 60 days each; short money: wait. Not *hard*.

**E10. Satellite carrier.** Pre: KIT 4 box "The Satellite-Carrier has been
invented" (an idea, not a finished product; 10–19 h after MSG 1,
sim.c:1384-1396). Do: `research(Satellite carrier)` (270/20 ≈ 14 h). Done:
"ready for production". Timeout: 60 h after MSG 1.

**E11. Miner station.** Pre: box "Apollo | is ready for mining" then MSG 2
(Apollo intel crosses 10, sim.c:640-650), then KIT 5 (20–59 h). Do:
`research(Miner station)` (550/20 ≈ 28 h). Done: "ready for production" and
MSG 3 "You cannot transfer the Miner Station to planets!" (sim.c:327-330).
Timeout: 10 days after E8.

**E12. University 1 (Dempsey).** Pre: E11 done; credits ≥ 30,000 + R; no
advisor away. Do: `university(Thomas Dempsey, Maths & physics.)`. Done:
"Your advisor has returned..." (50–69 h). Skills become 5/4/4/2 (Control
Centre needs P4). Research and idea timers stop while he is away
(sim.c:304, 1379); accepted here because nothing is due.

**E13. Transfer ship.** Pre: KIT 6 "The Transfer Ship is fully invented"
(30–39 h after E11, counting only developer-present hours). Do:
`research(Transfer ship)` (2,180 / 70 ≈ 31 h after the course; 109 h at
exp 20). Done: ready box. Timeout: 8 days after E12.

**E14. Hire pilot.** Pre: E13 done; credits ≥ 25,000 + R. Do:
`hire(Jack Benson)`. Then `university(Jack Benson)` when credits ≥ 30,000 + R
and nobody is away (+~50 experience: travel speed is ×(exp+1), sim.c:1008-1013).

**E15. Miner station on Apollo** (*hard*: unlocks Control Centre, which
gates colonies). Pre: NE stock ≥ D1,723 E1,545 K784 L268; credits ≥
13,500 + R. Do: `buy(Miner station, 1)`, `buy(Transfer ship, 1)`;
`new_group(Trade)` "Trade 1"; `equip(Trade 1, Transfer ship, 1)`;
`cargo(Trade 1, Miner station, +1)` at New Earth (has a Spaceport);
`launch(Trade 1, Apollo)`; on arrival `deploy_satellite(Miner station,
Apollo)`. Done: MSG 4 "This planet is suitable for living..."
(satellite.c:145-151). Timeout: 30 days from E13. Fail: if Detoxin is the
gate, add Derricks (S4 power first).

**E16. Carriers survey Amnesty.** Pre: E10 done; credits ≥ 4 × 2,310.
Do: `buy(Satellite carrier, 2)`, `buy(Satellite, 2)`; `new_group(Carrier)`
"Survey 1" with 2 carriers + 2 satellites (one per carrier, groups.c:248).
Shuttle it Klatoo → Zeus → Klatoo... (carriers need no pilot). Each day in
transit inside Amnesty has a 1/5 chance to reveal a status −2 planet, Amnesty 1
first, then Amnesty 2 (sim.c:763-787). Arriving at Zeus reveals its moons
("explored Zeus's moons", sim.c:1082-1091). Then `deploy_satellite(carrier,
Penelope)`; when Amnesty 2 is revealed (box + `name_prompt_accept()`), fly
there and `deploy_satellite(carrier, Mir)`. Done: both bodies list
"Satellite". Timeout: 40 days. **This does not reveal Jade** (status −3,
observatory only; §5 R1).

**E17. Control Centre.** Pre: KIT 7 "The Colony is invented" (20–59 h after
MSG 4). Do: `research(Control Centre)` (1,550/70 ≈ 22 h; needs P4). Done:
ready box. Timeout: 6 days after E15.

**E18. Colony Apollo.** Pre: E17 done; COLONIZATION icon on Apollo (intel
> 29, satellite.c:301-307); credits ≥ 142,000 + R. Do: `found_colony(Apollo,
{Builder Plant, Windtrap, Mine, Building})` (100,000 + 21,000 + 10,000 +
6,000 + 5,000). Done: "The building of a colony started on Apollo". Leave
tax Normal until S3 says otherwise. Then build in this order, each gated on
credits and S4 power: 2 Windtraps (Desert doubles them), Vehicle plant,
Spaceport, Park, Church, Leisure centre, Food resources, Hospital, Mines to 9
(each fed by `add_droid` after droids arrive, S7), then Buildings per S2.
Timeout: 60 days for the founding; build-out runs as S2.

**E19. Colony Mir.** Pre: Mir icon present; credits ≥ 142,000 + R. Do:
`found_colony(Mir, {Builder Plant, Windtrap, Mine, Building})`; then as E18
plus a **Medicine plant and a second Hospital before colony day 10**
(MP(10)). Done/timeout: as E18. Mir (E70) is the main Energon source until
Russel.

**E20. Colony Penelope.** Pre: icon; credits ≥ 167,000 + R. Do:
`found_colony(Penelope, {Nuclear Gen., Builder Plant, Mine, Building})`
(Earth-like: no windtraps). Raenium 90 and Texon 70. Then as E18. Not
*hard*; may slip into act 2.

**E21. New Earth to 9 mines, 20 Derricks** **[plan]** (guide "20 now, 60
later"). Pre: credits; S4. Do: `build(New Earth, Mine, 6)` with
`buy(Miner droid, 6)` and `add_droid` each (droid D10 E10 K5 L3, 3,150);
`build(New Earth, Derrick, 16)`. Done: 9 mines with 9 droids, 20 Derricks
Active. Runs as a background goal, lowest priority in act 1.

**E22. Watch for Jade** (*hard* deadline). Pre: none; starts at once. Do:
keep New Earth's Observatory standing (never Destroy it). Done: box " Your
observatory found a new planet in Amnesty system" whose rename prompt shows
"Jade" (sim.c:752-760, message.c:301-313); `name_prompt_accept()`. Timeout:
2928-04-02. The port's fix (`game/fixes.h`) reveals Jade on 2928-04-01 at
the latest, with the same box, so a timeout means a port bug: fail the run.

**E23. Stand-by trade group.** Pre: Trade 1 exists (E15). Do: after the
station is placed, keep Trade 1 parked at New Earth, undocked (in orbit), with
its Transfer ship, so it can launch the moment Jade appears. Haul ore with it
only while no Jade box is pending.

Act 1 money order when credits are short (highest first): E1, E2, E4, E7,
E15 (*hard*), E14, E17, E5, E6, E12, E10, E16, E18, E19, E9, E21, E20.

### Act 2: Jaanosians (Jade contact to the Galleon; 13 steps)

**J1. Contact.** Pre: Jade known (E22). Do: `launch(Trade 1, Jade)` (any
non-carrier group, sim.c:1099-1103). Done: box "Your pilots met with a new
alien race in Amnesty | They call themselves 'Jaanosians'" and MSG 5 "The
alien race's language is incomprehensible..." (intel.c:59-66). Record
T(contact). Timeout: 7 days from E22. Target: before 2928-04-15 (the timer
fires 2928-04-23 at the earliest, main_program.c:240). Fail: relaunch;
check pilot hired and Trade 1 has a ship.

**J2. Communicator** (*hard*). Pre: KIT 8 "The Communicator is invented"
(20–39 h after contact). Do: `research(Communicator)` at once, pausing any
other project (1,100/70 ≈ 16 h). Done: ready box. Timeout: T(contact) +
300 h. Must be done before meeting 1 (≥ 650 h after contact, intel.c:67-68);
without it each meeting is replaced by MSG 6 and lost (sim.c:1411-1413).

**J3. Keep the developer home.** From T(contact) until meeting 1: no
university for Dempsey (meeting 1 counts down only while he is present,
sim.c:1407-1409). Keep R ≥ 16,000 from T(contact) until J5 (talk 3's price
is not balance-checked).

**J4. Meeting 1** (interrupt I-T2). Pre: screen 34 opens (no box first).
Answer: "We are glad that we could finally decode your transmission..." →
"Could you be more specific ?" → "What can I do ?" → **"O.K."**. Done: reply
"We will hopefully contact you soon with good news." Then MSG 8 "Your
scientist has returned..." 60–89 h later: Trade ship granted (Done on screen
3; sim.c:1415-1424, main_program.c:1526-1533). Timeout for the talk: T(contact)
+ 900 h of developer-present time; for MSG 8: 120 h after the talk.

**J5. Meeting 2** (I-T3). Pre: MSG 9 " An incoming message from Jaanosians."
and screen 34. Answer: "Greetings Alien !" → "What is it ?" → **"What is the
price ?"** → **"Yes."** (−16,000). Done: "All right. We are sending it."
(main_program.c:1535-1539). Timeout: T(contact) + 1,700 h.

**J6. Meeting 3, the S.O.S.** (I-T4). Pre: MSG 10 "You receive an S.O.S.
message from the Jaanosians." and screen 34. Answer: "Greetings Alien." →
"What shall we fight with? We have no weapons!" → **"O.K. We will do our best
to help."** Done: screen 3 shows Hunter and Laser cannon Done; Army appears
in "Change type" (main_program.c:1540-1544). Record T(SOS). Timeout:
T(contact) + 3,200 h.

**J7. War commander.** Pre: J6 done; credits ≥ 45,000 + R. Do:
`hire(Kyle Douglas)`, then `university(Kyle Douglas)` when nobody is away.
Done: "I work for you". *Hard* (wave 1 ≥ 800 h after T(SOS); a space attack
needs a fighter, and damage scales with (exp+200)/220, spwar.c:223-226).

**J8. First army.** Pre: J6. Do: `buy(Hunter, 12)`, `buy(Laser cannon, 24)`
**[plan]** (wave 1 is 6 Hunters + 3 Troopers); `new_group(Army)` "Home
Guard" at New Earth; dock it; `equip(Home Guard, Hunter, 12)`,
`equip(Home Guard, Laser cannon, 24)`. Gate: 12 × 8,800 = 105,600 credits
and D6,600 E23,340 K5,256 L5,808 R2,808 (R from Apollo/Penelope, S6). Done:
grid shows 12 Hunters, 24 lasers. Timeout: T(SOS) + 750 h. Fallback minimum
8 Hunters (the battle is automatic; 8 equipped Hunters with a commander
outgun 6). Then keep buying to 20 (S9).

**J9. Base falls** (wait). Pre: J6. Wait for MSG 11 "The Morgruls have
attacked and destroyed the Jaanosian base!" + scene 1 (sim.c:1451-1466).
Record T(fall). Timeout: T(SOS) + 300 h. Story stage becomes 2 (developer
caps rise).

**J10. Radio.** Pre: T(fall). Do: `launch(Trade 1, Jade)`; on arrival
`dock(Trade 1)`. Done: MSG 13 "Whilst searching the ruins of the destroyed
Jaanosian base you find a Sub-Space radio transmitter." (shipmove.c:153-156);
then `research(Radio)` (1,000/70 ≈ 15 h). Timeout: 10 days after T(fall).
Fail: if docking is refused, deploy nothing; retry from orbit/dock after
wave 1 leaves Jade. Radio gates the Phoenix S.O.S. (sim.c:1485), which waits
for it (§5 R4).

**J11. Colony Russel.** Pre: T(fall); a carrier satellite on Russel (Jade
moon) until its icon appears; credits ≥ 167,000 + R. Do:
`found_colony(Russel, {Nuclear Gen., Builder Plant, Mine, Building})`, then
the E18 build-out with a Spaceport and 9 mines. Russel has E90 R80 T40; the base's fall clears the Jaanosians' moons to
owner 0 (intel.c:442-467), so the icon can appear.
Timeout: 60 days; not *hard*.

**J12. Hyperspace drive.** Pre: MSG 30 "Your scientists have found that the
bought item is a hyperspace drive." (300–349 h after the fall, counting only
developer-present hours, sim.c:1471-1475); screen 3 icon stops reading
"Unknown". Before it: `university(Thomas Dempsey, Phisics.& electr.)` (needs
P5; skills 5/6/5/2). Do: `research(Hyperspace drive)` (6,500/70 ≈ 93 h).
Done: ready box + MSG 12. Timeout: T(fall) + 900 h.

**J13. Galleon.** Pre: KIT 15 "The Galleon is invented" (50–119 h after J12;
waits for Trade ship Done, sim.c:1384-1385). Do: `research(Galleon)`
(4,600/70 ≈ 66 h), then `buy(Galleon, 1)` (47,000; D8,750 E12,670 K3,226
L11,000 R1,778 T1,936). `new_group(Trade)` "Galleon 1" with the Galleon,
equip at New Earth. Done: Galleon 1 exists. Timeout: T(fall) + 1,500 h.

### Act 3: Morgruls (18 steps)

**M1. Wave 1.** Pre: "ATTENTION! Enemy fleet approaching New Earth" or "New
alien race's ships arrived at your colony in Amnesty | They call themselves
'The Morgruls'" (sim.c:1216, 1237). Do: S8 evacuation, then let the battle
run (I-B29). Done: `war\spvict` and END BATTLE. Timeout: T(fall) + 750 h
(wave launches 600–649 h after the fall and travels 20–29 h). Fail: §5 R9.

**M2. Twin lasers and Troopers.** Pre: KIT 13 (20–39 h after the first
win), KIT 17 (90–139 h after the first space win). Do: `research(Twin laser
gun)`; `buy(Twin laser gun, 1 per Hunter)`; refit every Hunter.
`research(Trooper)`. Done: Hunters at 2L+1T.

**M3. Explore Phoenix.** Pre: Galleon 1 (J13), Jack hired. Do:
`launch(Galleon 1, Phoenix sun)`. Done: " Your ships explored Phoenix system
| They found new planets" + rename prompts (accept each) (sim.c:1050-1066).
First jump plays MSG 34 + scene 2 (sim.c:993-997). Timeout: 10 days.

**M4. Kalls.** Do: `launch(Galleon 1, Phoenix 3)`. Done: first-contact box
"'The Kalls'", MSG 15, talk 5 (I-T5): "Greetings Alien." → **"That is
right."** → **"When are we receiving your weapons?"** Starfighter becomes
analysable and 10 Starfighters appear in stock (main_program.c:1545-1548).
Record T(Kall). Timeout: 10 days.

**M5. Find Phoenix 1 early.** Pre: M4; Phoenix S.O.S. (MSG 17) **not yet
seen**. Do: shuttle Galleon 1 Phoenix 3 ↔ Phoenix 2 until " Your pilots found
a new planet in Phoenix system" names Phoenix 1 (status −2, 1/5 per transit
day; Phoenix 1 is checked first, sim.c:763-787). Never select Phoenix 1 after
MSG 17 has appeared until M11. Timeout: 30 days. Fail: continue; M11 then
needs this shuttle done before it (after MSG 17, shuttle without selecting
Phoenix 1; the find box does not select it).

**M6. Phelonians.** Pre: Phoenix explored; Phelonians unmet; Antares not yet
explored (the Eran item hides this tip). Do: `pub_ask(Informer, "Do you
have any information?" → "All right, talk.")` (−500). Done: the reply gives
the coordinates and Phoenix 5 is drawn on the Phoenix map; there is no rename
prompt (it sets the planet status directly, main_program.c:1056-1069).
Then before arrival at Phoenix 5, spend New Earth's Energon on queued orders
(the deal halves the stock, main_program.c:1558-1570). Do:
`launch(Galleon 1, Phoenix 5)`. Done: first contact "'Phelonians'". Timeout:
15 days.

**M7. Battle tank deal.** Pre: talk 7 (20–39 h after M6 contact, no box first,
sim.c:1520-1523). Answer: "Greetings Phelonians." → "What will you give in
return?" → **"I think something more valuable could be offered. A new
invention could be the base of our deal."** → **"It's a pleasure doing
business with you."** Done: Battle tank Done on screen 3. Timeout: 60 h after
contact. Fail: none (the talk is one-shot).

**M8. Wave 2 force** **[plan]**. Pre: M2, M4, M7. Do: `research(Starfighter)`
(3,490/70 ≈ 50 h). Build Home Guard to **24 Hunters (2L+1T), 16 Starfighters
(4 Twin), and a ground reserve of 20 Battle tanks (3L+1T)** in New Earth's
planet force (wave 2 is 25H + 10Tr, a lost space fight leads to a ground fight
at New Earth that must not be lost). Gate (from zero): 24 × 11,450 + 6 ×
19,000 (10 SF are free; 4 Twin each) + 16 × 4 × 2,650 (twins for the free
ones are included) + 20 × 13,300 ≈ 0.75 M credits; Energon ≈ 70k t.
Fallback: if by T(fall) + 1,600 h the target is not met, put every credit
into Battle tanks first (ground loss = game over), then Hunters.

**M9. Wave 2.** Pre: ATTENTION box for The Morgruls (wave launched 1,800–1,999
h after the fall; travel 20–29 h inside a system, 50–69 h between systems,
intel.c:385-400). Do: S8; battle. Done: MSG 16 "The Morgrul
invasion forces are defeated!" + scene 3 (stage 3, Tractor beam idea;
main_program.c:1250-1256). Record T(wave2). Timeout: T(fall) + 2,100 h.

**M10. Stage-3 course.** Pre: stage 3 (M9). Do: `university(Thomas Dempsey,
Artificial int.)` (A2 → 4, for Spy satellite and Missile). Price
(Random(10) + experience) × 1,000, about 70,000–79,000 once Dempsey is at his
cap of 70 (advisor.c:255); gate on credits ≥ price + R. Timeout: none.

**M11. Drifting Morgrul ship** (*ordering constraint*). Pre: MSG 17 (Phoenix
S.O.S.) seen **and** "Kall Fleet 2" listed in the Phoenix 5 star-map zoom
(§4 S-K), checked every 7 days; or, if the zoom is not readable, date ≥
T(Kall) + 2,100 h (fallback proxy, §4). Do: `launch(Galleon 1, Phoenix 1)`
(from a body in Phoenix; this leaves Phoenix 1 selected, launch.c:56-59) and
keep the map on Phoenix 1 until arrival. Done: MSG 18 "You have found a ship
drifting in space..." then talk 6 (I-T6): **"Tell me, what are your
plans?"** → "You will send a message to your leaders..." → **"All right."**
(adds 250 h to wave 3 if not yet sent; main_program.c:1549-1557). Then MSG 48
and `select_amnesty()`. Timeout: T(wave2) + 1,000 h. Fail: if MSG 18 has not
come 10 h after arrival, the S.O.S. was not received (Radio, J10).

**M12. Wave 3.** Pre: ATTENTION for The Morgruls (1,100–1,149 h after wave 2,
+250 h). Force **[plan]**: 30 Hunters, 24 Starfighters, ground reserve 30
Battle tanks (wave 3 is 20H 10SF 15Tr 2T). Done: MSG 19 "The second Morgrul
invasion has failed!" (space) or MSG 16 (ground) + scene 3; Spy present in the
pub (main_program.c:1257-1265, 1464-1472). Timeout: T(wave2) + 1,500 h.

**M13. Spy and Mirach.** Pre: M12; credits ≥ 30,000 + R. Do:
`pub_ask(Bartender, "Who is that suspicious character?" → "How dangerous is
he?" → "I think I will speak to him.")`;
`pub_ask(Spy, "Would you work for me?" → "Will you do the job for 30000?" →
"Do you know anything important?")` (−30,000, unchecked). Done: Mirach
appears in the star-map system list (no box, main_program.c:1025). Timeout:
3 days. Fail: Spy-Hunter route (§3 P-SH): Mirach after 200 h, MSG 37.

**M14. Spy satellite.** Pre: KIT 20 (30–39 h after M12). Do:
`research(Spy satellite)` (needs A4: M10). Then `buy(Spy satellite, 1)`,
`buy(Satellite carrier, 1)`, carrier group "Spy 1".

**M15. Explorer 1.** Do: `launch(Galleon 1, Mirach sun)` → rename prompts →
`launch(Galleon 1, Mirach 2 Moon 1)`, `dock`. Done: MSG 21 "You have found
Explorer 1!" + scene 4 with MSG 36; Missile and Destroyer analysable
(shipmove.c:158-163). Timeout: 15 days.

**M16. Missile, Destroyer, Space station.** Do: `university(Thomas
Dempsey, Electr and maths.)` (M6 E7) then `research(Missile)` (3/3/3/3),
`research(Destroyer)` (6/6/5/0; 9,500/70 ≈ 136 h; MSG 22 on completion);
KIT 24 50–119 h later; `university(Thomas Dempsey, Maths & physics.)`
(M7, stage ≥ 3); `research(Space station)` (7/5/7/0). Buy **2 Space
stations** **[plan]** (28,500 + D60,000 E48,000 K6,590 L2,585 R2,000 T3,270
each; Detoxin is the gate: build Derricks toward 40, S4), then Destroyers two
at a time (one per station, shop.c:268-273) to **5**. Timeout: 120 days.
Fallback: 1 station, 3 Destroyers.

**M17. Spy satellite on Mirach 3.** Do: `launch(Spy 1, Mirach 3)`,
`deploy_satellite(spy, Mirach 3)`. Done: box " Your satellite found The
Morgruls colony at | Mirach 3" (intel 40, sim.c:668-675). Timeout: 20 days.

**M18. Invade Mirach 3.** Pre: M17 done; Kyle hired; strike group "Strike"
(Army, ≥ 1 Destroyer, Jack can reach Mirach) **[plan]**: 20 Hunters, 30
Starfighters (4T+1M), 5 Destroyers, 60 Battle tanks; fallback 40 tanks.
Morgrul waves 1–3 are spent, so New Earth may be left with planet-force
tanks only. Do: `launch(Strike, Mirach 3)`; `invade(Strike)`. Battle per
I-B29/I-B19/I-B32. Done: " Your forces have eliminated 'The Morgruls' " +
MSG 35 + scene 3 (main_program.c:1284-1308). Timeout: 60 days. Fail: §5 R9;
rebuild and retry; wave 4 is ≥ 8,000 h after wave 3.

### Act 4: The League (13 steps)

**L1. Pilot.** Pre: credits ≥ 120,000 + R. Do: `hire(Julie-Anne Roxford)`;
`university(Julie-Anne Roxford)`.

**L2. Developer.** Pre: credits ≥ 250,000 + R. Do: `hire(Klaatoo)`. Then
courses as needed (each ~(90 + 0..9) × 1,000): Phisics.& electr. (Plasma
P8), Electr and maths., Maths & physics. (Mental radar M8 E8), Artificial
int. ×2 (A9). Keep one course in progress whenever Klaatoo is idle and
credits allow.

**L3. Pirate Leader (optional).** Pre: "Morgrul" person in the pub; credits ≥
500 + R. Do: `pub_ask(Informer, "Do you know anything about the Morgrul in
Space Local?" → "All right, talk.")`. This also unmasks the bartender's League
tip (main_program.c:976-979). Optionally `pub_ask(Pirate Leader, "I heard
that you accept dirty jobs for good money.")` and take deals with "You'll get
half of the cargo." (I-P9).

**L4. League tip** (*hard*, before the Lisonian wave launches). Do: buy
`Satellite carrier` ×1, group "Scout"; `launch(Scout, Orionis sun)`
(carriers make no contact, sim.c:1099/1103). Done: " Your ships explored
Orionis system". Then `pub_ask(Bartender, "Have you heard about the league?"
→ "Tell it!")` (−5,000). Done: reply text. Timeout: 3 days after the explore
box. Fail: if the bartender shows the Morgrul menu, ask "What is that
Morgrul doing in the bar?" once, then retry.

**L5. Antares colony.** Do: `launch(Galleon 1, Antares sun)` (Julie);
`found_colony(Antares 3, {Nuclear Gen.})` (William only, 135,000);
`build(Antares 3, Observatory, 1)` (42,000). Done: MSG 25 nova warning (50 h
after the Observatory completes, sim.c:1532-1538). Record T(nova). Timeout:
20 days after founding.

**L6. Erans, then leave Antares** (*ordering constraint*). Pre: T(nova). Do:
`launch(Galleon 1, Antares 3)`. Done: MSG 45 then talk 9: "Greetings
Erans..." → "Yes, we are sure..." → **"Yes ! My people will welcome you."**,
MSG 46. Record T(Eran). Then immediately `launch` every group in Antares to
Amnesty; never send a group to Antares again. Done: no group panel reads
Antares. Hard deadline: T(Eran) + 400 h (the supernova comes 560–569 h after
the talk, sim.c:1124-1126, and deletes everything in Antares).

**L7. Home guard before League contact** **[plan]**. Pre: L5 started. Build
Home Guard to **30 Hunters, 30 Starfighters, 5 Destroyers** plus a planet-force
ground reserve of **40 Battle tanks** (Lisonian wave: 20H 20SF 3D 30Tr 15T
5Air). Done: grid counts. Timeout: 120 days. Gate before L9.

**L8. Spy satellites on the League.** Do: 4 Spy satellites on 4 carriers;
`launch` and `deploy_satellite(spy, ...)` on Orionis 3, Lyrae 3, Lyrae 5,
Lyrae 7 (explore Lyrae first). Done: each body lists "Spy satellite"
(intel stalls at 31 until contact, sim.c:634-635).

**L9. League contact.** Pre: L4, L7. Do: `launch(Galleon 1, Lyrae 7)`, then
Lyrae 3, Lyrae 5, Orionis 3. Done: first-contact boxes for The Lisonians,
Undorlings, The Hirachi, The Druedians, and MSG 28 "The League declares war
against you" on the first (intel.c:86-92). Record T(league).

**L10. Eran's job.** Pre: L4 bought, Lisonians and Undorlings met. Do:
`pub_ask(Eran, "I have saved your race..." → "As I understand,your kind can
metamorph..." → "If any race joins our side..." → "Undorlings.")`. Done: 100–199
h later MSG 40 then MSG 44 (Undorlings allied, Ion cannon granted, Aircraft
analysable; sim.c:203-217). Timeout: 250 h. Fail: if he answers "I'm just
having a drink, that's all." a precondition is missing (tip or a contact).

**L11. Aircraft.** Do: `research(Aircraft)` (6/7/5/1); buy **20**
**[plan]**, fully equipped. Timeout: 60 days.

**L12. Lisonian wave.** Pre: ATTENTION for The Lisonians (200–249 h after
T(league)). Done: space win. Timeout: T(league) + 400 h.

**L13. Invade Orionis 3, Lyrae 5, Lyrae 7.** For each target: wait for its
"Your satellite found ... colony at" box, then `invade(Strike)`. Strike
**[plan]**: Orionis 3 (30H 23SF 3D; 90Tr 35T) and Lyrae 5 (30H 25SF 3D; 52Tr
10T): 30 Hunters, 30 SF, 5 D, 60 tanks, 20 Aircraft. Lyrae 7 last (40H 30SF
2D 1C; 70Tr 40T 10Air): 50 Hunters, 50 SF, 8 D, 60 tanks, 20 Aircraft. Rebuild
to target between attacks. Never invade Lyrae 3 (allied). Done (last one):
MSG 32 "The races of the League surrender!" + scene 5; Cruiser Done; Rigel in
the system list (main_program.c:1316-1330). Timeout: 120 days per target.

### Act 5: Earth (9 steps)

**X1. Rigel 1.** Do: `launch(Galleon 1, Rigel sun)`, then `launch(Galleon 1,
Rigel 1)`, `dock`. Done: MSG 31 + scene 6; Plasma gun and Missile launcher
analysable; MSG 33 then KIT 34 100–119 h later (shipmove.c:164-169,
sim.c:1388). Keep Galleon 1 or a carrier in Rigel (the system must stay
explored for the Psy radar).

**X2. Research.** Do (Klaatoo): `research(Plasma gun)`, `research(Missile
launcher)`, `research(Mental radar)` (10,000/90 ≈ 111 h). Refit Destroyers
with Plasma.

**X3. Cruisers and launchers** **[plan]**. Do: buy **6 Cruisers** (one per
station at a time; 373,000 equipped each) and **50 Missile launchers**
(41,000 each). Fallback: 4 Cruisers, 30 launchers. Final strike target:
50H, 50SF, 10D, 6C (+4 if affordable); ground 60 tanks, 20 Aircraft, 50 ML.

**X4. Rigel 2.** Pre: Mental radar Done. Wait for " Your Psy-radar radar found
a new planet in Rigel system" (1/10 per day, sim.c:789-797);
`name_prompt_accept()`. Timeout: 60 days. Then `launch(Galleon 1, Rigel 2)`.
Done: MSG 29 + talk 10: "Greetings Syonians !" → ... → **"Thank you for your
confidence in us..."** (only one option per state). Sun in the system list;
Bounty-Hunter in the pub (main_program.c:1576-1580).

**X5. Sun.** Do: `launch(Scout, Sun sun)` (carrier: no contact). Rename
prompts. Then a carrier with a Spy satellite to Earth (Sun 3),
`deploy_satellite(spy, Earth)`. Done: "Spy satellite" listed; intel stalls
at 31.

**X6. Home guard for Earth wave 1** **[plan]**. New Earth keeps 20 SF, 4 D,
2 C in space and 40 tanks, 20 ML in the planet force (Earth 1: 80SF 12D 3C
30Tr 30T 30Air, 1,000–1,199 h after contact). The rest is Strike.

**X7. Contact with the strike group.** Do: `launch(Strike, Earth)` with the
full X3 force. Done: MSG 38 "The Earthlings declare war aganist you"
(intel.c:97-101); Treasonable in the pub (sim.c:1097-1098). Record
T(earth).

**X8. Bounty-Hunter (optional).** Pre: credits ≥ 200,000 + R. Do:
`pub_ask(Treasonable, "Who are you?" → "And how can I help?" → "Will 100000
Credits be enough?" → "What would that be?" → "I know a bounty-hunter...")`;
`pub_ask(Bounty-Hunter, "Who are you?")`; then `pub_ask(Bounty-Hunter, "I have
a job for you." → "You would have to eliminate the leader of planet Earth..."
→ "O.K.")`. Done: MSG 41 100–199 h later. Do not wait for it if X9 is ready.

**X9. Invade Earth.** Pre: " Your satellite found Earthlings colony at |
Earth" (≤ 5 days after T(earth) at 2–6 per day). Do: `invade(Strike)`. Done:
ground win → screen 36 → exit code 10 (main_program.c:1308-1312). Timeout:
T(earth) + 900 h (before Earth wave 1). Fail: retreat is a loss; rebuild,
defend New Earth against Earth 1, retry.

Step counts: act 1: 23, act 2: 13, act 3: 18, act 4: 13, act 5: 9.

## 2. Standing orders

Run between steps, in this order, at least once per game day and after
every interrupt.

**S1. Map on Amnesty.** Mining reads the selected system's bodies only
(sim.c:921-940). After any action that selected another system (screens 7,
13, 16, 17, 18, 22, battles), call `select_amnesty()` before idling. Allowed
exceptions: M11 (Phoenix 1 held until arrival), screen-17 watches of a group
in transit.

**S2. Colony growth (every colony, daily).** From visible building counts and
population (planet info):
- Housing: if pop > 10,000 + 7,000 × Buildings − 3,000, build 1 Building.
- Food: if pop > 20,000 + 8,000 × Farms + 20,000 × Food resources − 3,000,
  build 1 Food resources.
- Hospitals: if pop × (2 on a diseased world, else 1) > 20,000 + 20,000 ×
  Hospitals − 3,000, build 1 Hospital. Mir from day 8, Russel/Antares 3 per
  MP(n): Medicine plant before day n.
- Leisure: one each of Park (Earth-like/Tropical/Desert), Church, Leisure
  centre, Stadium once a Builder and Vehicle plant stand.
- University (New Earth has one) before a colony passes 70,000.
- Each new building first passes S4.
React at once to the boxes in §3 group C (they name the missing building).

**S3. Tax.** Read the mood word on planet info (one band per 10 attitude:
"revolting men" 0–9 ... "loyal people" 60–69, planetinfo.c). Rules:
- mood "people don't like you" (20–29) → lower tax one step today;
- mood "people hate you" or "revolting men", or a "rebellious forces" box →
  set tax None until mood ≥ "neutral men";
- mood ≥ "people find you sympathetic" (40+) for 5 consecutive days and
  tax < Very difficult → raise one step.
Below 30 the population ceiling halves; below 10 a revolt destroys the colony
(sim.c:551-583). For New Earth a revolt is game over (screen 35).

**S4. Power.** Before building anything that draws power (all but Park,
Windtrap, Nuclear), and daily: if any Leisure centre, Stadium, Church or
Storage bay on that body shows "Status : Passive", power is short (these
switch off first). Then build one generator (Windtrap on Desert/Icy, else
Nuclear Gen. 35,000) before any other building there.

**S5. Droids.** If a body has mines > droids and stock > 0, `add_droid`.
New Earth: buy droids as needed (3,150 each). Colonies: droids come by trade
group and need a Spaceport there (cargo.c:113).

**S6. Ore hauling.** Every 10 days, or when a purchase is ore-gated: a trade
group (Trade 1; Galleon 1 when free) docks at each Amnesty colony, loads
everything (right-click per ore row, capped by ship space and by the planet's
storage), flies to New Earth, unloads. Priority: Energon, Raenium, Texon,
then the ore the next purchase lacks. Buy more Trade ships for Trade 1 when a
round trip leaves ore behind **[plan]**: 10 Trade ships by act 3, 25 by act 4
(guide uses ~50).

**S7. Droid delivery.** When a colony has a Spaceport and mines > droids:
`buy(Miner droid, k)`, load k on a trade group at New Earth, unload there,
`add_droid` k times.

**S8. Wave evacuation (reactive).** On an ATTENTION box (or first-contact
box) naming New Earth: within the same game hour, take off and `launch` every
Trade and Carrier group whose panel reads New Earth **or Apollo** (the battle
matches system and planet only, so New Earth's moons count,
intel.c:181-196, sim.c:1266-1270) to Penelope or Mir. Do not launch any group
to New Earth or Apollo until the battle result. Docked groups need
`take_off` first. Army groups stay.

**S9. Defence floor.** Fixed targets from the guide (not hidden state):
after J6, Home Guard ≥ 12 Hunters; after M1, ≥ 20; after M7, ≥ M8 targets;
after M12, ≥ 20 Hunters + 20 SF + 20 tanks; from L7 on, the L7 target; from
X6, the X6 target. When Strike leaves, Home Guard must still meet the
current floor.

**S10. Equip completeness.** After any purchase of ships, equip them fully
before the next battle (weapons are pooled per slot; unarmed hulls dilute,
walkthrough "Weapons and loadouts"). Re-check after each refit research
(Twin, Missile, Plasma).

**S11. Research queue.** When the developer is present and idle (no project
"Under development"): start the highest-priority available product in this
order: the current act step's product, Communicator, Radio, Hyperspace drive,
Galleon, Starfighter, Twin laser gun, Spy satellite, Missile, Destroyer, Space
station, Aircraft, Plasma gun, Missile launcher, Mental radar, Trooper, then
anything. Never click a running project (it pauses, research.c:66-121).

**S12. University for non-developers.** When nobody is away and credits ≥
course price + R + the next *hard* purchase: Jack, Kyle, Forbs, Julie, MACH in
that order, once each.

**S13. Idle.** With nothing to do: screen 11, map on Amnesty, clock running.

## 3. Interrupt catalogue

General handling:
- **Message box**: text centred near (160,124), status bar "Press mouse
  button" blinking (message.c:94-226). Close: release, press, release on an
  inert point. Logged boxes go to the message log.
- **Rename prompt**: "The new planet's name in &lt;sys&gt; system? ..."; press
  Enter only.
- **Press-leak**: scene and hologram waits end on a *press*
  (gfx.c:370-392, alientalk.c:88-108, command.c:291-302). Click an inert
  pixel (driver-defined) so the following screen-1 pass sees no hotspot.
- **Never press keys** during scenes: any key sets `Gfx_KeyAbort` for good
  and later scenes skip (gfx.c:360-373).
- On paused screens (29, 19, 32, 33, 34, 18, 21, 28, 24, 25) no hour runs, so
  no new interrupts arrive there.
- A screen switch to 29 in the same hour as a talk (34) or scene (33)
  overwrites it (§5 R7).

### A. Screens

| Id | Trigger (code) | Recognise | Action | Next |
|---|---|---|---|---|
| I-S33 story scene | `AlienTalk_StartAlienScene(n)` (alientalk.c:54-59) | Full-screen `pics\pic<n>`; music by scene (main_program.c:1478-1505). Scene 4 opens with MSG 36, scene 9 with MSG 1 | Scenes 3–7: one click; scene 1: click, release, click; scene 2: click (or ~7 min); scene 9 ends by itself | Screen 1 |
| I-S34 alien talk | `AlienTalk_StartAlienTalk(n)` (alientalk.c:178-184) | Music "talk", backdrop `pics\atvezeto`, alien portrait; menu rows from y=134 | Path from the talk table below | Screen 1 (+ MSG 46 or 48 on some) |
| I-B29 space battle | Enemy attack (sim.c:1276-1305) or own attack/invasion (launch.c:207-238) | `war\spwar` backdrop, fighter face | Never RETREAT (0x3E, counts as a loss) unless step says so. Wait for END BATTLE (0x41) over `war\spvict` / `war\splost`; click it | 7, or 19 (defending: only after a loss at a colony; attacking: only after a win) (main_program.c:1233-1246) |
| I-B19 unit assignment | After 29 | `war\grbeoszt`; slots pre-filled with all ground units at the body (grwarunits.c:276-279) | Keep pre-fill; click OK, ATTACK (0x0B). Own invasion also has CANCEL ATTACK (0x2A) | 32 |
| I-B32 ground battle | From 19 | `war\grf<surface>` field | Thread E tactics; minimum: select own stack, Attack 0x17, click nearest enemy stack, for each stack, once enemies are within 4 tiles; never Move Missile launchers forward. RETREAT 0x48 = loss. END BATTLE 0x41 over `war\grvict`/`war\grlost` | 7; **35** if lost at New Earth; **36** if won vs Earthlings |
| I-S35 death | Ground loss at New Earth, or New Earth destroyed (main_program.c:1442-1446, intel.c:229-240) | Death pictures | None (run failed) | Halt(15) |
| I-S36 victory | Ground win vs race 12 (main_program.c:1308-1312) | Palette fade | None | Halt(10) |
| I-S23 reload | Pub timer finishes while on 23 (sim.c:269-272) | Pub redraws | Re-read people | 23 |
| I-S1 forced | Advisor/scientist return while on 1; body destroyed while on 4, 8, 13, 16, 17, 20, 22 (sim.c:1354-1357, 1417-1421; intel.c:232-234) | Screen 1 | Resume step | 1 |

### B. Alien talks (screen 34)

Menus from the DS:0CDE tables and `KERDESn.AT`; effects in
`alien_talk_answer` (main_program.c:1523-1581). Effects apply when the
question is clicked.

| Talk | Trigger | Path (bold = the choice) | Effect |
|---|---|---|---|
| 2 Jaanosian meeting 1 | sim.c:1407-1414, no box | "We are glad that we could finally decode..." → "Could you be more specific ?" → "What can I do ?" → **"O.K."** | Scientist away 60–89 h; MSG 8; Trade ship granted (main_program.c:1526-1533). If shown "It's out of question !" state, still pick "O.K." |
| 3 Jaanosian meeting 2 | MSG 9 (sim.c:1425-1437) | "Greetings Alien !" → "What is it ?" → **"What is the price ?"** → **"Yes."** | −16,000 (unchecked); Hyperspace drive later (main_program.c:1535-1539) |
| 4 Jaanosian S.O.S. | MSG 10 (sim.c:1438-1450) | "Greetings Alien." → "What shall we fight with? We have no weapons!" → **"O.K. We will do our best to help."** | Hunter, Laser cannon, Army groups (1540-1544) |
| 5 Kalls | First contact + MSG 15 (intel.c:77-82) | "Greetings Alien." → **"That is right."** → **"When are we receiving your weapons?"** | Starfighter analysable, +10 in stock (1545-1548) |
| 6 drifting ship | MSG 18, then tractor-beam scene 10 (sim.c:1028-1033) | **"Tell me, what are your plans?"** (row 5) → "You will send a message to your leaders..." → **"All right."** | Cancels the Kall turn (only if their first timer fired); Kall force 2 to Mirach 3; +250 h to wave 3 if unsent (1549-1557); exit MSG 48 |
| 7 Phelonians | sim.c:1520-1523, no box | "Greetings Phelonians." → "What will you give in return?" → **"I think something more valuable could be offered..."** → **"It's a pleasure doing business with you."** | Battle tank granted; New Earth Energon halved (1558-1570) |
| 9 Erans | MSG 45 (sim.c:1120-1126) | "Greetings Erans..." → "Yes, we are sure..." → **"Yes ! My people will welcome you."** | Eran in pub, Erans allied (1572-1575); exit MSG 46; supernova 560–569 h later |
| 10 Syonians | First contact + MSG 29 (intel.c:93-96) | One option per state, ending **"Thank you for your confidence in us..."** | Sun known, Energy shield, Bounty-Hunter (1576-1580) |

### C. Message boxes nobody asked for

All need the close click. "Act" is what the policy does after closing.

| Box text (source) | Trigger (file:line) | Act |
|---|---|---|
| KIT k "The &lt;X&gt; is invented" (KITALAL k) | Idea timer fires (sim.c:1384-1396) | S11; step preconditions |
| "The &lt;name&gt; is ready for production" | Research done (sim.c:321) | Step done-conditions; refits (S10) |
| MSG 3 / MSG 12 / MSG 22 | Miner station / Hyperspace / Destroyer done (sim.c:327-340) | None |
| MSG 33 "...locate the electonically disguised planets" | Mental radar idea at 50 h (sim.c:1388) | None |
| MSG 1 + scene 9 | Stock satellite lost (sim.c:1402-1406) | E8 done |
| "&lt;X&gt; is ready for mining" / "not suitable for mining"; MSG 2 first time | Intel ≥ 10 (sim.c:640-650) | E11 |
| "&lt;X&gt; is a suitable place for living" / "not suitable"; "Your satellite reports alien activity on X" | Intel ≥ 30 (sim.c:653-665) | Colony icon check |
| " Your satellite found &lt;Race&gt; colony at X" | Intel ≥ 40 on alien body (sim.c:668-675) | Invasion precondition |
| " Your observatory/pilots/satellite carrier/Psy-radar radar found a new planet in &lt;sys&gt;" + rename | sim.c:752-797 | Enter; E16, E22, M5, X4 |
| " Your ships explored &lt;sys&gt; system \| They found new planets" + renames | sim.c:1050-1066 | Enter for each |
| "Your pilots/satellite carrier explored &lt;planet&gt;'s moons" | sim.c:1082-1091 | None |
| MSG 34 + scene 2 | First hyperspace jump (sim.c:993-997) | None |
| " Your pilots met with a new alien race in &lt;sys&gt; \| They call themselves '&lt;race&gt;'" | First contact (sim.c:1099-1115, intel.c:47-101) | J1, M4, M6, L9, X4, X7; follow-ups MSG 5, 15, 28, 29, 38 |
| "New alien race's ships arrived at your colony in &lt;sys&gt;..." | Unmet race's wave arrives (sim.c:1216) | S8 |
| "ATTENTION! Enemy fleet approaching &lt;where&gt;" | Hostile force arrives where you are (sim.c:1237) | S8 if New Earth; battle follows 5–105 h later |
| "Your colony is under attack by &lt;race&gt; at &lt;where&gt;" → screen 29 | Attack (sim.c:1277, 1297) | I-B29 |
| " Your ships around &lt;body&gt; is under attack by &lt;race&gt;" → 29 | Attack where an Army group is (sim.c:1287-1296) | I-B29 |
| MSG 8, 9, 10, 11, 30, 17, 24, 20, 37, 25, 27 | Story timers (sim.c:1402-1548) | See §4 |
| MSG 16, 19 (+ scene 3) | Wins over waves 2, 3 (main_program.c:1250-1265, 1458-1472) | M9, M12 |
| " Your forces have eliminated '&lt;race&gt;' "; MSG 35 / MSG 32 | Home-world ground win (main_program.c:1284-1330) | M18, L13 |
| MSG 13; MSG 21 + scene 4; MSG 31 + scene 6 | Docking at Jade (after fall), Mirach 2 Moon 1, Rigel 1 (shipmove.c:153-169) | J10, M15, X1 |
| MSG 4 | Miner station on a habitable body (satellite.c:145-151) | E15 |
| "The building of a colony started on X" | Colony countdown ends (sim.c:698-701) | S2/S3 |
| "Because of disease your people need a medicine plant on X" | sim.c:450-453 | Build Medicine plant |
| " Due to sanitary conditions you need a hospital on X" | sim.c:457-461 | Build Hospital |
| "Your people ask for a stadium on X" | pop > 50k, sim.c:486-489 | Build Stadium |
| "Your people demand a University on X" | pop > 70k, sim.c:491-494 | Build University (53,000) |
| " Your people are starving on X" | sim.c:541-544 | Build Food resources |
| " There isn't enough living space on X" | sim.c:548-551 | Build Building |
| "Your people are dying because of radiation on X" | Planet 1–2 colony, no shield (sim.c:496-505) | Never colonise planets 1–2; if seen, abandon investment there |
| Meteor boxes | Orionis/Lyrae 5 only (sim.c:507-537) | Rebuild |
| "Your advisors report that there are rebellious forces on X" | Attitude < 10 (sim.c:575) | S3: tax None now |
| " Your people have revolted on X" / " Your colony has been destroyed on X" | sim.c:573-583 | On New Earth: game over |
| "Your advisor has returned from the university" | sim.c:1359 | S12 |
| PIR k (radio) | Pirate deal window opens (sim.c:280-292) | I-P9 (optional) |
| MSG 39 + "Information on: ..." boxes | Spy report back (sim.c:192-201) | None |
| MSG 40 + MSG 44 (or 43) | Eran's job done (sim.c:203-217) | L10 |
| MSG 41 | Bounty-Hunter done (sim.c:218-232) | X8 |
| MSG 47 + "You got N t of X" / MSG 42 | Pirate report (sim.c:240-268) | None |
| MSG 6 "You have received a message from deep space..." | A Jaanosian meeting without Communicator, or the deadline without contact (sim.c:1413, 1435, 1444) | **Run failed** (§5 R1, R2) |
| Error boxes after own actions (§0 table) | – | Step failure branch |

### D. Pub people (screen 23 → 25)

Hotspot 0x14+k, caption = name; present when drawn (state 0 or 2 and no
running job, pub.c:81-99). No exit button: finish every path.

| Person | Use | Path |
|---|---|---|
| P-6 Bartender | Spy unlock (M13); League tip (L4) | "Who is that suspicious character?" → "How dangerous is he?" → "I think I will speak to him." / "Have you heard about the league?" → "Tell it!" (−5,000). Menu priority (main_program.c:976-979): Morgrul question masks the tip while the Pirate is present and unexplained |
| P-4 Informer | Phoenix 5 (M6); Pirate Leader (L3) | "Do you have any information?" → "All right, talk." (−500); "Do you know anything about the Morgrul in Space Local?" → "All right, talk." (−500) |
| P-2 Spy ("Stranger") | Mirach (M13) | "Would you work for me?" → "Will you do the job for 30000?" (10,000/20,000 refused, no charge) → "Do you know anything important?" (main_program.c:1021-1030) |
| P-1 Spy-Hunter (P-SH) | Mirach fallback | First talk "Have you seen or heard anything unusual?"; later "There is a spy in the pub. Do your job..." → "Are you sure?" → "Yes! Kill him!" → MSG 37 after 200 h (main_program.c:1016-1019, sim.c:1528-1530) |
| P-9 Pirate Leader ("Morgrul") (I-P9) | Optional cargo | "I heard that you accept dirty jobs for good money." starts 10 windows; in a window (after its PIR box): "I have a good job for you." → row 1 → "You'll get half of the cargo." |
| P-5 Eran | L10 | see L10; "I'm just having a drink, that's all." = not yet |
| P-10 Treasonable, P-7 Bounty-Hunter | X8 | see X8 |

## 4. Visible signals for story triggers

| Trigger | Visible signal | Notes |
|---|---|---|
| Jade revealed | Box " Your observatory found a new planet in Amnesty system" + rename prompt prefilled "Jade"; Jade then drawn on the Amnesty map | Only the observatory (status −3, sim.c:752-756) |
| Amnesty 1/2, Phoenix 1/4, Mirach 1/5, Lyrae 6 revealed | "...pilots/satellite carrier found a new planet..." | Status −2: groups in transit in that system, 1/5 per day (sim.c:763-787) |
| Phoenix 5 revealed | Informer reply; Phoenix 5 drawn on the Phoenix map (no box) | Status −3: Informer or a Phoenix observatory only |
| Rigel 2 revealed | "Your Psy-radar radar found a new planet in Rigel system" | sim.c:789-797 |
| New system known | Name appears in the star-map system list (needs Hyperspace drive or Galleon Done, planetinfo.c:899-915) | Mirach: no box after the Spy (main_program.c:1025), MSG 37 via Spy-Hunter; Antares/Orionis/Lyrae: MSG 35; Rigel: MSG 32; Sun: after talk 10 |
| Jaanosian deadline | None until it fires (MSG 6) | Use the date: contact by 2928-04-15 |
| Meetings 1–3 | Talk screen; MSG 9, MSG 10 | Hidden timers; wait with timeouts |
| Base destroyed | MSG 11 + scene 1 | T(fall) |
| Wave launched | **No signal** | Only arrival: ATTENTION box (≥ 5 h before the attack) |
| Wave at New Earth | ATTENTION box; "The Morgruls Fleet n" listed in the New Earth zoom | sim.c:1237, planetinfo.c:1024-1045 |
| Hyperspace drive available | MSG 30; icon caption leaves "Unknown" | research.c:122-140 |
| Phoenix S.O.S. | MSG 17 | Needs Radio Done; waits for it |
| **S-K: Kall fleet at Phoenix 5** | Star-map zoom on Phoenix 5 lists "Kall Fleet 2" (allied forces are listed, planetinfo.c:960-1053); it disappears from the Phoenix 3 zoom | Fleet in transit is not drawn. Fallback proxy if the list can't be read: date ≥ T(Kall) + 2,100 h (code max 1,999 + 29 h travel + margin). This uses an upper bound, not a guess, but it is hidden-timer reasoning; prefer the list |
| Kall alliance fired anyway | MSG 24 | Too late: Kalls hostile, wave with Morgrul force 4 300–349 h later |
| Phelonian call due | Contact box | Talk follows in 20–39 h without a box |
| Spy available | "Stranger" drawn in the pub after MSG 19 or MSG 16 for wave 3 | – |
| Intel ≥ 40 | " Your satellite found &lt;Race&gt; colony at X"; GROUND WAR icon available | Number never shown |
| Nova warning | MSG 25 | – |
| Supernova | MSG 27 + scene 7 | Too late if anything is in Antares |
| League contact | MSG 28 | T(league) |
| Undorling split | MSG 44 | – |
| League defeated | MSG 32 | – |
| Earth contact | MSG 38 | T(earth) |
| Race relations | ALIEN PLANETS list (screen 37): "Enemy colony", "Friendly colony", "Aliens on" | planetlist.c:42-63 |
| Group position | Panel "In the dock of", "In orbit", "Fly to"; log "arrived to" | groups.c:329-334 |
| Selected system | Star map shows it; list entry highlighted | Reset with PLANET MAIN |

## 5. Open questions and risks

**R1. Jade has no player-controllable reveal (fixed in the port).** The
port reveals Jade on 2928-04-01 if it is still hidden (`fixes.find_jade`,
docs/bugs.md); the rest of this note is the original's behaviour. Jade's status is −3, so
only New Earth's observatory finds it: 1/50 per day, one find per day across
the galaxy, shared with Amnesty 1, 2 and 6 (sim.c:752-760, DS:47D6). Flying
a group around Amnesty does **not** help; it only finds status −2 planets
(sim.c:763). The task brief and the walkthrough's route assume otherwise for
Jade. P(no find in ~240 days) ≈ (49/50)^237 ≈ 0.8%. If Jade is not found by
~2928-04-20 the Jaanosian chain runs without the player (MSG 6), Hunter and
Laser are never granted, and the game is unwinnable. No in-run retry exists;
the proof run must use a seed where it holds, and the robustness report will
show this as a seed-dependent failure. Second observatories don't help (the
code tests `> 0`).

**R2. A missed Jaanosian meeting is unrecoverable.** Each meeting fires once;
without Communicator it becomes MSG 6 (sim.c:1411-1449). Meeting 3 is the only
source of Hunter and Laser. J2 is *hard*.

**R3. Phoenix 5 cannot be found by flying.** It is −3 too. The walkthrough's
"or fly around Phoenix" is wrong; use the Informer. The Informer's
Phelonian item is replaced once Antares is explored, so M6 must precede L5.

**R4. Radio timing (guide vs code).** The guide says Radio must be done
before wave 2 is sent. In the code the S.O.S. countdown is set when wave 2 is
sent and only ticks while Radio is Done (sim.c:1485), so a late Radio only
delays the S.O.S. Low risk; keep J10 early anyway.

**R5. Drifting-ship trigger is wider than the guide says.** Any group
arriving anywhere while Phoenix 1 is the selected planet fires it once MSG 17
has appeared (sim.c:1028). Selecting Phoenix 1 early (e.g. to read its info)
with a group about to arrive anywhere would fire talk 6 before the Kall fleet
moves, and the alliance would still happen (sim.c:1498-1503). Rule: after MSG
17, never select Phoenix 1 until S-K holds.

**R6. Unchecked payments.** Talks 3, the Spy, bartender, informer,
Treasonable and Bounty-Hunter subtract without a balance check
(main_program.c:132-135); negative credits block shop, hire and build. Keep
R.

**R7. Same-hour overwrite.** An attack (screen 29) set in the same hour as a
talk or scene replaces it, and the talk is lost (CheckEnemyAttacks runs last,
screens.md §2). Before meeting 3 no hostile forces exist, so the Jaanosian
chain is safe. Talks 5, 6, 7, 9, 10 could collide with a wave. Mitigation:
don't trigger first contacts or arrivals while an ATTENTION box has been
seen and its battle hasn't come yet.

**R8. Economy is unverified.** Credit and ore totals per act are computed
from the tables, not simulated. Income is
pop × tax × development × (attitude+40) / 200,000 per colony per day
(sim.c:565-574): New Earth at start ≈ 3,200/day. Act 3's M8 needs ~0.75 M
credits and ~70 kt Energon in ~1,800 h. If the run falls behind, the order of
cuts is: Stadiums in colonies, Penelope, extra Trade ships, Troopers, the
Pirate Leader, Bounty-Hunter. Not to be cut: J2, J7, J8, M7, the Battle-tank
ground reserve, S8.

**R9. Battles are random.** Hit chance 11% per ship per round, instant-kill
rolls, 1/5 of losses survive (walkthrough "Space battle"). Retry plans:
- Defending New Earth: no retry exists for a lost ground battle (game over).
  So the ground reserve rule in M8/S9 is the real safety net.
- Lost space defence: trade/carrier groups at New Earth are gone (S8 should
  have moved them); Army units merge into the planet force (intel.c:181-196);
  rebuild.
- Lost invasion: survivors go home (intel.c:181-197), units in
  reserve survive; rebuild to target +25% and retry. Time pressure only for
  Earth (Earth 1 wave) and Mirach (wave 4 after 8,000 h).

**R10. Spy pricing and reports.** Hiring is deterministic (≥ 30,000
accepted). Report prices are fixed tables. No retry needed.

**R11. Pirate deals.** Each deal succeeds with its chance (40–100%) and each
window is one-shot. Optional; the policy never depends on one.

**R12. Mental-radar find is random.** 1/10 per day once Mental radar is Done
and Rigel is explored; a 60-day timeout fails with p ≈ 0.18%.

**R13. Antares deadline.** The supernova deletes every group there. L6 leaves
immediately; the colony (≥ 177,000) is a sunk cost.

**R14. Code vs guide, smaller points.**
- Satellite carrier needs no pilot; colony founding does not need a Miner
  station (satellite.c:301-307), only Control Centre, which needs one placed
  once.
- Tractor beam (wave 2 space win) is never used by any code; story stage 3
  is also set by a ground win against wave 2 (main_program.c:1458-1463).
- Talk 6: "All right." delays wave 3 by 250 h; the walkthrough calls this a
  plain delay, which the policy uses for preparation time.
- Pirate-type groups can't be created (no code enables them).
- Ion cannon and Energy shield have no combat effect (walkthrough "Attacks on
  New Earth").

**R15. Open questions for integration.**
- Exact inert click point after scenes/holograms (driver work).
- Whether Army groups carry ground units for invasions or the assignment
  screen draws from planet forces at the body only (grwarunits.c:191-279);
  the driver must check before M18.
- Group capacity limits per slot (groups.c) for the large strike groups.
- Whether docking at Jade while Morgrul force 1 sits there triggers anything
  (no mission set, so no attack is expected; sim.c:1247).
- Wave travel and wait times: arrival 20–29 h (in system) / 50–69 h
  (between systems) after launch; attack after Random(100 − speed) + 5 h
  (intel.c:126-133, 385-400). S8 relies on the ≥ 5 h gap.
