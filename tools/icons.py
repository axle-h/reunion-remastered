#!/usr/bin/env python3
"""Decode ICON/ICON.ALL (68 icons of 40x24) to PNG; see docs/formats/icons.md.

    icons.py ICON.ALL [-o OUTDIR] [--palette ICONMAIN.PIC]

Writes one PNG per icon (NN.png) plus a sheet.png with all of them.
"""

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pic

W, H = 40, 24
COUNT = 68
BIAS = 2  # the game adds 2 to every stored value (3a1b:0864)


def split_chunks(data):
    p, chunks = 0, []
    while p < len(data):
        (n,) = struct.unpack_from("<H", data, p)
        chunks.append(data[p + 2:p + 2 + n])
        p += 2 + n
    if p != len(data):
        raise ValueError("trailing bytes")
    return chunks


def unrle(chunk, count=W * H):
    out = bytearray()
    q = 0
    while len(out) < count:
        b = chunk[q]
        q += 1
        if b >= 0xC0:
            out += bytes([chunk[q]]) * (b & 0x3F)
            q += 1
        else:
            out.append(b)
    return bytes((v + BIAS) & 0xFF for v in out[:count])


def decode(data):
    chunks = split_chunks(data)
    if len(chunks) != COUNT:
        raise ValueError(f"{len(chunks)} icons, expected {COUNT}")
    return [unrle(c) for c in chunks]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("source", type=Path)
    ap.add_argument("-o", "--output", type=Path, default=Path(__file__).parent / "out" / "icons")
    ap.add_argument("--palette", type=Path)
    args = ap.parse_args()
    palette = pic.load_palette(args.palette or args.source.with_name("ICONMAIN.PIC"))
    icons = decode(args.source.read_bytes())
    args.output.mkdir(parents=True, exist_ok=True)
    for i, px in enumerate(icons):
        pic.write_png(args.output / f"{i:02d}.png", pic.Pic(W, H, px, palette))
    cols = 10
    rows = -(-COUNT // cols)
    sheet = bytearray(cols * W * rows * H)
    for i, px in enumerate(icons):
        x0, y0 = (i % cols) * W, (i // cols) * H
        for y in range(H):
            o = (y0 + y) * cols * W + x0
            sheet[o:o + W] = px[y * W:(y + 1) * W]
    pic.write_png(args.output / "sheet.png", pic.Pic(cols * W, rows * H, bytes(sheet), palette))
    print(f"{len(icons)} icons -> {args.output}")


if __name__ == "__main__":
    main()
