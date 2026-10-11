# Full playthrough fixture

Goal: show that the port can be won. A fixture plays a new game to the end
with a policy list taken from [walkthrough.md](walkthrough.md) and ends on the
Earth ground-battle win (`Halt(10)`, VICTORY).

## Rules

- **Input only.** The fixture acts only through simulated mouse moves, clicks
  and key presses (`input_set_mouse`, `input_push_key`). It never writes game
  state; a write barrier (the game's state, `game/state.h`, compared around
  every controller call) enforces this.
- **Player-visible information only** for decisions: the screen shown, its
  hotspots and captions, messages, date, credits, planet and group info the
  game displays. Hidden state (story timers, enemy forces not shown, RNG) may
  be read for logs and assertions, never to decide.
- **No date luck.** One fixed seed for now (`PLAY_SEED`), but every wait is
  on a visible condition with a timeout, never on a frame or date that only
  holds for this seed.
- **Saves.** Not required before battles. Checkpoints for iteration may be
  saved and loaded through an API rather than the disk screen. The proof run
  is one continuous game from "new game".
- **No cheats.**

## Proof

The fixture records every input it makes as a plain autopilot script (frame
number + event). Replaying that script with the same seed in the normal
`reunion` binary, without fixture code, must also end in exit code 10:

```sh
build/debug/reunion --replay=play_full.txt
# replay: exit 10 after N frames, hash H   (exit status 10)
```

`--replay` runs headless (offscreen video, dummy audio, unless `SDL_*_DRIVER`
says otherwise), skips the cut-scenes, prints the end as the fixture reports
it (halt code, retraces, hash of the game's state, `state_hash()`) and exits with the
halt code. A script that has not ended 100,000 retraces after its last event
fails (status 1).

### Result: seed 12345

`tests/test_play_game.c` plays one game from "new game" with seed 12345
through the 76 steps of acts 1-5 and wins: Halt(10) after 3,466,588
retraces, on 2930-02-26 04:00 (game date), state hash
`06c903c221ede3c0`. The recording, 3.2 MB of plain autopilot script, is
[tests/play/proof_12345.txt](../tests/play/proof_12345.txt); the fixture
replays it in-process and `reunion --replay` ends the same way, with exit
status 10:

```sh
build/debug/reunion --replay=tests/play/proof_12345.txt vendor/Reunion_DOS_EN
# replay: exit 10 after 3466588 frames, hash 06c903c221ede3c0   (exit status 10)
```

The recording was made again on 2026-10-11: the fixes to game logic that
followed the player's selection changed the run, and the first recording
(Halt(10) after 3,490,240 retraces, Earth taken 2930-03-05) no longer
replayed. Milestones of this one: Jade found 2927-08-25, Jaanosian contact
2928-03-02, the Galleon invented 2928-07-23, Mirach 3 taken (the Morgruls
eliminated) 2929-03-28, the League's surrender 2929-09-23, Earth
2930-02-26 (the dates of each step in
[playthrough-policy.md](playthrough-policy.md) §1 "As run" are the first
recording's). The Earth invasion: in space lost 45H 22SF, destroyed 64H
64SF 7D 5C; on the ground lost 10T 21Air 35ML, destroyed 30Tr 90T 50Air
30ML. `test_play_game` takes 3-4 minutes in a release build
(ctest `TIMEOUT` 900 s) and about 26 in a debug one, so ctest runs it only
in non-Debug builds (`ctest --test-dir build/release -R test_play_game`);
the debug binary still builds for running by hand.

### Robustness: other seeds

The same policy, unchanged per seed, from "new game" with `PLAY_SEED`
(release build, one recording each):

| Seed | Result | Where and why |
|---|---|---|
| 1 | lost at M18; with the fixes won 2930-01-27 | M18, GROUND WAR not offered: the click aimed at Mirach 3 on the star map landed on Mirach 2 as the planets passed each other, and the zoom showed Mirach 2. `ui_invade` now checks that the zoom names the planet and aims again. The rerun (both fixes) went differently from M16 on and needed no second aim. |
| 2 | won 2930-02-23 | |
| 3 | lost at M16 | Detoxin: New Earth stopped at 5 Derricks (seed 12345: 20) and gained ~270 t a day; the army's lines took it as it came, and the Space station (60,000 t) was never bought within M16's 60 days. The army goal now keeps the station's ores from its lines while M16 waits; then the Detoxin gathers, but too slowly (18,000 t after 60 days). Why E21 built no more Derricks is open. |
| 4 | won 2930-03-09 | |
| 5 | won 2930-02-29 | |
| 6 | won 2930-02-23 | |

Seeds 2, 4, 5 and 6 ran before the two fixes, 1 and 3 again after them;
five of six win. Seed 12345's recording is unchanged by both fixes (same
hash).

## Architecture

1. **Frame hook.** `timing_wait_retrace` calls the controller once per retrace
   after `input_pump`.
2. **Sequential policies.** The controller runs on its own thread with a strict
   hand-off (exactly one of game and controller runs at a time), so a policy
   reads top to bottom: `hire(...)`, `wait_until(pred, timeout)`, ...
   ([tests/play/play.h](../tests/play/play.h))
   - Write barrier: the game's state (`game/state.h`: the data segment
     field by field without pointers, the blocks behind them, the screen
     and the palette) is serialized before every policy slice and compared
     after it; a change fails the run and names the field
     (`DS Game_Credits+0`). Retraces the policy sleeps through
     (`play_wait(n)`) cost nothing.
   - The fixture boots like `src/main.c` does for a scripted run (fixes on,
     no intro), so its runs and `reunion --replay` end the same.
   - A policy that returns ends the run as `play_quit()` does. The recording
     ends with a `# end: exit E after F frames, hash H` comment.
