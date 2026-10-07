#!/usr/bin/env python3
"""Decode Reunion TEXT\\ files (dialogue, messages, info tables) to UTF-8 text.

    text.py FILE [-o out.txt]
    text.py DIR [-o OUTDIR]          (default OUTDIR: tools/out/text)

Line files (.AT, .LOC, .SP, .TXT) are obfuscated per line; the decoder follows
the game's TextFile.Decode (398f:0864). Output line N is the game's line N, with
'|' line breaks and "NN " prefixes kept as stored.

.RAW files are fixed-size Pascal string tables; each record is written as a
"[n]" header followed by one line per string slot.

See docs/formats/text.md.
"""

import argparse
import sys
from pathlib import Path

LINE_EXTS = {".at", ".loc", ".sp", ".txt"}

# .RAW tables: name -> (slot size in bytes, slots per record). The game reads
# each file with one BlockRead of exactly its size (4049:0011/0083/00f3).
RAW_TABLES = {
    "sz_face.raw": (41, 4),    # string[40]: 12 hireable people x 4 slots
    "sz_talal.raw": (31, 7),   # string[30]: 35 shop items x 7 slots
    "sz_faj.raw": (36, 12),    # string[35]: 12 alien races x 12 slots
}


class TextError(Exception):
    pass


def decode_line(raw):
    """398f:0864: s[i] := (s[i] + 5780h - i * 4Fh) mod E0h - 20h, i from 1."""
    return bytes(((b + 0x5780 - i * 0x4F) % 0xE0 - 0x20) & 0xFF for i, b in enumerate(raw, 1))


def encode_line(text):
    """Inverse of decode_line for bytes 00h-BFh; produces bytes 20h-FFh."""
    return bytes((c + i * 0x4F) % 0xE0 + 0x20 for i, c in enumerate(text, 1))


def split_lines(data, warn):
    """CRLF-terminated lines, as Readln sees them."""
    lines = data.split(b"\r\n")
    if lines and lines[-1] == b"":
        lines.pop()
    elif lines:
        warn("last line has no CR LF")
    for n, line in enumerate(lines, 1):
        if any(c in line for c in b"\r\n\x1a"):
            warn(f"line {n}: stray CR, LF or ^Z")
        if len(line) > 255:
            warn(f"line {n}: {len(line)} bytes, the game keeps 255")
    return lines


def decode_line_file(data, warn):
    """-> list of decoded str records (CP437)."""
    out = []
    for n, raw in enumerate(split_lines(data, warn), 1):
        text = decode_line(raw[:255])
        if any(c < 0x20 or c > 0x7E for c in text):
            warn(f"line {n}: non-ASCII after decoding (plain text file?)")
        out.append(text.decode("cp437"))
    return out


def decode_raw_file(data, slot, per_record, warn):
    """-> list of records, each a list of str. Bytes after each length are junk."""
    rec_size = slot * per_record
    if len(data) % rec_size:
        warn(f"{len(data)} bytes is not a multiple of {rec_size}")
    records = []
    for base in range(0, len(data) - rec_size + 1, rec_size):
        rec = []
        for off in range(base, base + rec_size, slot):
            n = data[off]
            if n >= slot:
                raise TextError(f"offset {off}: length {n} overflows a {slot}-byte slot")
            rec.append(data[off + 1:off + 1 + n].decode("cp437"))
        records.append(rec)
    return records


def convert(src, dst):
    warn = lambda msg: print(f"{src}: warning: {msg}", file=sys.stderr)
    data = src.read_bytes()
    name = src.name.lower()
    if name in RAW_TABLES:
        records = decode_raw_file(data, *RAW_TABLES[name], warn)
        lines = []
        for n, rec in enumerate(records, 1):
            lines.append(f"[{n}]")
            lines.extend(rec)
        summary = f"{len(records)} records"
    elif src.suffix.lower() in LINE_EXTS:
        lines = decode_line_file(data, warn)
        summary = f"{len(lines)} lines"
    else:
        raise TextError("unknown text file type")
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text("".join(line + "\n" for line in lines), encoding="utf-8")
    return summary


def is_text_file(path):
    return path.is_file() and (path.suffix.lower() in LINE_EXTS or path.name.lower() in RAW_TABLES)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("source", type=Path, help="a TEXT\\ file, or a directory of them")
    ap.add_argument("-o", "--output", type=Path, help="output .txt file or directory")
    args = ap.parse_args()

    if not args.source.is_dir():
        dst = args.output or Path(str(args.source) + ".txt")
        print(f"{dst}: {convert(args.source, dst)}")
        return 0

    out_dir = args.output or Path(__file__).parent / "out" / "text"
    ok = failed = 0
    for src in sorted(p for p in args.source.iterdir() if is_text_file(p)):
        # KERDES1.AT, .LOC and .SP share a stem, so keep the extension.
        try:
            convert(src, out_dir / (src.name + ".txt"))
            ok += 1
        except TextError as e:
            print(f"{src}: {e}", file=sys.stderr)
            failed += 1
    print(f"{ok} decoded, {failed} failed -> {out_dir}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
