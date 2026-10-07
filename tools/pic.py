#!/usr/bin/env python3
"""Decode Reunion .PIC images ("SpidyGfx") to indexed PNG.

    pic.py FILE.PIC [-o out.png] [--palette FILE]
    pic.py --batch DIR [-o OUTDIR] [--palette FILE]

The decoder follows the game's own loader (405f:0bef); see docs/formats/pic.md.
PNGs keep the original palette indices, with colours as the VGA would show them
(the game drops the low two bits of each 8-bit component).
"""

import argparse
import struct
import sys
import zlib
from pathlib import Path

MAGIC = b"SpidyGfx"
HEADER_SIZE = 12
PALETTE_SIZE = 768
PALETTE_MARKER = 0x0C


class PicError(Exception):
    pass


class Pic:
    def __init__(self, width, height, pixels, palette):
        self.width = width
        self.height = height
        self.pixels = pixels    # bytes, width * height palette indices
        self.palette = palette  # 256 (r, g, b) tuples, 6-bit VGA DAC values


def decode_rows(data, width, height):
    """Runs the game's RLE loop: returns (pixels, rows that overran width).

    Each row decodes until its column count passes `width`. A run may cross
    the end of a row; the loader writes it out anyway and the next row starts
    counting from zero, so the output is one linear stream.
    """
    out = bytearray()
    pos = HEADER_SIZE
    overruns = 0
    try:
        for _ in range(height):
            col = 0
            while col < width:
                b = data[pos]
                pos += 1
                if b >= 0xC0:
                    count = b & 0x3F
                    out += bytes([data[pos]]) * count
                    pos += 1
                    col += count
                else:
                    out.append(b)
                    col += 1
            overruns += col != width
    except IndexError:
        raise PicError("pixel data ends early") from None
    return out, overruns


def read_palette(raw):
    """768 bytes of 8-bit RGB -> 6-bit DAC values, as the loader does."""
    return [(raw[i] >> 2, raw[i + 1] >> 2, raw[i + 2] >> 2) for i in range(0, 768, 3)]


def decode(data, warn=None):
    if len(data) < HEADER_SIZE + PALETTE_SIZE + 1 or data[:8] != MAGIC:
        raise PicError("not a SpidyGfx picture")
    width, height = struct.unpack_from("<HH", data, 8)

    pixels, overruns = decode_rows(data, width, height)
    if overruns:
        # INFO/INFO26.PIC is stored 192 wide but its header says 191.
        fixed, fixed_overruns = decode_rows(data, width + 1, height)
        if fixed_overruns == 0:
            if warn:
                warn(f"header width {width} is wrong, rows are {width + 1} wide")
            width, pixels = width + 1, fixed
        elif warn:
            warn(f"{overruns} rows overrun the width")

    # The loader finds the palette from the end of the file, not the data.
    if data[-PALETTE_SIZE - 1] != PALETTE_MARKER and warn:
        warn("palette marker missing")
    palette = read_palette(data[-PALETTE_SIZE:])
    return Pic(width, height, bytes(pixels[: width * height]), palette)


def load_palette(path):
    """A palette from another .PIC, or a raw 768-byte file (CREDITS/PAL*.PIC)."""
    data = Path(path).read_bytes()
    if data[:8] == MAGIC:
        data = data[-PALETTE_SIZE:]
    if len(data) != PALETTE_SIZE:
        raise PicError(f"{path}: not a palette")
    return read_palette(data)


# ---- PNG output --------------------------------------------------------

def _chunk(kind, body):
    crc = zlib.crc32(kind + body)
    return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", crc)


def write_png(path, pic):
    def expand(v):  # 6-bit DAC value to 8 bits, as video.c does
        return v << 2 | v >> 4

    plte = bytes(expand(c) for rgb in pic.palette for c in rgb)
    rows = b"".join(
        b"\0" + pic.pixels[y * pic.width:(y + 1) * pic.width] for y in range(pic.height)
    )
    ihdr = struct.pack(">IIBBBBB", pic.width, pic.height, 8, 3, 0, 0, 0)
    Path(path).write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + _chunk(b"IHDR", ihdr)
        + _chunk(b"PLTE", plte)
        + _chunk(b"IDAT", zlib.compress(rows, 9))
        + _chunk(b"IEND", b"")
    )


# ---- command line ----------------------------------------------------------

def convert(src, dst, palette=None):
    pic = decode(src.read_bytes(), warn=lambda msg: print(f"{src}: warning: {msg}", file=sys.stderr))
    if palette:
        pic.palette = palette
    dst.parent.mkdir(parents=True, exist_ok=True)
    write_png(dst, pic)
    return pic


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("source", type=Path, help=".PIC file, or a directory with --batch")
    ap.add_argument("-o", "--output", type=Path, help="output PNG or directory")
    ap.add_argument("--batch", action="store_true", help="convert every .PIC under a directory")
    ap.add_argument("--palette", help="take the palette from this .PIC or raw palette file")
    args = ap.parse_args()

    palette = load_palette(args.palette) if args.palette else None

    if not args.batch:
        dst = args.output or args.source.with_suffix(".png")
        pic = convert(args.source, dst, palette)
        print(f"{dst}: {pic.width}x{pic.height}")
        return 0

    out_dir = args.output or Path(__file__).parent / "out" / "pic"
    ok = skipped = failed = 0
    for src in sorted(p for p in args.source.rglob("*") if p.suffix.lower() == ".pic"):
        rel = src.relative_to(args.source).with_suffix(".png")
        if src.read_bytes()[:8] != MAGIC:
            skipped += 1
            continue
        try:
            convert(src, out_dir / rel, palette)
            ok += 1
        except PicError as e:
            print(f"{src}: {e}", file=sys.stderr)
            failed += 1
    print(f"{ok} converted, {failed} failed, {skipped} skipped (not SpidyGfx) -> {out_dir}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
