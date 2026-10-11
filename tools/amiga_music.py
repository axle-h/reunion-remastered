#!/usr/bin/env python3
"""Extract the music modules from the Amiga AGA version's disks; see docs/formats/amiga.md.

    amiga_music.py [ZIPDIR] [-o OUTDIR] [--dos DOSDIR]

ZIPDIR holds the zipped ADFs (default vendor/Reunion_Amiga_EN). Every set with
an install disk (Disk 9) is tried, so a file missing from one dump comes from
another whose table has it. Writes OUTDIR/NAME.mod and compares each with the
DOS ANIM/*.SPD.
"""

import argparse
import re
import struct
import sys
import zipfile
from pathlib import Path

BLOCK = 512
ADF_SIZE = 901120
TABLE_OFF = 0x3F44   # in Install: u16 disk<<12|block, u32 unpacked, u32 packed
NAMES_OFF = 0x5330   # in Install: 18-character names
ENTRIES = 510
NAME_LEN = 18
TRAILER = 46

LITERAL_BASE = (6, 10, 10, 18)
LITERAL_BITS = (1, 1, 1, 1, 2, 3, 3, 4, 4, 5, 7, 14)


def read_adf(zip_path):
    with zipfile.ZipFile(zip_path) as z:
        for name in z.namelist():
            if name.lower().endswith(".adf"):
                data = z.read(name)
                if len(data) == ADF_SIZE:
                    return data
    raise ValueError(f"{zip_path.name}: no ADF")


def ofs_files(adf):
    """Name -> contents of the root directory's files on an OFS disk."""
    if adf[:4] != b"DOS\0":
        raise ValueError("not an OFS disk")

    def blk(n):
        return adf[n * BLOCK:(n + 1) * BLOCK]

    root = blk(880)
    files = {}
    for slot in range(72):
        key = struct.unpack_from(">I", root, 24 + slot * 4)[0]
        while key:
            hdr = blk(key)
            name = hdr[433:433 + hdr[432]].decode("latin-1")
            if struct.unpack_from(">i", hdr, 508)[0] == -3:
                size = struct.unpack_from(">I", hdr, 324)[0]
                out = bytearray()
                data = struct.unpack_from(">I", hdr, 16)[0]
                while data and len(out) < size:
                    d = blk(data)
                    n = struct.unpack_from(">I", d, 12)[0]
                    out += d[24:24 + n]
                    data = struct.unpack_from(">I", d, 16)[0]
                files[name] = bytes(out[:size])
            key = struct.unpack_from(">I", hdr, 496)[0]
    return files


def install_table(install):
    """(name, disk, block, unpacked, packed) for every used entry."""
    out = []
    for k in range(ENTRIES):
        loc = struct.unpack_from(">H", install, TABLE_OFF + 2 * k)[0]
        unpacked = struct.unpack_from(">I", install, TABLE_OFF + 2 * ENTRIES + 4 * k)[0]
        packed = struct.unpack_from(">I", install, TABLE_OFF + 6 * ENTRIES + 4 * k)[0]
        raw = install[NAMES_OFF + NAME_LEN * k:NAMES_OFF + NAME_LEN * (k + 1)]
        name = raw.split(b"\0")[0].decode("latin-1").strip()
        if name and packed:
            out.append((name, loc >> 12, loc & 0xFFF, unpacked, packed))
    return out


