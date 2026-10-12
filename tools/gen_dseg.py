#!/usr/bin/env python3
"""Generate src/game/dseg.h: a C mirror of REUNION.PRG's data segment.

    gen_dseg.py [--symbols ghidra/symbols.txt] [--types ghidra/types.pas]
                [-o src/game/dseg.h] [--fields src/game/dsfields.c]
                [--init src/game/dsinit.c] [--exe .../GRWAR/REUNION.PRG]

Reads the record types from types.pas and the data-segment symbols
(`4a81:XXXX Unit::Name: type // comment`) from symbols.txt and writes
`DSeg`, a packed struct with every named variable at its original offset,
plus static_asserts for the records' offsets, the field table
that walks DS by name (src/game/dsfields.c, for game/state.c), and DS's
initial values read from the executable (src/game/dsinit.c). Entries
that can't be typed or placed are reported and left out; the run itself
never fails on them. See docs/port.md.
"""

import argparse
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DS_SEG = 0x4A81
DS_SIZE = 0xA544        # end of System's variables; the stack follows
DS_INIT_SIZE = 0x759E   # typed constants, initialised from the executable

# Types Ghidra knows that types.pas doesn't define (Borland's Dos unit).
BUILTIN_TYPES = """
type
  TextRec = record
    Handle, Mode, BufSize, Private, BufPos, BufEnd: word;
    BufPtr: farptr;
    OpenFunc, InOutFunc, FlushFunc, CloseFunc: procedure;
    UserData: array[1..16] of byte;
    Name: array[0..79] of char;
    Buffer: array[0..127] of char;
  end;

  FileRec = record
    Handle, Mode, RecSize: word;
    Private: array[1..26] of byte;
    UserData: array[1..16] of byte;
    Name: array[0..79] of char;
  end;
"""

SCALARS = {
    "byte": ("uint8_t", 1),
    "shortint": ("int8_t", 1),
    "char": ("char", 1),
    "boolean": ("uint8_t", 1),   # any byte value must survive
    "word": ("uint16_t", 2),
    "integer": ("int16_t", 2),
    "longint": ("int32_t", 4),
    "pointer": ("uint32_t", 4),  # a host pointer (uint8_t *) in the port
    "farptr": ("uint32_t", 4),   # kept as a DOS far pointer value (seg:ofs)
    "procedure": ("uint32_t", 4),  # a far code pointer, kept as its value
    "real": ("Real48", 6),
    "single": ("float", 4),
    "double": ("double", 8),
    "comp": ("int64_t", 8),
    "extended": ("Extended80", 10),
}

C_RESERVED = set("""
auto break case char const continue default do double else enum extern float
for goto if inline int long register restrict return short signed sizeof static
struct switch typedef union unsigned void volatile while bool true false
nullptr alignas alignof static_assert thread_local typeof typeof_unqual
constexpr offsetof ds
""".split())


class TypeError_(Exception):
    pass


class CType:
    """A laid-out type: C base type, array dimensions (lo, hi), size."""

    def __init__(self, base, size, dims=(), note="", pointer=False, target=None, code=False):
        self.base, self.size, self.dims, self.note = base, size, list(dims), note
        self.pointer = pointer
        # Host pointers: (pointed-to C type, its array dimensions). size and
        # the layout stay the original's 4-byte far pointer.
        self.target = target
        # A procedural type: base is a C function pointer typedef.
        self.code = code

    @property
    def host(self):
        return bool(self.target) or self.code

    def decl(self, name):
        dims = "".join(f"[{hi - lo + 1}]" for lo, hi in self.dims)
        if self.target:
            base, inner = self.target
            if inner:
                return f"{base} (*{name}{dims})" + "".join(f"[{n}]" for n in inner)
            return f"{base} *{name}{dims}"
        return self.base + " " + name + dims

    def comment(self):
        parts = []
        if any(lo != 0 for lo, _ in self.dims):
            parts.append("[" + ",".join(f"{lo}..{hi}" for lo, hi in self.dims) + "]")
        if self.note:
            parts.append(self.note)
        return " ".join(parts)


