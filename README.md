# Reunion port

A native port of *Reunion* (Amnesty Design / Grandslam, 1994) for modern
systems, built by decompiling the DOS release. See [PLAN.md](PLAN.md) for the
approach and status.

The game data isn't part of this repository or of the game's builds: the
game imports the player's own copy of the DOS game. For development and the
tests, put a copy in `vendor/`.

## Building

Needs a C23 compiler (GCC 14+ or Clang 18+), CMake 3.25+ and Ninja. SDL3 is
used from the system when installed (`dnf install SDL3-devel`), otherwise it
is downloaded and built automatically, as are libxmp, Nuked OPL3 and miniz. On Linux, building SDL needs
`libdecor-devel` for the window's title bar under GNOME on Wayland; without it
the window has none and can't be moved.

```sh
cmake --preset debug          # or: release, asan, trap
cmake --build --preset debug
build/debug/reunion
```

The release preset is the build to hand out: unlike the others it has no
`REUNION_DATA_DIR` built in, so it uses only the player's imported copy.

## Running

Everything the game keeps is in the player's folder, the SDL pref directory
(`~/.local/share/Reunion/Reunion` on Linux): `reunion.ini`, `save/`,
`screenshots/` and the imported game in `data/`.

The game data is taken from, in this order: the `DATA_DIR` argument
(`reunion [DATA_DIR]`, the directory holding `GRWAR`, `GRAFIKA`, ...),
`$REUNION_DATA`, the imported copy in `data/`, and the `REUNION_DATA_DIR`
CMake setting (default `vendor/Reunion_DOS_EN`; not in release builds).
If none of them has the game, the game asks for the player's copy of the
DOS game: its folder, a ZIP of it, or the CD's ISO image, chosen in a file
dialog or dropped onto the window. The English floppy release and the
European CD are accepted (docs/versions.md). The copy is checked file by
file and copied to `data/`; the DOS game's saves go to `save/` where their
slot is free. `reunion --import=PATH` does the same without a window. The
port menu (System) imports another copy.

The CD's import has its English, German, French and Italian games; the port
menu (Game, Language) picks one, used from the next restart. Their menus,
captions and the program's messages are translated too (the CD left those
in English).
`--language=english|german|french|italian` does the same from the command
line (scripted runs read no `reunion.ini`). A `DATA_DIR` laid out like an
import (`DEU`, `FRA`, `ITA` next to `GRAFIKA`) has its languages too.

F1 opens the port menu over the game: cheats, music (None, Sound Blaster,
UltraSound), effects and volumes, display mode and size, skipping the
intro, the colony stats, the Detoxin rate, the language, the improvements,
the fixes, importing game data, restart and quit. Its settings are kept in `reunion.ini`.
Alt+Enter toggles full screen; F12 saves a BMP screenshot to
`screenshots/`.

The port fixes bugs in the original, from ones that can make the game
unwinnable to ones that unbalance colonies and battles
(`src/game/fixes.h`); each can be switched off in the port menu (Fixes), or
all of them at once.

Improvements change the game's design where it falls short
(`src/game/improvements.h`):

- More Miner stations: up to four Miner stations on a body, mining every
  hour and kept when a colony is founded there.
- Repeat build: on a planet's surface, Shift+click places a building and
  stays in build mode for another of the same kind; a plain click ends it.
- Trade routes: ROUTE on a trade group's control panel opens its route,
  up to six of your colonies or Miner-station bodies to load ore at, and
  New Earth to unload it. Started, the group flies the loop on its own,
  docking 5 game hours at each stop and at home, until you end it or give
  the group another order.
- Keyboard play: every screen can be played with keys alone. Tab and
  Shift+Tab move the pointer from one clickable thing to the next, with a
  frame around it; Enter clicks there and Space right-clicks (held, the
  button is held). On a map or a list the arrow keys go one cell at a time
  (off the surface map's edge they scroll it); elsewhere they go to the
  nearest thing that way, and Shift+arrows move the pointer a little.
  1-6 press the toolbar's buttons and 7 turns its page, Escape leaves the
  screen, H held runs the clock, F2 opens the command centre and F3-F10
  its rooms. The port menu (Keys) lists them; `[keys]` in `reunion.ini`
  changes them. The mouse works alongside.
- No building limit: the game holds more than the original's 1,000
  buildings across all colonies.

Each can be switched off in the port menu (Improvements); the Fixes
section's "All off (original)" turns them off too. Scripted runs play
without them. Saves stay in the DOS game's format except while Trade
routes or No building limit is on, a route is set up or the game holds
more than 1,000 buildings: those are written as `save/reunion.<n>`, which
the DOS game can't load (the port menu says so).

On the planet info screen of one of your colonies, clicking the POP row
shows its stats: housing, food and health capacity, the disease timer, how
close the people are to demanding a Stadium or a University, and where
their mood and numbers are heading. Switch it off in the port menu (Game).

Like the original `start.exe`, the port plays the CREDITS and INTRO
cut-scenes before the game and VICTORY after it is won. Skip intro in the
port menu goes straight to the game, as does any autopilot run;
`--cutscene=credits|intro|victory` plays one cut-scene and exits.

Headless runs take a script of mouse clicks, key presses and frame dumps by
frame number (see `src/platform/autopilot.h`), e.g. start a new game and save
the command centre:

```sh
SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy \
REUNION_AUTOPILOT='40:click:160,60;120:click:80,100;1000:dump:cc.png;1001:quit' \
build/debug/reunion
```

`reunion --replay=FILE` plays a script file headless and prints how the game
ended (docs/playthrough.md).
`REUNION_AUTOPILOT=random:SEED:FRAMES` plays random clicks and keys instead
(deterministic per seed). `tools/soak.sh` runs many seeds of that on the
`trap` preset (UBSan, trapping) and reports crashes and hangs with gdb
backtraces.

## Documentation

- [PLAN.md](PLAN.md): status and remaining work
- [docs/port.md](docs/port.md): how the port is put together (addresses,
  the data segment, memory, start-up, deliberate deviations, port
  additions). The code's header comments document each part, file formats
  included (`src/assets/*.h`)
- [docs/walkthrough.md](docs/walkthrough.md): the game itself, as a player's
  reference
- [docs/text.md](docs/text.md): what the game's text files hold
- [docs/versions.md](docs/versions.md): the floppy and CD releases, how
  they differ, and the files the game never loads
- [docs/playthrough.md](docs/playthrough.md): the fixture that plays the
  game to the end