3. **Checkpoints.** `play_checkpoint("name")` writes `# checkpoint name at
   frame F, hash H` into the recording. `play_resume(recording, "name",
   policy, ...)` replays the recording up to that line with no policy, checks
   the state hash at frame F, and wakes the policy there
   (`play_resumed_at()` names the checkpoint). The new recording starts with
   the prefix, so it is again one script from power-on; resumes can chain.
   Replaying a prefix runs at about 200,000 retraces/s, so the whole game
   replays in seconds: no in-memory snapshots (they would also have to carry
   the audio and AdLib player state, and the statics outside
   the emulated memory).
4. **Runner** ([tests/play/run.h](../tests/play/run.h)). A policy test is a
   list of steps; `play_main` takes a checkpoint before each step, so
   `PLAY_RESUME=step` restarts from that step of the last recording and
   `PLAY_STOP=step` ends before one. It then replays the recording in-process
   and with `reunion --replay` (`REUNION_BIN`, set by CMake for
   `test_play_*`) and checks all three ends match. Other variables:
   `PLAY_SEED`, `PLAY_RECORD`, `PLAY_RESUME_FROM`, `PLAY_VERIFY=0`,
   `PLAY_DUMP_DIR`, `PLAY_TRACE`, `PLAY_TRACE_DUMP`, `PLAY_STATE_DUMP`.
   `tests/test_play_bench.c` is the smallest
   example.
