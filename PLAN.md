# Reunion port plan

Goal: a native, modern build of *Reunion* (Amnesty Design / Grandslam, 1994) that
plays identically to the DOS original, then fix its bugs.

Approach: decompile the DOS executable with Ghidra and rewrite it, unit by unit,
in C23 on top of SDL3, built with CMake. The port loads the original game data
from the user's own copy; we never ship the assets.

## Decisions

| Topic | Choice | Why |
|---|---|---|
| Language | C23 | Fixed-underlying-type enums (`enum : uint8_t`) mirror Pascal byte enums/records exactly; `static_assert`, `bool`, `nullptr`, `constexpr`, binary literals without headers or workarounds. GCC 14+ / Clang 18+. On Windows build with clang-cl or MinGW, as MSVC's C23 support is incomplete |
| Platform | SDL3 | SDL2 is maintenance-only (Fedora already ships SDL2 as `sdl2-compat` on top of SDL3). SDL3's audio streams make feeding our own mixer simple, and its logical presentation does integer-scaled 320×200 out of the box |
| Music | libxmp | Plays the ProTracker MODs as-is |
| Build | CMake (≥ 3.25) with presets | Uses system SDL3/libxmp if found, otherwise fetches them with `FetchContent` |
| VCS | git | `vendor/` and the Ghidra database are git-ignored |

## What we're working from

Source binary: `vendor/Reunion_DOS_EN/GRWAR/REUNION.PRG` (English CD release).

| Fact | Detail |
|---|---|
| Compiler | Borland Pascal 7.0, real mode, `{$G+}` (ENTER/LEAVE frames), no stack checks |
| Packing | None. Plain MZ, 4,785 relocations, no overlays, no DOS extender |
| Size | ~240 KB code in 54 segments, 30 KB data segment (`3a81`) + BSS/stack |
| Functions | Ghidra finds 813; all decompile, 18 with warnings |
| Hand-written asm | Concentrated in the low-level units (blitters `47da`, sound `43d5`, `428e`) |
| Other programs | `INTRO.PRG`, `CREDITS.PRG`, `VICTORY.PRG` (~60 KB each), chained by `start.exe` |

The Amiga release is the original but is hand-written 68k assembly on packed
disks; we use it only as a reference (e.g. for asset formats and behaviour).

### Segment map

See [docs/segments.md](docs/segments.md) for every segment's role. Segment
numbers everywhere use Ghidra's numbering (file segment + `1000h`).

| Seg | Role |
|---|---|
| `1000` | Main program (one 14.5 KB procedure) |
| `1b8a` | Time/turn simulation |
| `2ef2` | Planet surface |
| `3abd` | Command centre |
| `405f` | Graphics core (palette, vsync, `.pic` loader); 619 call sites |
| `43d5` | Sound driver (GUS ports hard-coded) and PIT timer |
| `4758` / `47da` | Mouse / blitters |
| `4857` / `48b9` / `48c7` | Borland `Crt` / `Dos` / `System` |
| `4a81` | Data segment (DS) |

### Asset formats

| Files | Format | Status |
|---|---|---|
| `*.PIC` | `SpidyGfx` header, width/height, custom compression | Reverse |
| `*.ANI` | `SpidyAnim` header | Reverse |
| `ANIM/*.SPD` | Named animation/script data (`atv1`…) | Reverse |
| `*.MOD` | ProTracker `M.K.` | Standard (libxmp) |
| `SOUND*/*.SMP` | 6-byte header + 8-bit unsigned PCM | Confirm header |
| `VECTORS/*.VEC` | Vector/3D models | Reverse |
| `MAP/*.MAP` | 40×30 grid (`0x28 0x1e` header) | Reverse |
| `TEXT/*.AT/.LOC/.SP` | Encrypted/obfuscated text | Reverse |
| `ICON/ICON.ALL`, `WAR/*.DEF` | Unknown | Reverse |
| `SAVE/*` | Settings (`SETUP.SPD`), saves (`spidysav.*`) | Reverse |

Copy protection: a manual-word check ("word at the bottom of page…") plus a
password prompt. Both are bypassed in the port.

## Phases

### Phase 0: project setup
- Repository layout:
  ```
  PLAN.md
  CMakeLists.txt
  CMakePresets.json
  docs/            format notes, segment map, findings
  ghidra/          headless scripts (Java), exported symbol/type lists (text)
  tools/           asset viewers/converters (Python)
  src/             the port (C23)
  vendor/          original game data — never committed
  ```
- Git repository with `.gitignore` covering `vendor/`, build output and the
  Ghidra project database. *(done)*
- CMake skeleton: an SDL3 window showing a 320×200 framebuffer, so the
  toolchain is proven before any game code lands. *(done: palette test card,
  4:3 aspect, Alt+Enter fullscreen)*
- Ghidra project is disposable and git-ignored. Everything we learn goes into
  `ghidra/symbols.txt`, which `ghidra/rebuild.sh` applies to a fresh import, so
  it can be diffed, reviewed and re-applied. *(done, see ghidra/README.md)*

