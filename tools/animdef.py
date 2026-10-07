#!/usr/bin/env python3
"""Dump WAR/ANIM/ANIM.DEF (space-battle background animation scripts) as JSON.

    animdef.py ANIM.DEF [-o out.json]

Layout is documented in docs/formats/animdef.md (loader 20d4:0985).
"""

import argparse
import json
import sys
from pathlib import Path

SCRIPT_SIZE = 0x230
ENTRIES = SCRIPT_SIZE // 4


def decode(data):
    if len(data) % SCRIPT_SIZE:
        raise ValueError("size is not a multiple of 560")
    scripts = []
    for s in range(len(data) // SCRIPT_SIZE):
        base = s * SCRIPT_SIZE
        steps = []
        for e in range(ENTRIES):
            anim, frame, sound, delay = data[base + e * 4:base + e * 4 + 4]
            steps.append({"anim": anim, "frame": frame, "sound": sound, "delay": delay})
            if delay == 0xFF:  # end marker; the game loops or switches script here
                break
        scripts.append({"script": s + 1, "steps": steps})
    return scripts


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("source", type=Path)
    ap.add_argument("-o", "--output", type=Path, default=Path(__file__).parent / "out" / "animdef.json")
    args = ap.parse_args()
    scripts = decode(args.source.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(scripts, indent=1))
    print(f"{len(scripts)} scripts, {sum(len(s['steps']) for s in scripts)} steps -> {args.output}")


if __name__ == "__main__":
    sys.exit(main())
