#!/usr/bin/env python3
"""Decode Reunion .VEC vector models to JSON, Wavefront OBJ and PNG previews.

    vec.py FILE.VEC [-o OUTDIR] [--angles A,B,C] [--scale N]
    vec.py --batch DIR [-o OUTDIR] [--angles A,B,C] [--scale N]

For VECTORS/Vn.VEC this writes Vn.json (the parsed file plus the face normals
and view settings the game derives), Vn.obj (+ edges as `l` lines), Vn.png (a
flat-shaded render made the way the game draws it, cropped to its 115x114
viewport) and Vn_wire.png (the green wireframe view). The decoder follows the
game's loader (3d38:084a) and renderer (segment 3e17); see docs/formats/vec.md.
"""

import argparse
import json
import math
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from pic import Pic, write_png  # noqa: E402

MAX_OBJECTS = 17          # per-model buffer arrays are [1..17]
MAX_FACE_VERTS = 19       # 3e17:00cc stops at index 20h of a 20-byte record
FACE_SIZE = 20
END_MARK = -1

# Per-model view constants from the data segment, indexed by model number 1..35.
ZOOM = [51, 50, 50, 62, 70, 65, 60, 65, 71, 60, 65, 78, 70, 61, 63, 67, 70, 67,
        60, 70, 60, 62, 65, 56, 67, 67, 59, 65, 65, 53, 69, 67, 70, 55, 60]  # DS:56CE
MODE = [1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1]                    # DS:56F2
SPIN = [(5, 0, 0), (5, 0, 0), (5, 3, 1), (0, 5, 3), (5, 0, 0), (5, 3, 1), (5, 0, 0),
        (5, 3, 1), (5, 3, 1), (5, 0, 0), (5, 3, 1), (5, 0, 0), (5, 0, 0), (2, 3, 5),
        (3, 4, 2), (3, 4, 2), (5, 0, 0), (5, 3, 1), (5, 3, 1), (5, 3, 1), (5, 0, 0),
        (5, 0, 0), (2, 3, 1), (2, 3, 5), (5, 0, 0), (5, 3, 1), (2, 3, 4), (5, 0, 0),
        (5, 0, 0), (5, 0, 0), (5, 0, 0), (5, 0, 0), (5, 0, 0), (5, 0, 0), (5, 3, 1)]  # DS:5716
START_ANGLES = {14: (0xD7, 0x2D, 0x36), 24: (0, 0x15, 0x20)}  # else (0, 0, 300)
ANGLE_UNITS = 1080        # angles count in 1/3 degree

# Objects drawn back faces first (two-sided), and faces the game never draws.
TWO_SIDED = {(7, 1), (12, 7), (18, 5), (26, 7), (26, 9), (27, 4)}
SKIP_FRONT = {(7, 1): {19, 36, 64, 71}, (12, 7): {27}, (18, 5): {11, 13, 15, 17, 18}}
SKIP_BACK = {(7, 1): {19, 36, 40, 64, 71}, (12, 7): {27}, (18, 5): {11, 13, 15, 17, 18}}

SOLID_CENTRE = (0x86, 0x43)           # screen offset of model origin, 320-wide buffer
SOLID_WINDOW = (76, 10, 0x73, 0x72)   # x, y, w, h blitted to the screen (offset 0CCCh)
WIRE_CENTRE = (0x46, 0x32)
WIRE_WINDOW = (16, 0, 0x6A, 0x6C)     # region cleared at offset 10h
WIRE_ZOOM = 0x1E
NORMAL_SCALE = 0x40
BACKGROUND = 0x40
WIRE_COLOUR = 0x41
SHADE_BASE = 0xC0


class VecError(Exception):
    pass


# ---- parsing ---------------------------------------------------------------

