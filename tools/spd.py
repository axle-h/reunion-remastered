#!/usr/bin/env python3
"""Inspect ANIM/*.SPD (ProTracker modules) and SAVE/SETUP.SPD; see docs/formats/spd.md.

    spd.py ANIM/ATV1.SPD [--mod out.mod]      module summary
    spd.py --batch DIR [-o OUTDIR]            summarise all, write OUTDIR/NAME.mod + index.txt
    spd.py SAVE/SETUP.SPD                     sound settings
"""

import argparse
import struct
import sys
from pathlib import Path

SETUP_SIZE = 7


def parse_setup(data):
    if len(data) != SETUP_SIZE:
        raise ValueError("SETUP.SPD must be 7 bytes")
    kind, port, irq, dma = struct.unpack("<BHHH", data)
    return {"card": kind, "port": port, "irq": irq, "dma": dma}


def parse_mod(data):
    if data[1080:1084] != b"M.K.":
        raise ValueError("no M.K. signature")
    title = data[:20].split(b"\0")[0].decode("latin-1")
    samples = []
    for i in range(31):
        o = 20 + 30 * i
        name = data[o:o + 22].split(b"\0")[0].decode("latin-1")
        length, fine, vol, loop, looplen = struct.unpack(">HBBHH", data[o + 22:o + 30])
        samples.append((name, length * 2, vol))
    song_len = data[950]
    orders = data[952:1080]
    patterns = max(orders) + 1  # the game scans all 128 slots, not just song_len
    expect = 1084 + patterns * 1024 + sum(s[1] for s in samples)
    return {"title": title, "song_len": song_len, "patterns": patterns,
            "samples": [s for s in samples if s[1]], "expected_size": expect,
            "size": len(data)}


def describe(path):
    data = path.read_bytes()
    m = parse_mod(data)
    used = len(m["samples"])
    return (f"{path.name}: title={m['title']!r} orders={m['song_len']} patterns={m['patterns']} "
            f"samples={used} pcm={sum(s[1] for s in m['samples'])} size={m['size']}"
            f"{'' if m['size'] == m['expected_size'] else ' (SIZE MISMATCH)'}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("source", type=Path)
    ap.add_argument("--batch", action="store_true")
    ap.add_argument("--mod", type=Path, help="write a .mod copy")
    ap.add_argument("-o", "--output", type=Path, default=Path(__file__).parent / "out" / "spd")
    args = ap.parse_args()
    if args.batch:
        args.output.mkdir(parents=True, exist_ok=True)
        lines = []
        for p in sorted(args.source.rglob("*.SPD")):
            lines.append(describe(p))
            (args.output / (p.stem + ".mod")).write_bytes(p.read_bytes())
        (args.output / "index.txt").write_text("\n".join(lines) + "\n")
        print("\n".join(lines))
    elif args.source.name.upper() == "SETUP.SPD":
        print(parse_setup(args.source.read_bytes()))
    else:
        print(describe(args.source))
        if args.mod:
            args.mod.write_bytes(args.source.read_bytes())


if __name__ == "__main__":
    sys.exit(main())