### Phase 1: Ghidra groundwork
The goal is a database where most functions have names, prototypes and types.
1. **System runtime first.** *(done)* All 113 routines in `48c7` and the
   `System` variables are named and prototyped in `ghidra/symbols.txt` (heap,
   text/file I/O, strings, sets, longint and 6-byte Real maths, `Random`,
   `Move`, `FillChar`…).
2. **Calling convention.** *(done)* A small Ghidra language extension
   (`ghidra/lang/`) adds Borland Pascal conventions (callee pops, only
   BP/SP/SS/DS preserved) and fixes a SLEIGH issue that made every `PUSH CS`
   after a far call resolve to the wrong segment. Prototypes in
   `symbols.txt` use Pascal syntax and get exact stack/register storage.
3. **Inline constants.** *(done)* `MarkConstants.java` finds the 694 string
   constants stored in code segments from their `PUSH CS` references and
   marks them as Pascal strings; they decompile as named string references.
4. **Data segment.** *(in progress)* DS is set throughout the code, so all
   1,355 referenced globals resolve and have cross-references. The layout is
   mapped in `docs/globals.md`. About 180 globals and tables are named with 24
   record types (`ghidra/types.pas`): game state, screens, groups,
   buildings, galaxy, products, races, staff. Next: the remaining record
   fields and naming functions unit by unit.
5. **Unit map.** *(first pass done)* `docs/segments.md`.
6. **Export script** that writes names entered in the Ghidra GUI back to
   `symbols.txt`, so GUI work isn't lost on rebuild.

### Phase 2: asset tools (in parallel with Phase 1)
Python tools in `tools/` that decode each format to PNG/WAV/JSON. They:
- confirm our understanding of the loaders found in Phase 1,
- give early, visible progress,
- become the reference the C loaders are tested against.

Order: `PIC` + palette → `ANI` → `SMP` → text decryption → `MAP` → `VEC` → `SPD` → saves *(all done: `tools/*.py`, one doc per format in docs/formats/; `.SPD` are ProTracker modules, `ICON.ALL` and `ANIM.DEF` also decoded)*.

### Phase 3: platform layer (`src/platform/`)
Replace everything that touched DOS hardware:
- **Video:** 320×200 8-bit framebuffer + 256-colour palette, presented via
  SDL with integer/aspect-correct scaling. Palette fades handled in software.
- **Timing:** a fixed logic tick matching the original PIT rate, decoupled from
  render rate. Vsync waits become "wait for next tick".
- **Input:** keyboard and mouse events mapped to what the game polls (`Crt`
  `ReadKey`/`KeyPressed`, mouse state).
- **Audio:** SDL audio stream; MOD playback via libxmp; SMP effects mixed in.
- **Files:** case-insensitive lookup, `\` → `/`, game data path configurable,
  saves written to a user directory.
- **RTL shims:** Pascal strings, `Random`/`RandSeed` reproduced exactly
  (Borland LCG: `seed = seed * 134775813 + 1`) so behaviour matches the original.

### Phase 4: port the game
- Work leaf-first: graphics/blitters → loaders → text/UI → subsystems
  (research, production, trade, fleets, combat, diplomacy) → main loop.
- Start faithful, clean up afterwards. First pass keeps the original data
  layout (a struct mirroring DS offsets, with `static_assert`s) so the port can
  be checked against memory dumps. Refactor into idiomatic C only once a
  subsystem matches.
- Hand-written asm routines are rewritten from their behaviour, not translated
  instruction-by-instruction.
- `docs/progress.md` tracks every segment: *unknown → named → ported → verified*.

### Phase 5: intro, credits, victory
The three separate programs, ported last as cutscene modules inside the same
binary.

### Phase 6: verification
- **Side-by-side runs** in DOSBox-X (installed) vs the port from identical
  saves; compare screens and key state.
- **Memory diffs**: DOSBox-X debugger dumps of DS at chosen points vs the port's
  mirrored state struct.
- **Save compatibility**: load original saves in the port and vice versa.
- **Asset tests**: C loaders vs Python tool output.

### Phase 7: bug fixes and improvements
Only after the port matches the original. Each fix behind a clear commit/flag
so "original behaviour" stays reproducible.
- The known bugs are crashes on newer systems, most likely in the sound
  drivers and timer handling: `43d5` writes to Gravis Ultrasound registers at
  hard-coded ports (322h–327h) and reprograms the PIT. Phase 3 replaces all of
  that code, so these should disappear with the port. Confirm by reproducing
  the crashes in DOSBox-X and checking they don't occur in the port.
- The classic Borland Pascal "Runtime error 200" on fast CPUs is **not** the
  cause: the `Crt` delay-calibration code is absent from all binaries.
- Timing bugs from CPU-speed dependence, if any turn up.
- Quality of life: windowed/fullscreen, remappable keys, save anywhere, etc.

## Milestones
1. Ghidra DB with System RTL named and unit map drafted. *(done)*
2. All `PIC`/`ANI` assets viewable as PNG.
3. SDL shell boots and shows the title screen using original loaders.
4. Main menu → new game → first planet screen playable.
5. Full game loop playable start to finish.
6. Verified against original; bug-fix phase begins.

## Open decisions
- **Licence / publication:** the game is still under copyright. Keep the repo
  private or publish code only (no assets, no raw decompiler dumps) until
  that's decided.
