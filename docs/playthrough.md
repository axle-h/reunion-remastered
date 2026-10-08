# Full playthrough fixture

Goal: show that the port can be won. A fixture plays a new game to the end
with a policy list taken from [walkthrough.md](walkthrough.md) and ends on the
Earth ground-battle win (`Halt(10)`, VICTORY).

## Rules

- **Input only.** The fixture acts only through simulated mouse moves, clicks
  and key presses (`input_set_mouse`, `input_push_key`). It never writes game
  state; a write barrier (hash of the emulated memory around every controller
  call) enforces this.
- **Player-visible information only** for decisions: the screen shown, its
  hotspots and captions, messages, date, credits, planet and group info the
  game displays. Hidden state (story timers, enemy forces not shown, RNG) may
  be read for logs and assertions, never to decide.
- **No date luck.** One fixed seed for now (`REUNION_SEED`), but every wait is
  on a visible condition with a timeout, never on a frame or date that only
  holds for this seed.
- **Saves.** Not required before battles. Checkpoints for iteration may be
  saved and loaded through an API rather than the disk screen. The proof run
  is one continuous game from "new game".
- **No cheats.**

## Proof

The fixture records every input it makes as a plain autopilot script (frame
number + event). Replaying that script with the same seed in the normal
`reunion` binary, without fixture code, must also end in exit code 10.

## Architecture

1. **Frame hook.** `timing_wait_retrace` calls the controller once per retrace
   after `input_pump`.
2. **Sequential policies.** The controller runs on its own thread with a strict
   hand-off (exactly one of game and controller runs at a time), so a policy
   reads top to bottom: `hire(...)`, `wait_until(pred, timeout)`, ...
3. **Layers.**
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
