# Differential tests (retired)

Until 2026-10-11, `tests/test_diff_<unit>.c` (38 files, plus the harness in
`tests/diff/` and `tests/test_travel.c`) ran routines of the original
`REUNION.PRG` under [Unicorn](https://www.unicorn-engine.org/), an x86 CPU
emulator, and the port's C versions on the same memory, and compared
everything they left behind. They found most of the bugs in
[port-bugs.md](port-bugs.md) that say "`test_diff_...` fails on the old
code".

They worked because the port kept the original's real-mode address space
(`mem[]`): a port state was also a state of the original. Every unit was
ported and matched, and the whole game played through
([playthrough.md](playthrough.md)), so the port moved to native C memory
(docs/port.md, "Memory"), where the original code
can't run on the port's state any more.

What replaced them: golden hashes of the game's state, which doesn't depend
on the memory layout (`src/game/state.h`, `tests/test_golden.c`,
`tests/golden/`), and the playthrough's proof.

To run them, check out the tag `last-real-mode`; this file there describes
the harness.