class Types:
    def __init__(self, warn):
        self.warn = warn
        self.records = {}   # lower name -> (name, size, fields)
        self.procs = {}     # lower name -> (name, C typedef)
        self.order = []     # record names in definition order
        self.strings = set()

    def parse_int(self, s):
        s = s.strip()
        neg = s.startswith("-")
        s = s.lstrip("+-").strip()
        try:
            v = int(s[1:], 16) if s.startswith("$") else int(s, 10)
        except ValueError:
            raise TypeError_(f"bad array bound '{s}'") from None
        return -v if neg else v

    def resolve(self, text):
        t = " ".join(text.split())
        low = t.lower()
        if not t:
            raise TypeError_("empty type")
        if t.startswith("^"):
            # ^array[..] of T points at the first element; inner dimensions
            # stay (^array[0..1] of array[1..32] of T: T (*)[32]).
            to = self.resolve(t[1:])
            inner = [hi - lo + 1 for lo, hi in to.dims[1:]]
            note = "^" + t[1:].strip()
            return CType("uint32_t", 4, note=note, pointer=True, target=(to.base, inner))
        m = re.fullmatch(r"(?:packed\s+)?array\s*\[(.*?)\]\s*of\s+(.+)", t, re.I)
        if m:
            dims = []
            for r in m.group(1).split(","):
                if ".." not in r:
                    raise TypeError_(f"bad array range '{r}'")
                lo, hi = (self.parse_int(x) for x in r.split("..", 1))
                if hi < lo:
                    raise TypeError_(f"empty array range '{r}'")
                dims.append((lo, hi))
            elem = self.resolve(m.group(2))
            count = 1
            for lo, hi in dims:
                count *= hi - lo + 1
            return CType(elem.base, elem.size * count, dims + elem.dims, elem.note, elem.pointer,
                         elem.target, elem.code)
        m = re.fullmatch(r"string\s*\[\s*(\S+?)\s*\]", low)
        if m:
            n = self.parse_int(m.group(1))
            if not 1 <= n <= 255:
                raise TypeError_(f"bad string length {n}")
            self.strings.add(n)
            return CType(f"PStr{n}", n + 1)
        if low == "string":
            return CType("PString", 256)
        if low in SCALARS:
            base, size = SCALARS[low]
            if low == "pointer":
                return CType(base, size, pointer=True, target=("uint8_t", []))
            return CType(base, size, pointer=low in ("farptr", "procedure"))
        if low in self.records:
            name, size, _ = self.records[low]
            return CType(name, size)
        if low in self.procs:
            return CType(self.procs[low][0], 4, pointer=True, code=True)
        raise TypeError_(f"unknown type '{t}'")

    def parse_file(self, text, source):
        # Comments carry the documented sizes and offsets; collect them first.
        doc_size, doc_ofs = {}, {}
        cur = None
        for line in text.splitlines():
            m = re.match(r"\s*(\w+)\s*=\s*(?:packed\s+)?record\b.*?\{\s*(\d+)\s*bytes", line, re.I)
            if m:
                doc_size[m.group(1).lower()] = int(m.group(2))
            m = re.match(r"\s*(\w+)\s*=\s*(?:packed\s+)?record\b", line, re.I)
            if m:
                cur = m.group(1).lower()
                continue
            m = re.match(r"\s*(\w+)\s*[,:].*?\{\s*\+([0-9A-Fa-f]+)\b", line)
            if cur and m:
                doc_ofs[(cur, m.group(1).lower())] = int(m.group(2), 16)

        code = re.sub(r"\{.*?\}|\(\*.*?\*\)", " ", text, flags=re.S)
        code = re.sub(r"//[^\n]*", " ", code)
        # Procedural types: Name = procedure[(params)]; or function[(params)]: T;
        proc_re = r"(\w+)\s*=\s*(procedure|function)\s*(?:\((.*?)\))?\s*(?::\s*(\w+))?\s*;"
        for m in re.finditer(proc_re, code, re.I | re.S):
            name, kind, params, result = m.groups()
            c_params = []
            for group in (params or "").split(";"):
                if not group.strip():
                    continue
                names, ptype = group.split(":", 1)
                c_type = proc_c_type(ptype)
                c_params += [c_type] * len(names.split(","))
            c_result = proc_c_type(result) if kind.lower() == "function" else "void"
            self.procs[name.lower()] = (
                name, f"typedef {c_result} (*{name})({', '.join(c_params) or 'void'});")
        for m in re.finditer(r"(\w+)\s*=\s*(?:packed\s+)?record\b(.*?)\bend\s*;", code, re.I | re.S):
            name, body = m.group(1), m.group(2)
            try:
                if re.search(r"\b(case|record)\b", body, re.I):
                    raise TypeError_("variant and nested records aren't supported")
                fields, ofs = [], 0
                for decl in body.split(";"):
                    if not decl.strip():
                        continue
                    if ":" not in decl:
                        raise TypeError_(f"bad field '{decl.strip()}'")
                    names, ftype = decl.split(":", 1)
                    ct = self.resolve(ftype)
                    for fname in (n.strip() for n in names.split(",")):
                        if not re.fullmatch(r"[A-Za-z_]\w*", fname):
                            raise TypeError_(f"bad field name '{fname}'")
                        want = doc_ofs.get((name.lower(), fname.lower()))
                        if want is not None and want != ofs:
                            self.warn(f"{source}: {name}.{fname} at +{ofs:02X}, comment says +{want:02X}")
                        fields.append((c_ident(fname), ct, ofs))
                        ofs += ct.size
                if not fields:
                    raise TypeError_("no fields")
            except TypeError_ as e:
                self.warn(f"{source}: record {name} skipped: {e}")
                continue
            want = doc_size.get(name.lower())
            if want is not None and want != ofs:
                self.warn(f"{source}: {name} is {ofs} bytes, comment says {want}")
            if name.lower() in self.records:
                self.order.remove(self.records[name.lower()][0])
            self.records[name.lower()] = (name, ofs, fields)
            self.order.append(name)


