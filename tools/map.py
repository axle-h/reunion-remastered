#!/usr/bin/env python3
"""Decode Reunion planet-surface .MAP files to text, JSON or a PNG preview.

    map.py FILE.MAP [--json | --png [-o out.png]]
    map.py --batch DIR [-o OUTDIR]

Layout and tile lookup follow the game's loader (2ef2:03e7); see docs/formats/map.md.
The PNG uses the 16x16 tiles of PLANETS/FELSZ<terrain>.PIC, plus FANIM<terrain>.PIC
for the animated tiles (frame 0).
"""

import argparse
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pic  # noqa: E402

TILE = 16
TILES_PER_ROW = 20  # tile n = column n % 20, row n // 20 of the FELSZ sheet

# Word table at 4a81:1a38 (REUNION.PRG), indexed by the planet number in the
# file name: the terrain set used for PLANETS/FELSZ<n>.PIC etc. Index 0 unused.
PLANET_TERRAIN = [None, 1, 8, 2, 7, 10, 3, 9, 5, 4, 6, 11]


# Per terrain 1..11 (tables at 4a81:1810 and 4a81:1826): tiles in FELSZ<t>.PIC, and
# tiles per animation frame in FANIM<t>.PIC (0 = no animated tiles).
TERRAIN_TILES = [None, 80, 120, 240, 200, 240, 240, 120, 40, 120, 160, 240]
ANIM_FRAME_TILES = [None, 80, 0, 0, 0, 20, 0, 0, 0, 0, 80, 0]


class MapError(Exception):
    pass


class Map:
    def __init__(self, width, height, cells, planet=None, index=None):
        self.width = width
        self.height = height
        self.cells = cells  # bytes, row-major, width * height
        self.planet = planet
        self.index = index

    def row(self, y):
        return self.cells[y * self.width:(y + 1) * self.width]


def decode(data, name=""):
    if len(data) < 2:
        raise MapError("file too short")
    w, h = data[0], data[1]
    if len(data) != 2 + w * h:
        raise MapError(f"size {len(data)} != 2 + {w}*{h}")
    m = re.fullmatch(r"MAP(\d+)_(\d+)", Path(name).stem, re.I)
    planet, index = (int(m[1]), int(m[2])) if m else (None, None)
    return Map(w, h, data[2:], planet, index)


def terrain_of(m):
    if m.planet is None or not 1 <= m.planet < len(PLANET_TERRAIN):
        return None
    return PLANET_TERRAIN[m.planet]


def to_text(m):
    return "\n".join(" ".join(f"{c:3d}" for c in m.row(y)) for y in range(m.height))


def to_json(m):
    return json.dumps({
        "planet": m.planet, "index": m.index, "terrain": terrain_of(m),
        "width": m.width, "height": m.height,
        "cells": [list(m.row(y)) for y in range(m.height)],
    })


def render(m, planets_dir, frame=0):
    """Returns a pic.Pic of the map drawn with FELSZ tiles (missing tiles stay black)."""
    t = terrain_of(m)
    if t is None:
        raise MapError("unknown planet number, cannot pick a tile sheet")
    sheet = pic.decode((planets_dir / f"FELSZ{t}.PIC").read_bytes())
    W, H = m.width * TILE, m.height * TILE
    anim = None
    if ANIM_FRAME_TILES[t]:
        anim = pic.decode((planets_dir / f"FANIM{t}.PIC").read_bytes())
    out = bytearray(W * H)
    for y in range(m.height):
        for x in range(m.width):
            n = m.cells[y * m.width + x]
            src = sheet
            if n >= TERRAIN_TILES[t]:  # animated tile: FANIM, frame `frame`
                if anim is None:
                    continue
                src = anim
                n = n - TERRAIN_TILES[t] + frame * ANIM_FRAME_TILES[t]
            sx = (n % TILES_PER_ROW) * TILE
            sy = (n // TILES_PER_ROW) * TILE
            if sy + TILE > src.height:
                continue
            for r in range(TILE):
                s = (sy + r) * src.width + sx
                d = (y * TILE + r) * W + x * TILE
                out[d:d + TILE] = src.pixels[s:s + TILE]
    return pic.Pic(W, H, bytes(out), sheet.palette)


def default_planets(src):
    return src.resolve().parent.parent / "PLANETS"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("source", type=Path, help=".MAP file, or a directory with --batch")
    ap.add_argument("-o", "--output", type=Path)
    ap.add_argument("--json", action="store_true", help="print JSON instead of a text grid")
    ap.add_argument("--png", action="store_true", help="write a PNG preview")
    ap.add_argument("--planets", type=Path, help="PLANETS directory (default: sibling of MAP/)")
    ap.add_argument("--batch", action="store_true", help="write .txt, .json and .png for every map")
    args = ap.parse_args()

    if not args.batch:
        m = decode(args.source.read_bytes(), args.source.name)
        if args.png:
            dst = args.output or args.source.with_suffix(".png")
            pic.write_png(dst, render(m, args.planets or default_planets(args.source)))
            print(f"{dst}: {m.width}x{m.height} cells")
        else:
            print(to_json(m) if args.json else to_text(m))
        return 0

    out_dir = args.output or Path(__file__).parent / "out" / "map"
    out_dir.mkdir(parents=True, exist_ok=True)
    planets = args.planets or default_planets(args.source / "x")
    ok = failed = 0
    for src in sorted(p for p in args.source.iterdir() if p.suffix.upper() == ".MAP"):
        try:
            m = decode(src.read_bytes(), src.name)
            stem = src.stem
            (out_dir / f"{stem}.txt").write_text(to_text(m) + "\n")
            (out_dir / f"{stem}.json").write_text(to_json(m) + "\n")
            pic.write_png(out_dir / f"{stem}.png", render(m, planets))
            ok += 1
        except (MapError, pic.PicError, OSError) as e:
            print(f"{src}: {e}", file=sys.stderr)
            failed += 1
    print(f"{ok} converted, {failed} failed -> {out_dir}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
