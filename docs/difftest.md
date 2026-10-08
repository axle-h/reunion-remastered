# Differential tests

`tests/test_diff_<unit>.c` run routines of the **original** `REUNION.PRG`
under [Unicorn](https://www.unicorn-engine.org/) (an x86 CPU emulator) and the
port's C versions on the same memory, and compare everything they leave
behind. A mismatch is either a port bug (fix it, see [Workflow](#workflow))
or a gap in the harness. The harness is `tests/diff/diff.{h,c}`;
`tests/test_diff_sim.c` (unit Sim) is the worked example and
`tests/test_diff_harness.c` checks the calling conventions.

## How it works

The port's `mem[]` *is* the real-mode address space (docs/port.md), so a port
state is also a state of the original. Unicorn maps `mem[]` itself
(`uc_mem_map_ptr`) and the harness loads the code part of `REUNION.PRG`
(up to DS) at `MEM_LOAD_SEG` with the MZ relocations applied, so the code's
far calls and segment constants match the port's layout. One check
(`diff_call`):

1. snapshot: `mem[]`, the EMS driver's pages (`emm_state_save`), the VGA DAC
   (`vid_palette`);
2. **original**: push the arguments the Pascal way (left to right, callee
   pops) and a return address, start at the routine's entry and stop when it
   returns. Its whole call tree runs in the emulator, run-time library
   included;
3. keep the result (memory, EMS, DAC, AX/DX), roll back to the snapshot;
4. **port**: call the C routine through a thunk;
5. compare: all of memory except the original's stack segment and the
   routine's ignore ranges, the result register(s), the outcome (returned /
   run-time error / Halt), the DAC, the EMS pages and how many queued keys
   each side consumed.

Both sides get the same input: the mouse where `DS Mouse_X/Y` says, no
buttons, and 32 Enter keys queued in `platform/input`. The original reads
them through INT 16h, which the harness answers from that same queue. A
case's setup can queue other keys with `diff_set_keys(keys, n)` (an
extended key is 0 then its scan code; `n = 0`: no key at all), for one case.

### Prototypes

The routine is named by its symbols.txt name (`Sim::AdvanceHour`), its C
name (`Sim_AdvanceHour`, first `_` = `::`) or its address (`1b8a:0000`).
The prototype in `ghidra/symbols.txt` gives the stack layout, as
`ApplySymbols.java` does: `byte/char/boolean/shortint/word/integer` take a
word, `longint/pointer/^T` two words, `var`/`const`, strings and anything
over 4 bytes a far pointer; a `string` result adds a hidden first argument
(the buffer) that the callee leaves on the stack. Results: 1 byte in AL, 2
in AX, 4 in DX:AX. A routine is **near** when its comment contains the word
`near` or `nested`, or `DiffFn.near` is set. Not supported (the check fails
saying so): register arguments (`@REG`), `real` arguments or results,
`@keep`, more than 16 parameters. Only the lowercase words count: a comment
saying `NEAR` needs `DiffFn.near` (`Gfx::DrawText6x8`, `GfxPath::WaitVRetrace`).

Far routines return to `F000:0000`, near ones to `CS:FFFF`; the harness
also checks SP afterwards, so a wrong prototype or near/far guess is
reported, not silently compared.

### What the emulated machine provides

| | |
|---|---|
| INT 21h | open (read-only, through `fs_resolve`), read, close, seek, IOCTL get-info, write to handles 0-2 (dropped) |
| INT 16h | read key / key available / shift state, from the input queue |
| INT 10h | set mode: 13h clears A000:0000-FFFF, 3 (text) does nothing, as the port's `Gfx_SetVideoMode13`/`SetTextMode` |
| INT 33h | reset, get position/buttons from `input_mouse()`; show/hide/set position/range/handler are no-ops (as in the port) |
| INT 67h | the port's EMM (`emm_int67`) on the same memory |
| INT 0 | divide error = run-time error 200 (each run starts from a fresh CPU context: Unicorn hands the exception to the hook without finishing it, so the next divide error would become a double fault) |
| ports | VGA DAC 3C7h/3C8h/3C9h, retrace 3DAh (alternates in/out of retrace), PIC EOI 20h |
| `48c7:010f` / `48c7:0116` | RunError / Halt: the run stops there with the code in AX |

Anything else (another interrupt, another port, code running outside the
program, an unmapped address, more than `REUNION_DIFF_TIMEOUT` seconds,
default 10) stops the original and fails the check with the address in
Ghidra numbering. `diff_allow_port(port)` lets a test accept a port whose
effect is not compared (e.g. a sound card's). The port side fails if it is
still running after 100000 retraces (waiting for input it never gets).

**Output.** The RTL's InitSystem opens `Output` (`Assign(Output, '');
Rewrite(Output)`), the port never does, so in a booted state the original's
`Write(Output, ...)` (the "Cannot Open" and error exits) is run-time error
105. `diff_open_output()` after `diff_boot` runs those two RTL routines on
the state (`diff_run_original(name, args)` runs any original routine alone
and keeps its memory, for set-ups the port does not model).

**Procedure pointers.** The port stores procedure variables in Ghidra
numbering (`far_ptr(0x4758, 0x012D)`), the original the loaded segment. The
harness converts the DS fields listed in `diff.c` (`Blit_CursorHook`,
`System_ExitProc`, ...) before the original runs and back afterwards; add new
ones with `diff_code_ptr(offsetof(DSeg, Field))`.

### Reports

```
MISMATCH Sim::ProcessStoryTimers case 3 (base state 3)
  memory (address, name, original | port):
    43D12 DS:7AE2 Message_LogCount                         orig 0A | port 0B
    47B12 4000:7B12 heap Message_Log^+2                    orig 1D 54 68 65 20 .. | port 1E 22 54 68 65 ..
```

DS addresses are named from `tests/diff/dseg_names.c`, which
`tools/gen_dseg.py` writes together with `src/game/dseg.h` (array indices
are C indices, records are walked: `Pub_People[1].Timer`). Blocks outside DS
are named by region: the game data block (`Game_Groups^[i].Field`,
`Game_Buildings^[i].Field`) and `Message_Log^` by default, others with
`diff_name_region()`. Code addresses print as Ghidra `seg:off`, video as
x/y. `REUNION_DIFF_CASE=k` reruns one case.

## Inputs

Cases start from real game states and perturb what the routine reads:

- `diff_boot_new_game(frames)` / `diff_boot("900:click:72,16;1300:quit")`
  boot the port with the autopilot (start menu, new game, then your events)
  and leave the state in `mem[]`, with sound effects off (`diff_quiesce`).
  The menu bar at y=16 opens: x=24 shop (5), 72 research (3), 120 ship info
  (16), 168 system view (7), 216 surface (20), 264 message log (11), 296
  command centre (1). Routines redraw whatever the current screen shows, so
  test from several screens.
- `diff_save()` keeps a state; `diff_run(fn, bases, nbases, setup, n)` runs
  case k from `bases[k % nbases]` after `setup(rng, args)`, with the
  generator seeded from the routine's name and k: the same cases every run.
- In `setup`, write the globals through `ds.` and fill `args[]` in prototype
  order (far pointers as `FarPtr`; `DIFF_SCRATCH` is 4 KB of free low memory
  for `var` parameters and string results). `diff_range`, `diff_pick`,
  `diff_chance` keep values in valid ranges; pick boundary values often.

Environment: `REUNION_DIFF_CASES=n` (cases per routine), `REUNION_DIFF_CASE=k`,
`REUNION_DIFF_ONLY=text` (routines whose name contains text),
`REUNION_DIFF_TIMEOUT=s`, `REUNION_DIFF_COVERAGE=1` (after each routine:
which bytes of its code range the original executed, and the ranges it did
not; the range runs to the next symbol, so its tail is usually the next
routine's string constants), `REUNION_DIFF_RUNS=n` (differing memory runs
listed per mismatch, default 16).

Screens a new game cannot reach by clicks (no fleets exist yet) are entered
by the test itself: it sets up the state, calls the port's screen handler
with `Game_ScreenEntered` set, as the main loop does on a switch, and saves
the result as a base (`tests/test_diff_shipmove.c`); the entry is compared
as a case too.
The ground battle is built the same way from the units' own set-up
routines (`tests/diff_grwar.h`, shared by `test_diff_grwar.c` and
`test_diff_grwarunits.c`): forces at a body, `CollectAvailableForces`,
`InitBattleUnits`, `EnterGroundBattle`, then `BattleTick`s, saving states
along the way.

## Adding a routine or a unit

```c
// tests/test_diff_<unit>.c
#include "diff/diff.h"
#include "game/dseg.h"
#include "game/<unit>.h"

DIFF_PROC(Unit_Name, Unit_Name((int16_t)a[0], far_host((FarPtr)a[1])));  // procedure
DIFF_FUNC(Unit_Fn, Unit_Fn((int16_t)a[0]));                               // function

static void setup_name(DiffRng *r, int32_t *a)
{
    ds.Some_Global = (int16_t)diff_range(r, 0, 10);
    a[0] = diff_pick(r, 1, 2, 8);
    a[1] = (int32_t)DIFF_SCRATCH;
}

int main(int argc, char *argv[])
{
    if (!diff_init(argc, argv))
        return 77;  // no game data
    if (!diff_boot_new_game(1500))
        return 1;
    DiffState *base = diff_save();
    diff_run(&diff_Unit_Name, &base, 1, setup_name, diff_cases(30));
    diff_free(base);
    return diff_finish();
}
```

Any `tests/test_diff_*.c` becomes a ctest linked with the harness; another
test that uses it goes in `REUNION_DIFF_USERS` in `CMakeLists.txt` (as
`test_travel`). Ranges to leave out: `fn.ignore = (const DiffRange[]){DIFF_DS(0x7C50, 4), {0, 0}}`
(`diff_<cname>` is a static variable). A routine without a usable prototype
in symbols.txt takes one from the test: `fn.decl = "(src: pointer; tile: word; x, y: integer)"`
(the text after the name, same syntax; `fn.near` for a near one). Only routines with external linkage
can be tested; nested and `static` ones are covered through their callers
(`REUNION_DIFF_COVERAGE` shows how well).

## Workflow

On a mismatch: rerun the case alone (`REUNION_DIFF_ONLY`, `REUNION_DIFF_CASE`),
find the routine that writes the named field, compare the port with the
disassembly (`dis.sh <seg> <start> <end>`), fix the port, and add a numbered
section to [port-bugs.md](port-bugs.md). If the harness is wrong (a missing
interrupt, a representation difference that is not the port's fault), fix
the harness or ignore the range with a comment saying why.

## Limitations

- **Asynchronous events do not happen in the emulator**: no timer IRQ (music,
  BIOS tick counter at 0040:006C: a busy wait on it times out), no mouse
  driver callback (`4758:0000`). The port calls `Mouse_Update` in its wait
  loops instead; `diff_quiesce` settles the mouse state first so those calls
  change nothing unless the routine moves hotspots under the pointer.
- Sound effects are off (`Sound_Enabled = 0`): the port's sound is not
  memory-faithful (decoded samples, `platform/audio`), so sound paths are
  not compared.
- Host state outside `mem[]` is not compared, apart from the DAC, the EMS
  pages and the key queue (e.g. the port's audio voices, `vid_attr`).
- After Halt only the exit code is compared: the port has run the ExitProc
  chain, the original stopped at Halt's entry.
- Files are opened read-only; a routine that writes files fails.
- The 32 queued keys are all Enter unless a setup calls `diff_set_keys`; a
  routine waiting for more keys than are queued fails on both sides. A
  routine waiting for a mouse click cannot be tested (the original reads
  `Mouse_BtnDown`, which only the callback sets): `Title::StartMenu`,
  `HeroSelect`, the wait loops of `Message::MessageBox` and
  `Gfx::WaitFramesOrClick`.
- The port keeps some files' state on the host: the FileRec at DS:91EE
  (`Gfx_OpenDataFile`, ICON.ALL) is ignored where it is opened, and the
  routines that need it open (`Gfx_ReadChunk`, `IconAllReadChunk`, the
  closes) are only covered through their callers.
- Procedures with a temporary palette on the original's stack
  (`Gfx_FadeOutSteps` and the like, `LoadPic`'s fade) send ~64 KB from
  beyond it when `last < first`; the port's temporary lives elsewhere, so
  such ranges are not compared.
- The music is not memory-faithful either: tests set `Music_Backend = 0`
  where a routine would start a track (AdLib ports 388h/389h).
- Unicorn is built from source when the system has no `unicorn` package
  (pkg-config); `-DREUNION_DIFFTEST=OFF` drops the differential tests.