def explode(data, start, literal, bits, table, size):
    """Imploder's explode: reads `data` backwards from `start`, writes `size` bytes."""
    base = struct.unpack(">8H", table[:16])
    extra = table[16:28]
    out = bytearray(size)
    i, o = start, size

    def getbit():
        nonlocal bits, i
        bit = bits >> 7
        bits = (bits << 1) & 0xFF
        if not bits:
            i -= 1
            sentinel = bit
            bit = data[i] >> 7
            bits = ((data[i] << 1) & 0xFF) | sentinel
        return bit

    def getbits(n):
        v = 0
        for _ in range(n):
            v = v << 1 | getbit()
        return v

    while True:
        if literal > o or literal > i:
            raise ValueError("literal run overflows")
        out[o - literal:o] = data[i - literal:i]
        o -= literal
        i -= literal
        if o <= 0:
            break
        # Match length and selector: 0, 10, 110, 1110 = 1..4 (selector 0..3);
        # 11110 + 3 bits = 5..12, 11111 + a byte - 1.
        if not getbit():
            selector, length = 0, 1
        elif not getbit():
            selector, length = 1, 2
        elif not getbit():
            selector, length = 2, 3
        elif not getbit():
            selector, length = 3, 4
        elif not getbit():
            selector, length = 3, getbits(3) + 5
        else:
            i -= 1
            if not data[i]:
                raise ValueError("bad match length")
            selector, length = 3, data[i] - 1
        # The next literal run's length.
        lbase, x = 0, selector
        if getbit():
            if getbit():
                lbase, x = LITERAL_BASE[selector], selector + 8
            else:
                lbase, x = 2, selector + 4
        literal = getbits(LITERAL_BITS[x]) + lbase
        # Match distance.
        match, x = o + 1, selector
        if getbit():
            if getbit():
                match, x = match + base[selector + 4], selector + 8
            else:
                match, x = match + base[selector], selector + 4
        match += getbits(extra[x])
        if length + 1 > o or match > size:
            raise ValueError("match out of range")
        for _ in range(length + 1):
            o -= 1
            match -= 1
            out[o] = out[match]
    if i != 0:
        raise ValueError(f"{i} input bytes left over")
    return bytes(out)


def unpack_chunk(chunk, packed, unpacked):
    """An AM3 chunk: IMP! with a 4-byte magic and the two length fields dropped."""
    if chunk[:3] != b"AM3":
        raise ValueError("no AM3 magic")
    body = chunk[4:4 + packed - 12]
    trailer = chunk[4 + packed - 12:4 + packed - 12 + TRAILER]
    # The packer moved the first 12 bytes into the trailer, longs reversed.
    first = trailer[8:12] + trailer[4:8] + trailer[0:4]
    data = first + body
    literal = struct.unpack_from(">I", trailer, 12)[0]
    # A word: bit 15 clear = the stream ends a byte early; the low byte is the
    # first bit buffer.
    bits = trailer[17]
    start = packed if trailer[16] & 0x80 else packed - 1
    return explode(data, start, literal, bits, trailer[18:46], unpacked)


def disk_sets(zipdir):
    """Set name -> {disk number: zip path}."""
    sets = {}
    for z in sorted(zipdir.glob("*.zip")):
        m = re.match(r"(.*?)\(Disk (\d+) of 9\)", z.name)
        if m:
            sets.setdefault(m.group(1), {})[int(m.group(2))] = z
    return sets


def mod_summary(data):
    title = data[:20].split(b"\0")[0].decode("latin-1")
    return f"title={title!r} orders={data[950]} size={len(data)}"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("zipdir", nargs="?", default="vendor/Reunion_Amiga_EN", type=Path)
    ap.add_argument("-o", "--out", default="tools/out/amiga", type=Path)
    ap.add_argument("--dos", default="vendor/Reunion_DOS_EN", type=Path,
                    help="DOS game directory, to compare with ANIM/*.SPD")
    args = ap.parse_args()

    dos = {}
    for p in sorted((args.dos / "ANIM").glob("*.SPD")):
        dos[p.stem] = p.read_bytes()

    adfs = {}
    def adf(path):
        if path not in adfs:
            adfs[path] = read_adf(path)
        return adfs[path]

    done = {}
    for set_name, disks in disk_sets(args.zipdir).items():
        if 9 not in disks:
            continue
        install = ofs_files(adf(disks[9])).get("Install")
        if not install:
            continue
        for name, disk, block, unpacked, packed in install_table(install):
            if not name.upper().endswith(".MOD") or name in done or disk not in disks:
                continue
            image = adf(disks[disk])
            chunk = image[block * BLOCK:block * BLOCK + packed + TRAILER - 8]
            try:
                data = unpack_chunk(chunk, packed, unpacked)
            except ValueError as e:
                print(f"{name}: {set_name}disk {disk}: {e}", file=sys.stderr)
                continue
            if data[1080:1084] != b"M.K.":
                print(f"{name}: not a ProTracker module", file=sys.stderr)
                continue
            done[name] = data

    if not done:
        print("no modules found", file=sys.stderr)
        return 1
    args.out.mkdir(parents=True, exist_ok=True)
    for name, data in sorted(done.items()):
        stem = name.split("/")[-1].rsplit(".", 1)[0]
        (args.out / f"{stem}.mod").write_bytes(data)
        same = [k for k, v in dos.items() if v == data]
        note = f"same as DOS {same[0]}.SPD" if same else "no identical DOS module"
        print(f"{name}: {mod_summary(data)}; {note}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
