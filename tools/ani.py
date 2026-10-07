#!/usr/bin/env python3
"""Decode Reunion .ANI animations ("SpidyAnim") to PNG frames.

    ani.py FILE.ANI [-o OUTDIR] [--palette FILE] [--mode auto|delta|keyed]
    ani.py --batch DIR [-o OUTDIR] [--palette FILE] [--mode ...]

For each animation this writes OUTDIR/NNN.png per frame, sheet.png (all frames
on one image) and anim.png (an animated PNG). The decoder follows the game's
own (3c64:057c); see docs/formats/ani.md. ANI files carry no palette: by
default one is taken from the picture the game draws the animation over.
"""

import argparse
import math
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from pic import Pic, PicError, _chunk, load_palette, write_png  # noqa: E402

MAGIC = b"SpidyAnim"
HEADER_SIZE = 13  # magic, width, height


class AniError(Exception):
    pass


class Frame:
    def __init__(self, width, height, ops):
        self.width = width
        self.height = height
        self.ops = ops  # (offset, count, value) writes; skipped pixels are absent


def decode_frame(blk):
    """One chunk -> Frame. Same opcodes and end test as 3c64:057c."""
    width, height = struct.unpack_from("<HH", blk, 9)
    remaining = width * height
    pos, out, ops = HEADER_SIZE, 0, []
    try:
        while True:
            b = blk[pos]
            pos += 1
            if b < 0x80:                      # literal pixel
                ops.append((out, 1, b))
                n = 1
            elif b == 0x80:                   # long skip: word count
                n, = struct.unpack_from("<H", blk, pos)
                pos += 2
            elif b < 0xC0:                    # short skip
                n = b & 0x3F
            elif b == 0xC0:                   # long run: word count, value
                n, = struct.unpack_from("<H", blk, pos)
                ops.append((out, n, blk[pos + 2]))
                pos += 3
            else:                             # short run: value
                n = b & 0x3F
                ops.append((out, n, blk[pos]))
                pos += 1
            out += n
            # The game counts down a 16-bit register and stops only at exactly 0.
            remaining = (remaining - n) & 0xFFFF
            if remaining == 0:
                break
    except (IndexError, struct.error):
        raise AniError("frame data ends early") from None
    if out != width * height:
        raise AniError(f"frame covers {out} pixels, not {width}x{height}")
    if pos != len(blk):
        raise AniError(f"{len(blk) - pos} bytes left after frame data")
    return Frame(width, height, ops)


def parse(data):
    """The file is just chunks: word size, then size + 13 bytes (3c64/405f:0af8)."""
    frames, pos = [], 0
    while pos < len(data):
        if pos + 2 > len(data):
            raise AniError("stray byte at end of file")
        size, = struct.unpack_from("<H", data, pos)
        blk = data[pos + 2:pos + 2 + size + HEADER_SIZE]
        if len(blk) < size + HEADER_SIZE:
            raise AniError(f"frame {len(frames) + 1} is truncated")
        if blk[:9] != MAGIC:
            raise AniError(f"frame {len(frames) + 1} has no SpidyAnim header")
        frames.append(decode_frame(blk))
        pos += 2 + size + HEADER_SIZE
    if not frames:
        raise AniError("no frames")
    if len({(f.width, f.height) for f in frames}) != 1:
        raise AniError("frames differ in size")
    return frames


def _apply(canvas, frame):
    for off, n, v in frame.ops:
        canvas[off:off + n] = bytes([v]) * n


def default_mode(src):
    """What each frame is a delta against, as the game draws it.

    'keyed': frame 1. Only 3c64:0496 draws this way (frame 1, then frame N);
    it is used for WAR/WAR/SA*.ANI and ANIM/MAIN17.ANI.
    'delta': the previous frame. Everything else draws frame N over what is
    already on screen or in a persistent buffer (3c64:0516).
    """
    name = src.name.upper()
    return "keyed" if name.startswith("SA") or name == "MAIN17.ANI" else "delta"


def render(frames, mode):
    """Full images for every frame. Pixels never written stay 0."""
    size = frames[0].width * frames[0].height
    first = bytearray(size)
    _apply(first, frames[0])
    images, canvas = [bytes(first)], bytearray(first)
    for f in frames[1:]:
        if mode == "keyed":
            canvas = bytearray(first)
        _apply(canvas, f)
        images.append(bytes(canvas))
    return images


# ---- palettes ------------------------------------------------------------

# Animations in REUNION.PRG are drawn with colour offset 40h over screens whose
# picture was also loaded at offset 40h, so ANI value v is that picture's colour v.
SCREEN_PALETTES = {
    # Full-screen cut-scenes played over PICS/PICn.PIC by 2e4b:0044.
    "ANIM/MAIN8.ANI": "PICS/PIC9.PIC",
    "ANIM/MAIN9.ANI": "PICS/PIC9.PIC",
    "ANIM/MAIN10.ANI": "PICS/PIC1.PIC",
    "ANIM/MAIN13.ANI": "PICS/PIC10.PIC",
    "ANIM/MAIN14.ANI": "PICS/PIC2.PIC",
    # Death scenes: caller not found; frame 1 matches these pictures.
    "ANIM/MAIN15.ANI": "GRAFIKA/DEATH2.PIC",
    "ANIM/MAIN16.ANI": "GRAFIKA/DEATH.PIC",
    "ANIM/MAIN17.ANI": "GRAFIKA/DISK.PIC",  # disk menu, 1a3e:0ee5
    "ANIM": "GRAFIKA/MAIN.PIC",       # other MAIN*.ANI: main screen
    "WAR/WAR": "WAR/SPWAR.PIC",       # SA*.ANI: space battle screen
    "WAR/GRWAR": "WAR/GRWAR.PIC",     # GRANIM*.ANI: ground battle screen
}


