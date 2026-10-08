# Reunion port

A native port of *Reunion* (Amnesty Design / Grandslam, 1994) for modern
systems, built by decompiling the DOS release. See [PLAN.md](PLAN.md) for the
approach and status.

The game data isn't part of this repository; put your own copy in `vendor/`.

## Building

Needs a C23 compiler (GCC 14+ or Clang 18+), CMake 3.25+ and Ninja. SDL3 is
used from the system when installed (`dnf install SDL3-devel`), otherwise it
is downloaded and built automatically.

```sh
cmake --preset debug          # or: release, asan, trap
cmake --build --preset debug
build/debug/reunion
```

## Running

`reunion [DATA_DIR]` looks for the game data (the directory holding `GRWAR`,
`GRAFIKA`, ...) in `DATA_DIR`, else in `$REUNION_DATA`, else in the
`REUNION_DATA_DIR` CMake setting (default `vendor/Reunion_DOS_EN`).
Alt+Enter toggles full screen; F12 saves a BMP screenshot to the SDL pref
directory (`~/.local/share/Reunion/Reunion/screenshots` on Linux).

The port fixes a few things in the original that can make the game
unwinnable (docs/bugs.md, "Fixed in the port"); `REUNION_ORIGINAL=1` plays
without those fixes.

Like the original `start.exe`, the port plays the CREDITS and INTRO
cut-scenes before the game and VICTORY after it is won. `--skip-intro` (or
`REUNION_SKIP_INTRO=1`) goes straight to the game, as does any autopilot run;
`--cutscene=credits|intro|victory` plays one cut-scene and exits.

Headless runs take a script of mouse clicks, key presses and frame dumps by
frame number (see `src/platform/autopilot.h`), e.g. start a new game and save
the command centre:

```sh
SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy \
REUNION_AUTOPILOT='40:click:160,60;120:click:80,100;1000:dump:cc.png;1001:quit' \
build/debug/reunion
```

`REUNION_AUTOPILOT=random:SEED:FRAMES` plays random clicks and keys instead
(deterministic per seed). `tools/soak.sh` runs many seeds of that on the
`trap` preset (UBSan, trapping) and reports crashes and hangs with gdb
backtraces.

## Reverse engineering

- [ghidra/README.md](ghidra/README.md): rebuilding the Ghidra project and the
  `symbols.txt` format
- [docs/segments.md](docs/segments.md): what each code segment does
