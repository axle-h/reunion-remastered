# Reunion port

A native port of *Reunion* (Amnesty Design / Grandslam, 1994) for modern
systems, built by decompiling the DOS release. See [PLAN.md](PLAN.md) for the
approach and status.

The game data isn't part of this repository; put your own copy in `vendor/`.

## Building

Needs a C23 compiler (GCC 14+ or Clang 18+), CMake 3.25+ and Ninja. SDL3 is
used from the system when installed (`dnf install SDL3-devel`), otherwise it
is downloaded and built automatically. On Linux, building SDL needs
`libdecor-devel` for the window's title bar under GNOME on Wayland; without it
the window has none and can't be moved.

```sh
cmake --preset debug          # or: release, asan, trap
cmake --build --preset debug
build/debug/reunion
```

## Running

`reunion [DATA_DIR]` looks for the game data (the directory holding `GRWAR`,
`GRAFIKA`, ...) in `DATA_DIR`, else in `$REUNION_DATA`, else in the
`REUNION_DATA_DIR` CMake setting (default `vendor/Reunion_DOS_EN`).
F1 opens the port menu over the game: cheats, music (None, Sound Blaster,
UltraSound), effects and volumes, display mode and size, skipping the
intro, the colony stats, the fixes, restart and quit. Its settings are kept in `reunion.ini` in
the SDL pref directory (`~/.local/share/Reunion/Reunion` on Linux).
Alt+Enter toggles full screen; F12 saves a BMP screenshot to the
`screenshots` directory there.

The port fixes a few things in the original that can make the game
unwinnable (`src/game/fixes.h`); each can be switched off in
the port menu (Fixes), or all of them at once.

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

- [PLAN.md](PLAN.md): status, remaining work and open bugs
- [docs/port.md](docs/port.md): how the port is put together (addresses,
  the data segment, memory, start-up, deliberate deviations, port
  additions). The code's header comments document each part, file formats
  included (`src/assets/*.h`)
- [docs/walkthrough.md](docs/walkthrough.md): the game itself, as a player's
  reference
- [docs/text.md](docs/text.md): what the game's text files hold
- [docs/playthrough.md](docs/playthrough.md): the fixture that plays the
  game to the end
