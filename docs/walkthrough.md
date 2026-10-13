# Reunion: game reference and walkthrough

A condensed version of Hugo Guimarães' *"Merit's Galactic Reunion" Ultimate
Guide* (v1.0, 2018), written for an agent
that has to play the game through to the end. The guide's opinions, review,
spelling-error list and conclusion are gone. Numbers and triggers were checked
against the game's data and code (the port, `src/game/`). Each claim is
marked:

- **[src]**: confirmed in the code or data. The function is named where it
  helps.
- **[fixed]**: the guide was wrong; the value shown comes from the code.
- **[guide]**: from the guide and not checked, usually because it depends on
  play rather than on a constant.

Units: money in credits, ore in tonnes, time in game hours. 1 game hour =
101 frames at 70 Hz (≈1.44 s real time); months have 30 days.

Contents: [Walkthrough](#walkthrough), [Story events](#story-events), [Staff mechanics](#staff-mechanics), [Space Local (the pub)](#space-local-the-pub), [Colonies](#colonies), [War](#war), [Reference tables](#reference-tables).

## Walkthrough

This is the guide's route, corrected against the story chain below. Unit
counts and colony choices are the guide author's and were tested in play
**[guide]**. The ordering constraints come from the code.

Standing orders for the whole game:

- Keep the map on **Amnesty** when waiting, or off-system mines stop (the
  original only; the port mines every system).
- Answer colony demands for power, hospitals and food as they come.
- Keep New Earth's space defence able to beat the next wave. A lost space
  battle there deletes the trade and carrier groups at New Earth; a lost
  ground battle there ends the game.
- Equip every warship and vehicle fully, since weapons are averaged per slot.
- Save before every battle and every alien conversation. Results are random.

### 1. Economy (2927-08 to ~2928-03)

1. Hire **William Forbs** (builder, 70,000). He builds fastest, he is the only
   builder who can found colonies beyond Phoenix, and the Stadium needs at
   least Elanor.
2. On New Earth set tax to **Very difficult**, build a **Vehicle plant** and a
   **Stadium**, one Mine with the spare droid, and ~10 Buildings. New Earth
   already has the other leisure buildings.
3. Hire **Thomas Dempsey** (developer, 15,000). Research Miner droid and
   Satellite.
4. Buy 2 satellites. Deploy one on **Apollo** and one anywhere else. The
   second is destroyed, which unlocks Satellite carrier.
5. Hire a pilot. **Jack Benson** (25,000) is enough until Antares; Bruce
   can't leave Amnesty. Research Satellite carrier. Use carriers to survey
   Mir (Amnesty 2), Penelope (Zeus) and other bodies. The first mineable
   find unlocks Miner station.
6. Research Miner station, then Transfer ship. Place a Miner station on
   **Apollo**; this unlocks Control Centre.
7. Send Dempsey to university (Maths & physics), then research Control
   Centre. Found colonies on **Apollo** (Desert: windtraps), **Mir** (Desert,
   needs a Medicine plant and double hospitals from day 10) and **Penelope**
   (nuclear generators).
   - Give each colony a Builder plant, Vehicle plant, all four leisure
     buildings, Food resources, a Hospital and ~10 Buildings.
   - Set tax to Very difficult, and lower it as the colony passes ~60–100k.
8. On New Earth, add Mines up to 9 with droids, and build Derricks: the guide
   uses 20 at this point and 60 later. Detoxin is only needed at New Earth,
   so don't build Derricks elsewhere.
9. Watch for New Earth's observatory to reveal **Jade**. Do not demolish the
   observatory.

### 2. The Jaanosians (send the group by ~2928-04-10, before the 2928-04-23 deadline)

1. Send a trade group to **Jade**. Research **Communicator** right away; all
   three meetings need it.
2. Keep expanding:
   - Build 9 Mines plus a Spaceport in every colony (Apollo counts its Miner
     station), and fill them with droids.
   - Build many trade ships. The guide uses ~50, which carry 100k t per trip.
   - Haul ore home regularly, mainly Energon, Raenium and Texon.
3. Meeting 1: **send the scientist** (gives Trade ship).
4. Meeting 2: **buy the device** (16,000; gives Hyperspace drive).
5. Meeting 3: **agree to help**. You get Hunter and Laser cannon. Buy ~20
   Hunters and enough weapons to fill them (2 lasers + 1 twin each; twins
   come later), and form an **army group** at New Earth.
6. Hire a war commander before wave 1 arrives, 600–649 h after the base
   falls. **Kyle Douglas** (45,000) is the minimum for invasions later; MACH
   (300,000) is better. Experience grows only with time or university, so
   hire early.
7. After the base falls:
   - Land a trade group on Jade, then research **Radio**. It must be done
     before wave 2 is sent, 1,800–1,999 h after the fall.
   - Colonise **Russel** (Jade moon: Energon 9, Raenium 8) with Mines and a
     Spaceport.
8. Research **Hyperspace drive** and then **Galleon**. Buy a Galleon for the
   trade group.

### 3. The Morgruls

1. Beat wave 1 (6 Hunters). Twin laser gun unlocks; refit every Hunter.
   Trooper unlocks after your first space win.
2. Send the Galleon trade group to **Phoenix 3**: meet the Kalls and ask for
   their weapons (Starfighter plus 10 free). Find **Phoenix 5**: pay the
   Informer 500, or fly around Phoenix.
3. When the Phelonians call, take the **Battle tank** deal. Have little
   Energon at New Earth when you do, since it costs half of it.
4. Build up to ~20 Hunters and ~30 Starfighters fully equipped, plus Battle
   tanks (the guide aims for 60).
5. Beat wave 2 **in space**. This gives Tractor beam and story stage 3,
   which lets Dempsey's skills rise further.
6. After the Phoenix S.O.S., wait until the Kall fleet is visible at Phoenix
   5. Then send a group to **Phoenix 1** (with Phoenix 1 selected in the
   original; not needed in the port) and
   answer **"Tell me, what are your plans?"**.
7. Beat wave 3, which is sent 1,100–1,149 h after wave 2.
   - In Space Local, ask the bartender about the suspicious character, then
     hire the **Spy** for 30,000 and ask "Do you know anything important?".
     Mirach is revealed.
   - Research Spy satellite now and Spy ship when it appears.
8. Send a trade group (or a satellite) to **Mirach 2 Moon 1**. Research
   **Missile**, then **Destroyer**, then **Space station** (Dempsey needs
   courses for 6/6/5 and 7/5/7). Buy a Space station and ~5 Destroyers, fully
   equipped.
9. Place a **Spy satellite on Mirach 3** and wait for its intel to reach 40.
10. **Invade Mirach 3** with ~20 Hunters, 30 Starfighters, 5 Destroyers and
    60 Battle tanks. The Kall force sent there helps.
    - Garrison: 20 Hunters, 20 Starfighters and 3 Destroyers in space; 61
      Troopers and 40 Tanks on the ground.
    - Ground tactic **[guide]**: hold position and let the enemy advance into
      range, then send your best tank stacks at their tanks.

### 4. The League

1. Hire **Julie-Anne Roxford** (pilot, 120,000); Jack can't reach Antares,
   Orionis or Lyrae. Hire **Klaatoo** (250,000) and send him on courses: you
   need Physics 8+ for Plasma gun and 8/5/8/9 for Mental radar.
2. Buy the bartender's **League tip** (5,000) as soon as it is offered, which
   happens once Orionis or Lyrae is opened. The original withdraws it when the
   League's first attack force launches; the port keeps offering it, as the CD
   release does (`fixes.league_tip`). Optionally pay the Informer 500
   about the Morgrul to use the **Pirate Leader**: the cargo-raid table
   brings free ore, tanks, aircraft and missile launchers.
3. **Antares.**
   - Colonise **Antares 3** and build an Observatory.
   - After the nova warning, send a group to Antares 3 and accept the Erans.
   - Leave Antares before the supernova (560–569 h after that meeting).
4. Reinforce at home. The guide's target is 50 Hunters, 50 Starfighters, 10
   Destroyers and 60 Battle tanks. Expect the Lisonian wave (20H, 20SF, 3D,
   30Tr, 15T, 5Air) 200–249 h after first League contact.
5. Deploy Spy satellites on **Orionis 3, Lyrae 3, Lyrae 5 and Lyrae 7**.
   Then send a trade group to each to make contact. You need contact with
   both the Lisonians and the Undorlings for the Eran's job.
6. Talk to the **Eran** and send him to split off the **Undorlings**. This
   gives Ion cannon, Aircraft and an ally; Lyrae 3 then needs no invasion.
   Research Aircraft and build ~20, fully equipped.
7. Invade **Orionis 3**, **Lyrae 5** and **Lyrae 7**, rebuilding between
   attacks. Lyrae 7 is the strongest: 40H, 30SF, 2D, 1C in space; 70Tr, 40T
   and 10Air on the ground.
   - Ground tactic **[guide]**: flank their aircraft with yours and hit the
     weakest stack first.
8. The last of these victories gives the League's surrender: Cruiser, and
   Rigel opens.

### 5. Earth

1. Rebuild with Cruisers. Space station count limits parallel builds. The
   guide's final fleet is 50 Hunters, 50 Starfighters, 10 Destroyers and 10
   Cruisers.
2. Land on (or put a satellite on) **Rigel 1**. Research **Plasma gun** and
   **Missile launcher**, then **Mental radar** (Klaatoo).
3. Build Missile launchers; 100 is the useful cap per battle. The guide won
   with 50.
4. When the Mental radar reveals **Rigel 2**, deploy a Spy satellite there
   and send a trade group. Give the Syonians the final answer, "Thank you
   for your confidence…": Energy shield, and the Sun system opens.
5. Deploy a **Spy satellite on Earth** and wait for intel 40 after contact.
   Earth's waves follow 1,000–1,199 h after contact.
   - Earth's garrison: 80H, 80SF, 7D, 5C in space; 30Tr, 90T, 50Air, 30ML on
     the ground.
   - Optionally, after the Treasonable appears, pay the Bounty-Hunter
     100,000 to remove part of it.
6. **Invade Earth.** Winning the ground battle ends the game.

## Story events

The game is a fixed chain of timed and triggered events
(`Sim_ProcessStoryTimers`; dialogue outcomes in `alien_talk_answer`). All
**[src]** unless marked.

Hours are game hours. "Researchable" means the product's state becomes "can
be developed"/"can be analysed"; you still have to research it. New research
ideas only appear while a developer is hired and present (not at university,
not on loan to the Jaanosians).

*Story stage* (`Game::StoryStage`) caps developer skills:

| Stage | Reached when |
|---|---|
| 1 | Start |
| 2 | Jaanosian base destroyed |
| 3 | Space win against Morgrul wave 2 |
| 4 | Morgrul wave 3 beaten |
| 5 | Morgruls defeated |
| 6 | League defeated |

### Act 1: Amnesty

1. **Satellite lost.**
   - A satellite launched from stock survives only on New Earth or its moons
     (Apollo); anywhere else it is always destroyed
     (`Satellite_DeployStockSatellite`).
   - The first loss makes Satellite carrier researchable about 15–35 hours
     later.
   - Carrier-deployed satellites are fine. A normal satellite deployed on an
     alien-owned body is lost; use a Spy satellite there.
2. **Ore found.** When a satellite's survey reveals a mineable body, Miner
   station becomes researchable (20–59 h).
3. **Miner station researched.** Transfer ship becomes researchable
   (30–39 h).
4. **Miner station placed** on a surveyed habitable body. Control Centre
   becomes researchable (20–59 h).
5. **Jaanosian deadline.**
   - At a new game a timer is set to 6,000–6,399 hours, firing between
     2928-04-23 and 2928-05-10.
   - **Meet the Jaanosians before 2928-04-23** (the guide's 2928-05-05 is not
     safe) **[fixed]**. To meet them, a non-carrier group must arrive at Jade.
   - Jade is hidden until New Earth's observatory finds it (1/50 per day).
     In about 1 game in 150 that doesn't happen before the deadline and the
     game can't be won. The port finds Jade on 2928-04-01 if it is still
     hidden (`fixes.find_jade`, `src/game/fixes.h`); the port menu's Fixes
     turn that off.
   - First contact replaces the deadline with contact + ~2,590–2,808 h, so
     meeting them later pushes the whole war later.
   - First contact makes Communicator researchable (20–39 h).
   - If the timer fires before contact, or without Communicator researched,
     you get the undecodable "message from deep space" and the S.O.S. below
     happens without you.
6. **Jaanosian meetings.** Each needs **Communicator already researched**
   when it fires; otherwise it is lost for good.

   | Meeting | When (after first contact) | Answer to give | Result |
   |---|---|---|---|
   | 1 | 650–719 h (counts down only while a developer is present) | Agree to send your scientist | Developer away 60–89 h, then **Trade ship** granted. Without a Trade ship, Galleon never becomes researchable |
   | 2 | 720–769 h after meeting 1 | Buy the device (16,000) | **Hyperspace drive** becomes researchable after the base falls. The only source of Hyperspace drive |
   | 3 (S.O.S.) | 1,220–1,319 h after meeting 2 | "O.K." or "Send us the plans" | **Hunter** and **Laser cannon** granted, army groups unlocked. The only source of both |

   The meetings are timers, not consequences of earlier answers. The guide's
   "accept meeting 1 to get meeting 2" is wrong **[fixed]**.
7. **Jaanosian base destroyed.**
   - Happens 200–249 h after the S.O.S. and can't be prevented. The Morgruls
     take Jade and the story stage becomes 2.
   - Landing on Jade afterwards makes **Radio** researchable.
   - If you bought the device, Hyperspace drive becomes researchable
     300–349 h after the fall.
8. **Morgrul waves 1 and 2** reach New Earth 600–649 h and 1,800–1,999 h
   after the fall (sizes in War).
   - Your first won battle of any kind makes Twin laser gun researchable
     (20–39 h).
   - Your first won space battle makes Trooper researchable (90–139 h). The
     guide tied Trooper to wave 2 **[fixed]**.
   - A *space* win against wave 2 makes Tractor beam researchable and sets
     story stage 3.
9. **Galleon** becomes researchable 50–119 h after Hyperspace drive (or
   200–229 h after Twin laser), but only once Trade ship is done.
10. **First hyperspace jump** by any group plays the "leaving the system"
    scene.

### Act 2: Phoenix and the Morgruls

11. **Kalls (Phoenix 3).**
    - First contact opens a talk at once. Ask "When are we receiving your
      weapons?": **Starfighter** becomes researchable and you get 10
      Starfighters.
    - Contact also starts the **alliance clock**. After 1,500–1,999 h the
      Kall fleet (force 2) moves to Phoenix 5. After a further 4,000–4,049 h
      the Kalls turn hostile, wipe out the Phelonians, and 300–349 h later
      attack New Earth together with Morgrul force 4.
12. **Phelonians (Phoenix 5**, hidden until found by the Informer tip, a
    group flying in Phoenix or an observatory).
    - They call 20–39 h after contact. The battle-tank answer grants
      **Battle tank** and takes **half of your New Earth Energon stock**, not
      10,000 **[fixed]**.
    - The other answers sell up to 10,000 Energon at 10–12 credits per tonne.
    - Do this before the alliance clock runs out.
13. **Phoenix S.O.S.** Arrives 100–119 h after Morgrul wave 2 is sent, only
    if Radio is researched.
14. **Drifting Morgrul ship.**
    - After the S.O.S., have a group arrive at Phoenix 1 while Phoenix 1 is
      the selected planet (`Sim_ProcessTravellingGroups`). In the original
      any group arriving anywhere triggers it while Phoenix 1 is selected;
      the port needs a group arriving at Phoenix 1, whatever is selected
      (`fixes.drifting_ship`). Phoenix 1 is hidden until found. This makes
      **Pirate ship** researchable. It happens once.
    - **Answer "Tell me, what are your plans?"** This cancels the alliance
      clock and sends Kall force 2 (15 Hunters, 10 Starfighters, 10 Troopers,
      35 Tanks) to Mirach 3 to fight alongside you.
    - "All right" only delays Morgrul wave 3 by 250 h.
    - The cancel only sticks if the first Kall timer has already fired.
      **Don't go to Phoenix 1 until the Kall fleet is at Phoenix 5.**
      Otherwise the Kall–Morgrul alliance still happens.
15. **Morgrul wave 3** is sent 1,100–1,149 h after wave 2. Beating it (space
    or ground):
    - The **Spy** ("Stranger") appears in Space Local.
    - **Spy satellite** becomes researchable after 30–39 h, and **Spy ship**
      after 500–599 h.
    - Story stage becomes 4.
16. **Mirach.**
    - Revealed by hiring the Spy (30,000+) and asking "Do you know anything
      important?", or 200 h after the Spy-Hunter kills him.
    - Landing on, or deploying any satellite on, **Mirach 2 Moon 1**
      (Explorer 1) makes **Missile** and **Destroyer** researchable.
    - Destroyer done makes **Space station** researchable (50–119 h).
17. **Morgrul defeat.** Win the ground battle on **Mirach 3**. Results:
    - Antares, Orionis and Lyrae become known.
    - The Kalls become allies.
    - The Pirate Leader appears in Space Local.
    - Story stage becomes 5.

    Ion cannon does *not* come from this **[fixed]**. If you wait, Morgrul
    wave 4 (8,000–8,499 h after wave 3) and wave 5, which has Aircraft, come
    while they stay hostile.

### Act 3: Antares and the League

18. **Antares.**
    - First arrival makes **Solar plant** researchable (150–219 h).
    - An observatory anywhere in Antares (so a colony, i.e. William Forbs)
      gives the nova warning 50 h later.
    - After the warning, any group arriving at your Antares colony meets the
      Erans. Answer **"Yes! My people will welcome you"**: the Erans become
      allies and the **Eran** joins Space Local.
    - The supernova comes 1,000–1,099 h after the warning, or 560–569 h after
      the Eran meeting. It deletes everything in Antares, your groups
      included. **Get out.**
19. **League contact.**
    - Your first group to reach a League race makes *that* race hostile.
    - The Lisonian attack is sent 200–249 h after the first League contact.
      This is also the deadline for the bartender's 5,000 tip.
    - League home worlds: Druedians Orionis 3, Undorlings Lyrae 3, Hirachi
      Lyrae 5, Lisonians Lyrae 7.
    - The "type the date" question on first entering Orionis is copy
      protection; the port bypasses it.
20. **Breaking the League.**
    - Requirements: the bartender's League tip, plus contact with both the
      Lisonians and the Undorlings.
    - Send the Eran to the **Undorlings**. 100–199 h later they become
      allies, **Ion cannon** is granted, **Aircraft** becomes researchable,
      and the Undorling joins the pub.
21. **League defeat.**
    - Win the ground battle on the home world of every League race that is
      not allied. With the Undorlings split off, that is Orionis 3, Lyrae 5
      and Lyrae 7. The guide said all four **[fixed]**.
    - Results: **Cruiser** granted, Rigel becomes known, story stage 6.
22. **Meteor disintegrator** becomes researchable 250–349 h after a meteor
    destroys a building. It is optional.

### Act 4: Rigel and Earth

23. **Rigel 1.** Landing or deploying any satellite there makes **Plasma
    gun** and **Missile launcher** researchable, and **Mental radar**
    researchable after 100–119 h.
24. **Rigel 2** is revealed only by the Mental radar: 1/10 per day once
    Rigel has been explored.
25. **Syonians (Rigel 2).** Contact needs a non-carrier group. The final
    answer, "Thank you for your confidence…", grants **Energy shield**,
    reveals the **Sun** system and puts the Bounty-Hunter in Space Local.
26. **Earth.**
    - First contact declares war; Earth waves follow 1,000–1,199 h later and
      then every 2,000–2,499 h.
    - An army group arriving at Earth, or a space win in the Sun system,
      brings the Treasonable to the pub. The Bounty-Hunter route only thins
      Earth's garrison.
27. **Endings.**
    - **Victory**: win the ground battle on Earth (Sun 3). The game exits
      with code 10, which runs VICTORY.
    - **Defeat**: lose a ground battle on New Earth. Exit code 15, death
      ending.

### Invading a planet

- **Army group.** The Invade button needs an **Army** group at the planet.
- **Intel level.** The planet's intel level must be at least 40
  (`Launch_MapScreen`).
  - On alien worlds only a **Spy satellite** raises it, by 2–6 per day.
  - It stops at 31 until you have met the owning race.
  - So: deploy a spy satellite, make contact, then wait until it reaches 40.
- **Commander.** You need Kyle Douglas or MACH, and the group needs a
  Destroyer or Cruiser to have left its home system.
- **Allies.** Invading a race makes it hostile. Never invade an ally.

## Staff mechanics

All **[src]** unless marked.

- **Hiring.** You can only replace a commander with a better one; a cheaper or
  equal candidate is refused (`Hire_CheckHire`). A replaced commander keeps
  their experience and skills if rehired (`Hire_HireScreen`).
- **Experience.** Every hired commander starts at their level (10/20/40) and
  gains +1 with a 1% chance per game hour, up to their ability cap (30/70/90).
  Battles give no experience (`Sim_GrowStaffExperience`).
- **University** (advisor screen, `Advisor_AdvisorScreen`,
  `Sim_GrowStaffExperience`).
  - Cost: (Random(10) + current experience) × 1,000.
  - Away for 50–69 hours. Only one commander can be away at a time.
  - Returns with +105–108 experience, i.e. at the cap after one course.
  - Refused once experience is at the cap ("I don't think I could become more
    skilled"). The guide's "veterans of many battles can't go" is wrong
    **[fixed]**.
  - Developer courses add Maths & physics M+2 P+1, Physics & electronics
    P+2 E+1, Electronics & maths M+1 E+2, A.I. A+2. Each skill is capped by
    the developer-skills table above, indexed by the current **story stage**
    (not by courses taken). Thomas Dempsey's cap is 5/6/5/2 at stage 1,
    6/7/7/5 at stage 2 and 7/7/7/7 from stage 3 (after the third Morgrul
    force is beaten). Klaatoo's cap is 9 everywhere.
  - Destroying the University does not stop courses.
- **Research.** A product takes DevTime / developer experience hours
  (`Sim_ProgressResearch`). If the developer is short of the required skills,
  research stops at about 100 / (shortfall + 1) % complete, where *shortfall*
  is the sum of missing skill points (one missing point stops it at 50%).
  Research also stops while the developer is at university or on loan to the
  Jaanosians (`Sim_ProgressResearch`).
  - **Dempsey tops out at 7 in every skill.** Anti-radiation shield, Solar
    plant, Plasma gun and Mental radar need Physics 8 or more, so you need
    **Klaatoo** (250,000) to finish the game. Mental radar (8/5/8/9) is
    mandatory (it reveals Rigel 2).
- **Builder tier** (`Colonize_ColonizeScreen`, `surface_build_button`).
  - Arwen Nielson cannot found any colony.
  - Elanor Brady can found colonies only in Amnesty and Phoenix.
  - William Forbs can found colonies anywhere.
  - Stadium, Ion cannon and Anti-radiation shield need Elanor or better.
    Energy shield needs William.
  - Construction per hour = (4000 + body factor + experience × 100) / 1500,
    halved for Arwen and doubled for William (`Sim_UpdateBuildings`).
- **Pilot tier** (`Launch_DestinationScreen`).
  - Bruce Jordan cannot send groups out of the current system.
  - Jack Benson can fly between Amnesty, Phoenix and Mirach only.
  - Julie-Anne Roxford can fly anywhere. She is needed for Antares, Orionis,
    Lyrae, Rigel and Sun.
  - Satellite-carrier groups need no pilot.
  - Travel time is inversely proportional to pilot experience + 1
    (`Sim_ProcessTravellingGroups`).
- **Fighter (war commander)** (`Launch_MapScreen`, `SpWar_InitSpaceFleets`).
  - One must be hired to launch a space attack.
  - Ground battles need Kyle Douglas or MACH ("Your fighter not skilled to
    lead a ground battle").
  - Space damage is multiplied by (experience + 200) / 220, or by 2/3 with no
    fighter hired.
- **Pausing.** The clock stops on the advisor dialogue, the disk screen, pub
  conversations and the battle, destination, new-group, colonise and
  alien-contact screens (`Advisor_AdvisorScreen`; the screen list is in
  `src/game/main_program.h`). The pub room itself does not stop it.

## Space Local (the pub)

Ten people (`Pub_People`). The Bartender, Informer and Spy-Hunter
are there from the start; the rest appear on story events. All **[src]**
(`pub_dialog_enter`, `pub.c`, `Sim_ProcessPubTimers`) unless marked.

- **Bartender.** News is free and changes with the story.
  - When the spy is present: ask "Who is that suspicious character?", then
    "How dangerous is he?" or "Could you terminate him?". The bartender
    answers "I'll speak to him", which makes the spy hireable.
  - **League tip (5,000)** is offered once Orionis or Lyrae is opened. The
    original withdraws it for good when the League's first attack force
    launches, 200–249 hours after first contact with any League race
    (`Intel_FirstContact`), and the Eran job is impossible without it. The
    port, like the CD release, keeps offering it (`fixes.league_tip`).
- **Informer.** Each item costs 500 and depends on the story state:
  - Beware of the Morgruls.
  - Morgruls massing near you (after the Jaanosian base falls).
  - Phelonian coordinates (after Phoenix opens while the Phelonians are
    unmet). This also reveals Phoenix 5 on the map.
  - Erans are dangerous (after Antares opens).
  - "The Morgrul in Space Local" (after the Morgruls are defeated). This
    unlocks the Pirate Leader.
- **Spy-Hunter.** Ask "anything unusual?"; while the spy is present and not
  hired he offers to kill him. Killing the spy does **not** block the game:
  Mirach is then discovered automatically 200 hours later
  (`Sim_ProcessStoryTimers`) **[fixed]**.
- **Spy** (shown as "Stranger").
  - Appears when you beat the *third* Morgrul force (the game calls it the
    second invasion), not after the second wave **[fixed]**.
  - Offers of 10,000 or 20,000 are refused; 30,000 or more is accepted.
  - Then ask "Do you know anything important?" to reveal Mirach. Either
    hiring or killing him is required, or Mirach is never found.
  - Once hired he sells reports, delivered 100–199 hours later. The price is
    base × race multiplier.
    - Base: colonies 10,000, space forces 14,000, ground forces 20,000,
      weapons 7,000.
    - Multiplier: Morgruls ×5, Kalls ×3, Phelonians ×2, Erans ×1,
      Lisonians ×10, Undorlings ×8, Hirachi ×7, Druedians ×5, Earthlings ×15.
    - A ground-forces report makes that race's planets show their forces.
  - Spy satellite and Spy ship are *not* unlocked by hiring him. The same
    battle win that makes him appear schedules them after 30–39 and 500–599
    hours **[fixed]**.
- **Pirate Leader** (appears as "Morgrul").
  - Appears when the Morgruls are defeated; pay the Informer 500 about him
    first.
  - Ask "I heard you accept dirty jobs". This starts ten timed cargo deals,
    each announced by radio and orderable only inside its window. Each deal
    can be tried once.
  - Payment: "half the cargo" is always accepted. 10,000 works only for
    kind-1 deals, 50,000 for kind ≤ 2, and 100,000 for any deal.
  - He reports 100–199 hours later and succeeds with the deal's chance.

  | Deal | Opens after asking | Window | Kind | Chance | Cargo |
  |---|---|---|---|---|---|
  | 1 | 15 days | to 2 months | 2 | 70% | Detoxin 31,000, Energon 30,500, Raenium 20,000, Texon 10,000 |
  | 2 | 3 m 1 d | to 3 m 12 d | 3 | 90% | 2 Aircraft |
  | 3 | 4 m | to 5 m | 1 | 100% | Raenium 10,000, Texon 16,000 |
  | 4 | 7 m | to 7 m 20 d | 2 | 60% | Kremir 20,000, Lepitium 13,560, Energon 12,400 |
  | 5 | 8 m | to 8 m 15 d | 3 | 40% | 30 Battle tanks |
  | 6 | 11 m 10 d | ~1 day | 2 | 80% | Energon 36,000, Kremir 40,000, Raenium 20,000 |
  | 7 | 1 y | 1 month | 2 | 80% | Kremir 15,000, Lepitium 15,000, Raenium 14,000, Texon 15,500 |
  | 8 | 1 y 4 m | 1 month | 2 | 65% | 8 Spy ships, 2 Missile launchers |
  | 9 | 1 y 6 m | ~1 day | 2 | 100% | Kremir 2,900, Lepitium 30,000, Energon 25,000, Texon 19,400 |
  | 10 | 2 y 4 m | 2 months | 3 | 60% | 25 Missile launchers |

- **Eran.**
  - Appears after you agree to evacuate the Erans.
  - Offers his job only if you bought the bartender's League tip *and* you
    have met both the Lisonians and the Undorlings.
  - 100–199 hours later the chosen race leaves the League. **Choose the
    Undorlings**: they become allies, you are granted the **Ion cannon**,
    Aircraft becomes researchable (6/7/5/1) and the Undorling joins the pub.
    Choosing the Lisonians gives no technology.
  - Either way the separated race counts as beaten for the League-defeat check
    (`ground_battle_won_story`). Aircraft and Ion cannon come from nowhere
    else.
- **Undorling.** Information only.
- **Treasonable.** Appears when an army group reaches Earth or you win a space
  battle in the Sun system (`Sim_ProcessTravellingGroups`), not after a delay.
  Pay 100,000–200,000 (the amount doesn't matter), then answer "I know a
  bounty-hunter" or "I'll look around" to unlock the Bounty-Hunter's job.
- **Bounty-Hunter.** Appears after the Syonian meeting. His job (100,000)
  needs the Treasonable's request. It is not a no-op **[fixed]**: 100–199
  hours later Earth's garrison loses up to 20 Hunters, 1 Destroyer,
  2 Cruisers, 10 Tanks, 10 Aircraft and 10 Missile launchers. Earth must still
  be invaded.

## Colonies

All **[src]** (`Sim_ProcessBodyEvents` daily update, `surface.c`, `cargo.c`,
`droids.c`, `colonize.c`, `planetinfo.c`) unless marked.

### Starting position

- **Date and money.** The game starts at 2927-08-13 23:00 with 120,000
  credits. The first hour crosses midnight and pays the first day's tax
  (1,620), which is the guide's 121,620.
- **New Earth** (Amnesty 5) starts with 30,000 people, tax Normal, 2 droids
  working and 1 in stock.
- **Starting buildings** (`SAVE/INIT`): Command centre, 3 Nuclear generators,
  Radar, Derrick, 2 Mines, Observatory, 4 Buildings, Leisure centre, Church,
  Spaceport, University, Park, Hospital, Farm, Food resources, Builder plant
  and Medicine plant. There is no Vehicle plant and no Stadium.

### Founding a colony

1. Place a Miner station on a surveyed, habitable body. The first time, this
   makes Control Centre researchable.
2. Research Control Centre.
3. Found the colony with a builder who is allowed there (see Staff
   mechanics). It costs 100,000 plus any starter buildings you choose.

Habitable surfaces are Earth-like, Icy, Desert, Rocky and Tropical.

### Taxes, morale and growth

In the port, clicking the POP row on a colony's planet info screen shows
the numbers below for that colony: capacities, disease timer, Stadium and
University thresholds, target mood and growth (port menu, Game).

- **Tax levels**: None, Very low, Low, Normal, High, Difficult, Very
  difficult, Oppressing (0–7).
- **Daily income per colony**: round(pop × tax × development × (attitude +
  40) / 200,000). Development starts at 20 on a new colony and creeps up to
  60 (New Earth starts at 40).
- **Target attitude** = (7 − tax) × 10, then:
  - +10 each for a powered Park, Church, Leisure centre and Stadium. One of
    each is enough.
  - Population: −10 above 30k, −10 more above 60k, −20 more above 100k, −20
    more above 150k.
  - No Stadium above 50k: −20. No University above 70k: −20. Neither is a
    hard cap **[fixed]**. Without a University, population is hard-capped
    at 80k.
  - Radiation −40. Meteor world −30.
  - Disease without a Medicine plant −10. Short of hospitals, starving, or
    short of housing: −10 / −15 / −10. Each of these caps the target at 25.
- **Attitude effects.** Attitude moves toward the target and stays within
  0–69.
  - Below 30: the population ceiling is halved.
  - Below 20: the colony shrinks.
  - Below 10: a revolt counter starts, and after about 5 days the colony is
    destroyed. A colony under 1,000 people is also destroyed.
- **Rule of thumb.** With all four leisure buildings at Very difficult, the
  target is 50 below 30k people, 40 up to 60k, 30 up to 100k and 10 above.
  So colonies grow well to about 100k and then shrink. **Drop tax to
  Difficult or lower once a colony passes ~60–100k.** The guide's "Very
  difficult always grows" is wrong above 100k **[fixed]**.
- **The full leisure set** (Builder plant, Vehicle plant, Park, Church,
  Leisure centre, Stadium) costs 144,000, not 130,000 **[fixed]**.

### Buildings in practice

- **Prerequisites.**
  - Builder plant is needed for Leisure centre, Stadium, Church, Storage bay,
    Spaceport, Hospital, Vehicle plant and Medicine plant.
  - Vehicle plant is needed for Leisure centre, Stadium, Spaceport and Park.
  - Builder tier: Stadium, Ion cannon and Anti-radiation shield need Elanor
    or better; Energy shield needs William.
- **Surface limits.** Windtrap only on Icy or Desert worlds (double output
  on Desert). Park only on Earth-like, Tropical or Desert worlds.
- **Housing.** Command centre 10,000, Building 7,000. Placing a Building also
  adds 800–1,299 people at once.
- **Food.** Command centre 20,000, Farm 8,000, Food resources 20,000. Both
  food buildings are 2×2, so Food resources gives 2.5× the food per tile.
- **Health.** Command centre 20,000, plus 20,000 per Hospital. On a diseased
  world every person counts double, even with a Medicine plant (the plant
  only removes the −10 morale penalty).
- **Energy.** A generator's output is rolled when it is placed: base ×
  (factor × 40 + Random(80))%, where factor is 2 for a Windtrap on Desert
  and 1 otherwise.
  - Nuclear generator: 1,200–3,570 kWh.
  - Windtrap: 480–1,428 kWh on Icy, 960–1,908 kWh on Desert.
  - A new Command centre gives 240–714 kWh.
- **Power shortage.** When supply drops below half of demand, buildings
  switch off lowest priority first:
  1. Leisure buildings, Storage bay
  2. Radar, Ion cannon
  3. Observatory, University, both plants
  4. Spaceport, Hospital, food buildings, Medicine plant
  5. Mines, Derricks, Buildings, Miner station
  6. Shields

  An unpowered building counts for nothing. A shortage therefore hits morale
  first (the guide said Radar goes first **[fixed]**).
- **Storage** (per ore). Mine 10,000, Miner station 10,000, Derrick 5,000,
  Storage bay 20,000. The Command centre gives none. Production stops when
  storage is full. Mines beyond the ninth still add storage, which makes them
  cheaper storage than bays.
- **Mining.**
  - Each mine needs a droid; droids per body are capped at 9 and at mines +
    miner stations.
  - Ore per hour per droid = deposit / 10 (the world table below).
  - Detoxin comes only from Derricks: each powered Derrick has a 35% chance
    per hour to make deposit / 10. There is no derrick limit and no droid is
    needed, so build all Derricks on New Earth (Detoxin 7).
  - A body with only a Miner station mines in 30% of hours and can't take
    more droids.
- **Moving goods.** Moving droids or other items to or from a colony needs a
  Spaceport at that colony; ore doesn't. New Earth has a Spaceport.
- **Observatory.** Each day there is a 1-in-50 chance it reveals one hidden
  planet in its system; groups flying in a system also find planets (1 in 5
  per day). Hidden at the start:
  - Amnesty 1, Amnesty 2, Amnesty 3 and **Jade**
  - Phoenix 1, Phoenix 4 and Phoenix 5
  - Mirach 1, Mirach 4 and Mirach 5
  - Lyrae 6
  - Rigel 2 (Mental radar only)

  New Earth's starting observatory is what reveals Jade, so never demolish
  it before then.
- **Radiation.** Colonies on planets 1 and 2 of a system (moons included)
  suffer radiation unless a powered Anti-radiation shield stands there. The
  first deaths make the shield researchable 50–74 hours later. It needs
  Physics 8, i.e. Klaatoo.
- **Meteors.** Only Orionis 2, Orionis 3, Orionis 1 Moon 2, Orionis 2 Moon 1
  and Lyrae 5 are hit: 1 in 10 per day, never while you are viewing that
  surface. A hit that destroys a building schedules the Meteor disintegrator
  research.
- **University.** It has no effect on courses; the game never reads the
  count.

### Exploits and bugs to know

- **Mines only work in the currently selected star system** (`sim.c`,
  `Sim_MineBody`). The mining pass reads bodies from the system on screen, so
  every other system's mines (New Earth's included) stop while you view
  another system. **Leave the map on Amnesty while waiting.** The port mines
  every system whatever the map shows (`fixes.mine_all_systems`,
  `src/game/fixes.h`).
- **Droids stay when mines go.** Demolishing a mine (2,000 credits) never
  lowers the droid count, so 9 droids keep mining with 1 mine left. You lose
  that mine's storage. The port puts the droid back into the colony's stock
  (`fixes.demolish_droids`).
- **Cargo is capped by planet storage.** A ship can't load more of one ore
  than the source planet's storage capacity, and unloading is capped the same
  way (`Cargo_CargoScreen`). The port caps loading by the ship's room only
  (`fixes.cargo_planet_cap`).
- **Miner-station-only bodies stop mining.** They have no planet force, so
  their storage is read from fleet group 32's record, which starts at 0 and
  is overwritten whenever a body without a planet force has its power
  balanced (conquering a planet sets it to 0). While it is 0, mining and
  pickup at every such body stop; this is probably the guide's "station
  storage full" bug. The port gives each one a Miner station's 10,000 t
  (`fixes.station_storage`).
- **Losing a space battle at New Earth deletes your trade and carrier
  groups** there (see War).

### Useful worlds

Per-mine output = deposit / 10 tonnes per hour per droid. The D column is the
Derrick's Detoxin deposit. *Needs*:

- **AR**: radiation, needs an Anti-radiation shield.
- **MP(n)**: disease starts n days after colonising (Medicine plant and
  double hospitals).
- **MET**: meteor world.

Owners are as at the start of the game. From the `Galaxy::*Bodies` tables
**[src]**.

Habitable:

| System | World | Surface | Owner | D | E | K | L | R | T | Needs |
|---|---|---|---|---|---|---|---|---|---|---|
| Amnesty | New Earth | Earth-like | you | 7 | 3 | 2 | 3 | – | – | |
| | Apollo (New Earth moon) | Desert | | 5 | 1 | 3 | 1 | 9 | – | |
| | Vostok (Amnesty 2 moon) | Tropical | | – | – | – | – | – | – | |
| | Mir (Amnesty 2 moon) | Desert | | 5 | 7 | 1 | 2 | 5 | – | MP(10) |
| | Syren (Zeus moon) | Desert | | – | – | – | – | – | – | |
| | Penelope (Zeus moon) | Earth-like | | – | – | – | – | 9 | 7 | |
| | Jade | Tropical | Jaanosians | 9 | 2 | 2 | 3 | – | 1 | MP(90) |
| | Wright (Jade moon) | Rocky | Jaanosians | – | – | – | – | – | – | MP(55) |
| | Russel (Jade moon) | Earth-like | Jaanosians | 2 | 9 | – | – | 8 | 4 | |
| | Hartmann (Jade moon) | Icy | Jaanosians | – | – | – | – | – | – | |
| | Kepler (Jade moon) | Desert | Jaanosians | – | – | 9 | 3 | 3 | 6 | MP(99) |
| Phoenix | Phoenix 2 | Tropical | | – | 2 | 5 | 3 | 2 | 3 | AR |
| | Phoenix 3 | Earth-like | Kalls | 6 | 7 | 7 | 3 | 5 | 3 | MP(50) |
| | Phoenix 3 Moon 2 | Icy | Kalls | – | – | – | – | – | – | |
| | Phoenix 4 Moon 1 | Rocky | Kalls | 1 | – | – | 2 | – | 3 | |
| | Phoenix 5 | Desert | Phelonians | – | – | – | – | 1 | – | |
| | Phoenix 5 Moon 1 | Rocky | Phelonians | – | – | – | – | – | – | |
| Mirach | Mirach 2 | Desert | | – | – | – | – | – | – | AR |
| | Mirach 3 | Tropical | Morgruls | 4 | 4 | 4 | 4 | 2 | 3 | MP(30) |
| | Mirach 3 Moon 1 | Desert | Morgruls | – | – | – | – | – | – | |
| | Mirach 4 Moon 1 | Earth-like | Morgruls | 3 | 3 | 2 | 1 | 3 | 8 | MP(10) |
| | Mirach 4 Moon 4 | Icy | Morgruls | – | – | – | – | – | – | |
| | Mirach 5 Moon 1 | Rocky | Morgruls | – | – | – | – | – | – | |
| Antares | Antares 2 | Earth-like | Erans | 2 | 3 | 2 | 1 | 2 | 1 | AR, MP(21) |
| | Antares 2 Moon 1 | Desert | | – | – | – | – | – | – | AR, MP(66) |
| | Antares 3 | Rocky | | 3 | 7 | 3 | 1 | 6 | – | MP(80) |
| Orionis | Orionis 2 | Desert | Lisonians | 3 | 1 | – | – | – | 2 | AR, MET |
| | Orionis 3 | Earth-like | Druedians | 9 | 8 | 6 | 5 | 9 | 9 | MET |
| | Orionis 1 Moon 2 | Tropical | Hirachi | – | – | – | – | – | – | AR, MET |
| | Orionis 2 Moon 1 | Tropical | Druedians | – | – | – | – | – | – | AR, MET |
| | Orionis 2 Moon 4 | Earth-like | | – | – | – | – | – | – | AR |
| Lyrae | Lyrae 2 | Rocky | | 8 | – | – | – | – | 3 | AR |
| | Lyrae 3 | Earth-like | Undorlings | 9 | 9 | 9 | 6 | 3 | 9 | MP(10) |
| | Lyrae 3 Moon 1 | Desert | Lisonians | – | – | – | – | – | – | |
| | Lyrae 3 Moon 4 | Tropical | | – | – | – | – | – | – | |
| | Lyrae 4 | Earth-like | Undorlings | 2 | 2 | 2 | – | – | – | |
| | Lyrae 5 | Icy | Hirachi | – | – | – | – | – | – | MET |
| | Lyrae 5 Moon 3 | Tropical | Druedians | 3 | 5 | 4 | 6 | 4 | – | |
| | Lyrae 7 | Earth-like | Lisonians | 4 | 4 | 2 | 3 | 7 | 1 | MP(90) |
| Sun | Earth | Earth-like | Earthlings | 3 | 2 | – | – | 2 | 2 | |
| | Moon | Rocky | Earthlings | – | – | 1 | – | 2 | – | |
| | Mars | Rocky | Earthlings | – | 1 | 2 | 1 | – | – | |

Mineable only (Miner station; no Detoxin):

| World | E | K | L | R | T |
|---|---|---|---|---|---|
| Barada (Klatoo moon) | 5 | – | – | – | 2 |
| Amnesty 2 | – | 2 | 2 | 3 | 4 |
| Ithaca (Zeus moon) | 2 | 3 | – | 2 | 2 |
| Gallilei (Jade moon) | 2 | 7 | – | – | 6 |
| Phoenix 1 | 5 | 2 | 5 | 3 | 2 |
| Mirach 4 | 2 | – | 6 | – | 9 |
| Mirach 5 Moon 3 | 2 | – | 3 | – | 7 |
| Antares 2 Moon 2 | – | – | 4 | – | – |
| Orionis 1 Moon 1 | 4 | 3 | 2 | 2 | 2 |
| Orionis 2 Moon 3 | 2 | 2 | – | 2 | – |
| Lyrae 3 Moon 3 | 1 | – | – | 2 | – |
| Lyrae 5 Moon 1 | 3 | – | – | – | 2 |
| Lyrae 5 Moon 6 | 5 | 6 | – | – | – |
| Lyrae 7 Moon 1 | – | – | – | 7 | 4 |
| Rigel 1 | 4 | 2 | – | 1 | 3 |

Corrections to the guide's tables **[fixed]**:

- Jade's Texon is 1, not 9. Mars has no Texon.
- Wright, Antares 3 and Lyrae 7 need a Medicine plant.
- Antares 2 Moon 1 was missing.
- The guide's per-world AR/MP marks were incomplete; the table above
  replaces them.

## War

All **[src]** (`spwar.c`, `grwar.c`, `grwarunits.c`, `launch.c`, `intel.c`,
`Sim_CheckEnemyAttacks`) unless marked.

### Groups and travel

- **Group types.** Army, Trade, Pirate, Satellite carrier, plus the
  immovable planet forces.
- **Leaving a star system** (`Launch_DestinationScreen`):
  - Army and Pirate groups need a Destroyer or Cruiser in the group.
  - Trade groups need a Galleon.
  - Carrier groups need Hyperspace drive *or* Galleon researched.
  - Every group except carriers needs a hired pilot to launch, and the pilot
    tier limits where it can go (see Staff mechanics).
- **Trade capacity** (`Cargo::UnitSpace`).
  - Transfer ship (Sloop): 500 t, 1 droid, 1 miner station.
  - Trade ship: 2,000 t, 4 droids, 2 miner stations.
  - Pirate ship: 1,000 t, 2 droids, no miner stations.
  - Galleon: 8,000 t, 16 droids, 5 miner stations.
  - A droid takes 500 t. Trade groups can also carry ground units and
    weapons.
- **Satellite carrier**: one satellite of any kind per carrier
  (`Groups_GroupsScreen`).
- **Space station**: Destroyers and Cruisers appear in the shop only once
  Space station is researched. Each order is capped at one big ship per
  station you own (`Shop_ShopScreen`), so more stations means more
  parallel builds.
- **Attacking.**
  - A space attack (ATTACK on an alien fleet in the star-map zoom) needs a
    hired war commander. It is a space battle only.
  - An invasion (GROUND WAR in the star-map zoom, with your Army group
    selected at a body that is not allied and whose intel level is at
    least 40, shown by " Your satellite found ... colony at ...") needs
    Kyle Douglas or MACH (`Launch_MapScreen`). A won space battle leads
    to the ground battle; a lost one ends it.
  - Either attack makes the target race hostile.
  - Defending, it is the other way round: a lost space battle leads to a
    ground battle at the colony, a won one ends it.

### Weapons and loadouts

Full loadouts, from the group slot layouts:

| Unit | Full loadout | Space damage per hit / ground attack |
|---|---|---|
| Hunter | 2 Laser, 1 Twin | 11 |
| Starfighter | 4 Twin, 1 Missile | 68 |
| Destroyer | 2 Twin, 4 Missile, 1 Plasma | 374 |
| Cruiser | 6 Missile, 3 Plasma | 840 |
| Trooper | 1 Laser | 2 |
| Battle tank | 3 Laser, 1 Twin | 13 |
| Aircraft | 4 Twin, 1 Missile | 28 (its missile is unused on the ground) |
| Missile launcher | 5 Missile | 45 shell power |

Weapons are pooled per group slot: damage = (lasers×2 + twins×7 +
missiles×40 + plasma×200) ÷ ships in the slot. Unarmed ships therefore dilute
the whole slot. **Equip everything fully.**

### Space battle

The fight is automatic: the only control is RETREAT, which counts as a loss.

- Hit points: Hunter 800, Starfighter 2,000, Destroyer 5,000, Cruiser 12,000.
- Every 4th frame each ship picks a random living enemy and hits 11% of the
  time. Up to 500 ships per side.
- Small ships do 1/3 damage to Destroyers and Cruisers. The guide said big
  ships are immune; they are not, just very hard to kill with small ships
  **[fixed]**. Only Destroyers and Cruisers decide battles between big ships.
- Small ship against small ship also has an instant-kill roll: Hunter on
  Hunter 4%, Hunter on Starfighter 2%, Starfighter on Hunter 7%, Starfighter
  on Starfighter 4%. Big ships have none.
- Cheap Hunters soak up random fire. Your damage is multiplied by the war
  commander's (experience + 200) / 220, or by 2/3 if none is hired: Kyle
  Douglas is hired at experience 20 (×1.00) and grows to 70 (×1.23), MACH
  starts at 40 and grows to 90 (×1.32). Enemies get no modifier; a fully
  equipped ship of yours does the same damage as the enemy's of its class.
- One fifth of the destroyed ships survive, on both sides. Surviving enemies
  go home. Retreat counts as a loss.
- **Your side**: Army, Pirate and planet-force groups landed or in orbit at
  that planet (any moon), plus allied races (status 6) stationed there.
- **Enemy side**: hostile forces at the planet plus the ships stored on the
  planet and all its moons.

### Ground battle

- **Forces.** Your side is the ground units of your groups at the battle
  body (planet and moon must match: an Army group's ground row comes along
  on an invasion) or, defending, the colony's planet force, plus allied
  forces there. The enemy has every hostile force at the body plus the
  body's ground stores.
- **Slots.** Up to 20 slots per side. Each slot holds 30 Troopers, 30 Tanks,
  10 Aircraft or 5 Missile launchers, so 100 Missile launchers is the useful
  maximum. Unassigned units stay in reserve and survive. The assignment
  screen pre-fills the slots by type (troopers, tanks, aircraft,
  launchers); "Remove unit" returns a slot to the pool, the number adds
  (left button) or takes off (right button) one unit, and the ADD icons
  add a slot of that type.
- **Formation.** The field is 16 × 9 tiles. Slot order is the formation:
  slots 1–9 stand in the rearmost column (x = 2), 10–18 in the next, 19–20
  in the next, one row each, centred. The enemy stands from x = 13 leftwards.
  Launchers at the rear and aircraft in front work best.
- **Behaviour.** Every unit fires at any enemy in range whatever its order
  (the highest-numbered one in range). Your units start standing still;
  orders are Move to a tile or Attack a stack. Every enemy stack goes for
  your nearest stack and stops as soon as it is in range.
- **Ranges.** Trooper adjacent only; Tank ≤2 (Manhattan); Aircraft ≤2 in any
  direction, so it hits from the diagonal (2, 2) where a tank can't answer;
  Missile launcher shells at 2–4 tiles. A Missile launcher next to an enemy
  is almost harmless.
- **Damage per hit.** Units in the stack × average attack × target weakness
  ÷ 500, clamped to 1–15. Attack per unit: Trooper 2, Tank 13, Aircraft 28.
  Weakness: Trooper 40, Tank 20, Aircraft 5, Missile launcher 10. A
  launcher shell does units × 45 × weakness ÷ 180 on the target's tile.
  - A 10-Aircraft stack kills about 11 Tanks per hit; a 30-Tank stack kills
    about 3 Aircraft.
  - Troopers are nearly useless.
  - Missile launchers outrange everything.
- **Timing.** Frames between shots: Trooper 30, Tank 20, Aircraft 10,
  Launcher 60. Speed: one pixel per 7 (Trooper), 6 (Tank), 4 (Aircraft) or
  10 (Launcher) frames, so a tank crosses a tile in 112 frames.
- **Stun.** A stack that is hit plays its hit animation for about 10 frames
  and can't fire meanwhile. An aircraft stack fires every ~11 frames, so it
  keeps its target stunned for good: 30 tanks facing 5 aircraft on the
  diagonal die without a shot. Every hit counts at least 1, so troopers
  stun too. This is why aircraft decide ground battles, and why tanks alone
  lose to tanks plus troopers.
- **Chance.** The random generator only decides which side moves first in a
  round and ties, so the same forces fight almost the same way every time;
  small changes in numbers or formation can turn it round.
- **Result.** You win when every enemy slot is empty and lose when yours are
  (or on retreat). The base never fights.

### Attacks on New Earth

- Every enemy wave targets New Earth. No other colony is ever attacked.
- A space battle always happens; with no warships there you lose it at once.
  - Win it: no ground battle.
  - Lose it: ground battle. Losing *that* at New Earth is game over.
- **Losing a space battle at New Earth disbands every group landed or in orbit
  there.** Army and pirate units merge into the planet force; trade and
  carrier groups are lost with their cargo (`Intel_SpaceBattleLost`). This is
  the guide's "all my groups disappeared" bug, which it blamed on the
  defensive ground win **[fixed]**. Keep trade and carrier groups away from
  New Earth when a wave is due.
- **Garrisons after a space battle.** The planet and its moons count their
  surviving garrison ships by one id that only moon 1 matches: the planet
  and the other moons keep a fifth of their garrison whatever survived, and
  moon 1 also gets their survivors (`SpWar_ApplySpaceLosses`). A won battle
  destroys them all, so this only matters after a lost battle or a retreat.
  The port counts each body's own (`fixes.garrison_losses`).
- No race builds units. All enemy forces are preset; garrisons grow only when
  survivors return home. The guide's "the AI out-builds you" is wrong
  **[fixed]**.
- Each wave waits Random(100 − race speed) + 5 hours at New Earth before
  attacking.
- Ion cannon and Energy shield have **no combat effect**: the base slot never
  fires, and the shield is only listed on the planet-info screen
  (`PlanetInfo_DrawPlanetInfoText`). Skip them.

Enemy waves (H Hunter, SF Starfighter, D Destroyer, C Cruiser, Tr Trooper,
T Tank, Air Aircraft, ML Missile launcher). The guide's figures were the
survivors shown after the battle; these are the real sizes **[fixed]**.

| Wave | Force | Launches |
|---|---|---|
| Morgrul 1 | 6H, 3Tr | 600–649 h after the Jaanosian base is destroyed |
| Morgrul 2 | 25H, 10Tr | 1,800–1,999 h after the Jaanosian base is destroyed |
| Morgrul 3 | 20H, 10SF, 15Tr, 2T | 1,100–1,149 h after wave 2 |
| Morgrul 4 | 20H, 30SF, 2D, 40Tr, 40T | 8,000–8,499 h after wave 3, if the Morgruls are still hostile |
| Morgrul 5 | 90H, 90SF, 8D, 1C, 70Tr, 50T, 20Air | 4,500–4,599 h after wave 4, if still hostile |
| Lisonian | 20H, 20SF, 3D, 30Tr, 15T, 5Air | 200–249 h after first League contact |
| Undorling | 15H, 20SF, 3D, 10Tr, 20T, 5Air | 1,000–1,499 h after the Lisonian wave, if the Undorlings are still hostile |
| Earth 1 | 80SF, 12D, 3C, 30Tr, 30T, 30Air | 1,000–1,199 h after Earth contact |
| Earth 2 | 50SF, 10D, 6C, 60T, 30Air, 10ML | 2,000–2,499 h after Earth 1 |
| Earth 3 | 10H, 30SF, 20D, 15C, 40Air, 30ML | 2,000–2,499 h after Earth 2 |

Enemy home planets at the start of the game (space includes the moons and
forces stationed there):

| Target | Space | Ground |
|---|---|---|
| Mirach 3 (Morgruls) | 20H, 20SF, 3D | 61Tr, 40T |
| Orionis 3 (Druedians) | 30H, 23SF, 3D | 90Tr, 35T |
| Lyrae 3 (Undorlings) | 55H, 60SF, 12D (+25H, 20SF, 3D Lisonian if hostile) | 150Tr, 110T, 14Air |
| Lyrae 5 (Hirachi) | 20H, 19SF, 3D (+10H, 6SF Druedian moon) | 52Tr, 10T |
| Lyrae 7 (Lisonians) | 40H, 30SF, 2D, 1C | 70Tr, 40T, 10Air |
| Earth | 80H, 80SF, 7D, 5C | 30Tr, 90T, 50Air, 30ML |

Allied forces at a battle site fight for you: the Kalls once allied (their
force 2 is 10 Troopers and 35 Tanks), and the Syonians.

## Reference tables

### Products (research and purchase)

From `Research_Products` **[src]**. *Time* is build time in hours
(the table stores tenths of an hour). *Skills* is the Math/Physics/Electronics/
A.I. level a developer needs to research the product. Ores are in the order
Detoxin, Energon, Kremir, Lepitium, Raenium, Texon. Items marked *built* are
never bought: they unlock a building or a feature.

| # | Name | Price | Ores D/E/K/L/R/T | Time | Skills M/P/E/A | State at start |
|---|---|---|---|---|---|---|
| 1 | Nuclear generator | (building) | | | – | researched |
| 2 | Miner droid | 3,150 | 10/10/5/3/0/0 | 7 | 0/1/1/0 | can be analysed; 1 in stock |
| 3 | Satellite | 780 | 37/31/12/5/0/0 | 5 | 1/1/1/0 | can be developed |
| 4 | Satellite carrier | 1,530 | 55/105/27/18/0/0 | 25 | 1/2/2/0 | unknown |
| 5 | Miner station | 9,700 | 1500/1024/333/212/0/0 | 60 | 0/2/2/1 | unknown |
| 6 | Transfer ship ("Sloop") | 3,800 | 223/521/451/56/0/0 | 50 | 2/2/3/0 | unknown |
| 7 | Control Centre | built | | | 0/4/2/0 | unknown |
| 8 | Communicator | built | | | 1/0/1/2 | unknown |
| 9 | Trade ship | 4,590 | 300/1625/366/355/0/0 | 30 | 3/4/3/0 | unknown |
| 10 | Hunter | 5,700 | 450/1875/406/454/78/0 | 20 | 3/3/3/0 | unknown |
| 11 | Laser cannon | 1,550 | 50/35/16/15/78/0 | 8 | 1/3/1/0 | unknown |
| 12 | Radio | built | | | 1/2/0/0 | unknown |
| 13 | Twin laser gun | 2,650 | 85/75/35/25/118/0 | 13 | 2/3/2/0 | unknown |
| 14 | Hyperspace drive | built | | | 5/5/3/0 | unknown |
| 15 | Galleon | 47,000 | 8750/12670/3226/11000/1778/1936 | 60 | 5/5/4/0 | unknown |
| 16 | Starfighter | 8,400 | 1490/1730/506/490/120/515 | 30 | 2/4/4/0 | unknown |
| 17 | Trooper | 4,500 | 1000/970/60/335/130/236 | 35 | 2/4/3/0 | unknown |
| 18 | Tractor beam | built | | | 1/6/3/0 | unknown |
| 19 | Pirate ship | 7,000 | 1300/1550/600/505/800/50 | 35 | 2/5/5/0 | unknown |
| 20 | Spy satellite | 3,546 | 53/255/80/335/890/900 | 15 | 2/4/5/4 | unknown |
| 21 | Battle tank | 6,000 | 1300/1215/66/95/780/150 | 50 | 3/5/5/0 | unknown |
| 22 | Missile | 3,000 | 40/50/60/50/80/300 | 10 | 3/3/3/3 | unknown |
| 23 | Destroyer | 38,000 | 7500/13500/2056/8000/2000/1530 | 67 | 6/6/5/0 | unknown |
| 24 | Space station | 28,500 | 60000/48000/6590/2585/2000/3270 | 90 | 7/5/7/0 | unknown |
| 25 | Spy ship | 5,600 | 1800/3600/870/800/550/75 | 60 | 6/6/7/6 | unknown |
| 26 | Aircraft | 17,000 | 700/2455/1455/500/200/200 | 35 | 6/7/5/1 | unknown |
| 27 | Solar plant ("Solar sat") | 3,100 | 6000/5100/3335/5667/757/400 | 50 | 7/8/3/3 | unknown |
| 28 | Meteor disintegrator | built | | | 4/7/3/7 | unknown |
| 29 | Ion cannon | (building) | | | 4/6/5/8 | unknown |
| 30 | Anti-radiation shield | (building) | | | 1/8/2/0 | unknown |
| 31 | Plasma gun | 5,000 | 290/35/156/335/599/2300 | 22 | 3/8/4/0 | unknown |
| 32 | Missile launcher ("Missile tank") | 26,000 | 630/510/345/48/90/0 | 50 | 5/6/5/7 | unknown |
| 33 | Cruiser | 340,000 | 10000/18675/12377/6665/1200/2860 | 200 | 6/8/8/3 | unknown |
| 34 | Mental radar | built | | | 8/5/8/9 | unknown |
| 35 | Energy shield | (building) | | | 9/9/6/1 | unknown |

All of the guide's prices, ore costs, times and skill lists agree with the
table. The guide gave the skill needs of Trade ship, Hunter, Laser cannon,
Battle tank, Ion cannon, Cruiser and Energy shield as "unknown" because those
come as gifts in the story; the table has them **[fixed]**. Destroyer and
Cruiser are the only products flagged as built in space (`SpaceBuilt`), so
they need a Space station.

### Buildings

From `Surface_Buildings` **[src]**. *Energy* is consumption in kWh;
*Output* is the nominal energy output of generators (see the colony section
for what actually happens). *Needs* is the product that must be researched
first.

| Building | Cost | Size | Workers | Energy | Output | Needs |
|---|---|---|---|---|---|---|
| Command centre | 100,000 | 4×4 | 900 | 0 | 600 | Control Centre (#7) off New Earth |
| Nuclear generator | 35,000 | 4×4 | 120 | 0 | 3,000 | |
| Windtrap | 10,000 | 2×2 | 15 | 0 | 1,200 | |
| Mine | 6,000 | 3×3 | 0 | 1,100 | | |
| Derrick | 7,000 | 3×2 | 0 | 750 | | |
| Observatory | 42,000 | 3×3 | 50 | 750 | | |
| Building (housing) | 5,000 | 2×2 | 0 | 345 | | |
| Leisure centre | 27,000 | 4×3 | 34 | 240 | | |
| Stadium | 36,000 | 4×3 | 73 | 445 | | |
| Church | 17,000 | 2×2 | 7 | 115 | | |
| Storage bay | 15,000 | 4×4 | 114 | 450 | | |
| Spaceport | 50,000 | 4×4 | 343 | 753 | | |
| University | 53,000 | 4×4 | 72 | 234 | | |
| Park | 8,000 | 2×2 | 0 | 0 | | |
| Hospital | 7,000 | 4×4 | 156 | 330 | | |
| Farm | 6,000 | 2×2 | 54 | 785 | | |
| Radar | 15,000 | 3×3 | 0 | 750 | | |
| Ion cannon | 120,000 | 2×2 | 0 | 730 | | Ion cannon (#29) |
| Anti-radiation shield | 70,000 | 2×2 | 0 | 2,000 | | Anti rad shield (#30) |
| Energy shield | 170,000 | 4×2 | 0 | 3,000 | | Energy shield (#35) |
| Food resources | 15,000 | 2×2 | 245 | 590 | | |
| Builder plant | 21,000 | 4×3 | 120 | 890 | | |
| Vehicle plant | 35,000 | 4×3 | 80 | 1,230 | | |
| Medicine plant | 9,500 | 2×2 | 75 | 460 | | |
| Miner station | (placed from a ship) | 4×4 | | | | Miner station (#5) |

### Commanders (advisors)

From the `Staff_*` tables **[src]**. Four categories, three choices
each. *Ability* is the 30/70/90 rating; *level* is the "scholarly level".

| Category | Cheap | Average | Best |
|---|---|---|---|
| Pilot | Bruce Jordan, 4,500, lvl 10, ab. 30 | Jack Benson, 25,000, lvl 20, ab. 70 | Julie-Anne Roxford, 120,000, lvl 40, ab. 90 |
| Builder | Arwen Nielson, 1,000, lvl 10, ab. 30 | Elanor Brady, 26,000, lvl 20, ab. 70 | William Forbs, 70,000, lvl 40, ab. 90 |
| Fighter (war) | Michael James, 3,800, lvl 10, ab. 30 | Kyle Douglas, 45,000, lvl 20, ab. 70 | MACH, 300,000, lvl 40, ab. 90 |
| Developer | Sapphire Fox, 2,400, lvl 10, ab. 30 | Thomas Dempsey, 15,000, lvl 20, ab. 70 | Klaatoo, 250,000, lvl 40, ab. 90 |

The guide gave the pilots' training levels as 10/70/90; those are their
abilities, and the levels are 10/20/40 like the other categories **[fixed]**.

Developer skills (Math/Physics/Electronics/A.I.) by university grade, from
`Staff_DevSkills` **[src]**:

| Grade | Sapphire Fox | Thomas Dempsey | Klaatoo |
|---|---|---|---|
| 0 (hired) | 1/2/2/1 | 3/3/4/2 | 5/7/6/5 |
| 1 | 4/3/3/2 | 5/6/5/2 | 9/9/9/9 |
| 2 | 4/4/4/4 | 6/7/7/5 | 9/9/9/9 |
| 3 | 4/4/5/4 | 7/7/7/7 | 9/9/9/9 |
| 4–6 | 4/4/4/4 | 7/7/7/7 | 9/9/9/9 |

Fox's grade 3→4 drop (5→4 in Electronics) is in the data.
