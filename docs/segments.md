# REUNION.PRG segment map

First pass, from the strings embedded in each code segment, port I/O, call
graph, and a skim of the decompiled code. Roles marked *?* are guesses. Unit
names are ours; the original names are unknown.

Segments use Ghidra numbering (file segment + `1000h`). The table can be
regenerated with `ghidra/scripts/SegmentReport.java`.

Borland Pascal lays out a program as the main program first, then units in reverse
dependency order, with `System` last. So the low-level units sit at the end
(from `4049` onwards) and game screens sit in the middle.

## Memory layout of the data segment (`4a81`)

| Range | Contents |
|---|---|
| `0000–759d` | Typed constants (initialised data) of all units |
| `759e–75ed` | `System` variables (`HeapOrg`, `ExitProc`, `InOutRes`, `RandSeed`, `FileMode`, …) |
| `75ee–a2f5` | Uninitialised globals of all units (zeroed at startup): the game state |
| `a2f6–a544` | `System.Input`, `System.Output`, saved interrupt vectors |

## Code segments

| Seg | File seg | Size | Funcs | Role | Evidence |
|---|---|---|---|---|---|
| `1000` | `0000` | 14592 | 1 | **Main program**: main loop, building/colony commands, battle start | "You cannot have two command centres built!", "endbattl" |
| `1390` | `0390` | 2096 | 8 | Planet/system list UI | "Scroll bar", "Colony on", "Miner station on" |
| `1413` | `0413` | 464 | 1 | Base effects *?* | "baseeff" |
| `1430` | `0430` | 4208 | 5 | Ground war: unit assignment | `war\grbeoszt`, "Trooper", "Battle tank" |
| `1537` | `0537` | 14992 | 31 | Ground war: battle | `war\grwar`, "Move", "Attack", `grvict`/`grlost` |
| `18e0` | `08e0` | 2832 | 2 | Launch/landing checks for ships and fighters | "You haven't hired a pilot!", "No hyperdrive in this unit!" |
| `1991` | `0991` | 2768 | 9 | Advisor dialogue | `kerdes1.sp`, `valasz1.sp`, `ajanlas.sp` (question/answer/offer) |
| `1a3e` | `0a3e` | 5312 | 20 | Save/load and options (music, effects) | `save\spidysav.`, "Select slot", "Music 1" |
| `1b8a` | `0b8a` | 17536 | 14 | **Time/turn simulation**: date advance, production, events | Date counters at DS:95DE–95E4; "is ready for production", disease, meteor, radiation events |
| `1fd2` | `0fd2` | 4128 | 18 | Pub: pirates and aliens | `grafika\kocsma` (pub), `pirate.txt`, `.loc` dialogue |
| `20d4` | `10d4` | 8032 | 21 | Space battle | `war\spwar`, `spvict`/`splost` |
| `22ca` | `12ca` | 8720 | 16 | Ship groups: movement, docking, landing | "Docking", "Launch", "Transfer", "Change destination" |
| `24eb` | `14eb` | 960 | 1 | **Copy protection** (manual word and password) | "What is the word at the bottom of page", "Lamer detected" |
| `2527` | `1527` | 1472 | 3 | Droids *?* | `grafika\szamok`, "addroid", "Back to surface" |
| `2583` | `1583` | 3184 | 6 | Cargo transfer between planet and ship | "Transfer to planet", "Storage space on planet" |
| `264a` | `164a` | 1168 | 2 | Credits text | "PROGRAMMED BY: ISTVAN KISS" … |
| `2693` | `1693` | 1408 | 6 | Second protection check *?* | "Please type the date", `valami.txt` |
| `26eb` | `16eb` | 304 | 1 | **Money cheat** *?* (adds 10,000 credits, or sets 1,000,000) | Writes the longint at DS:95BE |
| `26fe` | `16fe` | 5168 | 13 | Ship information screen | `grafika\ship`, "Destination:", "Time remaining:" |
| `2841` | `1841` | 2240 | 4 | Colonisation | `grafika\koloniz`, "Cost of colony:" |
| `28cd` | `18cd` | 3776 | 18 | Satellites *?* | "You lost the contact with your satellite!" |
| `29b9` | `19b9` | 7488 | 30 | Intelligence reports on other races | "Information on:", "army power is" |
| `2b8d` | `1b8d` | 7568 | 17 | Ship group management | "New Group", "Change name", "In orbit" |
| `2d66` | `1d66` | 3664 | 11 | Research and design | `grafika\cds`, "Under development", "Can be analysed" |
| `2e4b` | `1e4b` | 2672 | 11 | Alien contact / satellite robbers *?* | `satrobb1..3`, `tractor`, `.at` dialogue |
| `2ef2` | `1ef2` | 12976 | 38 | **Planet surface**: map, buildings, production | `map\map.map`, `epulet\epulet` (building), "Production : mine" |
| `321d` | `221d` | 2416 | 4 | Title/hero select, main menu | `grafika\hero`, `grafika\options`, `grafika\charset1` |
| `32b4` | `22b4` | 8128 | 26 | Shop/production orders (INFO-BUY) | "Ore needed", "Total price:" |
| `34b0` | `24b0` | 3248 | 13 | Message boxes; naming new planets | `message.txt`, "Press mouse button" (called from 130 sites) |
| `357b` | `257b` | 15776 | 29 | Planet info screen | "PLANET'S NAME:", population attitudes |
| `3955` | `2955` | 928 | 17 | Record converters: copy fields between packed layouts | Asm-like field-by-field copies; probably save-game (de)serialisation |
| `398f` | `298f` | 2240 | 9 | Text file loading/drawing | `text\`, `text\sz_felsz.txt` |
| `3a1b` | `2a1b` | 2592 | 15 | Graphics file paths / screen helpers | `grafika\`, vsync (3DAh) |
| `3abd` | `2abd` | 6768 | 25 | **Command centre** (room navigation) | "RESEARCH-DESIGN", "STARMAP", "COMMANDERS", `door1..3` |
| `3c64` | `2c64` | 3392 | 10 | Animation (`.ani`) player | `anim\main.ani`, `war\grwar\granim.ani` |
| `3d38` | `2d38` | 3568 | 16 | Vector model (`.vec`) loader | `vectors\v*.vec` |
| `3e17` | `2e17` | 6464 | 8 | 3D vector rendering *?* | No strings, register-heavy, calls graphics core |
| `3fab` | `2fab` | 2528 | 5 | Hiring staff (pilots, builders, fighters, developers) | "My scholarly level:", "I work for you" |
| `4049` | `3049` | 352 | 6 | Loads raw text tables | `text\sz_face.raw`, `sz_talal.raw`, `sz_faj.raw` |
| `405f` | `305f` | 7856 | 52 | **Graphics core**: palette, vsync, `.pic` loader, `ICON.ALL`, fatal errors | Ports 3C8h/3C9h/3DAh, "Cannot Open ICON\ICON.ALL" (619 call sites) |
| `424a` | `324a` | 432 | 6 | Music selection *?* | `main1`, `main2` |
| `4265` | `3265` | 656 | 7 | Ground-war picture loader | `grwar\*.pic` |
| `428e` | `328e` | 2240 | 11 | Asm: picture decompression *?* | No frames, INT 21h, vsync |
| `431a` | `331a` | 2992 | 18 | Sound effects and setup | `sound\*.smp`, `save\setup.spd`, "Sound Blaster detected" |
| `43d5` | `33d5` | 12112 | 18 | **Sound driver**: GUS GF1 registers (hard-coded ports 322h–327h), PIT timer | Likely source of crashes on newer systems |
| `46ca` | `36ca` | 1264 | 25 | Gravis Ultrasound configuration | `ULTRASND` environment variable |
| `4719` | `3719` | 864 | 12 | EMS | `EMMXXXX0` |
| `474f` | `374f` | 144 | 8 | XMS driver calls | Calls through the far pointer at DS:8A60 |
| `4758` | `3758` | 2080 | 13 | **Mouse**: INT 33h, software cursor, hotspots | `Intr($33)`, 11×11 cursor, 27-byte hotspot records at DS:8A71 |
| `47da` | `37da` | 1792 | 21 | **Blitters**: transparent/flipped/rotated copies, fills | 320-byte stride, 236 call sites |
| `484a` | `384a` | 208 | 5 | Longint `Min`/`Max`, `Swap` | |
| `4857` | `3857` | 1568 | 18 | Borland `Crt` | INT 10h/16h |
| `48b9` | `38b9` | 224 | 2 | Borland `Dos` (`Intr`, `MsDos`) | Self-modifying `INT xx` |
| `48c7` | `38c7` | 7072 | 113 | Borland `System` runtime | Fully named in `ghidra/symbols.txt` |

## Notes

- **Hungarian names.** Many asset names are Hungarian: *grafika* (graphics),
  *kérdés* (question), *válasz* (answer), *ajánlás* (offer), *kocsma* (pub),
  *épület* (building), *felszín* (surface), *számok* (numbers), *hiba* (error),
  *kitalál* (guess), *faj* (race/species).
- **Large procedures.** Few, very large procedures: the main program is a
  single 14.5 KB procedure, and `1b8a` has 14 functions in 17.5 KB.
- **Sound and timer.** The sound driver talks to fixed GUS ports and
  reprograms the PIT; replacing both is the planned fix for the crashes (PLAN.md
  Phase 7).