def proc_c_type(pascal):
    """C type of a procedural type's parameter or result."""
    low = pascal.strip().lower()
    if low == "boolean":
        return "bool"
    if low not in SCALARS or low in ("pointer", "farptr", "procedure"):
        raise TypeError_(f"unsupported parameter type '{pascal}'")
    return SCALARS[low][0]


def c_ident(name):
    name = re.sub(r"\W", "_", name)
    if name[:1].isdigit():
        name = "_" + name
    return name + "_" if name in C_RESERVED else name


def c_comment(text):
    text = " ".join(text.split())
    return text + " ." if text.endswith("\\") else text


class Symbol:
    def __init__(self, ofs, name, ptype, comment, line):
        self.ofs, self.name, self.ptype, self.comment, self.line = ofs, name, ptype, comment, line
        self.ctype = None
        self.cname = None

    @property
    def size(self):
        return self.ctype.size if self.ctype else 1

    def source(self):
        s = f"{DS_SEG:04x}:{self.ofs:04x} {self.name}"
        return s + (f": {self.ptype}" if self.ptype else "")


def read_symbols(text, types, warn):
    default_unit = "System"
    for line in text.splitlines():
        m = re.match(rf"\s*@unit\s+{DS_SEG:04x}\s+(\w+)", line, re.I)
        if m:
            default_unit = m.group(1)

    syms = []
    for n, line in enumerate(text.splitlines(), 1):
        body, _, comment = line.partition("//")
        m = re.match(rf"\s*{DS_SEG:04x}:([0-9a-f]+)\s+(.*)$", body, re.I)
        if not m:
            continue
        rest = m.group(2).strip()
        mn = re.match(r"([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s*(.*)$", rest)
        if not mn:
            warn(f"symbols.txt:{n}: can't parse '{line.strip()}'")
            continue
        name, tail = mn.group(1), mn.group(2).strip()
        if tail.startswith("("):
            warn(f"symbols.txt:{n}: {name} looks like a procedure, skipped")
            continue
        if tail and not tail.startswith(":"):
            warn(f"symbols.txt:{n}: can't parse '{line.strip()}'")
            continue
        ptype = tail[1:].strip()
        sym = Symbol(int(m.group(1), 16), name, ptype, comment.strip(), n)
        if ptype:
            try:
                sym.ctype = types.resolve(ptype)
            except TypeError_ as e:
                warn(f"symbols.txt:{n}: {name}: {e}; kept as an untyped label")
                sym.comment = c_comment(f"type '{ptype}' not understood. {sym.comment}")
        unit, _, short = name.rpartition("::")
        sym.cname = c_ident((unit.replace("::", "_") or default_unit) + "_" + short)
        syms.append(sym)
    return syms