def parse(data):
    """Returns a list of objects: dict(size, vertices, edges, faces)."""
    objects = []
    pos = 0

    def take(fmt, count=1):
        nonlocal pos
        size = struct.calcsize(fmt) * count
        if pos + size > len(data):
            raise VecError(f"data ends early at offset {pos}")
        out = [struct.unpack_from(fmt, data, pos + i * struct.calcsize(fmt)) for i in range(count)]
        pos += size
        return out

    while True:
        (size,), = take("<h")
        if size == END_MARK:
            break
        start = pos
        (nv,), = take("<h")
        verts = [list(v) for v in take("<3h", nv)]
        (ne,), = take("<h")
        edges = [list(e) for e in take("<2B", ne)]
        (nf,), = take("<h")
        faces = []
        for (rec,) in take(f"<{FACE_SIZE}s", nf):
            idx = []
            for b in rec[:MAX_FACE_VERTS]:
                if b == 0xFF:
                    break
                idx.append(b)
            faces.append(idx)
        for a, b in edges:
            if not (1 <= a <= nv and 1 <= b <= nv):
                raise VecError(f"object {len(objects) + 1}: edge {a}-{b} out of range")
        for f in faces:
            if len(f) < 3 or not all(1 <= i <= nv for i in f):
                raise VecError(f"object {len(objects) + 1}: bad face {f}")
        objects.append(dict(size=size, size_ok=pos - start == size,
                            vertices=verts, edges=edges, faces=faces))
    if pos != len(data):
        raise VecError(f"{len(data) - pos} bytes after the end marker")
    if len(objects) > MAX_OBJECTS:
        raise VecError(f"{len(objects)} objects, the game has room for {MAX_OBJECTS}")
    return objects


# ---- the game's derived data (3d38:0696, 3e17:04d2) --------------------------

def tdiv(a, b):
    """Pascal div: truncates toward zero."""
    q = abs(a) // abs(b)
    return q if (a < 0) == (b < 0) else -q


def pround(x):
    """Pascal Round: halves away from zero."""
    return int(math.floor(abs(x) + 0.5)) * (1 if x >= 0 else -1)


def sbyte(v):
    return ((v + 0x80) & 0xFF) - 0x80


def game_vertices(obj):
    """Coordinates as the loader leaves them: each word div 10."""
    return [[tdiv(c, 10) for c in v] for v in obj["vertices"]]


def centroid(verts):
    n = len(verts)
    return [sum(v[i] for v in verts) / n for i in range(3)]


