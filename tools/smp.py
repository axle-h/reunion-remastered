#!/usr/bin/env python3
"""Convert Reunion .SMP sound effects (Creative Voice File blocks) to WAV.

    smp.py FILE.SMP [-o out.wav]
    smp.py --batch DIR [-o OUTDIR]

Each file is a headerless VOC stream: one type-1 (sound data) block followed by
a type-0 terminator. See docs/formats/smp.md. Output is 8-bit unsigned mono PCM
at 1000000 / (256 - time constant) Hz. --batch recurses into DIR and writes
OUTDIR/<subdir>_<NAME>.wav (default OUTDIR: tools/out/smp).
"""

import argparse
import sys
import wave
from pathlib import Path


class SmpError(Exception):
    pass


def decode(data):
    """Returns (rate_hz, pcm_bytes, time_constant)."""
    if len(data) < 7:
        raise SmpError("file too short")
    if data[0] != 1:
        raise SmpError(f"unexpected block type {data[0]}")
    length = int.from_bytes(data[1:4], "little")
    tc, codec = data[4], data[5]
    if codec != 0:
        raise SmpError(f"unsupported codec {codec}")
    if length < 2 or 4 + length > len(data):
        raise SmpError("block length exceeds file")
    if tc == 255:
        raise SmpError("invalid time constant")
    if data[4 + length:] != b"\x00":
        raise SmpError("missing terminator block")
    return 1000000 // (256 - tc), data[6:4 + length], tc


def write_wav(path, rate, pcm):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(1)
        w.setframerate(rate)
        w.writeframes(pcm)


def convert(src, dst):
    rate, pcm, _ = decode(Path(src).read_bytes())
    Path(dst).parent.mkdir(parents=True, exist_ok=True)
    write_wav(dst, rate, pcm)
    return rate, len(pcm)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("input", nargs="?", help="a .SMP file")
    ap.add_argument("--batch", metavar="DIR", help="convert every .SMP under DIR")
    ap.add_argument("-o", "--output", help="output .wav (single) or directory (batch)")
    args = ap.parse_args()

    if args.batch:
        root = Path(args.batch)
        out = Path(args.output or Path(__file__).parent / "out" / "smp")
        ok = bad = 0
        for f in sorted(p for p in root.rglob("*") if p.suffix.lower() == ".smp"):
            rel = f.relative_to(root).with_suffix(".wav")
            dst = out / "_".join(rel.parts).lower()
            try:
                convert(f, dst)
                ok += 1
            except SmpError as e:
                print(f"{f}: {e}", file=sys.stderr)
                bad += 1
        print(f"{ok} converted, {bad} failed -> {out}")
        return 1 if bad else 0
    if not args.input:
        ap.error("need FILE or --batch DIR")
    dst = args.output or Path(args.input).with_suffix(".wav")
    try:
        rate, n = convert(args.input, dst)
    except SmpError as e:
        sys.exit(f"{args.input}: {e}")
    print(f"{dst}: {n} samples at {rate} Hz")


if __name__ == "__main__":
    sys.exit(main())