def place(syms, warn):
    """Picks the fields: typed before untyped, larger before smaller."""
    kept, dropped = [], []
    for s in sorted(syms, key=lambda s: (s.ctype is not None, s.size, -s.ofs), reverse=True):
        if s.ofs + s.size > DS_SIZE:
            warn(f"symbols.txt:{s.line}: {s.name} runs past {DS_SIZE:04X}, skipped")
            dropped.append((s, None))
            continue
        clash = next((k for k in kept if s.ofs < k.ofs + k.size and k.ofs < s.ofs + s.size), None)
        if clash:
            if not (clash.ofs == s.ofs and s.ctype is None):
                warn(f"symbols.txt:{s.line}: {s.name} overlaps {clash.name}, commented out")
            dropped.append((s, clash))
        else:
            kept.append(s)
    kept.sort(key=lambda s: s.ofs)
    seen = set()
    for s in kept:
        if s.cname in seen:
            warn(f"symbols.txt:{s.line}: {s.name} is defined twice, renamed")
            s.cname += f"_{s.ofs:04X}"
        seen.add(s.cname)
    return kept, sorted(dropped, key=lambda d: d[0].ofs)


def field_line(ct, cname, comment=""):
    text = "    " + (ct.decl(cname) if ct else f"uint8_t {cname}") + ";"
    notes = [x for x in ((ct.comment() if ct else ""), comment) if x]
    if notes:
        text += " // " + c_comment(" ".join(notes))
    return text


def generate(types, kept, dropped):
    out = [
        "// Mirror of REUNION.PRG's data segment (DS = 4a81, Ghidra numbering).",
        "// generated by tools/gen_dseg.py, do not edit (see docs/port.md).",
        "#pragma once",
        "",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",

        "#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__",
        "#error \"the DS mirror assumes a little-endian host\"",
        "#endif",
        "",
        f"#define DSEG_SIZE 0x{DS_SIZE:04X} // the original's size",
        f"#define DSEG_INIT_SIZE 0x{DS_INIT_SIZE:04X} // the typed constants (game/dsinit.c)",
        "",
        "// No padding: records are the game's data formats (loaded, saved, kept",
        "// in heap blocks), and DSeg keeps the original's globals in order with",
        "// nothing between them, because the original reads and writes past",
        "// arrays into the next globals in places and those must stay the same",
        "// bytes. Only pointers differ: `pointer` and ^T are host pointers;",
        "// farptr and procedure are still DOS far pointer values (uint32_t,",
        "// segment:offset).",
        "#pragma pack(push, 1)",
        "",
        "// Pascal types: real is 6 bytes, strings are length-prefixed.",
        "typedef struct Real48 { uint8_t b[6]; } Real48;",
        "typedef struct Extended80 { uint8_t b[10]; } Extended80;",
        "typedef struct PString { uint8_t len; char s[255]; } PString;",
    ]
    for n in sorted(types.strings):
        out.append(f"typedef struct PStr{n} {{ uint8_t len; char s[{n}]; }} PStr{n};")
    out.append("")
    out.append("// Procedural types from ghidra/types.pas: the port's C functions.")
    out += [typedef for _, typedef in types.procs.values()]
    out.append("")
    out.append("// Records from ghidra/types.pas. Array comments give the Pascal bounds;")
    out.append("// the C index is the Pascal index minus the lower bound.")
    for name in types.order:
        _, size, fields = types.records[name.lower()]
        out.append(f"typedef struct {name} {{")
        out += [field_line(ct, f) for f, ct, _ in fields]
        out.append(f"}} {name};")
        if not any(ct.host for _, ct, _ in fields):
            out.append(f"static_assert(sizeof({name}) == {size});")
            out += [f"static_assert(offsetof({name}, {f}) == 0x{o:02X});" for f, _, o in fields]
        out.append("")

    out.append("typedef struct DSeg {")
    pos, di = 0, 0

    def flush_dropped(upto):
        nonlocal di
        while di < len(dropped) and dropped[di][0].ofs < upto:
            s, clash = dropped[di]
            why = f"inside {clash.cname}" if clash else "past the end of DS"
            out.append("    // " + c_comment(f"{s.source()} ({why})" + (f" {s.comment}" if s.comment else "")))
            di += 1

    for s in kept:
        if s.ofs > pos:
            flush_dropped(s.ofs)
            out.append(f"    uint8_t _pad_{pos:04X}[0x{s.ofs - pos:X}];")
        flush_dropped(s.ofs + 1)
        out.append(field_line(s.ctype, s.cname, s.comment))
        pos = s.ofs + s.size
    flush_dropped(DS_SIZE + 0x10000)
    if pos < DS_SIZE:
        out.append(f"    uint8_t _pad_{pos:04X}[0x{DS_SIZE - pos:X}];")
    out.append("} DSeg;")
    out.append("")
    out.append("#pragma pack(pop)")
    out.append("")
    out += [
        "",
        "// The data segment (game/dseg.c).",
        "extern DSeg ds;",
        "",
        "// Gives DS its initial values: the typed constants, the rest zero, as",
        "// the original at startup.",
        "void dseg_init(void);",
        "// The values dseg_init gives DS (game/dsinit.c).",
        "extern const DSeg ds_initial;",
        "// Zeroes DS (dseg_init does so first).",
        "void dseg_clear(void);",
        "// The byte at offset orig of the original's DS, where the port keeps it",
        "// (for the far pointers DS held, and code that reads past an array as",
        "// the original does). nullptr past the end.",
        "uint8_t *dseg_orig(uint16_t orig);",
        "",
    ]
    return "\n".join(out)