def _game_root(path):
    for p in path.resolve().parents:
        if (p / "GRAFIKA").is_dir():
            return p
    return None


def find_palette(src):
    """Best guess at the palette file for `src`, or None."""
    # INTRO/, CREDITS/, VICTORY/: ANIMn.ANI plays over PALn.PIC (nearest lower n).
    digits = "".join(c for c in src.stem if c.isdigit())
    if digits:
        pals = {}
        for p in src.parent.iterdir():
            d = p.stem[3:]
            if p.suffix.upper() == ".PIC" and p.stem.upper().startswith("PAL") and d.isdigit():
                pals[int(d)] = p
        lower = [n for n in pals if n <= int(digits)]
        if lower:
            return pals[max(lower)]
    root = _game_root(src)
    if root:
        rel = src.resolve().relative_to(root).as_posix().upper()
        for key in (rel, rel.rsplit("/", 1)[0]):
            if key in SCREEN_PALETTES and (root / SCREEN_PALETTES[key]).exists():
                return root / SCREEN_PALETTES[key]
    return None


GREY = [(i >> 2, i >> 2, i >> 2) for i in range(256)]


# ---- output ----------------------------------------------------------------

def make_sheet(images, width, height, gap=2, max_width=2560):
    cols = max(1, min(len(images), (max_width + gap) // (width + gap)))
    rows = math.ceil(len(images) / cols)
    sw, sh = cols * (width + gap) - gap, rows * (height + gap) - gap
    sheet = bytearray(sw * sh)
    for i, img in enumerate(images):
        x0, y0 = (i % cols) * (width + gap), (i // cols) * (height + gap)
        for y in range(height):
            o = (y0 + y) * sw + x0
            sheet[o:o + width] = img[y * width:(y + 1) * width]
    return sw, sh, bytes(sheet)


def write_apng(path, images, width, height, palette, delay_ms):
    def expand(v):
        return v << 2 | v >> 4

    def idat(img):
        rows = b"".join(b"\0" + img[y * width:(y + 1) * width] for y in range(height))
        return zlib.compress(rows, 9)

    out = [b"\x89PNG\r\n\x1a\n",
           _chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 3, 0, 0, 0)),
           _chunk(b"PLTE", bytes(expand(c) for rgb in palette for c in rgb)),
           _chunk(b"acTL", struct.pack(">II", len(images), 0))]
    seq = 0
    for i, img in enumerate(images):
        out.append(_chunk(b"fcTL", struct.pack(">IIIIIHHBB", seq, width, height, 0, 0,
                                               delay_ms, 1000, 0, 0)))
        seq += 1
        if i == 0:
            out.append(_chunk(b"IDAT", idat(img)))
        else:
            out.append(_chunk(b"fdAT", struct.pack(">I", seq) + idat(img)))
            seq += 1
    out.append(_chunk(b"IEND", b""))
    Path(path).write_bytes(b"".join(out))


def convert(src, out_dir, palette=None, mode="auto", delay_ms=100):
    frames = parse(src.read_bytes())
    if mode == "auto":
        mode = default_mode(src)
    pal_src = None
    if palette is None:
        pal_src = find_palette(src)
        palette = load_palette(pal_src) if pal_src else GREY
    w, h = frames[0].width, frames[0].height
    images = render(frames, mode)
    out_dir.mkdir(parents=True, exist_ok=True)
    for i, img in enumerate(images, 1):
        write_png(out_dir / f"{i:03d}.png", Pic(w, h, img, palette))
    write_png(out_dir / "sheet.png", Pic(*make_sheet(images, w, h), palette))
    write_apng(out_dir / "anim.png", images, w, h, palette, delay_ms)
    return frames, mode, pal_src


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("source", type=Path, help=".ANI file, or a directory with --batch")
    ap.add_argument("-o", "--output", type=Path, help="output directory")
    ap.add_argument("--batch", action="store_true", help="convert every .ANI under a directory")
    ap.add_argument("--palette", help="take the palette from this .PIC or raw palette file")
    ap.add_argument("--mode", choices=("auto", "delta", "keyed"), default="auto",
                    help="frames are deltas of the previous frame or of frame 1 "
                         "(auto: as the game draws that file)")
    ap.add_argument("--delay", type=int, default=100, help="ms per frame in anim.png")
    args = ap.parse_args()

    palette = load_palette(args.palette) if args.palette else None

    def describe(frames, mode, pal_src):
        f = frames[0]
        pal = pal_src.name if pal_src else ("given" if palette else "grey")
        return f"{len(frames)} frames {f.width}x{f.height} {mode}, palette {pal}"

    if not args.batch:
        dst = args.output or Path(__file__).parent / "out" / "ani" / args.source.stem
        info = convert(args.source, dst, palette, args.mode, args.delay)
        print(f"{dst}: {describe(*info)}")
        return 0

    out_root = args.output or Path(__file__).parent / "out" / "ani"
    ok = failed = 0
    for src in sorted(p for p in args.source.rglob("*") if p.suffix.lower() == ".ani"):
        dst = out_root / src.relative_to(args.source).with_suffix("")
        try:
            info = convert(src, dst, palette, args.mode, args.delay)
            print(f"{src.relative_to(args.source)}: {describe(*info)}")
            ok += 1
        except (AniError, PicError) as e:
            print(f"{src}: {e}", file=sys.stderr)
            failed += 1
    print(f"{ok} converted, {failed} failed -> {out_root}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
