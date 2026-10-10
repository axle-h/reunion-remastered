# Playthrough policy

The policy the full-playthrough fixture ([playthrough.md](playthrough.md))
follows from "new game" to the Earth ground win (`Halt(10)`). Sources: the
guide as condensed in [walkthrough.md](walkthrough.md), the screen map in
[screens.md](screens.md) and the port in `src/game/`. Decoded texts are in
`tools/out/text/`: "MSG n" is line n of `MESSAGE.TXT`, "KIT n" of
`KITALAL.TXT`, "PIR n" of `PIRATE.TXT`.

Line numbers are for `src/game/` at commit e0a077f (after the Jade fix in
`sim.c` and the driver hooks in `main_program.c`). Corrections from the UI
driver threads (`tests/play/ui_*.c`) and the battle measurements in
[playthrough-battles.md](playthrough-battles.md) are folded in; "observed"
marks behaviour seen in runs but not yet traced in the code.

Markings: **[src]** checked in the code, **[plan]** a number chosen here
(the guide's vague count made concrete), **[guide]** untested play advice.

Contents: [0 Conventions](#0-conventions) · [1 Policy list](#1-policy-list) ·
[2 Standing orders](#2-standing-orders) · [3 Interrupt catalogue](#3-interrupt-catalogue) ·
[4 Visible signals](#4-visible-signals-for-story-triggers) · [5 Open questions and risks](#5-open-questions-and-risks)

## 0. Conventions

### Time and waiting

- h = game hour (101 running frames, [screens.md](screens.md) §5). The date
  `Y-M-D-H` is drawn at x=224 on the status bar, credits at x=144 (gfx.c:763, 780).
- `wait_until(pred, timeout)` lets the clock run on the **idle screen**:
  screen 11 MESSAGES (cmd 4), with the selected system set to Amnesty (S1).
  Screen 11 keeps the clock running and redraws the log, so group arrivals
  ("&lt;group&gt; arrived to ...", sim.c:1054-1065, log only, no box) are readable.
  Do not idle on screen 1: a research that finishes there shows a hologram
  that ends on a press and leaks that press into the command-centre hotspots
  (sim.c:367-368, command.c:291-302). Screen 11's toolbar has only BACK TO
  M.SCREEN.
- The status bar shows no credits during the first game hour; read credits
  from hour 1 on.
- Fast-forward: clicking the clock button (hotspot 0xE) gives one hour per
  click on screens whose clock runs (main_program.c:1835-1838). Allowed but
  not required.
- `T(x)` is the date on which visible event x was seen. The fixture records it
  when the event's box appears.
- Timeouts are sized from the code's ranges plus margin. A timeout never
  triggers an action that assumes the hidden event happened; it triggers the
  step's failure branch.

### Money and ore gates

- Every purchase is gated on **credits ≥ cost + reserve**. Reserve (R) is
  20,000 in act 1 and 50,000 from act 2 on; it covers dialogue payments, which
  are **not** balance-checked (`add_credits`, main_program.c:132-135). As
  run: 50,000 from the Jaanosian contact (J1 waits for its date with
  20,000: a 50,000 reserve from Jade's discovery on held Apollo's founding
  back two weeks, to 2927-12-14).
- The shop takes ore only from New Earth's stock and silently cuts the
  quantity to what credits and ore allow (shop.c:276-279, 435-449). After
  "OK,BUY" the driver reads "Bought items:" in the queue panel and compares it
  with the request. A short order is a failure: haul ore (S6) and buy again.
- Each product's queue builds one unit at a time at `Time` h per unit; all
  products build in parallel (sim.c:383-410, ProductionRate 10 fixed,
  main_program.c:255). Production stops while the shop's quantity editor is
  open (sim.c:389). So a big order of one unit type is limited by build hours,
  not only money: 90 Battle tanks are 4,500 h of tank line.

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
3/2/3 t per droid-hour (sim.c:939-958). Detoxin comes only from Derricks,
each 35% × deposit/10 per hour (sim.c:887-901): about 2.45 t/h per Derrick on
New Earth. **New Earth's ore stock starts at 0** (main_program.c:214-215).

**Short-money fallback (applies to every "buy n" and "build n"):** buy
`k = min(n, floor((credits − R) / unit cost))`. If `k ≥ 1`, buy k now and
repeat each game day until n is reached. If `k = 0`, wait (S-orders keep
running) with the step's timeout. Steps marked *hard* (a story deadline
depends on them) take priority over every other purchase in the step list.

### UI operations

Screen, command and refusal details are from the code; the drivers own the
pixel positions. Buttons are found by their toolbar label (quoted in capitals
below).

**Clicks.** Hotspots other than toolbar buttons repeat their handler every
retrace while the button is held and once more on the release over the same
hotspot: a 4-retrace click on the equip grid adds 5. This is the original's
main loop (docs/screens.md §2). Drivers use `UI_PRESS` (press for 1 retrace,
then slide off before releasing), so each action runs once; the counts below
are per handler run.

| Operation | Where | Success signal | Refusals (box text, unlogged) |
|---|---|---|---|
| `hire(name)` | Screen 2 (cmd 14): category cmd 50–53, candidate frame 0x15–0x17, cmd 40 HIRE MAN | Reply "OK, I will work for you."; info line "I work for you" (hire.c:122-141, 219) | "You don't have enough credits to hire me.", "I'm not skilled enough for the job." (only upgrades, hire.c:39-46) |
| `university(who, course)` | Command-centre staff hotspot 1–4 → screen 24. "Go to university?" → (developer) "What course shall I take?" → course → "It will be N credits" → "I will pay." | "All right, I'll go."; later box "Your advisor has returned from the university" (sim.c:1377) | "I don't think I could become more skilled...", "I cannot leave my work behind right now, sorry" (someone already away), "You don't have enough cash to pay the costs." (advisor.c:79-124) |
| `set_tax(colony, level)` | Screen 8 of the colony: INCREASE TAX 0x26 / DECREASE TAX 0x27, one step per click | Tax text at (0x61,0x55) reads the level (planetinfo.c:311) | – |
| `build(colony, kind, n)` | Screen 20: Invention up/down 0x17/0x18 to pick the kind, Build 0x1E, click a terrain cell | Cost deducted; the new site's info reads "Status : Passive", "Production : NONE", "Working : 0%" (there is no "under construction" text), later "Status : Active" (surface.c:362-422; Passive also means unpowered, S4). A finished Mine or Miner station opens the droid screen 4 when clicked (main_program.c:670-674): that is the "mine done" signal | " You have no builder! ", " This building needs a builder plant! ", " ... vehicle plant! ", "&lt;builder&gt; is not able to construct this building" (main_program.c:742-764); credits flash = no money |
| `research(product)` | Screen 3 (cmd 2): click the product icon. Only one project runs: starting another stops the current one (research.c:74-105). **Clicking a running project pauses it.** Before clicking, "can be developed/analysed" is read from the icon art and its hint caption; the panel text needs a selecting click, which starts or pauses | Panel "Under development"/"Under analysis", "Completed: n%" **falling** from 100% to 0% (it shows the part left, research.c:174-182, sim.c:328-329; the original's behaviour); box "The &lt;name&gt; is ready for production" (sim.c:335) | % stops falling = skills short (sim.c:320-329) |
| `buy(item, n)` | Screen 5 (cmd 23): SELECT 31 / SELECT OFF 35 toggle the list mode; pick the item, BUY ITEM 18 opens the editor: ADD ONE 26, MINUS ONE 27, ADD TEN 28, MINUS TEN 29, OK,BUY 30, CANCEL BUY 32. The editor starts at the current queue: set it to queued + n. The editor stops the clock (shop.c:145-146). The list shows only Done products; BUY ITEM does nothing for items without the Produced flag (e.g. Nuclear gen.: "This can be built on your colonies", shop.c:567-579) | Queue panel "Bought items". Ore gate before OK,BUY: "Ores in stock" in the editor (includes the current queue's ores) | Silent cut by credits/ore; " You can build only one Destroyer or Cruiser on a space station" (queue per product ≤ stations, shop.c:280-286) |
| `add_droid(body)` | Screen 4, reached only by clicking a finished Mine or Miner station on the surface (screen 20): ADD DROIDS 25 | Droid count +1 (droids.c:59-73) | Nothing happens if stock 0, droids ≥ mines+stations, or ≥ 9 |
| `new_group(type)` | Screen 16 (cmd 13): NEW UNIT 15 (offered only once Satellite carrier or Transfer ship is Done) → screen 21 "Change type" → 60 OK,CREATE IT → screen 22 (groups.c:283-286). The name field opens in edit mode reading "New Group"; press Enter before clicking anything | Screen 22 shows the group; listed on screen 16. **New groups start docked** | Type missing until unlocked: Army after Hunter, Trade after Transfer ship, Carrier after Satellite carrier; Pirate never (shipinfo.c:146, sim.c:358-366) |
| `equip(group, unit, n)` | Screen 22: ship slots 0x1C–0x20 (left +1, right −1), weapons 0x21–0x34, page 0x19/0x1A. **Only while docked at a colony** (groups.c:454). A Miner station is equipment, not cargo: it goes in the "Mine" weapon column of a Trade group | Counts in the grid | Grid dim = not docked |
| `cargo(group, item, ±n)` | Screen 13 via screen 17 "Transfer": ores left 100 t / right all, items one per click | "Space: used/cap", "Storage space on planet" (cargo.c:309-318) | " You need a spaceport to transfer items " for non-ore items (cargo.c:113, 157) |
| `dock / take_off(group)` | Screen 17 Docking/Launch toggle | Screen 22 panel "In the dock of" / "In orbit" (groups.c:330-334; screen 16 shows only "Currently on:"/"Destination:", shipinfo.c:356-365) | Gaseous planet, " The satellite carriers can only land on your main planet" (New Earth itself, not its moons), " No ships in this unit ! " (shipmove.c:136-145) |
| `launch(group, dest)` | Screen 17 Move (in orbit only; take off first) → screen 18, opened zoomed on the group's current planet: ZOOM OUT before picking another planet. System view: system list (0x15–0x1C), right-click planet = destination, left-click = zoom to its moons, sun 0x27 = explore system. Zoomed view: either button picks a destination. Moons are listed only after a group has arrived at that planet ("explored &lt;planet&gt;'s moons") | Screen 22 panel "Fly to &lt;sys&gt; &lt;body&gt;", redrawn on arrival | Move: " You haven't got a pilot! " (non-carrier groups, shipmove.c:189-215). Screen 18: " You haven't hired a pilot! ", " No ships in this unit! ", " The ships are on ground! ", " No hyperdrive in this unit! ", " Your pilot is unable to undertake such a long voyage!" (launch.c:245-305) |
| `deploy_satellite(kind, body)` | Body toolbar on screens 8/20/17: ADD SATELLITE (0x46 stock satellite, or 0x14 a carrier's satellite: same label), ADD SPY SAT 0x43 (carrier), ADD SPY SHIP 0x44, ADD SOLAR SAT 0x45, ADD MINER STATION 10 (a Trade group at the body with a Miner station; body info level > 9, unowned, no colony) (satellite.c:261-312) | Stock satellite: no animation (satellite.c:48-59); carrier, spy-satellite and spy-ship deploys play a ~5 s animation. Object list line "Satellite"/"Spy satellite"/"Miner Station" (planetinfo.c:395-462) | "\| You lost the contact with your satellite ! \|" (normal satellite on alien body, satellite.c:113) |
| `found_colony(body, starters)` | COLONIZATION icon 5 on screens 8/20/17 → screen 28, starter hotspots 0x18–0x1D, 59 OK,BUILD IT (or ABORT) | Box "The building of a colony started \| on &lt;sys&gt; system,planet &lt;planet&gt;[,moon &lt;moon&gt;]" at a midnight 1–2 days later (sim.c:712-715) | "You haven't got a builder!", "You builder not able to build this colony" (colonize.c:65-69); credits flash |
| `invade(group)` | Star-map zoom, select own Army group at the target: GROUND WAR icon 0x47 | Screen 29 | " You haven't got a battle advisor! ", " Your fighter not skilled to lead a ground battle " (launch.c:213-226) |
| `pub_ask(person, path)` | Screen 23 via SPACE LOCAL (cmd 45, toolbar button and command-centre hotspot; nothing is labelled "pub"): click person hotspot 0x14+k → screen 25, click question rows until a `00` reply | Reply text; effects apply on the click | No exit button: every path must be clicked to its end |
| `answer(text)` | Screen 34 or 25: hover the question's first row, y = 129 + 9·row, wait 1 frame, click; click again to continue | Reply drawn | – |
| `name_prompt_accept()` | Rename box "The new planet's name in &lt;sys&gt; system?" | Box closes | Closes only on **Enter** with a non-empty name; never type (textfile.c:152-174) |
| `select_amnesty()` | Command centre PLANET MAIN (hotspot 0x20 / cmd 41) | Forces system 1, New Earth (main_program.c:595-599) | – |
| `open_body(body, screen)` | Galactic map (GALACTIC MAP): right-click a body = planet info (8); left-click a planet = zoom to it and its moons; left-click a body there = surface (20); ZOOM OUT = back to the system view | Screen 8 or 20 names the body | – |

**Travel.** Pilots are not attached to groups: the one hired pilot flies
every group, and each group in flight covers pilot experience + 1 legs per
hour (sim.c:1026-1031). A trip is 80–99 legs to another body around the same
planet, 150–199 to another planet, 200–249 + 1 h jump + 200–249 to another
system (intel.c:407-427). Without a pilot only carriers move, at 1 leg/h
(80–99 h to a moon, 150–199 h to another planet); with Jack at hire 4–10 h.

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
Building needs a hired builder (" You have no builder! ", main_program.c:744).

**E2. Hire developer.** Pre: credits ≥ 15,000. Do: `hire(Thomas Dempsey)`.
Done: as E1. Earlier than the guide's order: research is the critical path,
and Jaanosian meeting 1 only counts down while a developer is present
(sim.c:1425-1426).

**E3. Tax New Earth.** Do: `set_tax(New Earth, Very difficult)` (INCREASE
TAX three times from Normal). Done: tax text. Then S3 governs it. At Very
difficult New Earth's target attitude is 30 (10 for the tax + 30 for Leisure
centre, Church and Park − 10 for population > 30,000; sim.c:457-496), right
at the line below which the population ceiling halves (sim.c:571-572).
Observed (thread F, seed 12345): "people like you" → "people find you
sympathetic" on day 3 → "neutral men" by day 6–20, and there it stays at
Very difficult from 30,000 to 50,000 people and, with the Stadium (+10),
from 60,000 to 76,000 (> 60,000 is −10): the attitude settles on its
target and never dips into "people don't like you". The one fall seen was
the Stadium missing past 50,000 (−20): "neutral men" → "people don't like
you" → "people hate you" in two days; the old S3 then set tax None for 12
days (no income at all, so the Stadium came later still). So E3 stays Very
difficult, S3 computes the tax from what is built (§2), and the Stadium
(E9) comes before 50,000.

**E4. Research Satellite.** Pre: screen 3 shows Satellite "Can be developed"
(state at start, DS:5D77 table). Do: `research(Satellite)`. Done: box "The
Satellite is ready for production". Timeout: 20 h (dev 140 / exp 20 = 7 h).
‖ then `research(Miner droid)` ("Can be analysed"; 10 h, timeout 30 h).

**E5. Third mine.** Pre: credits ≥ 6,000 + R. Do: `build(New Earth, Mine, 1)`;
when a click on the new Mine opens screen 4 (§0 `build`), `add_droid(New
Earth)`. Done: droid count 3. Timeout: 72 h (not enforced: S2 and S4 come
first; observed done on day 2–3, the droid from stock).

**E6. First Derricks.** Pre: credits ≥ 4 × 7,000 + R. Do: `build(New Earth,
Derrick, 4)` **[plan]**, with S4 (power) checked after each. Done: 4 Active.
Timeout: 10 days, not enforced: S2 outranks E6 for money and the 4th
Derrick came after 30–65 days (2927-09-15 to 10-18), yet E15's Detoxin was
in stock before E15's money in every run. Short money: fallback rule.

**E7. Two satellites.** Pre: Satellite done; NE stock ≥ D74 E62 K24 L10;
credits ≥ 1,560. Does not depend on E6: New Earth starts with a Derrick
(~2.45 t/h, so D74 in ~30 h; seed 12345 had it ~27–36 h in, then waited
for 780 + R credits until the midnight tax). Do:
`buy(Satellite, 2)`. Done: shop "Stores: 2". Timeout: 200 h. Fail: build 2
more Derricks.

**E8. Deploy satellites.** Pre: 2 in stock. Do:
`deploy_satellite(stock, Apollo)` (New Earth's moon; survives,
satellite.c:51-53), then `deploy_satellite(stock, Klatoo)` (destroyed by
design, satellite.c:54-57). Neither plays an animation. Done: Apollo object list shows "Satellite"; within
5–14 h scene 9 with MSG 1 "Your satellites were destroyed!..." (sim.c:1420-1424,
main_program.c:1510-1513). Timeout for MSG 1: 30 h. Fail: deploy a third on
Zeus.

**E9. Vehicle plant, Stadium.** Pre: credits ≥ 35,000 + R, then ≥ 36,000 + R.
Do: `build(New Earth, Vehicle plant, 1)`, then `build(New Earth, Stadium, 1)`
(needs Elanor or better, Builder and Vehicle plant; William Forbs is the
best builder, staff 3, so it is offered). Done: both Active. Timeout: 60
days each; short money: wait. *Hard* in effect before New Earth passes
50,000 people (no Stadium there: −20 attitude, 40–50% of the income): from
40,000 people on E9 goes before S2 in the money order, and S2 builds no
more housing past 45,000 until the Stadium works. Observed: 40,000 people
on 2927-09-19, Vehicle plant 09-30, Stadium 10-13, 50,000 in late October.

**E10. Satellite carrier.** Pre: KIT 4 box "The Satellite-Carrier has been
invented" (an idea, not a finished product; 10–19 h after MSG 1,
sim.c:1402-1414). Do: `research(Satellite carrier)` (270/20 ≈ 14 h). Done:
"ready for production". Timeout: 60 h after MSG 1.

**E11. Miner station.** Pre: box "Apollo | is ready for mining" then MSG 2
(Apollo intel crosses 10, sim.c:654-664), then KIT 5 (20–59 h). Do:
`research(Miner station)` (550/20 ≈ 28 h). Done: "ready for production" and
MSG 3 "You cannot transfer the Miner Station to planets!" (sim.c:341-344).
Timeout: 10 days after E8.

**E12. University 1 (Dempsey).** Pre: E11 done; credits ≥ 30,000 + R; no
advisor away. Do: `university(Thomas Dempsey, Maths & physics.)`. Done:
"Your advisor has returned..." (50–69 h). Skills become 5/4/4/2 (Control
Centre needs P4). Research and idea timers stop while he is away
(sim.c:318, 1397); accepted here because nothing is due. The course costs
(Random(10) + experience) × 1000 (advisor.c:247-251), quoted as "It will
be N credits" before paying; Dempsey's experience grows with research, so
the quotes were 29,000–37,000 (70,000 and more after the course). The
policy pays when credits − R − higher claims cover the quote, else answers
"You shouldn't go." and asks again the next day; the Physics skill under
the research screen (≥ 4) says the course is done. Money order: right
after E17, since Control Centre stalls at "Completed: 50%" without P4
(sim.c:322-326); observed 2927-09-12.

**E13. Transfer ship.** Pre: KIT 6 "The Transfer Ship is fully invented"
(30–39 h after E11, counting only developer-present hours). Do:
`research(Transfer ship)` (2,180 / 70 ≈ 31 h after the course; 109 h at
exp 20). Done: ready box. Timeout: 8 days after E12.

**E14. Hire pilot.** Pre: E13 done; credits ≥ 25,000 + R. Do:
`hire(Jack Benson)`, **before E16** (the pilot speeds every group, carriers
included: §0 "Travel"). Then `university(Jack Benson)` when credits ≥
30,000 + R and nobody is away (+~50 experience: travel speed is ×(exp+1),
sim.c:1026-1031); as for E12 the quote (22,000–31,000 observed) decides,
and a quote of 60,000 or more means he has been. Observed: hired
2927-08-25 to 09-01 (waiting for 25,000 + R), course in September or
October.

**E15. Miner station on Apollo** (*hard*: unlocks Control Centre, which
gates colonies). Pre: NE stock ≥ D1,723 E1,545 K784 L268; credits ≥
13,500 + R. Do: `buy(Miner station, 1)`, `buy(Transfer ship, 1)`;
`new_group(Trade)` "Trade 1" (starts docked at New Earth);
`equip(Trade 1, Transfer ship, 1)`; `equip(Trade 1, Miner station, 1)` in
the "Mine" column (it is not cargo); `take_off(Trade 1)`;
`launch(Trade 1, Apollo)`; on arrival `deploy_satellite(Miner station,
Apollo)` (ADD MINER STATION; Apollo must be unowned, without a colony and at
body info level > 9, satellite.c:300-304). Done: MSG 4 "This planet is
suitable for living..." (satellite.c:136-142). Timeout: 30 days from E13. Fail: if Detoxin is the
gate, add Derricks (S4 power first). Observed: the ores were in stock when
E15 started (2927-08-27 to 09-01; money was the gate), station and ship
built in parallel in 60 h, Trade 1 at Apollo 3–5 h after launch with Jack,
MSG 4 at once, back in orbit at New Earth by 09-04.

**E16. Carriers survey Amnesty.** Pre: E10 done; Jack hired (E14);
credits ≥ 4 × 2,310. Do: `buy(Satellite carrier, 2)`, `buy(Satellite, 2)`;
`new_group(Carrier)` "Survey 1" with 2 carriers + 2 satellites (one per
carrier, groups.c:248); `take_off` (new groups start docked; carriers dock
only at New Earth). Shuttle it Klatoo → Zeus → Klatoo..., relaunching on
each arrival. Carriers need no pilot to move, but without one they cover
1 leg/h (150–199 h per hop); with Jack 7–10 h. Each day in
transit inside Amnesty has a 1/5 chance to reveal a status −2 planet, Amnesty 1
first, then Amnesty 2 (sim.c:781-805). Arriving at Zeus reveals its moons
("explored Zeus's moons", sim.c:1100-1109); Penelope is listed on the
destination screen only after that, so fly to Zeus before Penelope. Then
`deploy_satellite(carrier, Penelope)` (ADD SATELLITE); when Amnesty 2 is revealed (box + `name_prompt_accept()`), fly
there and `deploy_satellite(carrier, Mir)`. Done: both bodies list
"Satellite". Timeout: 40 days. **This does not reveal Jade** (status −3,
observatory only; §5 R1). Amnesty 2's moons, like Zeus's, are listed only
after a group arrived there ("explored Amnesty 2's moons"): fly to
Amnesty 2 before Mir. The group's grid tells the progress (2 satellites
left: Penelope next, 1: Mir, 0: done). Observed: the observatory or the
pilots of Trade 1 found Amnesty 1, 2 and 3 within the first month, so no
shuttling was needed; done 2927-09-20 to 10-26 (the carriers wait for E15,
whose ore they would share, and for E16's place in the money order).

**E17. Control Centre.** Pre: KIT 7 "The Colony is invented" (20–59 h after
MSG 4). Do: `research(Control Centre)` (1,550/70 ≈ 22 h; needs P4). Done:
ready box. Timeout: 6 days after E15 when E12 is done; the policy allows 30
days from the step, as the course may still wait for money (observed done
2927-09-15 to 09-20).

**E18. Colony Apollo.** Pre: E17 done; COLONIZATION icon on Apollo (intel
> 29, satellite.c:305-310); credits ≥ 142,000 + R. Do: `found_colony(Apollo,
{Builder Plant, Windtrap, Mine, Building})` (100,000 + 21,000 + 10,000 +
6,000 + 5,000). Done: box "The building of a colony started | on Amnesty
system,planet New Earth,moon Apollo", at a midnight 1–2 days after OK,BUILD
IT. Leave
tax Normal until S3 says otherwise. Then build in this order, each gated on
credits and S4 power: 2 Windtraps (Desert doubles them), Vehicle plant,
Spaceport, Park, Church, Leisure centre, Food resources, Hospital, Mines to 9
(each fed by `add_droid` after droids arrive, S7), then Buildings per S2.
Timeout: 60 days for the founding; build-out runs as S2. As run (thread F,
from the economy model): the colonies in the order Apollo, Penelope, Mir,
each colony's core (S2's needs, S4 power, Vehicle plant, Spaceport) in the
money order before the next founding; Park, Church and Leisure centre wait
for act 2, and so do the Mines, whose droids need the Spaceport and a
trade group (S7). Observed (act 1 held to 2928-03-01): Apollo founded 2927-11-28,
Penelope 2928-01-12, Mir 2928-02-15. The founding goals are not
*hard*: when Jade comes first they go on in act 2. From act 2 on the
build-out goes on after the core: a Stadium once the colony passes 40,000
people, Mines to 9 (the next one only when every finished Mine has its
droid, S7), Park, Church, Leisure centre; S3 sets the colony's tax and S4
looks ahead (§2). Observed (Jade on 2927-09-08, contact 2928-03-02):
Apollo founded 2927-11-30, Penelope 2928-02-12, Mir 03-28, Russel (J11)
08-02 (Penelope and Mir later than in the act-1 run above, as Apollo's
mines and leisure come before the next founding); on 08-01 Apollo had
68,000 people, Penelope 52,000, Mir 40,000 (§5 R8).

**E19. Colony Mir.** Pre: Mir icon present; credits ≥ 142,000 + R. Do:
`found_colony(Mir, {Builder Plant, Windtrap, Mine, Building})`; then as E18
plus a **Medicine plant and a second Hospital before colony day 10**
(MP(10)). Done/timeout: as E18. Mir (E70) is the main Energon source until
Russel. Order: after Penelope (see E18).

**E20. Colony Penelope.** Pre: icon; credits ≥ 167,000 + R. Do:
`found_colony(Penelope, {Nuclear Gen., Builder Plant, Mine, Building})`
(Earth-like: no windtraps). Raenium 90 and Texon 70. Then as E18. Not
*hard*; may slip into act 2. Founded before Mir: it is the only early
Texon source (economy model; Texon is the first ore short in act 3).

**E21. New Earth to 9 mines, 20 Derricks** **[plan]** (guide "20 now, 60
later"). Pre: credits; S4. Do: `build(New Earth, Mine, 6)` with
`buy(Miner droid, 6)` and `add_droid` each (droid D10 E10 K5 L3, 3,150);
`build(New Earth, Derrick, 16)`. Done: 9 mines with 9 droids, 20 Derricks
Active. Runs as a background goal, lowest priority in act 1.

**E22. Watch for Jade** (*hard* deadline). Pre: none; starts at once. Do:
keep New Earth's Observatory standing (never Destroy it). Done: box " Your
observatory found a new planet in Amnesty system" whose rename prompt shows
"Jade" (sim.c:766-778, message.c:301-313); `name_prompt_accept()`. Timeout:
2928-04-02. The port's fix (`game/fixes.h`) reveals Jade on 2928-04-01 at
the latest, with the same box, so a timeout means a port bug: fail the run.
Observed: seed 12345 revealed Jade on 2927-09-08, 09-16, 09-25, 10-13,
12-18 and 2928-01-21 in runs whose input differed: the observatory's daily
roll uses the game's one random generator, which the policy's own actions
(random advisor answers, sounds) advance, so the date is no fixed property
of the seed. On resume, Jade drawn on the Amnesty map (galactic map) shows
it was found.

**E23. Stand-by trade group.** Pre: Trade 1 exists (E15). Do: after the
station is placed, keep Trade 1 parked at New Earth, undocked (in orbit), with
its Transfer ship, so it can launch the moment Jade appears. Haul ore with it
only while no Jade box is pending. Act 1 ends when E22 and E23 hold; the
background goals not yet done (E9, E16, E18–E21, S12) go on in act 2. The
economy model puts J1 (contact) no earlier than ~2928-02-15, so after an
early Jade the stand-by can be long: J1 waits for 2928-03-01, and Trade 1
hauls (S6, S7) meanwhile.

Act 1 money order when credits are short (highest first): E1, E2, E4, E7,
E15 (*hard*), E14, E17, E5, E6, E12, E10, E16, E18, E19, E9, E21, E20.
As run (`PolPriority`, tests/play/policy.h): E1, E2, S4 (New Earth's
generator), E4, E7, E15, E14, E17, E12, E9 (from 40,000 people), S2, E5,
E6, E10, E16, S12, E18, Apollo's core, E9, E20, Penelope's core, E19, Mir's
core, E21. A goal waiting for money claims it: lower ones spend only what
credits − R − the claims above them leave.

### Act 2: Jaanosians (Jade contact to the Galleon; 13 steps)

Times "as run" are from the proof run (thread F, seed 12345, one game
from "new game": Jade's box 2927-09-08, contact 2928-03-02 15:00, the
Galleon group on 08-05), with a run resumed at E23 (contact 03-01 04:00)
in brackets where it differed. Hours count the game's 30-day months.

**J1. Contact.** Pre: Jade known (E22). Do: `launch(Trade 1, Jade)` (any
non-carrier group, sim.c:1117-1121). Done: box "Your pilots met with a new
alien race in Amnesty | They call themselves 'Jaanosians'" and MSG 5 "The
alien race's language is incomprehensible..." (intel.c:59-66). Record
T(contact). Timeout: 7 days from E22. Target: before 2928-04-15 (the timer
fires 2928-04-23 at the earliest, main_program.c:240). Fail: relaunch;
check pilot hired and Trade 1 has a ship. As run: launched once the status
bar showed 2928-03-01 (the orders pass running then made it 03-02 11:00),
whatever day Jade came. Everything after
the contact is a fixed offset from it (`Intel_FirstContact`: meeting 1
650-719 developer hours, meeting 2 a further 720-769 h, the S.O.S.
2,590-2,807 h after the contact), and before it only the pre-contact
deadline runs (`Race2Timer3`, 6,000-6,399 h from the start, counting
while the Jaanosians are unmet): a contact on the day Jade is found (seed
12345: 2927-09-08) would bring wave 1 with no colony mining (economy
model, scenario B: lost at wave 1). Until the contact the reserve stays at
act 1's 20,000 (nothing is paid unchecked before talk 3). The hauler (S6)
holds Trade 1 from wherever it is: a flight is waited out, a docked group
takes off; Jade's moons are explored on the way (Russel, J11).

**J2. Communicator** (*hard*). Pre: KIT 8 "The Communicator is invented"
(20–39 h after contact). Do: `research(Communicator)` at once, pausing any
other project (1,100/70 ≈ 16 h). Done: ready box. Timeout: T(contact) +
300 h. Must be done before meeting 1 (≥ 650 h after contact, intel.c:67-68);
without it each meeting is replaced by MSG 6 and lost (sim.c:1429-1431).
As run: ready 77 h after the contact.

**J3. Keep the developer home.** From T(contact) until meeting 1: no
university for Dempsey (meeting 1 counts down only while he is present,
sim.c:1425-1427). Keep R ≥ 16,000 from T(contact) until J5 (talk 3's price
is not balance-checked). As run: holds by construction (Dempsey's only act-2
course, J12's, waits for the base's fall; R is 50,000 from the contact);
the step only reads his skills (5/4/4/2) under the research screen.

**J4. Meeting 1** (interrupt I-T2). Pre: screen 34 opens (no box first).
Answer: "We are glad that we could finally decode your transmission..." →
"Could you be more specific ?" → "What can I do ?" → **"O.K."** (always 4
clicks; the 4th menu also offers "It's out of question !", never click it).
Done: reply
"We will hopefully contact you soon with good news." Then MSG 8 "Your
scientist has returned..." 60–89 h later: Trade ship granted (Done on screen
3; sim.c:1433-1442, main_program.c:1529-1536). Timeout for the talk:
T(contact) + 900 h of developer-present time; for MSG 8: 120 h after the talk. As run:
talk 2 at T(contact) + 674 h (675 h), MSG 8 87 h later (88 h). The talk
comes up at any hour, also in the middle of a goal's clicks: every driver
sequence is left there (`pol_try`) and the talk played (`pol_service`; it
is noted as "talk 2", the step waits for that). A step sees such a note
only when the orders pass it came in is over, up to ~16 h later.

**J5. Meeting 2** (I-T3). Pre: MSG 9 " An incoming message from Jaanosians."
(close it), then screen 34. Answer: "Greetings Alien !" → "What is it ?" → **"What is the
price ?"** → **"Yes."** (−16,000). Done: "All right. We are sending it."
(main_program.c:1538-1542). Timeout: T(contact) + 1,700 h. As run: talk 3
at T(contact) + 1,402 h (1,429 h), 728 h after meeting 1.

**J6. Meeting 3, the S.O.S.** (I-T4). Pre: MSG 10 "You receive an S.O.S.
message from the Jaanosians." (close it), then screen 34. Answer: "Greetings Alien." →
"What shall we fight with? We have no weapons!" → **"O.K. We will do our best
to help."** Done: screen 3 shows Hunter and Laser cannon Done; Army appears
in "Change type" (main_program.c:1543-1547). Record T(SOS). Timeout:
T(contact) + 3,200 h. As run: the step waits 1,500 h from meeting 2 and,
from 700 h on (T(contact) + ~2,100 h, ~500 h before the earliest
S.O.S.), claims J7's and J8's 150,600 credits at J8's rank (economy model
"save for J8"). The S.O.S. came at T(contact) + 2,718 h (2,705 h) with
616,000 (627,000) credits: income ran far ahead of the claim, which never
had to hold anything back.

**J7. War commander.** Pre: J6 done; credits ≥ 45,000 + R. Do:
`hire(Kyle Douglas)`, then `university(Kyle Douglas)` when nobody is away.
Done: "I work for you". *Hard* (wave 1 ≥ 800 h after T(SOS); a space attack
needs a fighter, and damage scales with (exp+200)/220, spwar.c:223-226).
As run: hired 18 h after the S.O.S. (category FIGHTERS; the talk came
during a goal's pass, see J4); his course (S12, quoted 22,000-24,000)
2-3 days later.

**J8. First army** (*hard*). Pre: J6. Do: `buy(Hunter, 12)`,
`buy(Laser cannon, 24)` (wave 1 is 6 Hunters + 3 Troopers); `new_group(Army)`
"Home Guard" at New Earth (starts docked); `equip(Home Guard, Hunter, 12)`,
`equip(Home Guard, Laser cannon, 24)`. Gate: 12 × 8,800 = 105,600 credits
and D6,600 E23,340 K5,256 L5,808 R2,808 (R from Apollo/Penelope, S6); 240 h
of Hunter line. Done: grid shows 12 Hunters, 24 lasers. Timeout: T(SOS) +
750 h. **No fallback below 12**: measured 12H 10/10, 10H 10/10, 8H 7/10
([playthrough-battles.md](playthrough-battles.md) §6), and a lost space
battle lands the troopers on a New Earth with no ground units (no tanks
yet): game over. Short money: every credit goes to Hunters and lasers first.
Then keep buying to 20 (S9). As run: all 12 and 24 ordered at once (the
money and S6's Raenium, ~100,000 t of it by then, were there); "Home Guard"
made from the first Hunters out of the shop and filled as they came,
complete 323 h after the S.O.S. (240 h of Hunter line). Docked at New Earth it
fights (a docked Army group counts, sim.c:1287-1291).

**J9. Base falls** (wait). Pre: J6. Wait for MSG 11 "The Morgruls have
attacked and destroyed the Jaanosian base!" + scene 1 (sim.c:1469-1484).
Record T(fall). Timeout: T(SOS) + 300 h. Story stage becomes 2 (developer
caps rise). As run: the fall came during J8 (T(SOS) + 215 h; 245 h); the
step accepts the box seen or, after a resume, the first line in the
message log.

**J10. Radio.** Pre: T(fall). Do: `launch(Trade 1, Jade)`; on arrival
`dock(Trade 1)`. Done: MSG 13 "Whilst searching the ruins of the destroyed
Jaanosian base you find a Sub-Space radio transmitter." (shipmove.c:153-156);
then `research(Radio)` (1,000/70 ≈ 15 h). Timeout: 10 days after T(fall).
Fail: if docking is refused, deploy nothing; retry from orbit/dock after
wave 1 leaves Jade. Radio gates the Phoenix S.O.S. (sim.c:1503), which waits
for it (§5 R4). As run: the arrival at Jade is also the first contact with
the Morgruls, who hold Jade since the fall ("Your pilots met with a new
alien race in Amnesty | They call themselves 'The Morgruls'"): from then on
they are hostile, and wave 1 announces itself with an ATTENTION box. Force
1 sitting at Jade has no mission and left Trade 1 alone; docking gave MSG
13 at once (T(fall) + 116 h: J10 starts after J8), Radio was ready 72 h
later.

**J11. Colony Russel.** Pre: T(fall); a carrier satellite on Russel (Jade
moon) until its icon appears; credits ≥ 167,000 + R. Do:
`found_colony(Russel, {Nuclear Gen., Builder Plant, Mine, Building})`, then
the E18 build-out with a Spaceport and 9 mines. Russel has E90 R80 T40; the base's fall clears the Jaanosians' moons to owner 0 (intel.c:442-467), so the icon can appear.
Timeout: 60 days; not *hard*. As run: the satellite flies on a carrier
group of its own, "Survey 2" (one Satellite carrier and one Satellite,
bought for it): Survey 1's satellite count is E16's progress, and a
satellite put back on it made a resumed E16 fly to Mir again. Russel was
founded 28 days after the fall (25), 9 days after its satellite; from
then on it has the colonies' build-out (E18) and S6/S7.

**J12. Hyperspace drive.** Pre: MSG 30 "Your scientists have found that the
bought item is a hyperspace drive." (300–349 h after the fall, counting only
developer-present hours, sim.c:1489-1493); screen 3 icon stops reading
"Unknown". Before it: `university(Thomas Dempsey, Phisics.& electr.)` (needs
P5; skills 5/6/5/2). Do: `research(Hyperspace drive)` (6,500/70 ≈ 93 h).
Done: ready box + MSG 12. Timeout: T(fall) + 900 h. As run: the course
(77,000) right after the fall, as a goal that waits for MSG 11 (before it
the stage-1 caps make the advisor answer "I don't think I could become
more skilled...", which the policy reads as "has been"); MSG 30 at
T(fall) + 360 h (the course's ~60 h included); the research caption is
"Hyperspace drv", ready 102 h after MSG 30.

**J13. Galleon.** Pre: KIT 15 "The Galleon is invented" (50–119 h after J12;
waits for Trade ship Done, sim.c:1402-1403). Do: `research(Galleon)`
(4,600/70 ≈ 66 h), then `buy(Galleon, 1)` (47,000; D8,750 E12,670 K3,226
L11,000 R1,778 T1,936). `new_group(Trade)` "Galleon 1" with the Galleon,
equip at New Earth. Done: Galleon 1 exists. Timeout: T(fall) + 1,500 h.
As run: KIT 15 110 h after J12, the research 82 h, the Galleon (ores from
S6 in stock) bought at once and built in 60 h; "Galleon 1" docked at New
Earth at T(fall) + 741 h, 2928-08-05 17:00. Act 2 ends here, at frame
1,235,256.

**Wave 1 in act 2.** M1's wave can come before J13 is done: the ATTENTION
box at T(fall) + 666 h (643 h), the attack 20 h (31 h) later (the wave
leaves 600-649 h after the fall from Jade, 20-29 h away). As run: S8 sent
the trade and carrier groups at New Earth to Penelope 2 h after the box;
Home Guard's 12 Hunters (2 lasers each, Kyle after his course) beat the 6
Hunters on 2928-08-03, losing none (0-2 in the other runs; the loss table
counts 5 of the 6, a fifth of the destroyed survive). KIT 13 (Twin laser
gun) came 29 h after the win; S11 was researching it when the act ended
(in other runs it was Done 2.5 days after KIT 13, and KIT 17, Troop
Carrier, followed).

Act 2 money order (`PolPriority`, highest first): E1, E2, S4, J8 (and J6's
saving for J7 and J8 at its rank), J7, the act-1 hard steps, S2's
shortages, the urgent Stadium, J12's course, J13, S2's lead, S6/S7 (droids,
Transfer and Trade ships), the act-1 goals, the colonies in the order
Apollo, Penelope, Mir, then J11 (Russel), E21. As run nothing waited for
money after March: credits rose from 73,000 at the contact to 854,000 at
the end (§5 R8).

### Act 3: Morgruls (18 steps)

Times "as run" are from the proof run (thread F, seed 12345, one game
from "new game": the fall 2928-07-04 20:00, the Galleon 08-05, Mirach 3
taken 2929-04-04 04:00), with runs resumed at J13 in brackets where they
differed. The plan runs M6 and M7 before M5: the Battle tank they unlock
is the act's gate (§5 R8). One goal, `M8_army` (tests/play/army.c), runs
the army from M2 to M18: it keeps every line the target force needs
busy (each product queued about three days deep, tanks first when an
ore runs short) and fills Home Guard, docked at New Earth, from the
stores; the target grows with the step (M8, M12, M18). A pass of all
the goals takes ~3 game days of clicks by act 3 (every screen switch
runs the clock), so nothing in act 3 may rely on a goal's reaction
within a day. Act 4 changed three things the earlier acts use: a story
scene leaves the driver sequence it came into (§3 General handling), M5
keeps Galleon 1 in flight over midnight, M11 checks that its zoom shows
Phoenix 5. That re-recorded the proof run from the fall (J9) on: there
its dates and losses differ a little from those given in acts 2 and 3
(Mirach 3 taken 2929-04-01 01:00, losing 24H 23SF 3D in space and 14T on
the ground).

**M1. Wave 1.** Pre: "ATTENTION! Enemy fleet approaching New Earth" or "New
alien race's ships arrived at your colony in Amnesty | They call themselves
'The Morgruls'" (sim.c:1234, 1255). Do: S8 evacuation, then let the battle
run (I-B29). Done: `war\spvict` and END BATTLE. Timeout: T(fall) + 750 h
(wave launches 600–649 h after the fall and travels 20–29 h). Fail: §5 R9.
As run: won in act 2 (2928-08-03); KIT 13 (Twin laser gun) 20–39 h after
the first win tells a resumed step.

**M2. Twin lasers and Troopers.** Pre: KIT 13 (20–39 h after the first
win), KIT 17 (90–139 h after the first space win). Do: `research(Twin laser
gun)`; `buy(Twin laser gun, 1 per Hunter)`; refit every Hunter.
`research(Trooper)`. Done: Hunters at 2L+1T. As run: Twin laser gun
takes 3 h of research (200 / 70), ready 2928-08-05; the step ends there
and the army goal fits the twins as the line makes them (13 h each:
waiting for the refit held M3 back 24 days in one run). Trooper comes
through S11 (KIT 17 08-07, ready 08-12) and is never bought (M18 needs
no Troopers).

**M3. Explore Phoenix.** Pre: Galleon 1 (J13), Jack hired. Do:
`launch(Galleon 1, Phoenix sun)`. Done: " Your ships explored Phoenix system
| They found new planets" + rename prompts (accept each) (sim.c:1068-1084).
First jump plays MSG 34 + scene 2 (sim.c:1011-1015). Timeout: 10 days.
As run: arriving at the sun the group is put at the last planet found,
Phoenix 3 (sim.c:1068-1084), so the explore box, the Kall contact, MSG 15
and talk 5 came together (2928-08-06 08:00); M4 then had nothing left to do.

**M4. Kalls.** Do: `launch(Galleon 1, Phoenix 3)`. Done: first-contact box
"'The Kalls'", MSG 15, talk 5 (I-T5): "Greetings Alien." → **"That is
right."** → **"When are we receiving your weapons?"** Starfighter becomes
analysable and 10 Starfighters appear in stock (main_program.c:1548-1551).
Record T(Kall). Timeout: 10 days. As run: the step starts Starfighter at
once (S11 does not stop a running Trooper); ready 2928-08-11.

**M5. Find Phoenix 1 early.** Pre: M4; Phoenix S.O.S. (MSG 17) **not yet
seen**. Do: shuttle Galleon 1 Phoenix 3 ↔ Phoenix 2 until " Your pilots
found a new planet in Phoenix system" names Phoenix 1 (status −2, 1/5 per
transit day; Phoenix 1 is checked first, sim.c:781-805). Never select
Phoenix 1 after MSG 17 has appeared until M11. Timeout: 30 days. Fail:
continue; M11 then needs this shuttle done before it (after MSG 17, shuttle
without selecting Phoenix 1; the find box does not select it). As run: each
hidden planet rolls 1 in 5 on its own each midnight, the first that hits is
found, so Phoenix 4 can come first (it did in two runs); the finds come only
at hour 0 (sim.c, Sim_ProcessAllBodyEvents before the groups move). With a
pass of the goals taking ~3 days, launches made by the step at a timed hour
covered one midnight in four and missed Phoenix 1 for 30 days in one run;
now Galleon 1 is sent off again whenever it is found in orbit between two
goals (`pol_between_goals`), and is in flight at most midnights. Phoenix 1
2928-08-14 [08-17], 3 days after the step started. Run after M7. Sent off
only on landing it was in flight at one midnight in three or four, and
found nothing in 30 days in a run resumed at E16; so from 18:00 on the
status bar's clock it is kept in flight until the clock turns to 0:00
(sent off again on each landing, its group screen shown).

**M6. Phelonians.** Pre: Phoenix explored; Phelonians unmet; Antares not yet
explored (the Eran item hides this tip). Do: `pub_ask(Informer, "Do you have
any information?" → "All right, talk.")` (−500). Done: the reply gives the
coordinates and Phoenix 5 is drawn on the Phoenix map; there is no rename
prompt (it sets the planet status directly, main_program.c:1059-1072). Then
before arrival at Phoenix 5, spend New Earth's Energon on queued orders (the
deal halves the stock, main_program.c:1561-1573). Do: `launch(Galleon 1,
Phoenix 5)`. Done: first contact "'Phelonians'". Timeout: 15 days. As run:
the Informer (−500) said "A strong and powerful race lives in the second
solar system. The co-ordinates of their planet are: 0142:3400:2310"; contact
2928-08-09 03:00. New Earth's Energon was not spent first: its halving (107,000 t to
41,000 t) cost nothing that mattered, as the hauler brought Russel's and
Mir's.

**M7. Battle tank deal.** Pre: talk 7 (20–39 h after M6 contact, no box
first, sim.c:1538-1541). Answer: "Greetings Phelonians." → "What will you
give in return?" → **"I think something more valuable could be offered. A
new invention could be the base of our deal."** → **"It's a pleasure doing
business with you."** Done: Battle tank Done on screen 3. Timeout: 60 h
after contact. Fail: none (the talk is one-shot). "It's a pleasure doing
business with you." is also offered in the menu right after "What will you
give in return?", where it sells up to 10,000 t of Energon for credits
instead: click the rows in this order. As run: talk 7 2928-08-10 08:00
(T(Phelonians) + 29 h); the tank line started the next pass.

**M8. Wave 2 force** **[plan]**. Pre: M2, M4, M7. Do:
`research(Starfighter)` (3,490/70 ≈ 50 h). Build Home Guard to **24 Hunters
(2L+1T), 16 Starfighters (4 Twin), and a ground reserve of 20 Battle tanks
(3L+1T)** in New Earth's planet force (wave 2 is 25H + 10Tr, a lost space
fight leads to a ground fight at New Earth that must not be lost). Gate
(from zero): 24 × 11,450 + 6 × 19,000 (10 SF are free; 4 Twin each) + 16 × 4
× 2,650 (twins for the free ones are included) + 20 × 13,300 ≈ 0.75 M
credits; Energon ≈ 70k t. Fallback: if by T(fall) + 1,600 h the target is
not met, put every credit into Battle tanks first (ground loss = game over),
then Hunters. Planning from act 2 as run (economy model §4): money is no
gate (~850,000 credits when act 2 ended, income ~25,000 a day), the Twin
laser line is. Twin laser gun comes ~30 h after wave 1 (KIT 13, then ~60 h
of research: Done about T(fall) + 760 h), so by the earliest wave 2
(T(fall) + 1,825 h) the line gives ~1,060 h, 81 twins at 13 h each; the loadouts
above want 108 (24 Hunters, 16 × 4 Starfighters, 20 tanks). Order twins the
hour the research is done, the Hunters' first, then the Starfighters', and
give the M8 tanks 3 Lasers (their twin comes for M12); Hunters (20 h) and
Battle tanks (50 h, 1,000 h for 20 from M7) build on their own lines
meanwhile. As run: the ground reserve cannot sit in the shop's stores, which
no battle counts (grwarunits.c: only Army groups' second unit row and planet
forces at the body): the tanks ride in Home Guard's ground row, docked at
New Earth (a docked Army group defends, and after a lost space battle its
units join the planet force). The step waits for the force or the wave's
ATTENTION box: at the box Home Guard had 24 Hunters (2L+1T), 16 Starfighters
(4T) and 19 tanks (3L). Starfighter research was ready on 08-11 and the 10
free ones went in at once.

**M9. Wave 2.** Pre: ATTENTION box for The Morgruls (wave launched
1,800–1,999 h after the fall; travel 20–29 h inside a system, 50–69 h
between systems, intel.c:385-400). Do: S8; battle. Done: MSG 16 "The Morgrul
invasion forces are defeated!" + scene 3 (stage 3, Tractor beam idea;
main_program.c:1253-1259). Record T(wave2). Timeout: T(fall) + 2,100 h. As
run: ATTENTION 2928-09-29 01:00 (T(fall) + 2,045 h), attack 6 h later: 40
own ships against 25, won, lost 2H 2SF [4H 0SF] (the 20 Hunters destroyed
are the table's; 1/5 survive); the Tractor Beam idea 3 h later tells a
resumed step.

**M10. Stage-3 course.** Pre: stage 3 (M9). Do: `university(Thomas Dempsey,
Artificial int.)` (A2 → 4, for Spy satellite and Missile). Price
(Random(10) + experience) × 1,000, about 70,000–79,000 once Dempsey is at his cap of 70
(advisor.c:255); gate on credits ≥ price + R. Timeout: none. As run:
Artificial int. needs no stage 3. The developer's caps by story stage
(Staff::DevSkills, DS:57EB; Dempsey) are 6/7/7/5 at stage 2 and 7/7/7/7 at
stage 3, and a course adds Maths & physics. +2/+1/0/0, Phisics.& electr.
0/+2/+1/0, Electr and maths. +1/0/+2/0, Artificial int. 0/0/0/+2 (DS:0125):
so Artificial int. (A4: Spy satellite, Missile) and Electr and maths. (M6
E7: Destroyer) go at stage 2, as soon as Twin laser gun and Starfighter are
done (research stops while he is away), and only Maths & physics. (M7: Space
station) waits for stage 3. Goal `M10_courses`: Artificial int. on
2928-08-16 (77,000), Electr and maths. on 08-20 (79,000), Maths & physics.
on 10-03, after wave 2 (73,000).

**M11. Drifting Morgrul ship** (*ordering constraint*). Pre: MSG 17 (Phoenix
S.O.S.) seen **and** "Kall Fleet 2" listed in the Phoenix 5 star-map zoom
(§4 S-K), checked every 7 days; or, if the zoom is not readable, date ≥
T(Kall) + 2,100 h (fallback proxy, §4). Do: `launch(Galleon 1, Phoenix 1)`
(from a body in Phoenix; this leaves Phoenix 1 selected, launch.c:56-59) and
keep the map on Phoenix 1 until arrival. Done: MSG 18 "You have found a ship
drifting in space..." then talk 6 (I-T6): **"Tell me, what are your
plans?"** → "You will send a message to your leaders..." → **"All right."**
(adds 250 h to wave 3 if not yet sent; main_program.c:1552-1560). Then MSG
48 and `select_amnesty()`. Timeout: T(wave2) + 1,000 h. Fail: if MSG 18 has
not come 10 h after arrival, the S.O.S. was not received (Radio, J10). As
run: MSG 17 2928-10-01 01:00 (wave 2 left ~09-28); the Phoenix 5 zoom lists
the fleet as "Kall Fleet 2" (checked daily); seen 2928-10-29 03:00 [10-18]
(T(Kall) + 1,963 h, inside the code's 1,500–1,999 h + 50–69 h of travel).
Galleon 1 flew from Phoenix 3 or 5 straight to Phoenix 1 and the step waited
on its group screen (which selects the group's target) with no orders pass:
MSG 18, talk 6 (scene 10) and MSG 48 3 h after the launch. "All right." came
before wave 3 left (wave 2 + 1,100–1,149 h), so wave 3 was 250 h later.
After talk 6 the Kalls' fleet 2 (15H 10SF, 10Tr 35T) sits at Mirach 3 as an
ally (status 6), and allied forces at the battle body fight on the player's
side in space and on the ground (spwar.c, grwarunits.c): it joined the M18
invasion. The planets move on at each game hour, so the click that zooms
in on Phoenix 5 can zoom in on another one: in one run it zoomed in on
Phoenix 3, the Kalls' home, where fleet 2 is listed before it leaves;
talk 6 came before their timer had moved the fleet, the timer then
started their turn (4,000 h), and MSG 24 and a Kall attack on New Earth
came in act 4 (2929-04-15, beaten by Strike). The check now also needs
the zoom to list Phoenix 5 and not Phoenix 3.

**M12. Wave 3.** Pre: ATTENTION for The Morgruls (1,100–1,149 h after wave
2, +250 h). Force **[plan]**: 30 Hunters, 24 Starfighters, ground reserve 30
Battle tanks (wave 3 is 20H 10SF 15Tr 2T). Done: MSG 19 "The second Morgrul
invasion has failed!" (space) or MSG 16 (ground) + scene 3; Spy present in
the pub (main_program.c:1260-1268, 1467-1475). Timeout: T(wave2) + 1,500 h.
As run: ATTENTION 2928-11-26 02:00, attack 11 h later: 54 own ships (30H
24SF) against 30, won, lost 3H 2SF [7H 3SF]. MSG 20 ("Your Spy reports that
a ship can be found near to Mirach system's second planet") comes 50–99 h
after wave 3 leaves whether or not the Spy is hired (sim.c:1504-1510).

**M13. Spy and Mirach.** Pre: M12; credits ≥ 30,000 + R. Do:
`pub_ask(Bartender, "Who is that suspicious character?" → "How dangerous is
he?" → "I think I will speak to him.")`;
`pub_ask(Spy, "Would you work for me?" → "Will you do the job for 30000?" →
"Do you know anything important?")` (−30,000, unchecked). Done: Mirach
appears in the star-map system list (no box, main_program.c:1028). Timeout:
3 days. Fail: Spy-Hunter route (§3 P-SH): Mirach after 200 h, MSG 37.
As run: after the bartender's path the pub captions the Stranger "Spy";
he answered "Yes, the Morgruls are in the third solar system, the
co-ordinates are: 176;34;963." the day after wave 3.

**M14. Spy satellite.** Pre: KIT 20 (30–39 h after M12). Do:
`research(Spy satellite)` (needs A4: M10). Then `buy(Spy satellite, 1)`,
`buy(Satellite carrier, 1)`, carrier group "Spy 1". As run: KIT 20 a
day after wave 3, ready 3 days later (A4 since September); "Spy 1"
docked at New Earth until M17.

**M15. Explorer 1.** Do: `launch(Galleon 1, Mirach sun)` → rename prompts →
`launch(Galleon 1, Mirach 2)` (its moons are listed only after a group has
arrived there) → `launch(Galleon 1, Mirach 2 Moon 1)`, `dock`. Done: MSG 21
"You have found Explorer 1!" + scene 4 with MSG 36; Missile and Destroyer
analysable (shipmove.c:158-163). Timeout: 15 days. As run: the destination
map names moons "Moon 1", "Moon 2"; arriving at Mirach's sun put Galleon 1
at Mirach 3 (the Morgrul home: nothing attacked a trade group there);
Explorer 1 2928-12-05 15:00.

**M16. Missile, Destroyer, Space station.** Do: `university(Thomas
Dempsey, Electr and maths.)` (M6 E7) then `research(Missile)` (3/3/3/3),
`research(Destroyer)` (6/6/5/0; 9,500/70 ≈ 136 h; MSG 22 on completion);
KIT 24 50–119 h later; `university(Thomas Dempsey, Maths & physics.)`
(M7, stage ≥ 3); `research(Space station)` (7/5/7/0). Buy **2 Space
stations** **[plan]** (28,500 + D60,000 E48,000 K6,590 L2,585 R2,000 T3,270
each; Detoxin is the gate: build Derricks toward 40, S4), then Destroyers two
at a time (each product's queue is capped at the number of stations,
shop.c:280-286; units still build one at a time) to **5**. Timeout: 120 days.
Fallback: 1 station, 3 Destroyers. As run: **one** station: the cap only
limits how many are queued, and the line builds one at a time either
way, so the army goal queues the next Destroyer when one is done.
Missile ready 12-07, Destroyer 12-14 (with MSG 22 "It is
impossible to build destroyers on a planet or moon"), KIT 24 4 days
later, Space station 12-21; the fifth Destroyer 2929-01-21. Detoxin was no
gate (New Earth kept 80,000–140,000 t with E21's 20 Derricks), so no
Derricks beyond 20. Other seeds (thread H): with fewer Derricks the army's
lines took New Earth's Detoxin as it came in and the station's 60,000 t
never gathered (seed 3); the army goal now keeps the station's ores from
its lines while M16 waits for it (army.c), which did not change seed
12345's run. Seed 3 still fails here: New Earth stopped at 5 Derricks and
brought in ~270 t of Detoxin a day.

**M17. Spy satellite on Mirach 3.** Do: `launch(Spy 1, Mirach 3)`,
`deploy_satellite(spy, Mirach 3)`. Done: box " Your satellite found The
Morgruls colony at | Mirach 3" (intel 40, sim.c:682-689). Timeout: 20 days.
As run: ADD SPY SAT on the group's control panel; "alien activity" after
6 days, the colony box 2929-01-29 (8 days after the launch).

**M18. Invade Mirach 3.** Pre: M17 done; Kyle hired; strike group "Strike"
(Army, ≥ 1 Destroyer, Jack can reach Mirach) **[plan]**: 20 Hunters, 30
Starfighters (4T+1M), 5 Destroyers (space 30/30), and **≥ 90 Battle tanks**
in the group (Army groups carry their ground units into an invasion).
Measured on the ground against 61Tr 40T: 60T 6/10, 40T 4/10, 75T 9/10, 90T
20/20 (loses ~29T); 60T + 30 Troopers 9/10
([playthrough-battles.md](playthrough-battles.md) §6). Aircraft (30T + 10Air
10/10) come only in act 4. Fallback, if the tank line is the gate: 60T +
30Tr (Troopers build on their own line from M2). With S9's 20 tanks at home
this is 110 tanks, 5,500 h of tank line from M7: keep the line busy from M7
on. Morgrul waves 1–3 are spent, so New Earth may be left with planet-force
tanks only. Do: `launch(Strike, Mirach 3)`; `invade(Strike)`. Battle per
I-B29/I-B19/I-B32. Done: " Your forces have eliminated 'The Morgruls' " +
MSG 35 + scene 3 (main_program.c:1287-1311). Timeout: 60 days. Fail: §5 R9;
rebuild and retry; wave 4 is ≥ 8,000 h after wave 3. As run: the 60 days are
the policy's own number; no code timer starts with the invasion. The real
deadline is Morgrul wave 4 (force 6: 20H 30SF 2D, 40Tr 40T, to New Earth),
launched 8,000–8,499 h after wave 3 left (Intel_LaunchTimer,
sim.c:1504-1510): the step waits up to 200 days for the force. Strike is
Home Guard renamed (waves 1–3 spent, wave 4 a year away, so New Earth keeps
no army meanwhile; S9's home floor is not applied in M18). As the tank line
set the date (90 tanks done in March; the Twin line, 302 twins for this
force, came last, 2929-04-03), the ship target went up to 40H 40SF 6D: the
waves' survivors had gone home to Mirach 3, and the enemy fleet was 55
ships, not the measured 20H 20SF 3D, and 89Tr 42T on the ground, not 61Tr
40T (30H 30SF 5D with the Kalls won there, losing 31H 19SF 3D). Proof run:
GROUND WAR 2929-04-04 04:00, 111 own ships (40H 40SF 6D and the Kalls' 15H
10SF) against 55, won, lost 28H 20SF 3D (Kalls' included); ground 10Tr 125T
(90 ours, the Kalls' 10Tr 35T) against 89Tr 42T, won, lost 38T [14-69T in
other runs].

### Act 4: The League (13 steps)

Times "as run" are from the proof run (thread F, seed 12345, one game
from "new game": Mirach 3 taken 2929-04-01 01:00, the League's surrender
2929-10-01 03:00), with runs resumed at L01 in brackets where they
differed. The army goal moved to `tests/play/army.c`; in act 4 it fills
two Army groups docked at New Earth, Strike (the Mirach force, brought
home, L7) and Home Guard (the ground reserve while Strike is away, S9),
tier by tier: a tier's units only once every group holds the tier before
with its weapons (more units would take weapons the tier before still
lacks, S10). Units a group holds beyond its target go back to the stores
for the other group (Strike came home from Mirach with 79 tanks, Home
Guard had none). The Twin laser line is the act's gate: Aircraft carry
4 Twins each, so the line is queued 10 deep from act 4 on, and the
invasions waited for it, and for the tank line (Home Guard's 40 tanks,
queued 3 deep) when Strike brought fewer home (§5 R8). Each home world taken becomes a colony
of the player (`Intel_ClaimBodyForPlayer`: Mirach 3 in act 3, then
Orionis 3, Lyrae 5, Lyrae 7), with its own planet force: the ship-info
screen then shows the planet-forces list, where NEW UNIT is missing, so
the group drivers switch back first ("Change mode"). The policy leaves
these colonies to themselves (their disease and meteor boxes are only
closed). The box reader skips the pixels under the mouse cursor, so a
partly hidden glyph can read as another ("Superhova" for MSG 25): act
4's waits match boxes allowing one character in eight to differ
(`pol_seen_like`), or read the message log.

**L1. Pilot.** Pre: credits ≥ 120,000 + R. Do: `hire(Julie-Anne Roxford)`;
`university(Julie-Anne Roxford)`. As run: hired at the step's start
(2929-04-01), replacing Jack Benson; her course the same day (42,000):
experience 90, so a hop to a moon of the same planet (80-99 legs) ends
within the hour the control panel runs after the launch (the launch
driver takes a changed location for a launch). The goals of the men
replaced (Jack's course, Dempsey's: E12, J12, M10) end once their hotspot
is gone from the command centre.

**L2. Developer.** Pre: credits ≥ 250,000 + R. Do: `hire(Klaatoo)`. Then
courses as needed (each ~(90 + 0..9) × 1,000): Phisics.& electr. (Plasma
P8), Electr and maths., Maths & physics. (Mental radar M8 E8), Artificial
int. ×2 (A9). Keep one course in progress whenever Klaatoo is idle and
credits allow. As run: hired 2929-04-01, replacing Dempsey (7/7/7/4).
Klaatoo starts at 5/7/6/5 and experience 40, below Aircraft's M6 (L11),
so his courses come first: Maths & physics. (43,000; 7/8/6/5), Electr and
maths. (99,000; 8/8/8/5), Artificial int. twice (91,000, 95,000): 8/8/8/9
by 2929-04-23, one at a time (goal `L1_courses`, none while Aircraft
waits for him), Julie's first. Phisics.& electr. is not needed: Maths &
physics. already gives P8.

**L3. Pirate Leader (optional).** Pre: "Morgrul" person in the pub; credits ≥
500 + R. Do: `pub_ask(Informer, "Do you know anything about the Morgrul in
Space Local?" → "All right, talk.")`. This also unmasks the bartender's League
tip (main_program.c:979-982). Optionally `pub_ask(Pirate Leader, "I heard
that you accept dirty jobs for good money.")` and take deals with "You'll get
half of the cargo." (I-P9). As run: the Informer (500) said "He accepts
any work, and would do anything for money."; the pub then captions the
Morgrul "Pirate Leader". No deals (§5 R11).

**L4. League tip** (*hard*, before the Lisonian wave launches). Do: buy
`Satellite carrier` ×1, group "Scout"; `launch(Scout, Orionis sun)`
(carriers make no contact, sim.c:1117/1121). Done: " Your ships explored
Orionis system". Then `pub_ask(Bartender, "Have you heard about the league?"
→ "Tell it!")` (−5,000). Done: reply text. Timeout: 3 days after the explore
box. Fail: if the bartender shows the Morgrul menu, ask "What is that
Morgrul doing in the bar?" once, then retry. As run: not a new group but
Spy 1 (M17's carrier, its spy satellite left on Mirach 3), flown home,
docked and refitted to three carriers with a Spy satellite each for L8:
on its way to Orionis 3 it explored Orionis (2929-04-13 [04-11]); the
explore box puts the group at the last planet found, Orionis 3, but the
contact test skips carriers. The bartender (5,000) answered "The League
doesn't like the fact that you are developing so quickly..." (04-15).

**L5. Antares colony.** Do: `launch(Galleon 1, Antares sun)` (Julie);
`found_colony(Antares 3, {Nuclear Gen.})` (William only, 135,000);
`build(Antares 3, Observatory, 1)` (42,000). Done: MSG 25 nova warning (50 h
after the Observatory completes, sim.c:1550-1556). Record T(nova). Timeout:
20 days after founding. As run: the COLONIZATION icon needs intel 30 on
Antares 3 (satellite.c:305-310), so a satellite first: "Scout", a carrier
with a Satellite, explored Antares (04-06; the Solar plant idea), put the
satellite on Antares 3 (intel 10 "ready for mining" 04-14, 30 "a
suitable place for living" 04-19) and went home. Colony with only the
Nuclear Gen. 04-20 (box at midnight 04-21), Observatory 04-25, MSG 25
04-27 21:00 [04-26 07:00]: the 50 h count, armed by exploring Antares
(sim.c:1088), runs only while a finished observatory stands there.

**L6. Erans, then leave Antares** (*ordering constraint*). Pre: T(nova). Do:
`launch(Galleon 1, Antares 3)`. Done: MSG 45 then talk 9: "Greetings
Erans..." → "Yes, we are sure..." → **"Yes ! My people will welcome you."**,
MSG 46. Record T(Eran). Then immediately `launch` every group in Antares to
Amnesty; never send a group to Antares again. Done: no group panel reads
Antares. Hard deadline: T(Eran) + 400 h (the supernova comes 560–569 h after
the talk, sim.c:1142-1144, and deletes everything in Antares). As run:
Galleon 1 (from Mirach 2 Moon 1) arrived 2929-04-28 05:00 [04-26 15:00]:
MSG 45, talk 9, MSG 46; it left two hours later. The supernova came
2929-05-21 16:00 [05-20 07:00], 563 h [568 h] after the talk, with scene 7; like
every story scene it ends on the command centre, and a driver sequence it
came into (it came into shop and colony-surface sequences) is left, as
for a talk, and runs again later.

**L7. Home guard before League contact** **[plan]**. Pre: L5 started. Build
Home Guard to **30 Hunters, 30 Starfighters, 5 Destroyers** plus a planet-force
ground reserve of **40 Battle tanks** (Lisonian wave: 20H 20SF 3D 30Tr 15T
5Air). Done: grid counts. Timeout: 120 days. Gate before L9. **Winning in
space is the only defence:** the space target won 10/10 (losing ~11H 5SF),
but the 40-tank reserve loses the landing 0/10 (the 5 aircraft stun-lock the
tanks; 40T + 10Air wins 10/10), and aircraft only arrive with L11, after this
wave. Add Starfighters beyond 30 if affordable; keep the tanks for L13.
As run: the army is Strike, docked at New Earth, with the act-4 target
(army.c): 40H 40SF 6D (won 10/10 without Plasma, losing ~9H 2SF) as the
step's gate, then 50H 50SF 8D; Home Guard took the tanks Strike had
beyond its 30. Strike came home from Mirach 3 at once (goal
`L7_strike_home`) and held 50H 48SF 8D 40T when the step began (04-28):
the act-3 lines had kept running.

**L8. Spy satellites on the League.** Do: 4 Spy satellites on 4 carriers;
`launch` and `deploy_satellite(spy, ...)` on Orionis 3, Lyrae 3, Lyrae 5,
Lyrae 7 (explore Lyrae first). Done: each body lists "Spy satellite"
(intel stalls at 31 until contact, sim.c:648-649). As run: three, on
Spy 1 (L4): Lyrae 3 is the Undorlings', allied after L10 and never
invaded. ADD SPY SAT on the group's control panel: Orionis 3 04-17,
Lyrae (explored on the way) 5 04-29, Lyrae 7 05-07; each stopped at
"alien activity" until the contact.

**L9. League contact.** Pre: L4, L7. Do: `launch(Galleon 1, Lyrae 7)`, then
Lyrae 3, Lyrae 5, Orionis 3. Done: first-contact boxes for The Lisonians,
Undorlings, The Hirachi, The Druedians, and MSG 28 "The League declares war
against you" on the first (intel.c:86-92). Record T(league). As run: one
trip, each arrival waited for: Lyrae 7 2929-05-14 09:00 ('The
Lisonians' and MSG 28 together) [05-06 02:00], Lyrae 3, Lyrae 5, Orionis
3 05-14 20:00; then home. The " Your satellite found <race> colony at"
boxes came 3-5 days later.

**L10. Eran's job.** Pre: L4 bought, Lisonians and Undorlings met. Do:
`pub_ask(Eran, "I have saved your race..." → "As I understand,your kind can
metamorph..." → "If any race joins our side..." → "Undorlings.")`. Done: 100–199
h later MSG 40 then MSG 44 (Undorlings allied, Ion cannon granted, Aircraft
analysable; sim.c:217-231). Timeout: 250 h. Fail: if he answers "I'm just
having a drink, that's all." a precondition is missing (tip or a contact).
As run: asked 05-14 23:00 ("All right,I'll give it a try."; while his job
runs the pub does not draw him); MSG 40 and 44 05-20 15:00 (136 h)
[120 h].

**L11. Aircraft.** Do: `research(Aircraft)` (6/7/5/1); buy **20**, fully
equipped (essential for Lyrae 7, L13). Timeout: 60 days. As run: 2,500
/ 90+ ≈ 28 h of research, ready 05-22 19:00; the army goal builds them
(35 h each on their line, 4 Twins and a Missile each): 40 by Lyrae 7
(Strike 30, Home Guard 10) and 7 more for those lost at Orionis 3 and
Lyrae 5; 50 were bought in the act.

**L12. Lisonian wave.** Pre: ATTENTION for The Lisonians (200–249 h after
T(league)). Done: space win. Timeout: T(league) + 400 h. As run:
ATTENTION 2929-05-27 02:00 (T(league) + 305 h), attack 21 h later: 108
own ships (Strike 50H 50SF 8D, no Plasma) against 43, won, lost 3H 1SF
[3H 1SF]; the trade and carrier groups went to Penelope (S8). The
aircraft came after it, so the space win was the defence.

**L13. Invade Orionis 3, Lyrae 5, Lyrae 7.** For each target: wait for its
"Your satellite found ... colony at" box, then `invade(Strike)`. Strike
**[plan]**: Orionis 3 (30H 23SF 3D; 90Tr 35T) and Lyrae 5 (30H 25SF 3D; 52Tr
10T): 30 Hunters, 30 SF, 5 D, 60 tanks, 20 Aircraft. Lyrae 7 last (40H 30SF
2D 1C; 70Tr 40T 10Air): 50 Hunters, 50 SF, 8 D, 60 tanks, 20 Aircraft.
Measured: all three 10/10 in space and on the ground with these forces; the
**20 Aircraft are essential** for Lyrae 7 (60T 10Air 4/10, 40T 20Air 10/10).
Rebuild to target between attacks. Never invade Lyrae 3 (allied). Done (last one):
MSG 32 "The races of the League surrender!" + scene 5; Cruiser Done; Rigel in
the system list (main_program.c:1319-1333). Timeout: 120 days per target.
As run: the forces measured without Plasma (Destroyers get it in act 5;
[playthrough-battles.md](playthrough-battles.md) §6): Orionis 3 and
Lyrae 5 with 40H 40SF 6D, 30T 20Air (40T 10Air lost 3/20 at Orionis);
Lyrae 7 with 50H 50SF 8D, 30T **30Air**: the Lisonian wave, beaten in
space at New Earth, took the fifth of its ships that survived and all
its troops home, so Lyrae 7 had 81 ships and 100Tr 55T 15Air, against
which 40T 20Air won 11/20 and 30T or 40T with 30Air 20/20 and 30/30. Home Guard kept 40T 10Air
at New Earth while Strike was away (S9). Each target: Strike waits
docked for its force, flies out, GROUND WAR as soon as it is offered
(the intel boxes had come long before), and comes home to refit. Strike
kept the 40 tanks it had left after giving 39 to Home Guard. Orionis 3
2929-07-29 02:00 (space 108 against 56, lost 9H 5SF; ground 40T 20Air
against 90Tr 35T, lost 6Air), Lyrae 5 08-10 18:00 (107 against 42, lost
7H 2SF; ground lost 1Air), Lyrae 7 10-01 03:00 (108 against 81, lost 17H
4SF by the grids; ground 40T 30Air against 100Tr 55T 15Air, lost 1T
12Air), MSG 32 at once [07-19, 07-29, 09-21]. The Twin line set the
dates: the aircraft for Strike and Home Guard and the twins of the ships
lost at Orionis 3 and Lyrae 5 took it to late September. Strike then
held 33H 46SF 8D 39T 18Air, Home Guard 40T 10Air.

### Act 5: Earth (9 steps)

Times "as run" are from the proof run (thread F, seed 12345, one game
from "new game": the League's surrender 2929-10-01 03:00, act 5 from
2929-10-04 20:00, Earth taken 2930-03-05 01:00), with runs resumed at
X01 in brackets where they differed. The army goal (`tests/play/army.c`)
fills Home Guard first and then Strike, one tier each, both docked at New
Earth: Home Guard 40T 10Air 10ML (X6), Strike 60H 60SF 12D 10C, 40T
20Air 40ML (X3); the Destroyers take a Plasma gun each, the Cruisers 6
Missiles and 3 Plasma guns, the launchers 5 Missiles. The Missile line
was the act's gate (~340 missiles of 10 h: 250 for the launchers, 60 for
the Cruisers), queued 16 deep from the act's start, before the launchers
were even researched; the Cruiser line (one queued per Space station, 200
h each) ended a month before it.

**X1. Rigel 1.** Do: `launch(Galleon 1, Rigel sun)`, then `launch(Galleon 1,
Rigel 1)`, `dock`. Done: MSG 31 + scene 6; Plasma gun and Missile launcher
analysable; KIT 34 ("The Psy-Radar is invented") 100–119 h later, then
Mental radar analysable (shipmove.c:164-169, sim.c:1406). As run:
Galleon 1 explored Rigel 2929-10-05 08:00 (the rename prompt for Rigel 1),
docked, MSG 31 at 10:00. The doc's "keep a group in Rigel (the system
must stay explored)" is not needed: a system's explored flag (DS:4815+s)
is only ever set (sim.c:1069-1077); Galleon 1 stayed docked at Rigel 1
anyway until X4.

**X2. Research.** Do (Klaatoo): `research(Missile launcher)`,
`research(Plasma gun)`, `research(Mental radar)` (10,000/90 ≈ 111 h).
Refit Destroyers with Plasma (the army goal). As run: Missile launcher
ready 10-06 00:00, KIT 34 10-09 18:00, Plasma gun 10-11 19:00 [10-10],
Mental radar 10-16 14:00 [10-16 08:00] (its box reads "The Mental radar is ready for
production").

**X3. Cruisers and launchers**. Do: buy **≥ 10 Cruisers** (373,000 equipped
each; the queue is capped at the number of stations and they build one at a
time, 200 h each: 2,000 h of Cruiser line from L13) and **50 Missile
launchers** (41,000 each). Earth's space fleet (80H 80SF 7D 5C) beats 6 and 8
Cruisers 0/10 and loses to 10 Cruisers 20/20 (the strike loses ~40H 39SF 4D)
([playthrough-battles.md](playthrough-battles.md) §6): **no Cruiser
fallback**. Final strike target **[plan]**: 60H 60SF 12D 10C (10/10, lost
~48H 40SF 0-2D; 50H 50SF 10D 10C 10/10 but lost up to 8D and 1C); ground
40T 20Air 40ML (20/20, lost 10T 20Air 30ML). The guide's 60T 20Air 50ML
also wins, but 40 launchers are the least that wins every time (40T
20Air 30ML 9/10, 40T 30Air 30ML 6/10) and every launcher costs the gate
line 5 missiles. As run: the step only checks that the Cruiser line runs
(10-16); one Space station, no more: the Missile line, not the Cruiser
line, set the date (10 Cruisers on 2930-02-02, 40 launchers on 01-28,
their 200 missiles on 02-30).

**X4. Rigel 2.** Pre: Mental radar Done. Wait for " Your Psy-radar radar found
a new planet in Rigel system" (1/10 per day, sim.c:807-815);
`name_prompt_accept()`. Timeout: 60 days. Then `launch(Galleon 1, Rigel 2)`.
Done: the first-contact box ('The Syonians'), MSG 29 + talk 10: "Greetings
Syonians !" → ... → **"Thank you for your confidence in us..."** (only one
option per state). Sun in the system list; Bounty-Hunter in the pub
(main_program.c:1579-1583). As run: Rigel 2 found 10-18 00:00 [10-20]
(the prompt names it "Rigel 2"), seen by the step at 10-20 02:00 (an
orders pass in between), talk 10 at 10-20 08:00 [10-20 05:00].

**X5. Sun.** Do: `launch(Scout, Sun sun)` (carrier: no contact). Rename
prompts. Then a carrier with a Spy satellite to Earth (Sun 3),
`deploy_satellite(spy, Earth)`. Done: "Spy satellite" listed; intel stalls
at 31. As run: one trip of Scout (L5's carrier) from its dock at New
Earth with a Spy satellite (goal `X5_spy_on_earth`): the explore box 11-03
15:00 [11-02] (prompts Mercury, Venus, Earth, Mars, Jupiter, Saturn,
Uranus, ...), ADD SPY SAT at Earth 11-10 21:00 [11-08]. The goal's passes are days apart, so the
refit took two weeks; nothing waited for it.

**X6. Home guard for Earth wave 1**. New Earth keeps **no warships** and a
ground reserve of **40T 10Air 10ML** in Home Guard (Earth 1: 80SF 12D 3C
30Tr 30T 30Air, 1,000–1,199 h after contact). Measured: a home guard of
20SF 4D 2C loses in space 0/10 (so does 50SF 10D 4C; only ≥ 50H 80SF 15D
6C wins); on the ground 40T 10Air loses 0/20 and 40T 30Air 8/20, but
5 launchers turn it (40T 10Air 5ML 20/20, 40T 10Air 10ML 20/20; 40T 20ML
and 20T 10ML 20/20). Every warship goes to Strike. S8 still applies (a
lost space battle at New Earth loses trade and carrier groups there).
The wave only comes if Earth stands 1,000 h after the contact (X9's
failure branch). As run: Home Guard held it when the step began (11-17):
the army goal fills it first.

**X7. Contact with the strike group.** Do: `launch(Strike, Earth)` with the
full X3 force. Done: MSG 38 "The Earthlings declare war aganist you"
(intel.c:97-101); Treasonable in the pub (sim.c:1115-1116). Record
T(earth). As run: Strike ready 2930-02-30 11:00 [02-29 14:00], at Earth
03-01 01:00 [02-30 03:00]: 'Earthlings' and MSG 38 together. The Earthlings' contact
also sets the Syonians hostile (intel.c, `Intel_FirstContact`); nothing
of theirs came.

**X8. Bounty-Hunter (optional).** Pre: credits ≥ 200,000 + R. Do:
`pub_ask(Treasonable, "Who are you?" → "And how can I help?" → "Will 100000
Credits be enough?" → "What would that be?" → "I know a bounty-hunter...")`;
`pub_ask(Bounty-Hunter, "Who are you?")`; then `pub_ask(Bounty-Hunter, "I have
a job for you." → "You would have to eliminate the leader of planet Earth..."
→ "O.K.")`. Done: MSG 41 100–199 h later. Do not wait for it if X9 is ready.
As run: not taken (§5 R8 cuts it first; the X3 force wins without it).

**X9. Invade Earth.** Pre: " Your satellite found Earthlings colony at |
Earth" (≤ 5 days after T(earth) at 2–6 per day); Strike holds ≥ 10 Cruisers. Do: `invade(Strike)`. Done:
ground win → screen 36 → exit code 10 (main_program.c:1311-1315). Timeout:
T(earth) + 900 h (before Earth wave 1). Fail: retreat is a loss; rebuild,
defend New Earth against Earth 1, retry (the step goes home, waits for the
full force again and flies back, three times at most). As run: the intel
box 03-05 00:00 [03-01], GROUND WAR at 01:00 [03-02 20:00]: space 142 ships against 172
(80H 80SF 7D 5C, plus 'Earthling Fleet 1' listed in orbit), won, lost 48H
42SF [46H 37SF]; ground 40T 20Air 40ML against 30Tr 90T 50Air 30ML, won,
lost 10T 20Air 30ML, as measured; "Your forces have eliminated
'Earthlings'", then VICTORY and Halt(10).

Act 3 money order (`PolPriority`, highest first): E1, E2, S4, J8, the
army (M8), J7, the act-1 hard steps, S2's shortages, the urgent Stadium,
J12's and Dempsey's act-3 courses (M10), J13, the spy satellite and its
carrier (M14), the space station (M16), S2's lead, S6/S7, the act-1
goals and colonies, J11, E21. As in act 2 nothing waited for money:
854,000 at the Galleon, 3,020,000 when M18 began, 5,480,000 at the end; 8.47 M came in, 3.84 M went out (§5 R8).

Act 4 money order: E1, E2, S4, J8, the army (M8, now army.c), the staff
and their courses and the pub's tips (L1-L4), J7, the act-1 hard steps,
S2's shortages, the urgent Stadium, J12, M10, J13, M14, M16, the Antares
colony and its carrier (L5), the spy satellites (L8), S2's lead, S6/S7,
the colonies, J11, E21. Nothing waited for money: 5.69 M when act 4
began, 11.04 M at its end (§5 R8).

Act 5 money order: as act 4, the Cruisers, launchers and Plasma guns at
the army's place, the X5 carrier and its spy satellite at L8's. Nothing
waited for money: 11.09 M when act 5 began, 12.48 M at the contact
(§5 R8).

Step counts: act 1: 23, act 2: 13, act 3: 18, act 4: 13, act 5: 9.

## 2. Standing orders

Run between steps, in this order, at least once per game day and after
every interrupt.

**S1. Map on Amnesty.** Mining reads the selected system's bodies only
(sim.c:939-958). After any action that selected another system (screens 7,
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
- As run on New Earth (thread F, from the economy model): housing and
  hospitals are kept ahead of the population, built while 10,000 + 7,000 ×
  Buildings < 1.6 × pop (up to 12 Buildings, a ceiling of ~95,000 that
  keeps New Earth under the −20 at 100,000) and 20,000 + 20,000 × Hospitals
  < 1.6 × pop (up to 5); the ceiling grows with them and so does the
  growth. Without a working Stadium no housing past 45,000 people, and
  hospitals only 3,000 ahead. Observed: 30,200 people at the start, 44,000
  on 2927-10-01, 54,000 on 11-01, 68,000 on 2928-01-01, 78,000 on 02-28
  (each new Building also adds 800–1,300 people at once). Shortages
  (within 3,000 people of a capacity, or a box) come before the lead and
  before the Stadium in the money order: once the lead stopped at 45,000
  for the Stadium, food ran short and New Earth starved for two weeks
  ("people don't like you") until food came first.
React at once to the boxes in §3 group C (they name the missing building).

**S3. Tax.** Daily, from what the colony's screens show (thread F; it
replaces the mood-band rules, which on New Earth reacted to a fall they
could not stop): compute the target attitude for each tax level as the
code does (sim.c:457-564): (7 − tax) × 10, +10 for each working Leisure
centre, Stadium, Church and Park, −10 above 30,000 people, −10 above
60,000, −20 above 100,000, −20 above 150,000, −20 above 50,000 without a
Stadium, −20 above 70,000 without a University ("working": finished and
Active on the surface; the code counts the powered ones). The tax is the
highest level (at most Very difficult) whose target is 30 or more: lower
it at once when the count falls, raise one step a day while the mood word
is "neutral men" or better. The mood word still catches the penalties
that only boxes show (food, housing, hospital, medicine, radiation, which
S2 builds against): "people hate you" or "revolting men" lowers one step
below the computed level; a "rebellious forces" box (attitude < 10, a
revolt after 5 such days, sim.c:614-627) sets None. Below 30 the
population ceiling halves, below 20 it is at most half the population, so
the policy never trades attitude for tax. Observed on New Earth: Very
difficult throughout, the mood "neutral men" (later "people find you
sympathetic" for a few weeks after the Stadium), never lower. One trap
seen: unpowered leisure buildings (S4) count as missing, the computed tax
fell to None and the generator's money never came; hence S4's priority.
In act 1 the fixture applies S3 to New Earth only; the colonies stay on
Normal (their target at Normal is 40 until they grow). From act 2 their
build-out pass applies it too (the rebellious box must name the colony).
Observed: High from the founding (target 30 below 30,000 people), one
step up with each of Park and Church, so Very difficult within weeks, and
there they stayed. Before S4 looked ahead on the colonies, Apollo's Mines
(1,100 kWh each) switched its leisure buildings off now and then, the
computed level fell two steps and the mood with it.
(The earlier rules lowered one step on "people don't like you" and set
None on "people hate you": at 50,000 people without a Stadium that sent
New Earth from Very difficult to None in two days and kept it there for
12, with no income at all.)

**S4. Power.** Before building anything that draws power (all but Park,
Windtrap, Nuclear), and daily: if any Leisure centre, Stadium, Church or
Storage bay on that body shows "Status : Passive" after having read Active,
power is short (these switch off first; a building under construction also
reads Passive). Then build one generator (Windtrap on Desert/Icy, else
Nuclear Gen. 35,000) before any other building there. As run: any
finished building that reads Passive counts, checked daily on New Earth
and before each build elsewhere, and New Earth's generator comes right
after the first hires in the money order (S3 above). Ahead of the
shortage, a generator also comes when a building's "Energy : n kwh" (its
nominal energy × supply / demand, planetinfo.c apply_building_efficiency)
is below 60%: a Derrick's (750) on New Earth, from act 2 on also a Vehicle
plant's (1,230) or Spaceport's (753) in each colony's daily pass. The
colonies' Mines draw 1,100 each, so a Desert colony with 9 Mines runs on
5-6 Windtraps.

**S5. Droids.** If a body has mines > droids and stock > 0, `add_droid`.
New Earth: buy droids as needed (3,150 each). Colonies: droids come by trade
group and need a Spaceport there (cargo.c:113).

**S6. Ore hauling.** Every 10 days, or when a purchase is ore-gated: a trade
group (Trade 1; Galleon 1 when free) docks at each Amnesty colony, loads
everything (right-click per ore row, capped by ship space and by the planet's
storage), flies to New Earth, unloads. Priority: Energon, Raenium, Texon,
then the ore the next purchase lacks. Buy more Trade ships for Trade 1 when a
round trip leaves ore behind **[plan]**: 10 Trade ships by act 3, 25 by act 4
(guide uses ~50). As run (act 2, goal `S6_S7_haul`): one round trip a
daily pass, Trade 1 waited for at each end (a trip inside Amnesty is 1-4 h
with Jack): docked at New Earth it unloads, reads New Earth's column on the
cargo screen (stock and "Storage space on planet") and goes, with droids if
a colony waits for them (S7), else empty to the colony visited longest ago
(at most every 3 days); there it puts back ore New Earth had no room for
and loads the ores New Earth has least of, each up to New Earth's room
(more than its room: 100 t left presses up to 3,000 t, cargo.c:135-160).
Loading Raenium first regardless filled the ship with it and no Texon
came, then New Earth's Raenium storage was full and the rest stayed
aboard. Trade 1 grew to 3 Transfer ships (September) and 4 Trade ships
(after MSG 8): 9,500 t a trip. New Earth's storage (10,000 t per Mine,
5,000 per Derrick: 190,000 t with 9 and 20) is the limit: every ore was
near it by the end of July (D141,000 E129,000 K122,000 L127,000 R125,000
T117,000 on 07-30). During a wave (S8) it goes neither to New Earth nor to
Apollo. Act 3 (the army: tanks alone take 1,215 t of Energon each, and
the Phelonian deal halves New Earth's stock) grows Trade 1 to 10 Trade
ships, 21,500 t a trip: with 4 New Earth's Energon fell to 7,000 t in
one run, with 10 it stayed above 40,000 t.

**S7. Droid delivery.** When a colony has a Spaceport and mines > droids:
`buy(Miner droid, k)`, load k on a trade group at New Earth, unload there,
`add_droid` k times. As run: in S6's trip; a droid takes 500 t and one
place per Transfer ship, four per Trade ship; New Earth's own empty mines
(E21) take the shop's droids first. A colony builds its next Mine only
when every finished one has its droid (E18), so a colony gained about one
droid a week: Apollo had 9 on 02-07, Penelope on 05-28, Mir on 07-25. In
one run Penelope stopped at 6 with a droid left in its stock, and its 7th
Mine waited for that droid for good: each visit puts the stock's droids
to work.

**S8. Wave evacuation (reactive).** On an ATTENTION box (or first-contact
box) naming New Earth: within the same game hour, take off and `launch` every
Trade and Carrier group whose panel reads New Earth **or Apollo** (the battle
matches system and planet only, so New Earth's moons count,
intel.c:181-196, sim.c:1284-1288) to Penelope or Mir. Do not launch any group
to New Earth or Apollo until the battle result. Docked groups need
`take_off` first. Army groups stay. As run: checked after every box and
after every goal of a pass (a box can come in the middle of one), for the
groups the policy creates (Trade 1, Survey 1, Survey 2, Galleon 1, ...),
all to Penelope; over when a space battle has been fought after the box,
or 120 h after it. The box reads "ATTENTION! | Enemy fleet approaching |
Amnesty system,planet New Earth"; the Morgruls' first-arrival box names no
body and counts as New Earth. Wave 1: the box 10-31 h before the attack.

**S9. Defence floor.** Fixed targets from the guide (not hidden state):
after J6, Home Guard ≥ 12 Hunters; after M1, ≥ 20; after M7, ≥ M8 targets;
after M12, ≥ 20 Hunters + 20 SF + 20 tanks; from L7 on, the L7 target; from
X6, the X6 target. When Strike leaves, Home Guard must still meet the
current floor. As run (act 3): the army goal holds the M8 and M12
targets (more, as the tank line runs on to 90), and M18 takes all of it
to Mirach: after wave 3 no wave is due for 8,000 h (wave 4), so the
after-M12 floor is not kept at home; act 4 rebuilds Home Guard (L7).
As run (act 4): the ships stay in Strike, docked at New Earth until the
Lisonian wave is beaten (L12); while Strike is away for the invasions
(L13) Home Guard holds the ground reserve that beats the Lisonian
landing on its own, 40T 10Air (10/10; 40T alone 0/10), and no warships
(the doc's 30H 30SF 5D would double the Twin line's work, the act's
gate, for a wave the guide does not announce).

**S10. Equip completeness.** After any purchase of ships, equip them fully
before the next battle (weapons are pooled per slot; unarmed hulls dilute,
walkthrough "Weapons and loadouts"). Re-check after each refit research
(Twin, Missile, Plasma).

**S11. Research queue.** When the developer is present and idle (no project
"Under development"): start the highest-priority available product in this
order (the icons are captioned with the product names of the game's data:
"Sat.carrier", "Hyperspace drv", "Missile launcher", ...): the current act step's product, Communicator, Radio, Hyperspace drive,
Galleon, Starfighter, Twin laser gun, Spy satellite, Missile, Destroyer, Space
station, Aircraft, Plasma gun, Missile launcher, Mental radar, Trooper, then
anything. Never click a running project (it pauses), and starting a project
stops the running one (research.c:74-105). Read a product's state from its
icon art and hint caption; the panel text needs a selecting click.

**S12. University for non-developers.** When nobody is away and credits ≥
course price + R + the next *hard* purchase: Jack, Kyle, Forbs, Julie, MACH in
that order, once each.

**S13. Idle.** With nothing to do: screen 11, map on Amnesty, clock running.

## 3. Interrupt catalogue

General handling:
- **Message box**: text centred near (160,124), status bar "Press mouse
  button" blinking with a 50-retrace period, blank half the time
  (message.c:89-98, 94-226). A box stops the main loop and the clock. Close:
  release, press, release on an inert point. Logged boxes go to the message
  log.
- **Rename prompt**: "The new planet's name in &lt;sys&gt; system? ..."; press
  Enter only.
- **Press-leak**: scene and hologram waits end on a *press*
  (gfx.c:372-394, alientalk.c:88-108, command.c:291-302). Click an inert
  pixel (driver-defined) so the following screen-1 pass sees no hotspot.
- **Never press keys** during scenes: any key sets `Gfx_KeyAbort` for good
  and later scenes skip (gfx.c:362-375).
- On paused screens (29, 19, 32, 33, 34, 18, 21, 28, 24, 25) no hour runs, so
  no new interrupts arrive there.
- A screen switch to 29 in the same hour as a talk (34) or scene (33)
  overwrites it (§5 R7).
- A story scene ends on the command centre: a driver sequence under
  `pol_try` that it came into is left there as for a talk and runs
  again later (the supernova's scene 7, act 4, came into shop and colony
  surface sequences).
- Talks and battles come up at any hour, also while a goal or a step is
  in the middle of its clicks (act 2: meetings 1-3 came during S6's trips
  and colony passes): every driver sequence runs under `pol_try`, which
  leaves it at its next wait when screen 34, 29, 19 or 32 appears, has it
  handled (`pol_service`) and takes the sequence up again from what the
  screens show. A click on the talk screen meant for the screen before
  would pick a question.

### A. Screens

| Id | Trigger (code) | Recognise | Action | Next |
|---|---|---|---|---|
| I-S33 story scene | `AlienTalk_StartAlienScene(n)` (alientalk.c:54-59) | Full-screen `pics\pic<n>`; music by scene (main_program.c:1481-1508). Scene 4 opens with MSG 36, scene 9 with MSG 1 | Scenes 3, 5–7: one click; scene 4: two (the MSG 36 box, then the scene); scene 1 polls the button every 10 retraces: click, release, click, holding each press and release ≥ 10 retraces; scene 2: click (or ~7 min); scene 9: one click for the MSG 1 box, then it ends by itself; scene 10 (in talk 6) ends by itself. The fade after a scene takes 6 retraces, so every click lands on an inert point | Screen 1 |
| I-S34 alien talk | `AlienTalk_StartAlienTalk(n)` (alientalk.c:178-184) | Music "talk", backdrop `pics\atvezeto`, alien portrait; menu rows from y=134 | Path from the talk table below | Screen 1 (+ MSG 46 or 48 on some) |
| I-B29 space battle | Enemy attack (sim.c:1294-1323) or own attack/invasion (launch.c:207-238) | `war\spwar` backdrop, fighter face | Fully automatic: the only control is RETREAT (0x3E, counts as a loss); never use it unless the step says so. Wait for END BATTLE (0x41) over `war\spvict` / `war\splost`; click it | 7, or 19 (defending: only after a loss at a colony; attacking: only after a win) (main_program.c:1236-1249) |
| I-B19 unit assignment | After 29 | `war\grbeoszt`; slots pre-filled with all ground units at the body (grwarunits.c:276-279) | Army groups at the body bring their ground units (grwarunits.c:191-279). Reorder the slots to the formation launchers rear, then troopers, tanks, aircraft front (`ui_assign_units("mrta")`); click OK, ATTACK (0x0B). Own invasion also has CANCEL ATTACK (0x2A) | 32 |
| I-B32 ground battle | From 19 | `war\grf<surface>` field | Tactic COUNTER (`ui_ground_battle`, [playthrough-battles.md](playthrough-battles.md) §5): hold, and send a tank or aircraft stack only against an enemy that reaches it while nothing is in its own reach; never Move Missile launchers forward. A hit stack cannot fire for ~10 retraces (grwar.c:714), which makes aircraft dominant. RETREAT 0x48 = loss. END BATTLE 0x41 over `war\grvict`/`war\grlost` | 7; **35** if lost at New Earth; **36** if won vs Earthlings |
| I-S35 death | Ground loss at New Earth, or New Earth destroyed (main_program.c:1445-1449, intel.c:229-240) | Death pictures | None (run failed) | Halt(15) |
| I-S36 victory | Ground win vs race 12 (main_program.c:1311-1315) | Palette fade | None | Halt(10) |
| I-S23 reload | Pub timer finishes while on 23 (sim.c:283-286) | Pub redraws | Re-read people | 23 |
| I-S1 forced | Advisor/scientist return while on 1; body destroyed while on 4, 8, 13, 16, 17, 20, 22 (sim.c:1372-1375, 1435-1439; intel.c:232-234) | Screen 1 | Resume step | 1 |

### B. Alien talks (screen 34)

Menus from the DS:0CDE tables and `KERDESn.AT`; effects in
`alien_talk_answer` (main_program.c:1526-1584). Effects apply when the
question is clicked.

| Talk | Trigger | Path (bold = the choice) | Effect |
|---|---|---|---|
| 2 Jaanosian meeting 1 | sim.c:1425-1432, no box | "We are glad that we could finally decode..." → "Could you be more specific ?" → "What can I do ?" → **"O.K."** (always 4 clicks) | Scientist away 60–89 h; MSG 8; Trade ship granted (main_program.c:1529-1536). "It's out of question !" is the row beside "O.K." in the 4th menu, not a state: it declines (→ "Think about it again." → "O.K." / "I don't change my decisions."); never click it |
| 3 Jaanosian meeting 2 | MSG 9 box, close it first (sim.c:1443-1455) | "Greetings Alien !" → "What is it ?" → **"What is the price ?"** → **"Yes."** | −16,000 (unchecked); Hyperspace drive later (main_program.c:1538-1542) |
| 4 Jaanosian S.O.S. | MSG 10 box, close it first (sim.c:1456-1468) | "Greetings Alien." → "What shall we fight with? We have no weapons!" → **"O.K. We will do our best to help."** | Hunter, Laser cannon, Army groups (1543-1547) |
| 5 Kalls | First contact + MSG 15 box, close it first (intel.c:77-82) | "Greetings Alien." → **"That is right."** → **"When are we receiving your weapons?"** | Starfighter analysable, +10 in stock (1548-1551) |
| 6 drifting ship | MSG 18, then tractor-beam scene 10 (sim.c:1046-1051) | **"Tell me, what are your plans?"** (row 5) → "You will send a message to your leaders..." → **"All right."** | Cancels the Kall turn (only if their first timer fired); Kall force 2 to Mirach 3; +250 h to wave 3 if unsent (1552-1560); exit MSG 48 |
| 7 Phelonians | sim.c:1538-1541, no box | "Greetings Phelonians." → "What will you give in return?" → **"I think something more valuable could be offered..."** → **"It's a pleasure doing business with you."** Click in order: the same "pleasure" row in the menu right after "What will you give in return?" sells up to 10,000 t of Energon for credits instead | Battle tank granted; New Earth Energon halved (1561-1573) |
| 9 Erans | MSG 45 box, close it first (sim.c:1138-1144) | "Greetings Erans..." → "Yes, we are sure..." → **"Yes ! My people will welcome you."** | Eran in pub, Erans allied (1575-1578); exit MSG 46; supernova 560–569 h later |
| 10 Syonians | First contact + MSG 29 box, close it first (intel.c:93-96) | One option per state, ending **"Thank you for your confidence in us..."** | Sun known, Energy shield, Bounty-Hunter (1579-1583) |

### C. Message boxes nobody asked for

All need the close click. "Act" is what the policy does after closing.

| Box text (source) | Trigger (file:line) | Act |
|---|---|---|
| KIT k "The &lt;X&gt; is invented" (KITALAL k) | Idea timer fires (sim.c:1402-1414) | S11; step preconditions |
| "The &lt;name&gt; is ready for production" | Research done (sim.c:335) | Step done-conditions; refits (S10) |
| MSG 3 / MSG 12 / MSG 22 | Miner station / Hyperspace / Destroyer done (sim.c:341-354) | None |
| MSG 33 "...locate the electonically disguised planets" | Mental radar idea at 50 h (sim.c:1406) | None |
| MSG 1 + scene 9 | Stock satellite lost (sim.c:1420-1424) | E8 done |
| "&lt;X&gt; is ready for mining" / "not suitable for mining"; MSG 2 first time | Intel ≥ 10 (sim.c:654-664) | E11 |
| "&lt;X&gt; is a suitable place for living" / "not suitable"; "Your satellite reports alien activity on X" | Intel ≥ 30 (sim.c:667-679) | Colony icon check |
| " Your satellite found &lt;Race&gt; colony at X" | Intel ≥ 40 on alien body (sim.c:682-689) | Invasion precondition |
| " Your observatory/pilots/satellite carrier/Psy-radar radar found a new planet in &lt;sys&gt;" + rename | sim.c:766-815 | Enter; E16, E22, M5, X4. Can come any day (e.g. seed 12345: the observatory found Amnesty 1 on 2927-08-20) |
| " Your ships explored &lt;sys&gt; system \| They found new planets" + renames | sim.c:1068-1084 | Enter for each |
| "Your pilots/satellite carrier explored &lt;planet&gt;'s moons" | sim.c:1100-1109 | None |
| MSG 34 + scene 2 | First hyperspace jump (sim.c:1011-1015) | None |
| " Your pilots met with a new alien race in &lt;sys&gt; \| They call themselves '&lt;race&gt;'" | First contact (sim.c:1117-1133, intel.c:47-101) | J1, M4, M6, L9, X4, X7; follow-ups MSG 5, 15, 28, 29, 38 |
| "New alien race's ships arrived at your colony in &lt;sys&gt;..." | Unmet race's wave arrives (sim.c:1234) | S8 |
| "ATTENTION! Enemy fleet approaching &lt;where&gt;" | Hostile force arrives where you are (sim.c:1255) | S8 if New Earth; battle follows 5–105 h later |
| "Your colony is under attack by &lt;race&gt; at &lt;where&gt;" → screen 29 | Attack (sim.c:1295, 1315) | I-B29 |
| " Your ships around &lt;body&gt; is under attack by &lt;race&gt;" → 29 | Attack where an Army group is (sim.c:1305-1314) | I-B29 |
| MSG 8, 9, 10, 11, 30, 17, 24, 20, 37, 25, 27 | Story timers (sim.c:1420-1566) | See §4 |
| MSG 16, 19 (+ scene 3) | Wins over waves 2, 3 (main_program.c:1253-1268, 1461-1475) | M9, M12 |
| " Your forces have eliminated '&lt;race&gt;' "; MSG 35 / MSG 32 | Home-world ground win (main_program.c:1287-1333) | M18, L13 |
| MSG 13; MSG 21 + scene 4; MSG 31 + scene 6 | Docking at Jade (after fall), Mirach 2 Moon 1, Rigel 1 (shipmove.c:153-169) | J10, M15, X1 |
| MSG 4 | Miner station on a habitable body (satellite.c:136-142) | E15 |
| "The building of a colony started \| on &lt;sys&gt; system,planet &lt;planet&gt;[,moon &lt;moon&gt;]" | Colony countdown ends, at midnight (sim.c:712-715) | S2/S3 |
| "Because of disease your people need a medicine plant on X" | sim.c:464-467 | Build Medicine plant |
| " Due to sanitary conditions you need a hospital on X" | sim.c:471-475 | Build Hospital |
| "Your people ask for a stadium on X" | pop > 50k, sim.c:500-503 | Build Stadium |
| "Your people demand a University on X" | pop > 70k, sim.c:505-508 | Build University (53,000) |
| " Your people are starving on X" | sim.c:555-558 | Build Food resources |
| " There isn't enough living space on X" | sim.c:562-565 | Build Building |
| "Your people are dying because of radiation on X" | Planet 1–2 colony, no shield (sim.c:510-519) | Never colonise planets 1–2; if seen, abandon investment there |
| Meteor boxes | Orionis/Lyrae 5 only (sim.c:521-551) | Rebuild |
| "Your advisors report that there are rebellious forces on X" | Attitude < 10 (sim.c:589) | S3: tax None now |
| " Your people have revolted on X" / " Your colony has been destroyed on X" | sim.c:587-597 | On New Earth: game over |
| "Your advisor has returned from the university" | sim.c:1377 | S12 |
| PIR k (radio) | Pirate deal window opens (sim.c:294-306) | I-P9 (optional) |
| MSG 39 + "Information on: ..." boxes | Spy report back (sim.c:206-215) | None |
| MSG 40 + MSG 44 (or 43) | Eran's job done (sim.c:217-231) | L10 |
| MSG 41 | Bounty-Hunter done (sim.c:232-246) | X8 |
| MSG 47 + "You got N t of X" / MSG 42 | Pirate report (sim.c:254-282) | None |
| MSG 6 "You have received a message from deep space..." | A Jaanosian meeting without Communicator, or the deadline without contact (sim.c:1431, 1453, 1462) | **Run failed** (§5 R1, R2) |
| Error boxes after own actions (§0 table) | – | Step failure branch |

### D. Pub people (screen 23 → 25)

Screen 23 is the SPACE LOCAL button. Hotspot 0x14+k, caption = name;
present when drawn (state 0 or 2 and no running job, pub.c:81-99). Two people
can be captioned "Stranger": P-2 (Spy) and P-7 (Bounty-Hunter, until first
spoken to); if both are present, tell them apart by portrait. No exit button:
finish every path.

| Person | Use | Path |
|---|---|---|
| P-6 Bartender | Spy unlock (M13); League tip (L4) | "Who is that suspicious character?" → "How dangerous is he?" → "I think I will speak to him." / "Have you heard about the league?" → "Tell it!" (−5,000). Menu priority (main_program.c:979-982): Morgrul question masks the tip while the Pirate is present and unexplained |
| P-4 Informer | Phoenix 5 (M6); Pirate Leader (L3) | "Do you have any information?" → "All right, talk." (−500); "Do you know anything about the Morgrul in Space Local?" → "All right, talk." (−500). While he has nothing to sell the path has no "All right, talk." step |
| P-2 Spy ("Stranger") | Mirach (M13) | "Would you work for me?" → "Will you do the job for 30000?" (10,000/20,000 refused, no charge) → "Do you know anything important?" (main_program.c:1024-1033) |
| P-1 Spy-Hunter (P-SH) | Mirach fallback | First talk "Have you seen or heard anything unusual?"; later "There is a spy in the pub. Do your job..." → "Are you sure?" → "Yes! Kill him!" → MSG 37 after 200 h (main_program.c:1019-1022, sim.c:1546-1548) |
| P-9 Pirate Leader ("Morgrul") (I-P9) | Optional cargo | "I heard that you accept dirty jobs for good money." starts 10 windows; in a window (after its PIR box): "I have a good job for you." → row 1 → "You'll get half of the cargo." |
| P-5 Eran | L10 | see L10; "I'm just having a drink, that's all." = not yet |
| P-10 Treasonable, P-7 Bounty-Hunter ("Stranger" until first spoken to) | X8 | see X8 |

## 4. Visible signals for story triggers

| Trigger | Visible signal | Notes |
|---|---|---|
| Jade revealed | Box " Your observatory found a new planet in Amnesty system" + rename prompt prefilled "Jade"; Jade then drawn on the Amnesty map | Only the observatory (status −3, sim.c:766-770) |
| Amnesty 1/2, Phoenix 1/4, Mirach 1/5, Lyrae 6 revealed | "...pilots/satellite carrier found a new planet..." | Status −2: groups in transit in that system, 1/5 per day (sim.c:781-805) |
| Phoenix 5 revealed | Informer reply; Phoenix 5 drawn on the Phoenix map (no box) | Status −3: Informer or a Phoenix observatory only |
| Rigel 2 revealed | "Your Psy-radar radar found a new planet in Rigel system" | sim.c:807-815 |
| New system known | Name appears in the star-map system list (needs Hyperspace drive or Galleon Done, planetinfo.c:899-915) | Mirach: no box after the Spy (main_program.c:1028), MSG 37 via Spy-Hunter; Antares/Orionis/Lyrae: MSG 35; Rigel: MSG 32; Sun: after talk 10 |
| Jaanosian deadline | None until it fires (MSG 6) | Use the date: contact by 2928-04-15 |
| Meetings 1–3 | Talk screen; MSG 9, MSG 10 | Hidden timers; wait with timeouts |
| Base destroyed | MSG 11 + scene 1 | T(fall) |
| Wave launched | **No signal** | Only arrival: ATTENTION box (≥ 5 h before the attack) |
| Wave at New Earth | ATTENTION box; "The Morgruls Fleet n" listed in the New Earth zoom | sim.c:1255, planetinfo.c:1024-1045 |
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
| Group position | Screen 22 panel "In the dock of", "In orbit", "Fly to" (redrawn on arrival); screen 16 shows only "Currently on:"/"Destination:"; log "arrived to" | groups.c:330-334, shipinfo.c:356-365 |
| Selected system | Star map shows it; list entry highlighted | Reset with PLANET MAIN |

## 5. Open questions and risks

**R1. Jade has no player-controllable reveal (fixed in the port).** The
port reveals Jade on 2928-04-01 if it is still hidden (`fixes.find_jade`,
docs/bugs.md); the rest of this note is the original's behaviour. Jade's status is −3, so
only New Earth's observatory finds it: 1/50 per day, one find per day across
the galaxy, shared with Amnesty 1, 2 and 6 (sim.c:766-778, DS:47D6). Flying
a group around Amnesty does **not** help; it only finds status −2 planets
(sim.c:781). The task brief and the walkthrough's route assume otherwise for
Jade. P(no find in ~240 days) ≈ (49/50)^237 ≈ 0.8%. If Jade is not found by
~2928-04-20 the Jaanosian chain runs without the player (MSG 6), Hunter and
Laser are never granted, and the game is unwinnable. No in-run retry exists;
the proof run must use a seed where it holds, and the robustness report will
show this as a seed-dependent failure. Second observatories don't help (the
code tests `> 0`).

**R2. A missed Jaanosian meeting is unrecoverable.** Each meeting fires once;
without Communicator it becomes MSG 6 (sim.c:1429-1467). Meeting 3 is the only
source of Hunter and Laser. J2 is *hard*.

**R3. Phoenix 5 cannot be found by flying.** It is −3 too. The walkthrough's
"or fly around Phoenix" is wrong; use the Informer. The Informer's
Phelonian item is replaced once Antares is explored, so M6 must precede L5.

**R4. Radio timing (guide vs code).** The guide says Radio must be done
before wave 2 is sent. In the code the S.O.S. countdown is set when wave 2 is
sent and only ticks while Radio is Done (sim.c:1503), so a late Radio only
delays the S.O.S. Low risk; keep J10 early anyway.

**R5. Drifting-ship trigger is wider than the guide says.** Any group
arriving anywhere while Phoenix 1 is the selected planet fires it once MSG 17
has appeared (sim.c:1046). Selecting Phoenix 1 early (e.g. to read its info)
with a group about to arrive anywhere would fire talk 6 before the Kall fleet
moves, and the alliance would still happen (sim.c:1516-1521). Rule: after MSG
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
(sim.c:579-588): New Earth at start ≈ 3,200/day. Act 3's M8 needs ~0.75 M
credits and ~70 kt Energon in ~1,800 h. The measured battle forces raise
two targets: M18's 90 tanks (+30: +399,000 credits, D +46 kt, E +42 kt,
R +34 kt) and X3's 10 Cruisers (+4 over 6: +1.49 M credits, K +53 kt,
T +46 kt; X6 no longer keeps 2 Cruisers at home, so +2 Cruisers and
+746,000 net). The final forces (50H 50SF 10D 10C, 110 tanks, 20 Aircraft,
70 ML) cost about 11 M credits before losses. Build hours gate as much as
money: the tank line needs 5,500 h for 110 tanks from M7, the Cruiser line
2,000 h from L13, and the Missile and Twin laser lines ~4,200 h and ~6,000 h
over the game, so those lines must never sit idle. If the run falls behind,
the order of cuts is: Stadiums in colonies, Penelope, extra Trade ships,
Troopers (unless M18 uses the 60T + 30Tr fallback), the Pirate Leader,
Bounty-Hunter. Not to be cut: J2, J7, J8, M7, the Battle-tank ground
reserve, S8, M18's tanks, X3's 10 Cruisers.

Measured in act 1 (thread F, seed 12345, the policy as run; New Earth at
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
there); the proof run's act 1 ends at Jade. About 1.1 M credits came in from 2927-08-13 to 2928-03-01: the staff
(110,000), courses (~60,000), New Earth's growth (S2: ~12 Buildings, 4
Hospitals, Food resources, Vehicle plant, Stadium, generators, Derricks)
and the three colonies (142,000, 167,000, 142,000 + cores) took it. The
colonies were founded on 2927-11-28 (Apollo), 2928-01-12 (Penelope) and
2928-02-15 (Mir); none mines yet (no droids, S7). The economy model's act-1
estimate (0.94 M, 6.1k/day at the end, with Jade on 2928-04-02) matches.

Measured through act 2 (the proof run, one game from "new game": Jade
2927-09-08, contact 2928-03-02; populations and the income, the credits'
step at midnight, were read off the game's memory for this table only;
every colony's mood "neutral men" or better, New Earth at Very difficult,
the colonies Very difficult from their Park and Church on, S3):

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

Russel (founded 08-02) had 4,087 people at the end. 3.65 M came in from
Jade (E23, 2927-09-16) to the Galleon, 2.59 M of it after the contact;
the economy model's recommended scenario (C) has 1.65 M and 19,600/day
from the contact to the fall, the run 1.94 M and ~22,000/day. From May
on the goals bought far less than came in (the J8 claim of 150,600 never
held anything back), so the credits piled up: 616,000 at the S.O.S.,
854,000 at the end (Russel's founding, 167,000, among the last buys). New Earth's housing stop (S2, ~95,000
ceiling) held it at 90,000; the colonies carry the growth. Ore is no
gate either: New Earth's storage (190,000 t per ore) was nearly full of
every ore by August (S6), Texon included. Act 3 can spend from the
start: M8's ~0.75 M is in hand, and its lines (Twin laser, Battle tank),
not money, set the pace.

Measured through act 3 (the proof run, from the Galleon to Mirach 3;
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
64,000 t in March, so 20 Derricks did. The proof's continuous run took 22
days longer than runs resumed at J13 (Mirach 2929-03-09 to 03-16): their
goals start afresh and the passes fall differently.

Measured through act 4 (the proof run re-recorded in act 4, from Mirach
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

Measured through act 5 (the proof run, from the League's surrender to
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

**R9. Battles are random.** Space: hit chance 11% per ship per round,
instant-kill rolls, 1/5 of losses survive (walkthrough "Space battle"); the
outcome depends only on the fleets brought. Ground battles use the random
generator only for move order and ties, so the same forces play almost the
same way, but small changes in numbers or formation can turn them. Force
sizes in §1 are the measured ones ([playthrough-battles.md](playthrough-battles.md)
§6, Kyle at hire). Retry plans:
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
and Rigel is explored; a 60-day timeout fails with p ≈ 0.18%. As run:
found 1.4 days after Mental radar was done (3.6 days in the run
resumed at X01; 16 days in an earlier one).

**R13. Antares deadline.** The supernova deletes every group there. L6 leaves
immediately; the colony (≥ 177,000) is a sunk cost. As run: the colony,
its Observatory and the carrier that brought the satellite cost
178,000; the supernova came 560-563 h after talk 9, with no group there.

**R14. Code vs guide, smaller points.**
- Satellite carrier needs no pilot to move, but without one it covers only
  1 leg/h (§0 "Travel"); colony founding does not need a Miner station
  (satellite.c:305-310), only Control Centre, which needs one placed once.
- Tractor beam (wave 2 space win) is never used by any code; story stage 3
  is also set by a ground win against wave 2 (main_program.c:1461-1466).
- Talk 6: "All right." delays wave 3 by 250 h; the walkthrough calls this a
  plain delay, which the policy uses for preparation time.
- Pirate-type groups can't be created (no code enables them).
- Ion cannon and Energy shield have no combat effect (walkthrough "Attacks on
  New Earth").

**R15. Open questions for integration.**
- Exact inert click point after scenes/holograms (driver work).
- Answered: Army groups do carry their ground units into an invasion (the
  group's second unit row, grwarunits.c:191-279). Space battles are fully
  automatic (RETREAT is the only control); in ground battles a hit stack
  cannot fire for ~10 retraces (grwar.c:714), which makes aircraft dominant;
  formation launchers rear, aircraft front
  ([playthrough-battles.md](playthrough-battles.md) §2-5).
- Answered: a group's slot takes up to 1,000 units, each weapon column up
  to its capacity × the units (groups.c, Groups_GroupsScreen); the M18
  strike (40H 40SF 6D 90T) fits one Army group.
- Whether docking at Jade while Morgrul force 1 sits there triggers anything
  (no mission set, so no attack is expected; sim.c:1265). Answered (act 2):
  nothing; nor did a trade group at Mirach 3, the Morgrul home (act 3).
- Wave travel and wait times: arrival 20–29 h (in system) / 50–69 h
  (between systems) after launch; attack after Random(100 − speed) + 5 h
  (intel.c:126-133, 385-400). S8 relies on the ≥ 5 h gap.
