#!/usr/bin/env python3
"""Side by side with the DOS original: a new game in the port and in the
original under DOSBox-X, with the same seed and the same two clicks (NEW
GAME, the left hero), compared after game hours.

  tools/dosbox/side_by_side.py [-s SEED] [-n HOURS] [-e EVERY]
                               [-b BUILD_DIR] [-d DATA_DIR] [-o OUT_DIR]

Every EVERY-th of the first HOURS hourly Sims, the original's memory is
dumped at its next frame and turned into the port's state stream (dosside
dos); the port's stream at the same point is saved by dosside port. The two
are compared part by part, leaving out what the port doesn't keep (dosside.c,
IGNORED).

Needs dosbox-x built with its debugger, tmux, and the floppy release in
DATA_DIR (default vendor/Reunion_DOS_EN), copied to OUT_DIR/dos/REUNION with
GRWAR\\REUNION.EXE made from REUNION.PRG and the sound set to none. Build
dosside first: cmake --build --preset release --target dosside. The original
runs at full speed between Sims (about 3 s per game hour, plus 8 s per
dump), but input means stepping it a retrace at a time (about 0.5 s per
frame), so only the start of a game is scripted.
Exit status: 0 if every compared hour matches, 1 otherwise.
"""
import argparse
import os
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dbx  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# (frame, x, y, buttons) as the port's autopilot counts frames: retraces.
CLICKS = [(40, 160, 60, 1), (44, 160, 60, 0), (126, 80, 100, 1), (130, 80, 100, 0)]

SIM = 0x1B8A, 0x0000  # Sim_AdvanceHour
GAME_DATA, TOOLBAR = 0x8A4E, 0x7DFA  # DS:Ems_GameData, DS:GfxPath_ToolbarBlock


def prepare(data, out):
    """The game for both sides: SETUP.SPD's card set to none (byte 0 = 5)."""
    game = os.path.join(out, "dos", "REUNION")
    if not os.path.isdir(game):
        shutil.copytree(data, game)
        for name in ("REUNION", "INTRO", "CREDITS", "VICTORY"):
            shutil.copyfile(os.path.join(game, "GRWAR", name + ".PRG"), os.path.join(game, "GRWAR", name + ".EXE"))
    spd = os.path.join(game, "SAVE", "SETUP.SPD")
    with open(spd, "r+b") as f:
        f.write(b"\x05")
    return game


def dos_hours(args, out, game, hours):
    """Plays the clicks in the original and dumps memory after the hours
    asked for: OUT/dos-N.bin, the first 960K and then each EMS block's
    pages as a 64K view (dosside dos reads them back)."""
    t0 = time.time()
    dbx.boot(out, os.path.dirname(game), args.seed)
    frame = 0
    for f, x, y, b in CLICKS:
        while frame < f:
            dbx.vrt()
            frame += 1
        dbx.mouse(x, y, b)
    print(f"original: clicks done after {time.time() - t0:.0f} s", flush=True)
    sim = dbx.seg(SIM[0]), SIM[1]
    for h in range(1, max(hours) + 1):
        dbx.bpdel()
        dbx.bp(*sim)
        if not dbx.run_until(*sim, timeout=300):
            raise RuntimeError(f"hour {h}: no Sim, stopped at {dbx.at()}")
        if h not in hours:
            continue
        dbx.bpdel()
        dbx.bp(*dbx.main_frame())
        if not dbx.run_until(*dbx.main_frame()):
            raise RuntimeError(f"hour {h}: no frame after the Sim")
        mem = b"".join(dbx.dump(k << 12, 0, 0x10000) for k in range(0xE))
        views = []
        for ofs in (GAME_DATA, TOOLBAR):
            # TEmsBlock: State, Data, Size, Handle, FrameSeg, PageCount;
            # State 2 is a block in EMS.
            r = dbx.dump(dbx.DS, ofs, 13)
            if r[0] == 2:
                views.append([(dbx.u16(r, 7), p) for p in range(dbx.u16(r, 11))])
        mem += b"".join(dbx.ems_dump(views))
        with open(os.path.join(out, f"dos-{h}.bin"), "wb") as f:
            f.write(mem)
        print(f"original: hour {h} after {time.time() - t0:.0f} s", flush=True)
    dbx.stop()
    return dbx.DS


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("-s", "--seed", type=int, default=12345)
    p.add_argument("-n", "--hours", type=int, default=50)
    p.add_argument("-e", "--every", type=int, default=10)
    p.add_argument("-b", "--build", default=os.path.join(ROOT, "build", "release"))
    p.add_argument("-d", "--data", default=os.path.join(ROOT, "vendor", "Reunion_DOS_EN"))
    p.add_argument("-o", "--out", default=os.path.join(ROOT, "build", "dosbox"))
    args = p.parse_args()

    out = os.path.abspath(args.out)
    os.makedirs(out, exist_ok=True)
    dosside = os.path.join(args.build, "dosside")
    if not os.path.exists(dosside):
        sys.exit(f"no {dosside}: cmake --build --preset release --target dosside")
    game = prepare(args.data, out)
    hours = [h for h in range(1, args.hours + 1) if h % args.every == 0 or h == args.hours]

    script = os.path.join(out, "script.txt")
    with open(script, "w") as f:
        f.write(f"seed:{args.seed}\n")
        f.writelines(f"{fr}:mouse:{x},{y},{b}\n" for fr, x, y, b in CLICKS)
    subprocess.run([dosside, "port", game, "@" + script, out, str(args.hours)], check=True)

    ds = dos_hours(args, out, game, hours)
    failed = 0
    for h in hours:
        state = os.path.join(out, f"dos-{h}.state")
        port = os.path.join(out, f"port-{h}.state")
        subprocess.run([dosside, "dos", os.path.join(out, f"dos-{h}.bin"), f"{ds:X}", state, port],
                       check=True, stdout=subprocess.DEVNULL)
        print(f"hour {h}:", flush=True)
        failed |= subprocess.run([dosside, "compare", port, state]).returncode
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