def face_normals(verts, faces):
    """3e17:04d2: normal of the first three vertices, turned to point at the
    object's centroid, scaled to length 50."""
    c = centroid(verts)
    out = []
    for f in faces:
        p1, p2, p3 = (verts[i - 1] for i in f[:3])
        a = [p2[i] - p1[i] for i in range(3)]
        b = [p3[i] - p1[i] for i in range(3)]
        n = [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
        if sum(n[i] * (c[i] - p1[i]) for i in range(3)) < 0:
            n = [-x for x in n]
        k = math.sqrt(sum(x * x for x in n)) / 50 or 1.0
        out.append([pround(x / k) for x in n])
    return out


# ---- projection (3e17:1500) ------------------------------------------------

COS = [pround(math.cos(i * math.pi / 180) * 500) for i in range(361)]  # 3e17:18db


def sin500(a):
    return COS[a + 270] if a < 90 else COS[a - 90]


def matrix(a, b, c, mode=1):
    """Rows of the game's rotation matrix (entries scaled by 500) for angles in
    degrees, a, b, c being the 1st..3rd arguments of 3e17:1500."""
    a, b, c = a % 360, b % 360, c % 360
    ca, sa, cb, sb, cc, sc = COS[a], sin500(a), COS[b], sin500(b), COS[c], sin500(c)
    r1 = [tdiv(cb * ca, 500), tdiv(cb * sa, 500), sb]
    scsb = tdiv(sc * sb, 500)
    ccsb = tdiv(cc * sb, 500)
    r2 = [tdiv(cc * sa, 500) + tdiv(scsb * ca, 500),
          tdiv(scsb * sa, 500) + tdiv(-cc * ca, 500),
          tdiv(-sc * cb, 500)]
    r3 = [tdiv(sc * sa, 500) - tdiv(ccsb * ca, 500),
          tdiv(-sc * ca, 500) - tdiv(ccsb * sa, 500),
          tdiv(cc * cb, 500)]
    if mode == 2:
        r1, r2 = r2, r1
    return r1, r2, r3


def project(points, m, scale):
    """Each coordinate's low byte times scale (signed 8x8 multiply), times the
    matrix, keeping the high word of the 32-bit sum."""
    out = []
    for p in points:
        x, y, z = (sbyte(c) * sbyte(scale) for c in p)
        out.append([(x * r[0] + y * r[1] + z * r[2]) >> 16 for r in m])
    return out


def view_angles(number, angles=None):
    """Angles in game units (1/3 degree) -> degrees as passed to 3e17:1500."""
    a, b, c = angles if angles else START_ANGLES.get(number, (0, 0, 300))
    return a % ANGLE_UNITS // 3, b % ANGLE_UNITS // 3, c % ANGLE_UNITS // 3


# ---- rasterising -----------------------------------------------------------

class Canvas:
    def __init__(self, width, height, fill):
        self.width, self.height = width, height
        self.px = bytearray([fill]) * (width * height)

    def put(self, x, y, c):
        if 0 <= x < self.width and 0 <= y < self.height:
            self.px[y * self.width + x] = c

    def crop(self, x0, y0, w, h):
        rows = (self.px[(y0 + y) * self.width + x0:(y0 + y) * self.width + x0 + w] for y in range(h))
        return bytes(b"".join(rows))


def edge_into(table, x1, y1, x2, y2):
    """3e17:1438: x of an edge for every scanline it covers; horizontal edges
    write nothing."""
    if y1 == y2:
        if x1 == x2:
            table[y1] = x1
        return
    if y1 > y2:
        x1, y1, x2, y2 = x2, y2, x1, y1
    for y in range(y1, y2 + 1):
        table[y] = x1 + pround((x2 - x1) * (y - y1) / (y2 - y1))


def fill_polygon(canvas, pts, colour):
    """3e17:11f9 + 13d6: split the outline at its top and bottom vertices into
    two edge tables, then fill |right-left| pixels per row, dithering colour
    and colour+1 on even/odd columns."""
    n = len(pts)
    ring = pts + [pts[0]]
    ys = [p[1] & 0xFFFF for p in pts]      # compared unsigned
    lo = hi = 0
    for i in range(n - 1, 0, -1):
        if ys[i] > ys[hi]:
            hi = i
        if ys[i] < ys[lo]:
            lo = i
    first, last = min(lo, hi), max(lo, hi)
    a, b = {}, {}
    for k in range(first, last):
        edge_into(a, *ring[k], *ring[k + 1])
    for k in list(range(last, n)) + list(range(0, first)):
        edge_into(b, *ring[k], *ring[k + 1])
    y_min, y_max = min(p[1] for p in pts), max(p[1] for p in pts)
    for y in range(y_min, y_max + 1):
        if y not in a or y not in b:          # the game reuses stale entries
            continue
        xa, xb = a[y], b[y]
        x0, count = min(xa, xb), abs(xa - xb)
        for i in range(count):
            x = x0 + i
            odd_start = i == 0 and x & 1
            canvas.put(x, y, colour + 1 if (x & 1) == 0 or odd_start else colour)


def draw_line(canvas, x0, y0, x1, y1, colour):
    dx, dy = abs(x1 - x0), -abs(y1 - y0)
    sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
    err = dx + dy
    while True:
        canvas.put(x0, y0, colour)
        if x0 == x1 and y0 == y1:
            return
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy


def shade(n, front):
    """Light from -x/+z: 0C0h + clamp((nz - nx + 24) * 4 div 5, 0, 2Eh); the
    back-face pass of two-sided objects uses nx - nz."""
    d = n[2] - n[0] if front else n[0] - n[2]
    v = tdiv((d + 24) * 4, 5)
    return SHADE_BASE + max(0, min(0x2E, v))


def render_solid(objects, number, angles=None):
    """3d38:01aa: depth-sort objects by their projected centroid, then draw
    each object's front faces (3d38:0000, 3e17:00cc)."""
    mode = MODE[number - 1] if 1 <= number <= len(MODE) else 1
    zoom = ZOOM[number - 1] if 1 <= number <= len(ZOOM) else 64
    m = matrix(*view_angles(number, angles), mode)
    canvas = Canvas(320, 0xAF00 // 320, BACKGROUND)

    gverts = [game_vertices(o) for o in objects]
    cents = [[pround(c) for c in centroid(v)] for v in gverts]
    depth = [p[2] for p in project(cents, m, NORMAL_SCALE)]
    order = list(range(len(objects)))
    for i in range(len(order) - 1):            # 3d38:00e5, selection sort
        best = i
        for j in range(i + 1, len(order)):
            if depth[order[best]] < depth[order[j]]:
                best = j
        order[i], order[best] = order[best], order[i]
    if number == 35:                           # the game draws only object 1
        order = [0]

    for k in order:
        obj, slot = objects[k], k + 1
        sv = project(gverts[k], m, zoom)
        sn = project(face_normals(gverts[k], obj["faces"]), m, NORMAL_SCALE)
        passes = [(False, SKIP_BACK)] if (number, slot) in TWO_SIDED else []
        passes.append((True, SKIP_FRONT))
        for front, skip in passes:
            for fi, (face, n) in enumerate(zip(obj["faces"], sn), 1):
                if (n[2] >= 0) != front or fi in skip.get((number, slot), ()):
                    continue
                pts = [(sv[i - 1][0] + SOLID_CENTRE[0], sv[i - 1][1] + SOLID_CENTRE[1]) for i in face]
                fill_polygon(canvas, pts, shade(n, front))
    return canvas.crop(*SOLID_WINDOW), SOLID_WINDOW[2:]


def render_wire(objects, angles=None):
    """3d38:0490 / 0413: every object's edges in colour 41h, zoom 30, mode 1."""
    m = matrix(*view_angles(0, angles), 1)
    canvas = Canvas(320, 0x8980 // 320, BACKGROUND)
    for obj in objects:
        sv = project(game_vertices(obj), m, WIRE_ZOOM)
        for a, b in obj["edges"]:
            (x0, y0), (x1, y1) = sv[a - 1][:2], sv[b - 1][:2]
            draw_line(canvas, x0 + WIRE_CENTRE[0], y0 + WIRE_CENTRE[1],
                      x1 + WIRE_CENTRE[0], y1 + WIRE_CENTRE[1], WIRE_COLOUR)
    return canvas.crop(*WIRE_WINDOW), WIRE_WINDOW[2:]


def palette():
    pal = [(0, 0, 0)] * 256
    pal[WIRE_COLOUR] = (0, 0x38, 0)                        # 3d38:04f9
    for i in range(48):                                    # 3d38:03b3
        pal[SHADE_BASE + i] = (0, 0, (i * 40) // 48 + 10)
    return pal


def upscale(pixels, w, h, k):
    if k == 1:
        return pixels, w, h
    rows = []
    for y in range(h):
        row = bytes(b for b in pixels[y * w:(y + 1) * w] for _ in range(k))
        rows.extend([row] * k)
    return b"".join(rows), w * k, h * k


def save_png(path, pixels, size, scale):
    px, w, h = upscale(pixels, *size, scale)
    write_png(path, Pic(w, h, px, palette()))


# ---- JSON / OBJ ------------------------------------------------------------

def model_number(path):
    digits = "".join(ch for ch in Path(path).stem if ch.isdigit())
    return int(digits) if digits else 0


def to_json(objects, number):
    out = dict(model=number, objects=[])
    if 1 <= number <= len(ZOOM):
        out["view"] = dict(zoom=ZOOM[number - 1], mode=MODE[number - 1],
                           spin=list(SPIN[number - 1]),
                           start_angles=list(START_ANGLES.get(number, (0, 0, 300))))
    for o in objects:
        gv = game_vertices(o)
        out["objects"].append(dict(
            size=o["size"],
            vertices=o["vertices"],
            edges=o["edges"],
            faces=o["faces"],
            normals=face_normals(gv, o["faces"]),
            centroid=[round(c, 2) for c in centroid(gv)],
        ))
    return out


def to_obj(objects, name):
    """Vertices as (x, y, -z): the game's projection mirrors the file's axes,
    so this keeps the model the right way round in y-up viewers. Faces are
    wound counter-clockwise seen from outside, using the game's inside test."""
    lines = [f"# {name}: Reunion vector model, file units (game divides by 10)"]
    base = 0
    for k, o in enumerate(objects, 1):
        lines.append(f"o object{k}")
        pts = [(x, y, -z) for x, y, z in o["vertices"]]
        lines += [f"v {x} {y} {z}" for x, y, z in pts]
        c = centroid(pts)
        for f in o["faces"]:
            p1, p2, p3 = (pts[i - 1] for i in f[:3])
            a = [p2[i] - p1[i] for i in range(3)]
            b = [p3[i] - p1[i] for i in range(3)]
            n = [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
            idx = f if sum(n[i] * (c[i] - p1[i]) for i in range(3)) <= 0 else f[::-1]
            lines.append("f " + " ".join(str(base + i) for i in idx))
        lines += [f"l {base + a} {base + b}" for a, b in o["edges"]]
        base += len(pts)
    return "\n".join(lines) + "\n"


# ---- command line ------------------------------------------------------------

def convert(src, out_dir, angles=None, scale=3):
    objects = parse(src.read_bytes())
    for k, o in enumerate(objects, 1):
        if not o["size_ok"]:
            print(f"{src}: warning: object {k} size field {o['size']} is wrong", file=sys.stderr)
    number = model_number(src)
    stem = src.stem.upper()
    out_dir.mkdir(parents=True, exist_ok=True)
    text = json.dumps(to_json(objects, number), indent=1)
    text = re.sub(r"\[[^][{}]*\]", lambda m: json.dumps(json.loads(m.group())), text)
    (out_dir / f"{stem}.json").write_text(text + "\n")
    (out_dir / f"{stem}.obj").write_text(to_obj(objects, src.name))
    save_png(out_dir / f"{stem}.png", *render_solid(objects, number, angles), scale)
    save_png(out_dir / f"{stem}_wire.png", *render_wire(objects, angles), scale)
    return objects


def counts(objects):
    return (len(objects), sum(len(o["vertices"]) for o in objects),
            sum(len(o["edges"]) for o in objects), sum(len(o["faces"]) for o in objects))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("source", type=Path, help=".VEC file, or a directory with --batch")
    ap.add_argument("-o", "--output", type=Path, help="output directory")
    ap.add_argument("--batch", action="store_true", help="convert every .VEC under a directory")
    ap.add_argument("--angles", help="view angles A,B,C in game units (1080 = full turn); "
                                     "default: the game's start angles")
    ap.add_argument("--scale", type=int, default=3, help="PNG pixel scale (default 3)")
    args = ap.parse_args()
    angles = tuple(int(v) for v in args.angles.split(",")) if args.angles else None
    if angles and len(angles) != 3:
        ap.error("--angles needs three values")

    out_dir = args.output or (Path(__file__).parent / "out" / "vec" if args.batch
                              else args.source.parent)
    if not args.batch:
        objects = convert(args.source, out_dir, angles, args.scale)
        print("{}: {} objects, {} vertices, {} edges, {} faces".format(args.source, *counts(objects)))
        return 0

    total = [0, 0, 0, 0]
    ok = failed = 0
    for src in sorted((p for p in args.source.rglob("*") if p.suffix.lower() == ".vec"),
                      key=lambda p: (model_number(p), p.name)):
        try:
            objects = convert(src, out_dir, angles, args.scale)
        except VecError as e:
            print(f"{src}: {e}", file=sys.stderr)
            failed += 1
            continue
        total = [t + c for t, c in zip(total, counts(objects))]
        ok += 1
    print("{} converted, {} failed: {} objects, {} vertices, {} edges, {} faces -> {}".format(
        ok, failed, *total, out_dir))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