5. **Speed** (debug build, one core). The clock runs about 98 retraces per
   game hour on the command centre, so two game years are about 1.7 million
   retraces. A policy that checks a condition every retrace runs about 22,000
   retraces/s (the write barrier is most of it),
   a sleeping policy or a replay about 200,000: a whole game is about 1.5
   minutes to play and 10 s to replay. Unpaced autopilot runs (the fixture,
   `--replay`, soak runs) advance the audio without synthesising it
   (`audio_advance`: music position, the AdLib timer and which sound effects
   still play move on exactly as when mixing) and present at most 60 frames
   per wall-clock second. Before: 740 retraces/s per-retrace policy, 1,300
   replay (Nuked OPL synthesis and the barrier's two 1.1 MB hashes).
6. **Layers.**
   - Actuator + recorder: the only code that produces input; logs the replay
     script.
   - UI drivers, one module per screen: navigate, find hotspots by caption,
     hire, research, buy, build, set tax, groups, cargo, launch, pub and alien
     dialogues, battle screens.
   - Observers: read-only, player-visible predicates.
   - Policy list: steps of precondition → action → done-condition, with
     timeouts.
   - Interrupt handlers for screens and messages that appear unasked (alien
     contact, story scenes, battles, unit assignment, colony demands, research
     ideas).
   - Battle tactics: unit assignment and ground-battle moves.
7. **Modules** (`tests/play/`).
   - `play.c`: fixture core (boot, hand-off, recorder, write barrier,
     checkpoints, replay).
   - `run.c`: runner (steps, `PLAY_*` variables, verification).
   - `ui.c`: acting. Waits (`ui_wait_until`, `ui_wait_idle`, `ui_wait_screen`,
     `ui_wait_hours`), clicks (`UI_CLICK`, `UI_PRESS`, right button), toolbar
     by label or command, hotspots by caption (the k-th of a caption, zero-size
     ones skipped, a point no lower hotspot covers), the clock, navigation,
     hiring, new game.
   - `ui_message.c`: reading the screen (6x8 font text reader, status bar:
     hint line, credits, date) and the interrupts (message boxes, rename
     prompt, story scenes, hologram, inert point).
   - `ui_office.c`: research (3), info/buy (5), message log (11).
   - `ui_planet.c`: planet info (8), surface (20), droids (4), colonisation
     (28), body actions, colony list (37), bodies on the galactic map (7).
   - `ui_ships.c`: ship info (16), new group (21), equipment (22), control
     panel (17), cargo (13), destination map (18).
   - `ui_talk.c`: alien talks (34), pub (23, 25), advisor (24).
   - `ui_battle.c`: space battle (29), unit assignment (19), ground battle
     (32), GROUND WAR.
   - `policy.c`: the whole-game policy's shared part
     ([playthrough-policy.md](playthrough-policy.md) §0, §2, §3): money
     and ore gates with claims by priority (a goal waiting for money keeps
     lower ones from spending it), background goals for the steps marked
     ‖, the idle wait on the message log that runs the standing orders
     (S3 tax, S4 power, S8 wave evacuation, S11 research) at least daily
     and after every box, the dispatch of talks and battles (`pol_try`
     below), a step's chore run between the goals (`pol_between_goals`),
     and the dates of the boxes, talks and battles seen.
   - `act1.c` ... `act5.c`: the steps of each act, run by
     `tests/test_play_game.c` (one checkpoint per step:
     `PLAY_RESUME=E15_station_on_apollo`). A step rereads what it needs
     from the screen, so it can run from its checkpoint knowing only its
     name; the goals of the steps before it are registered again
     (`pol_run_step`). A resumed run therefore goes on differently from the
     continuous one (its goals start afresh), and both must pass.
   - `army.c`: the army goal of acts 3 to 5 (every production line its
     target force needs kept busy, the Army groups docked at New Earth
     filled from the stores, tier by tier).

   **Interrupt rule.** Every wait and click in `ui.c` services the interrupts
   while it waits (`ui_handle_interrupts`): it closes message boxes (their
   text stays in `ui_last_box`/`ui_box_seen`), accepts rename prompts with
   Enter, clicks through story scenes and presses away holograms. So a box
   that opens just before a click is closed instead of stalling it, and a
   driver that expects a box (a refusal) looks it up afterwards. A story
   scene ends on the command centre, so a click meant for the screen before
   it is dropped, and the driver finds its way back (`ui_open_screen`,
   `ui_open_body` try again); a sequence under `pol_try` (below) is left
   there, as for a talk (the supernova's scene, act 4, came in the middle of
   shop and surface sequences). Drivers wait with these; `play_wait_until`
   only to watch a hover or a box themselves.
   Alien talks, battles and endings are the policy's (`ui_interrupt_screen`);
   a click fails the run if one of them replaced the screen while it waited.
   So the policy runs every driver sequence (a goal's pass, a standing
   order, a step's check) under `pol_try`: `ui_on_interrupt_screen` calls
   back from the first wait that finds such a screen, the sequence is left
   there (longjmp), the screen is handled (`pol_service`) and the sequence
   runs again later from what the screens then show.

   **Game data block.** Groups and Buildings share one 8770h-byte block
   (`game/ems.h`). Observers read groups and buildings through
   `ui_group_record` and `ui_building_record`. (When the port still
   emulated EMS, the block shared the page frame with the toolbar graphics,
   and read straight from the frame a group's name came back empty right
   after a screen switch.)

## Threads of work

| # | Thread | Depends on |
|---|---|---|
| 0 | Spike: hook, recorder, hand-off; new game → hire Forbs → set tax → build; replay matches | – |
| A | Infrastructure: `REUNION_SEED`, script from file without the event cap, write barrier, replay verifier, headless speed | 0 |
| B | UI drivers per screen group, each with a small test | 0 |
| C | Policy list and interrupt catalogue from the walkthrough and the code ([playthrough-policy.md](playthrough-policy.md)) | – |
| D | Observers and predicates | 0 |
| E | Battle tactics | B (battle screens) |
| F | Integration, act by act, from API checkpoints | A–D, E by act 3 |
| G | Port-bug triage: reproduce against the original (difftest), fix, log in [port-bugs.md](port-bugs.md) | from F |
| H | Final run: one fixed seed, emit script, replay to exit 10; then more seeds as a robustness report | all |