def generate_fields(types, kept):
    """src/game/dsfields.c: the DS fields in offset order, padding included,
    with their sizes and which hold pointers (game/dsfields.h)."""
    rec_index = {name: i for i, name in enumerate(types.order)}

    def dims_of(ct, count):
        dims = [hi - lo + 1 for lo, hi in ct.dims]
        while len(dims) < 3:
            dims.append(1)
        if len(dims) > 3:  # fold the outer dimensions
            dims = [count // (dims[-1] * dims[-2]), dims[-2], dims[-1]]
        return ", ".join(str(x) for x in dims[:3])

    def entry(cname, ct, ofs, top=None):
        at = f"offsetof({top}, {cname})"
        if ct is None:
            return f'    {{"{cname}", 0x{ofs:04X}, {at}, 1, 1, -1, DSF_LABEL, {{1, 1, 1}}}},'
        count = 1
        for lo, hi in ct.dims:
            count *= hi - lo + 1
        rec = rec_index.get(ct.base, -1)
        flags = ("DSF_POINTER | DSF_HOST" if ct.host else "DSF_POINTER") if ct.pointer else "0"
        return (f'    {{"{cname}", 0x{ofs:04X}, {at}, {ct.size // count}, {count}, '
                f'{rec}, {flags}, {{{dims_of(ct, count)}}}}},')

    def pad(ofs, size):
        return (f'    {{"_pad_{ofs:04X}", 0x{ofs:04X}, offsetof(DSeg, _pad_{ofs:04X}), 1, {size}, -1, '
                f'DSF_UNNAMED, {{{size}, 1, 1}}}},')

    out = [
        "// The DS mirror's fields and record fields with their sizes, for code",
        "// that walks the data segment by name (game/state.c).",
        "// generated by tools/gen_dseg.py, do not edit.",
        "#include \"game/dsfields.h\"",
        "",
        "#include <stddef.h>",
        "",
        "#include \"game/dseg.h\"",
        "",
    ]
    for name in types.order:
        _, size, fields = types.records[name.lower()]
        out.append(f"static const DsField fields_{name}[] = {{")
        out += [entry(f, ct, o, name) for f, ct, o in fields]
        out.append("};")
    out.append("")
    out.append("const DsRecord ds_records[] = {")
    for name in types.order:
        _, size, fields = types.records[name.lower()]
        out.append(f'    {{"{name}", {size}, sizeof({name}), {len(fields)}, fields_{name}}},')
    out.append("};")
    out.append("const int ds_record_count = sizeof ds_records / sizeof ds_records[0];")
    out.append("")
    out.append("const DsField ds_fields[] = {")
    pos = 0
    for s in kept:
        if s.ofs > pos:
            out.append(pad(pos, s.ofs - pos))
        out.append(entry(s.cname, s.ctype, s.ofs, "DSeg"))
        pos = s.ofs + s.size
    if pos < DS_SIZE:
        out.append(pad(pos, DS_SIZE - pos))
    out.append("};")
    out.append("const int ds_field_count = sizeof ds_fields / sizeof ds_fields[0];")
    out.append("")
    return "\n".join(out)


DS_FILE_SEG = DS_SEG - 0x1000  # DS's segment in the load image
SCALAR_BASES = {base for base, _ in SCALARS.values()} - {"Real48", "Extended80"}


def ds_layout(kept):
    """DSeg's members in order, padding included: (cname, CType, ofs)."""
    out, pos = [], 0
    for s in kept:
        if s.ofs > pos:
            out.append((f"_pad_{pos:04X}", CType("uint8_t", s.ofs - pos, [(0, s.ofs - pos - 1)]), pos))
        out.append((s.cname, s.ctype or CType("uint8_t", 1), s.ofs))
        pos = s.ofs + s.size
    if pos < DS_SIZE:
        out.append((f"_pad_{pos:04X}", CType("uint8_t", DS_SIZE - pos, [(0, DS_SIZE - pos - 1)]), pos))
    return out


def elem_of(ct):
    """The element type of an array type (ct itself for a scalar)."""
    count = 1
    for lo, hi in ct.dims:
        count *= hi - lo + 1
    return CType(ct.base, ct.size // count, [], ct.note, ct.pointer, ct.target, ct.code), \
        [hi - lo + 1 for lo, hi in ct.dims]


class InitWriter:
    """src/game/dsinit.c: DS's initial values (the typed constants of the
    load image, relocated as DOS does) as a C initializer."""

    WIDTH = 100

    def __init__(self, types, kept, exe):
        self.types = types
        self.layout = ds_layout(kept)
        if exe[:2] != b"MZ":
            raise SystemExit("not an MZ executable")
        u16 = lambda b, o: b[o] | b[o + 1] << 8
        start = u16(exe, 8) * 16 + DS_FILE_SEG * 16
        image = bytearray(exe[start:start + DS_INIT_SIZE])
        if len(image) != DS_INIT_SIZE:
            raise SystemExit("executable too short")
        # Past the typed constants DS starts zeroed (System's ClearBss).
        image += bytes(DS_SIZE - DS_INIT_SIZE)
        # DOS adds the load segment to every word in the relocation table.
        # In DS those are far pointers' segments: pointers into DS become
        # host pointers (other host pointers nil), far values elsewhere keep
        # Ghidra's numbering (segment + 1000h).
        self.host = {}
        table = u16(exe, 0x18)
        base = DS_FILE_SEG * 16
        patched = bytearray(image)
        for i in range(u16(exe, 6)):
            e = table + i * 4
            linear = u16(exe, e + 2) * 16 + u16(exe, e)
            if linear < base + 2 or linear + 2 > base + DS_INIT_SIZE:
                continue
            at = linear - base - 2
            off, seg = u16(image, at), u16(image, at + 2)
            where = self.locate(at)
            if where and where[1].host:
                self.host[at] = (self.pointer_to(off, where[1].target) if seg == DS_FILE_SEG
                                 and where[1].target else None)
            else:
                patched[at:at + 2] = off.to_bytes(2, "little")
                patched[at + 2:at + 4] = ((seg + 0x1000) & 0xFFFF).to_bytes(2, "little")
        self.image = bytes(patched)

    # --- addresses -------------------------------------------------------

    def locate(self, off, fields=None, prefix="ds."):
        """The scalar holding DS byte off: (C lvalue, its CType, byte in it)."""
        for cname, ct, fofs in fields if fields is not None else self.layout:
            if not fofs <= off < fofs + ct.size:
                continue
            elem, dims = elem_of(ct)
            e, rest = divmod(off - fofs, elem.size)
            idx = ""
            for n in reversed(dims):
                e, i = divmod(e, n)
                idx = f"[{i}]" + idx
            expr = prefix + cname + idx
            rec = self.types.records.get(elem.base.lower())
            if rec and not elem.host:
                return self.locate(rest, rec[2], expr + ".")
            return expr, elem, rest
        return None

    def pointer_to(self, off, target, fields=None, prefix="ds."):
        """A C address constant of type target (base, inner dimensions) for
        the byte at DS offset off: the element that starts there where its
        type fits, else a cast. None where the port keeps a host pointer (as
        dseg_orig)."""
        base, inner = target
        for cname, ct, fofs in fields if fields is not None else self.layout:
            if not fofs <= off < fofs + ct.size:
                continue
            elem, dims = elem_of(ct)
            if elem.host:
                return None
            e, rest = divmod(off - fofs, elem.size)
            idx = []
            for n in reversed(dims):
                e, i = divmod(e, n)
                idx.insert(0, i)
            if rest == 0 and elem.base == base:
                k = len(inner)
                if k == 0:
                    return f"&{prefix}{cname}" + "".join(f"[{i}]" for i in idx)
                if dims[len(dims) - k:] == inner and not any(idx[len(idx) - k:]):
                    return f"&{prefix}{cname}" + "".join(f"[{i}]" for i in idx[:len(idx) - k])
            expr = prefix + cname + "".join(f"[{i}]" for i in idx)
            rec = self.types.records.get(elem.base.lower())
            if rec and rest:
                return self.pointer_to(rest, target, rec[2], expr + ".")
            cast = f"{base} (*)" + "".join(f"[{n}]" for n in inner) if inner else f"{base} *"
            if not rest:
                return f"({cast})&{expr}"
            if base == "uint8_t" and not inner:
                return f"(uint8_t *)&{expr} + {rest}"
            return f"({cast})((uint8_t *)&{expr} + {rest})"
        return None

    # --- values ----------------------------------------------------------

    def value(self, ct, ofs):
        """Initializer for a ct at DS offset ofs: a string, a list of
        member initializers (braced), or None for all zero."""
        elem, dims = elem_of(ct)
        if dims:
            return self.array(elem, dims, ofs)
        return self.scalar(elem, ofs)

    def array(self, elem, dims, ofs):
        if len(dims) == 1 and elem.base == "char" and not elem.host:
            return self.chars(ofs, dims[0])
        step = elem.size
        for n in dims[1:]:
            step *= n
        items = []
        for i in range(dims[0]):
            v = self.array(elem, dims[1:], ofs + i * step) if len(dims) > 1 else self.scalar(elem, ofs + i * step)
            items.append(v)
        while items and items[-1] is None:
            items.pop()
        if not items:
            return None
        return [("", v if v is not None else self.zero(elem, len(dims) > 1)) for v in items]

    @staticmethod
    def zero(ct, aggregate):
        if aggregate or ct.base not in SCALAR_BASES and not ct.host:
            return [("", "0")]
        return "nullptr" if ct.host else "0"

    def scalar(self, ct, ofs):
        data = self.image[ofs:ofs + ct.size]
        if ct.host:
            return self.host.get(ofs)
        if not any(data):
            return None
        rec = self.types.records.get(ct.base.lower())
        if rec:
            items = [(f".{f} = ", v) for f, fct, fofs in rec[2]
                     if (v := self.value(fct, ofs + fofs)) is not None]
            return items or None
        if ct.base == "PString" or ct.base.startswith("PStr"):
            n = ct.size - 1
            chars = self.chars(ofs + 1, n)
            return [("", str(data[0]))] + ([("", chars)] if chars is not None else [])
        if ct.base in ("Real48", "Extended80"):
            text = "{" + ", ".join(f"0x{b:02X}" for b in data) + "}"
            if ct.base == "Real48":
                return [("", text + f" /* {real48(data):g} */")]
            return [("", text)]
        if ct.base == "char":
            return char_literal(data[0])
        if ct.base == "float":
            return float_literal(struct.unpack("<f", data)[0]) + "f"
        if ct.base == "double":
            return float_literal(struct.unpack("<d", data)[0])
        signed = ct.base.startswith("int")
        v = int.from_bytes(data, "little", signed=signed)
        if ct.pointer:  # a far pointer value (farptr, procedure)
            return f"0x{v:08X}"
        if v == -(1 << (8 * ct.size - 1)):
            return f"({v + 1} - 1)"  # INT_MIN isn't a literal
        return str(v)

    def chars(self, ofs, n):
        """A char[n] at ofs: a string literal while it has room for the
        terminator, else character constants."""
        data = self.image[ofs:ofs + n]
        end = len(data.rstrip(b"\0"))
        if end == 0:
            return None
        if end < n:
            return string_literal(data[:end])
        return [("", char_literal(b)) for b in data]

    # --- output ----------------------------------------------------------

    def fmt(self, v, indent, lead):
        """Lines for v, its members at indent + 4; the first line continues
        one that already has lead columns."""
        if isinstance(v, str):
            return [v]
        flat = self.flat(v)
        if flat is not None and lead + len(flat) + 1 <= self.WIDTH:
            return [flat]
        pad = " " * (indent + 4)
        lines, row = ["{"], ""
        for prefix, item in v:
            if not prefix and isinstance(item, str) and "/*" not in item and not item.startswith('"'):
                # Numbers and characters: as many as fit on a line.
                if row and len(pad) + len(row) + len(item) + 1 > self.WIDTH:
                    lines.append(pad + row.rstrip())
                    row = ""
                row += item + ", "
                continue
            if row:
                lines.append(pad + row.rstrip())
                row = ""
            sub = self.fmt(item, indent + 4, len(pad) + len(prefix))
            lines.append(pad + prefix + sub[0])
            lines += sub[1:]
            lines[-1] += ","
        if row:
            lines.append(pad + row.rstrip())
        lines.append(" " * indent + "}")
        return lines

    def flat(self, v):
        if isinstance(v, str):
            return v
        parts = [self.flat(item) for _, item in v]
        if any(p is None or "\n" in p for p in parts):
            return None
        return "{" + ", ".join(prefix + p for (prefix, _), p in zip(v, parts)) + "}"

    def generate(self):
        out = [
            "// The data segment's initial values: the typed constants of",
            "// REUNION.PRG's load image (DS 0000..DSEG_INIT_SIZE-1), with the far",
            "// pointers DOS relocates as host pointers into DS. Everything else",
            "// starts zero, as after System's ClearBss.",
            "// generated by tools/gen_dseg.py, do not edit (see docs/port.md).",
            "#include \"game/dseg.h\"",
            "",
            "const DSeg ds_initial = {",
        ]
        for cname, ct, ofs in self.layout:
            if ofs >= DS_INIT_SIZE:
                break
            v = self.value(ct, ofs)
            if v is None:
                continue
            prefix = f".{cname} = "
            lines = self.fmt(v, 4, 4 + len(prefix))
            out.append(f"    // {DS_SEG:04x}:{ofs:04x}")
            out.append("    " + prefix + lines[0])
            out += lines[1:]
            out[-1] += ","
        out.append("};")
        out.append("")
        return "\n".join(out)


def real48(b):
    """The value of a 6-byte Real (b[0] exponent, bias 129; sign in b[5])."""
    if b[0] == 0:
        return 0.0
    mant = int.from_bytes(b[1:6], "little") & ((1 << 39) - 1)
    v = (1 + mant / (1 << 39)) * 2.0 ** (b[0] - 129)
    return -v if b[5] & 0x80 else v


def char_literal(b):
    if b == 0x27 or b == 0x5C:
        return f"'\\{chr(b)}'"
    if 0x20 <= b < 0x7F:
        return f"'{chr(b)}'"
    return f"'\\x{b:02X}'"


def string_literal(data):
    out = '"'
    for b in data:
        if b in (0x22, 0x5C):
            out += "\\" + chr(b)
        elif 0x20 <= b < 0x7F:
            out += chr(b)
        else:
            out += f"\\{b:03o}"
    return out + '"'


def float_literal(x):
    if x != x or x in (float("inf"), float("-inf")):
        raise SystemExit(f"non-finite float {x} in the typed constants")
    return x.hex()


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--symbols", type=Path, default=ROOT / "ghidra/symbols.txt")
    ap.add_argument("--types", type=Path, default=ROOT / "ghidra/types.pas")
    ap.add_argument("-o", "--output", type=Path, default=ROOT / "src/game/dseg.h")
    ap.add_argument("--fields", type=Path, default=ROOT / "src/game/dsfields.c",
                    help="field table for walking DS by name (game/dsfields.h)")
    ap.add_argument("--init", type=Path, default=ROOT / "src/game/dsinit.c",
                    help="DS's initial values, read from --exe")
    ap.add_argument("--exe", type=Path, default=ROOT / "vendor/Reunion_DOS_EN/GRWAR/REUNION.PRG",
                    help="the original executable (only read for --init)")
    args = ap.parse_args()

    warnings = []
    warn = warnings.append
    types = Types(warn)
    types.parse_file(BUILTIN_TYPES, "builtin")
    types.parse_file(args.types.read_text(encoding="utf-8", errors="replace"), args.types.name)
    syms = read_symbols(args.symbols.read_text(encoding="utf-8", errors="replace"), types, warn)
    kept, dropped = place(syms, warn)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(generate(types, kept, dropped), encoding="utf-8")
    args.fields.write_text(generate_fields(types, kept), encoding="utf-8")
    if args.exe.exists():
        args.init.write_text(InitWriter(types, kept, args.exe.read_bytes()).generate(), encoding="utf-8")
    else:
        warn(f"{args.exe} not found, {args.init} not written")
    for w in warnings:
        print("warning:", w, file=sys.stderr)
    print(f"{args.output}: {len(types.order)} records, {len(kept)} fields, "
          f"{len(dropped)} commented out, {len(warnings)} warnings")


if __name__ == "__main__":
    main()
