# Plan: native C memory (retire the DOS address space)

Status: **done** (2026-10-11). Decided by Alex, 2026-10-11. This is the
refactor PLAN.md Phase 4 always intended ("Start faithful, clean up
afterwards ... refactor into idiomatic C once a subsystem matches"). Every
unit is ported and the whole game plays through (`test_play_game`), so the
faithful memory model has done its job.

## Result

`mem[]`, `FarPtr`, `far_ptr`/`far_host`, `ds_at`, `ghidra_host`, the Borland
heap and the EMM are gone (docs/port.md, "Memory"). `ds` is a C global with
every global the code uses named (734 fields) and typed host pointers;
procedure variables are C function pointers; heap blocks are C
allocations; video memory is the video layer's own 64 KB. Saves are
written field by field and stay byte-identical. The proof still wins and
`test_play_game` records it byte for byte.

Where the work differed from the plan below:

- **Order.** A typed pointer needs 8 bytes and the packed mirror had 4, so
  interfaces and the screen came first, then naming (4.1), saves (4.2), DS
  out of `mem[]` and typed pointer fields, then the heap and procedures
  (Stage 3 "Order as done").
- **DS keeps the original's order with no padding** (Stage 4.3 "As done"):
  the original reads and writes past arrays into the next globals.
- **The proof was re-recorded first:** the committed one no longer replayed
  at the start (later game-logic fixes had changed the run).
- **Goldens re-baselined twice, from the previous commit's code:** the
  message log's unused bytes (stale heap contents) left the state stream
  (Stage 2), and four constant bytes that turned out to be data, not a
  pointer, joined it (Alien::MenuData, Stage 4). Both changed only what is
  hashed; the code of the day matched the new goldens.
- **Two original behaviours found and kept on purpose:** race 10's last
  alien-talk menu is the 4 bytes after the menu table (where the pointer
  table's unused race-1 entry was), and stars of the flight view drawn past
  the backdrop land in the star table that follows it on the heap, which
  changes the game's random numbers (`src/game/shipmove.c`).
- **One accepted difference, on screen only:** where the original shows
  heap memory the game never wrote (the flight view's bottom rows on its
  first frames, the bottom rows of shop pictures shorter than their
  window), the port shows colour 0. Reproducing the original's leftovers
  would need the Borland heap back. One proof golden sample was updated
  for it.
- **Kept:** the original's DS offsets in comments and the field table, as
  the link to the disassembly.
- **After the plan:** the initial data moved into C source
  (`src/game/dsinit.c`, generated from `REUNION.PRG`), so the game needs no
  executable at run time (the open question below).

The rest of this file is the plan as written.

## Goal

The game runs like an ordinary C program. Its state lives in C globals,
`malloc`ed blocks and the stack, linked by typed host pointers. Nothing
keeps the real-mode address space alive:

- no `mem[]`, `FarPtr`, `far_ptr`, `far_host`, `ds_at`, `ghidra_host`;
- no Borland heap emulation (`System_GetMem`/`FreeMem` become `malloc`/`free`
  or disappear), no 640 KB limit, no `MemAvail` arithmetic;
- no EMS: no emulated driver (`emm.c`), and unit Ems shrinks to plain
  allocations;
- the screen is a `uint8_t[200][320]`, and A000/B000/B800/E000 mean nothing;
- procedure variables are C function pointers;
- `DSeg` stops mirroring the original data segment byte for byte. It may
  stay one global struct, or be split into per-unit globals; either is fine
  as long as it is natural C.

What must **not** change:

- **Behaviour.** The game plays exactly as now, fixes on or off.
- **The save format.** Saves stay byte-identical to the original's
  (`docs/formats/save.md`), and original DOS saves still load.
- **The playthrough proof.** `tests/play/proof_12345.txt` still wins
  (exit 10 after 3,490,240 retraces on 2930-03-05).

## What goes away, and why that is accepted

The differential tests (`tests/test_diff_*.c`, 38 files, plus
`tests/diff/`, about 12,000 lines; `docs/difftest.md`) run the original x86
code under Unicorn on the port's own `mem[]`. They cannot survive native
pointers. They are retired in Stage 1, after a final green run. A git tag
keeps them reachable, and Stage 0 builds the safety net that replaces them.

DOSBox-X memory comparison (PLAN.md Phase 6) goes as well; it was never
used.

## Size of the job (measured 2026-10-11)

| What | Count |
|---|---|
| `src/game/*.c` using `far_ptr`/`far_host` | 42 of 56 files |
| `FarPtr` / `far_ptr` / `far_host` mentions in `src/` | 385 / 205 / 155 |
| segment:offset arithmetic (`p >> 16`, `(uint16_t)p + off`) | about 100 |
| `System_GetMem` / `System_FreeMem` calls | 133 / 142 |
| DS globals reached by raw offset (`ds_at`, `B8(`, `W16(`) | 477, unnamed globals that need names |
| `ds.` field uses (fine to keep) | 3,069 |
| pointer-typed DS fields | about 38 |
| screen-segment uses (`Gfx_ScreenSeg`, `System_SegA000`, 0xA000) | about 102 |
| procedure pointers kept in Ghidra numbering | 14 sites (`gfx.c:call_far_proc`, `Game_*Proc` fields, exit procs) |
| comments about 16-bit offset wraparound | 19 |
| non-difftest unit tests touching the model | 39 files |
| playthrough files reading game memory (`tests/play/`) | 14 files |

Densest files: `main_program.c` (218 hits), `command.c` (81),
`planetinfo.c`, `surface.c`, `grwar.c`, `gfx.c`, `gfxpath.c`, `spwar.c`,
`shipmove.c`, `pub.c`, `system.c`. Recount with:

```sh
for f in src/game/*.c; do echo "$(grep -cE 'far_ptr|far_host|FarPtr|System_GetMem|System_FreeMem|ds_at|B8\(|W16\(|>> 16' $f) $f"; done | sort -rn
```

**What makes it tractable.** Game code never steps segments or computes
linear addresses. Every heap block is at most 64 KB. All pointer
arithmetic is "this block + 16-bit offset". So every `FarPtr` maps onto
`base + offset` with a C pointer.

## What depends on the address space today

Read these before starting.

- `src/game/mem.{h,c}`: `mem[]` (1 MB + 64 KB), `far_ptr`, `far_host`,
  `ds_at`, `ghidra_host`, and the segment map (PSP, load segment, DS, SS,
  heap, video). `mem_init` also installs the EMM.
- `src/game/dseg.{h,c}`: `#define ds (*(DSeg *)(mem + MEM_DS_SEG * 16))`.
  `dseg.h` is generated by `tools/gen_dseg.py` from `ghidra/symbols.txt` and
  `ghidra/types.pas`: a packed struct (`#pragma pack(1)`) with 647 offset
  `static_assert`s. `dseg_init` copies the initial data segment out of
  `GRWAR\REUNION.PRG` and applies MZ relocations. Some typed constants are
  far pointers into DS (the galaxy tables), and those need converting to
  host pointers when loaded.
- `src/game/system.c`: Borland heap (`System_GetMem`, `FreeMem`, `MemAvail`,
  `MaxAvail`), ExitProc chain, Halt/RunError.
- `src/game/ems.c` + `src/game/emm.c`: unit Ems moves the game data block
  (Groups, Buildings, charset; 8770h bytes) and the toolbar/icon block
  (GfxPath) into an emulated EMS page frame at E000. Both blocks share the
  frame, and callers must `Ems_EmsMapAll` before use (18 call sites).
  Without EMS (block state 1) the map calls do nothing and `Data` is a plain
  pointer.
- **Fixed segments.**
  - DS-relative far pointers: `blit.c:325,347`, `gfx.c:42`, `textfile.c:52`,
    `vecload.c:142`, `spwar.c:150,660`, `saveload.c:377`.
  - Pointers into the stack segment that stand in for the original's locals:
    `spwar.c:60` SCRATCH_PAL, `gfx.c:50`.
  - Off-screen buffers chosen by segment (`command.c:242,268` OFFSCREEN).
- **Video.** `vid_screen` points at A000:0000 in `mem[]` after `mem_init`
  (`src/platform/video.c` otherwise has its own `screen_buffer`). The game
  switches drawing targets by changing `ds.Gfx_ScreenSeg`.
- **Memory checks that change behaviour.**
  - `main_program.c:180`: the start-up check, 238,000 bytes, or 266,000 with
    a GUS.
  - `message.c:258`: the satellite animation is skipped if
    `MaxAvail <= 0x199FF`.
  - `message.c:151`, plus `Game_MinMemAvail` bookkeeping in `anim.c`,
    `vecload.c`, `gfx.c`, `gfxpath.c` and `planetinfo.c`.
  - With EMS, today's heap never reaches these limits in the playthrough
    (lowest point measured 204,648 bytes). So "always enough memory" keeps
    the behaviour.
- **Saves** (`saveload.c`, block list at line ~108). Saves are written as
  raw byte ranges of DS (`B(off, size)`), plus three blocks behind DS
  pointers (`P(...)`: message log, buildings, groups). Some saved ranges
  may contain pointer fields; check each against `dseg.h`. The new
  serializer must write the same bytes, including whatever pointer values
  the original wrote there.
- **Hashes of `mem[]`.**
  - `src/main.c:266`: the `--replay` hash.
  - `tests/play/play.c`: the write barrier (shadow copy of `mem[]`),
    checkpoint hashes, `PLAY_TRACE`.
  - The `# end` / `# checkpoint` comments in `proof_12345.txt`.
  - `docs/playthrough.md`.
- **Tests that build states in `mem[]`.** For example `test_surface.c:33`
  clears `mem + 0x31000`, `test_adlib.c` checks `System_MemAvail`, and
  `test_anim.c` checks MemAvail.

## Stages

Each stage ends with every test green: debug, release (which runs
`test_play_game`), and ASan for the touched tests. Stage 0's golden
fingerprints must also match. Keep stages small enough to review.
Commit only when Alex asks (directly on main, one commit per coherent
step).

### Stage 0: a safety net that doesn't depend on layout

Build this **on the current model**, before anything moves.

1. **State fingerprint.** Add `game/state.{h,c}` with
   `void state_serialize(Buffer *)` and `uint64_t state_hash(void)`. They
   must give the same bytes before and after the migration:
   - every DS field that is not a pointer, by name, in symbols.txt order,
     with its size. `tools/gen_dseg.py` already emits
     `tests/diff/dseg_names.c` with names, offsets and sizes; have it also
     emit a field table in `src/game` with an `is_pointer` flag. The table
     must outlive the difftests and later the struct mirror.
   - the contents of the state blocks behind pointers: the game data block
     (Groups, Buildings, charset), `Message_Log`, and every `P(...)` block
     in the save list. Add others the game keeps across frames (e.g.
     `Surface_BufC`, `AlienTalk_QuestionBuf`) when they turn out to matter.
   - the screen (`vid_screen`, 64,000 bytes) and the palette/DAC
     (`vid_palette`).
   - excluded: pointer values themselves, `Game_MinMemAvail`, and anything
     else that only describes the memory layout. List each exclusion in a
     comment with its reason.
2. **Goldens.** Add a recorder that replays a script and writes
   `frame state_hash` every N retraces, and store its output under
   `tests/golden/`:
   - `proof_12345.txt`, every 10,000 retraces (release-only ctest, like
     `test_play_game`);
   - boot to the command centre, then each menu-bar screen (see the
     `diff_boot` click list in `docs/difftest.md`), every 100 retraces;
   - 4-6 soak seeds (`REUNION_AUTOPILOT=random:SEED:30000`, as in
     `tools/soak.sh`), every 500 retraces;
   - each cut-scene (`--cutscene=credits|intro|victory`), every 100
     retraces;
   - fixes off (`REUNION_ORIGINAL=1`) for at least the proof and one soak
     seed.

   A ctest compares a fresh run with the goldens and reports the first frame
   that differs. When there is a mismatch, `state_serialize` dumps from both
   runs at that frame should name the field (give the serializer a
   "describe offset" helper, as the write barrier does).
3. **Switch the playthrough to the fingerprint.**
   - The `--replay` hash, the write barrier, checkpoint hashes and
     `PLAY_TRACE` use `state_serialize`/`state_hash` instead of `mem[]`.
     The barrier compares serialized buffers so it can still name the
     field.
   - Regenerate the hash comments in `proof_12345.txt`: the inputs are
     unchanged, only the hash lines change.
   - Update `docs/playthrough.md` (the hash values and the "hash of the
     emulated memory" wording).
4. **Save fixtures.** Save at 3-4 points of the proof run (`SaveLoad_Transfer`
   through the API, as the fixture rules allow) and keep the files as goldens.
   Add a test that loads each one, saves it again and gets identical bytes,
   and that the loaded state hashes as at the point it was saved.
5. **Check the net.** Prove it catches things: deliberately break a routine
   (e.g. an off-by-one in `Sim_AdvanceHour`) and make sure a golden fails.
   Then revert.

### Stage 1: retire the differential tests

1. Run the full suite once more, all green, and tag the commit
   `last-real-mode`. Ask Alex before creating the tag.
2. Delete `tests/test_diff_*.c`, `tests/diff/`, the Unicorn dependency and its
   CMake wiring, and the `--names` output of `gen_dseg.py`. Keep the field
   table from Stage 0.
3. Update `docs/difftest.md` to a short note: what the tests were, the tag
   to check out, and why they were retired. Update PLAN.md (Decisions,
   Phase 6) and `docs/bugs.md`, which says the difftests run with fixes off.

### Stage 2: drop EMS and the heap limit

These are small, contained changes. The game code calling them stays as it is.

1. Don't install the EMM (`mem_init`). Unit Ems then takes its original
   no-EMS path: blocks stay in state 1 and the map calls are no-ops.
   Delete `emm.c` and `emm.h`, and simplify `ems.c` to allocate and free.
   Remove the EMS parts of the playthrough observers
   (`tests/play/ui_message.c` uses `emm_page_view`).
2. Make the heap big enough that it never runs out. The heap still has to
   be inside `mem[]` until Stage 3 moves blocks out. Recommended: make
   `System_MemAvail` and `MaxAvail` report a fixed "plenty" value, so the
   checks in "What depends on the address space today" always pass, as they
   do today with EMS. If the heap itself overflows, grow `mem[]` only as a
   stop-gap (far pointers still cap at 1 MB + 64 KB, so prefer Stage 3).
   Decide what `Game_MinMemAvail` should hold: it is diagnostic only, and
   excluded from the fingerprint.
3. Goldens must match. Expected differences are none: the measured minimum
   free heap with EMS never came near the limits. If one differs, find out
   which memory check flipped.

### Stage 3: typed pointers, leaf first

**Order as done (2026-10-11).** A typed pointer field needs 8 bytes, but
the packed DS mirror has 4-byte slots until Stage 4 drops it. So storage
waits for Stage 4: (1) interfaces and the screen (3.1, 3.2) with
`far_host` at call sites; (2) name the unnamed globals (4.1); (3) explicit
saves (4.2); (4) DS unpacked and out of `mem[]`, its pointer fields typed,
pointing into `mem[]` where heap blocks still live (4.3, 4.4, 3.5, 3.7);
(5) heap blocks to `malloc` and the procedure pointers (3.3, 3.4, 3.6);
(6) Stage 5.

Two independent axes:

- **(a) Interfaces.** A function taking or returning a `FarPtr` takes a
  typed host pointer instead. Callers that still hold a `FarPtr` call
  `far_host` at the call site, which keeps the tree compiling in a mixed
  state.
- **(b) Storage.** A block or field moves out of `mem[]` into C storage, and
  the `FarPtr` field holding it becomes a typed pointer. Each such move
  changes every user of that field at once (grep the field name).

Order:

1. **Primitives** (interfaces):
   - `blit.c`, `scale.c` (`Scale_RowProcTable`), `gfx.c` (LoadPic,
     palette, text drawing);
   - `anim.c`, `vecload.c`, `vec3d.c`, `rawtext.c`, `textfile.c`;
   - the `System_Move*`/`FillChar` helpers in `system.c`.

   Pointer arithmetic `far_ptr(seg, (uint16_t)p + off)` becomes `p + off`.
   At each place that relies on the 16-bit wrap (grep "wrap"), work out
   whether the wrap can happen. If it can, reproduce it on purpose (index
   modulo 64 KB within the block) with a comment. If it can't, assert it.
2. **Screen.**
   - `vid_screen` is the video layer's own buffer.
   - `Gfx_ScreenSeg` (a segment) becomes a `uint8_t *` drawing target, or an
     enum of screen / off-screen buffer.
   - Off-screen buffers (`OFFSCREEN` in `command.c`, the `0xBCC0` and
     `64000`-byte picture buffers) become `malloc`ed blocks.
   - `System_SegA000`/`SegB000`/`SegB800` go.
3. **Heap blocks** (storage): the `System_GetMem` sites (`grep -n
   System_GetMem src/game/*.c`). Allocate with `malloc` and free with
   `free`. Keep the original allocation order only where it matters, which
   should be nowhere once `MemAvail` is gone.
4. **Game data block.** `Game_Groups`, `Game_Buildings` and `Gfx_Charset`
   become typed pointers into one allocation, or three. The `TGroup` and
   `TBuilding` records are already structs. With EMS gone, `Ems_EmsMapAll`
   calls disappear.
5. **Fixed-segment pointers** (listed above). DS-relative ones become `&ds.X`
   or pointers to named globals. The stack-segment ones (`SCRATCH_PAL`,
   `gfx.c:50`) become locals or statics.
6. **Procedure pointers.**
   - `Game_FindBuildingProc`, `FindPlanetForceProc`, `GrWarProc`,
     `GfxPathProc`, `Ems_RestoreProc`, `System_ExitProc` and
     `Gfx_SavedExitProc` become C function pointers.
   - `call_far_proc` goes.
   - `system_add_exit_proc(far_ptr(0x405F, 0x1A62), ...)` in `main.c` takes
     just the function.
7. **The `dseg_init` relocated pointers** (galaxy tables and other typed
   constants holding far pointers into DS) become host pointers at load, or
   better, compile-time `&ds.X` initialisers once DS is no longer loaded as
   raw bytes (Stage 4).

After each step: build, unit tests, goldens. Run ASan more than before:
out-of-bounds reads that `mem[]` used to hide become visible. When ASan
finds one, find out what the original read there and decide whether to
reproduce it (deliberately, with a comment) or treat it as a bug. Record
real bugs in `docs/bugs.md` and fix them behind `fixes` only if behaviour
changes.

### Stage 4: DS stops being a memory image

1. **Name the unnamed globals.** Give every `ds_at(0x....)`, `B8(...)` and
   `W16(...)` a name, typed in `ghidra/symbols.txt`/`types.pas` so
   `gen_dseg.py` picks it up, or as a static in the unit that owns it.
   477 sites; many repeat the same offsets.
2. **Saves become an explicit serializer.**
   - Rewrite `saveload.c` to serialize fields (and the `P(...)` blocks) in
     the documented order with the documented sizes, little-endian, instead
     of copying DS byte ranges.
   - Pointer values inside saved ranges, if any: write what the original
     wrote, or zeros if the original's values are ignored on load. Find out
     which.
   - The Stage 0 save fixtures must round-trip byte-identically, and an
     original DOS save must load.
3. **Drop the packed mirror.** `DSeg` loses `#pragma pack(1)` and the
   offset `static_assert`s, `gen_dseg.py` stops generating offsets, and
   pointer fields take their natural size. If it reads better, split `DSeg`
   into per-unit globals; the Stage 0 field table must still serialize the
   same names and sizes in the same order.

   **As done:** the offset `static_assert`s went and pointer fields are
   8-byte host pointers, but `DSeg` (and the records) keep `pack(1)`: the
   original's globals in order with no padding. Naming the globals (4.1)
   found a dozen places where the original reads or writes past an array
   into the next globals (alien-talk lines into Surface::Mode, race 0/1
   records before Races, building kind 0, the toolbar scans, ...), and
   there may be untested ones (index 0 or -1 on 1-based tables). Without
   padding those stay the same bytes; with natural alignment each would
   need its own rewrite. The records are the game's data formats (loaded
   from the executable, saved, kept in heap blocks), so they stay packed
   anyway. Overruns into a pointer field are handled where they occur
   (Pub's menu lines).
4. **Initial values.** `dseg_init` copies initialised data (tables, strings,
   typed constants) from `REUNION.PRG`. Keep loading it from the user's
   executable (we can't ship it), but copy field by field through the field
   table instead of as one image, and build the pointer constants as in
   Stage 3.7.

### Stage 5: remove the model

1. Delete `mem[]`, `mem.c`/`mem.h`, `FarPtr`, `far_ptr`, `far_host`,
   `far_linear`, `ghidra_host`, `ds_at`, the Borland heap in `system.c`, and
   `MEM_*` constants. The compiler finds anything left.
2. `vid_screen` is plain; video.c's `screen_buffer` default is the only
   screen.
3. **Docs.** Rewrite the Memory model section of `docs/port.md`, and update
   `docs/globals.md`, `docs/segments.md` where it talks about the port, the
   PLAN.md decisions table and README.md if it mentions memory or EMS.
   Delete this plan, or mark it done with a summary of what changed.

## Rules for the work

- Behaviour first: a golden mismatch is a bug in the change until proven
  otherwise. Don't regenerate goldens to make a stage pass. The only
  allowed regeneration is Stage 0's own, before any code moves.
- Keep the Ghidra address comments (`// 405f:1a62`) on routines. They remain
  the link to the original even without the difftests.
- Match the surrounding code's style (C23, `nullptr`, `enum : type`,
  comments explaining the original's behaviour). See `docs/port.md`.
- Playthrough fixture rules still apply (memory
  `playthrough-fixture-rules`): visible information only, clicks and keys
  only, saves through the API are fine, no date luck.
- `test_play_game` runs only in release builds (about 4-5 minutes).
  Debug/ASan/trap builds register the other tests.

## Open questions for Alex

- Split `DSeg` into per-unit globals in Stage 4, or keep one struct?
  Default: keep one struct unless a unit is clearly self-contained.
- After Stage 5, should `REUNION.PRG` still be required at run time for the
  initial data? Today the port needs the user's executable anyway. Moving
  the tables into C source would mean shipping data derived from the game,
  so check against the licence note in PLAN.md first.
  **Answered (2026-10-12):** no; Alex prefers needing only the data files.
  The tables are in `src/game/dsinit.c`, like the cut-scenes' tables and
  the AdLib player's image before them.
