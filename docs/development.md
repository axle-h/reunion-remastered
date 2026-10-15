# Development

Building is in the README ("Building from source"). This page covers what
a developer needs beyond that.

## Presets

```sh
cmake --preset debug          # or: release, asan, trap, web
cmake --build --preset debug
build/debug/reunion
```

The release preset is the build to hand out: unlike the others it has no
`REUNION_DATA_DIR` built in, so it uses only the player's imported copy.

## The web build

The web preset builds the page with Emscripten (`build/web/reunion.html`,
`.js` and `.wasm`); the release workflow puts it on GitHub Pages. It needs
[emsdk](https://emscripten.org/docs/getting_started/downloads.html) 6.0.12
activated in the shell:

```sh
source ~/emsdk/emsdk_env.sh
cmake --preset web
cmake --build --preset web
python3 -m http.server -d build/web 8000   # http://localhost:8000/reunion.html
```

It is the same program: Asyncify lets its waiting loops return to the
browser between frames (`timing_wait_retrace`), and `src/platform/web/`
holds the page and its JavaScript (`src/platform/web.h`). Like the
release preset it has no game data built in. The player's folder is kept
in the browser's IndexedDB.

`node tools/web/replay.mjs [RECORDING...]` replays playthrough recordings
in the web build, headless in Chromium and Firefox (the setup is in its
header).

## The game data

The data isn't part of the repository or of the builds. For development
and the tests, put a copy of the DOS game in `vendor/` (git-ignored):
`vendor/Reunion_DOS_EN` is the English floppy release, and the CD's ISO
goes next to it for the tests that use it.

The game data is taken from, in this order: the `DATA_DIR` argument
(`reunion [DATA_DIR]`, the directory holding `GRWAR`, `GRAFIKA`, ...),
`$REUNION_DATA`, the imported copy in the player's `data/`, and the
`REUNION_DATA_DIR` CMake setting (default `vendor/Reunion_DOS_EN`; not in
release builds). If none of them has the game, the import screen asks for
the player's copy. `DATA_DIR` and `REUNION_DATA` are used without the
import's checks. A `DATA_DIR` laid out like an import (`DEU`, `FRA`, `ITA`
next to `GRAFIKA`) has the CD's languages too.

## Tests

`ctest` in a build directory runs them all; they need `vendor/`. The
golden state hashes and the playthrough are described in PLAN.md
("Where we stand") and [playthrough.md](playthrough.md).

Scripted runs (autopilot, `--replay`, the tests) read no `reunion.ini` and
play with every improvement off, so they show the original's behaviour; a
test of an improvement switches it on.

## Headless runs

Headless runs take a script of mouse clicks, key presses and frame dumps by
frame number (see `src/platform/autopilot.h`), e.g. start a new game and save
the command centre:

```sh
SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy \
REUNION_AUTOPILOT='40:click:160,60;120:click:80,100;1000:dump:cc.png;1001:quit' \
build/debug/reunion
```

`reunion --replay=FILE` plays a script file headless and prints how the game
ended ([playthrough.md](playthrough.md)).
`REUNION_AUTOPILOT=random:SEED:FRAMES` plays random clicks and keys instead
(deterministic per seed). `tools/soak.sh` runs many seeds of that on the
`trap` preset (UBSan, trapping) and reports crashes and hangs with gdb
backtraces.
`tools/dosbox/side_by_side.py` plays the start of a new game in the port
and in the original under DOSBox-X (its debugger driven through tmux) and
compares the two states every few game hours.

## Documentation

- [PLAN.md](../PLAN.md): status and decisions
- [port.md](port.md): how the port is put together (addresses, the data
  segment, memory, start-up, deliberate deviations, port additions). The
  code's header comments document each part, file formats included
  (`src/assets/*.h`)
- [walkthrough.md](walkthrough.md): the game itself, as a player's
  reference
- [text.md](text.md): what the game's text files hold
- [versions.md](versions.md): the floppy and CD releases, how they differ,
  and the files the game never loads
- [playthrough.md](playthrough.md): the fixture that plays the game to the
  end
