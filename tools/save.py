#!/usr/bin/env python3
"""Dump a Reunion saved game (SAVE/SPIDYSAV.<n>) or the new-game file SAVE/INIT to JSON.

    save.py FILE [-o out.json] [--all]
    save.py FILE --layout

The block list follows the game's save/load routine (1a3e:029c, SaveLoad.SaveLoadGame)
and the INIT loader (1a3e:0bb1); see docs/formats/save.md. Record layouts are read
from ghidra/types.pas, so field names there and in the JSON stay the same.

By default Message::Log and Game::Buildings are cut to Message::Count and
Game::BuildingCount entries (the game only uses those); --all keeps every slot.
"""

import argparse
import json
import re
import struct
import sys
from pathlib import Path

TYPES_PAS = Path(__file__).resolve().parent.parent / "ghidra" / "types.pas"

# Records that are not (yet) in ghidra/types.pas.
EXTRA_TYPES = """
type
  TMessage = record          { 53 bytes; message log line, 34b0 }
    Kind: integer;           { +00  1 or 99 drawn highlighted (34b0:0c14); 99 = new }
    Text: string[50];        { +02 }
  end;
"""

# (name, data address in DS or "[ptr]" for a far pointer's target, Pascal type, size
# pushed by the game). Order and sizes are the BlockRead/BlockWrite calls in 1a3e:029c.
SAVE_BLOCKS = [
    ("SlotName", None, "string[19]", 0x14),   # header: slot caption (slot table DS:7734^ + 1)
    ("Staff::Level", 0x57CC, "array[1..4,1..3] of integer", 0x18),
    ("Staff::DevSkills[0]", 0x57F0, "array[1..3,1..4] of byte", 0x0C),
    ("Message::Log", "[7ADE]", "array[1..15] of TMessage", 0x31B),
    ("Message::Count", 0x7AE2, "integer", 2),
    ("Pub::People", 0x013A, "array[1..10] of TPubPerson", 0x10E),
    ("Pub::Deals", 0x0424, "array[1..10] of TPubDeal", 0xF0),
    ("Races::Hostile", 0x6518, "array[1..12] of boolean", 0x0C),
] + [
    (f"DS:{a:04X}", a, "byte", 1) for a in range(0x773E, 0x774A)
] + [
    ("Galaxy::AmnestyNames", 0x1A9C, "array[1..32] of TBodyName", 0x1A0),
    ("Galaxy::AmnestyBodies", 0x1C3C, "array[1..32] of TBody", 0x820),
    ("Galaxy::PhoenixNames", 0x248A, "array[1..12] of TBodyName", 0x9C),
    ("Galaxy::PhoenixBodies", 0x2526, "array[1..12] of TBody", 0x30C),
    ("Galaxy::MirachNames", 0x2860, "array[1..18] of TBodyName", 0xEA),
    ("Galaxy::MirachBodies", 0x294A, "array[1..18] of TBody", 0x492),
    ("Galaxy::AntaresNames", 0x2DF8, "array[1..7] of TBodyName", 0x5B),
    ("Galaxy::AntaresBodies", 0x2E54, "array[1..7] of TBody", 0x1C7),
    ("Galaxy::OrionisNames", 0x3038, "array[1..13] of TBodyName", 0xA9),
    ("Galaxy::OrionisBodies", 0x30E2, "array[1..13] of TBody", 0x34D),
    ("Galaxy::LyraeNames", 0x3470, "array[1..23] of TBodyName", 0x12B),
    ("Galaxy::LyraeBodies", 0x359C, "array[1..23] of TBody", 0x5D7),
    ("Galaxy::RigelNames", 0x3B86, "array[1..5] of TBodyName", 0x41),
    ("Galaxy::RigelBodies", 0x3BC8, "array[1..5] of TBody", 0x145),
    ("Galaxy::SunNames", 0x3D56, "array[1..32] of TBodyName", 0x1A0),
    ("Galaxy::SunBodies", 0x3EF6, "array[1..32] of TBody", 0x820),
    ("Galaxy::PlanetStatus", 0x47D6, "array[1..8,1..8] of shortint", 0x40),
    ("Galaxy::SystemStatus", 0x4816, "array[1..8] of shortint", 8),
    ("DS:481E", 0x481E, "array[1..8] of integer", 0x10),   # per system: kind-6 buildings (357b)
    ("DS:482E", 0x482E, "integer", 2),                     # kind-13 buildings (357b)
    ("Game::CurSystem", 0x7AE4, "integer", 2),
    ("Game::CurPlanet", 0x7AE6, "array[1..8] of integer", 0x10),
    ("Game::CurMoon", 0x7AF6, "array[1..8] of integer", 0x10),
    ("Game::Hero", 0x9276, "integer", 2),
    ("Game::HeroSpriteX", 0x9278, "integer", 2),
    ("Research::Products", 0x5DAC, "array[1..35] of TProduct", 0x73F),
    ("Game::SelInvention", 0x95A0, "integer", 2),
    ("Game::CurResearch", 0x95A2, "integer", 2),
    ("Game::StaffExp", 0x95A4, "array[1..4] of word", 8),
    ("Game::DevSkill", 0x95AC, "array[1..4] of word", 8),
    ("Game::ProductionRate", 0x95B4, "word", 2),
    ("Game::StaffHired", 0x95B6, "array[1..4] of word", 8),
    ("Game::Credits", 0x95BE, "longint", 4),
    ("Game::HomeStock", 0x95C6, "array[1..6] of longint", 0x18),
    ("Game::Year", 0x95DE, "word", 2),
    ("Game::Month", 0x95E0, "word", 2),
    ("Game::Day", 0x95E2, "word", 2),
    ("Game::Hour", 0x95E4, "word", 2),
    ("Game::ColonySetupCount", 0xA21A, "integer", 2),
    ("Game::ColonySetups", 0xA21C, "array[1..10] of TColonySetup", 0xA0),
    ("Game::BuildingCount", 0xA2C0, "integer", 2),
    ("Game::Buildings", "[A2BC]", "array[1..1000] of TBuilding", 14000),
    ("Races::Races", 0x6BCE, "array[2..12] of TRace", 0x9CC),
    ("Game::ShownGroupCount", 0xA2C2, "integer", 2),
    ("Game::GroupCount", 0xA2C4, "integer", 2),
    ("Game::PlanetForceCount", 0xA2C6, "integer", 2),
    ("Game::SelGroup", 0xA2C8, "integer", 2),
    ("Game::SelGroup0", 0xA2CA, "integer", 2),
    ("Game::SelGroup1", 0xA2CC, "integer", 2),
    ("Game::GroupList", 0xA2CE, "integer", 2),
    ("Game::Groups", "[A2D0]", "array[0..1,1..32] of TGroup", 0x2840),
    ("Game::GroupsEnabled", 0xA2D4, "boolean", 1),
    ("Game::GroupKindAllowed", 0xA2D6, "array[1..4] of boolean", 4),
    ("Game::IdeaTimer", 0x91A0, "array[1..35] of integer", 0x46),
    ("Game::StoryStage", 0x91EA, "integer", 2),
] + [
    # 50 unnamed globals at DS:5D3C..5DA5, just below Research::Products (story/event
    # state used by Sim 1b8a and Intel 29b9). Type picked by the size the save uses.
    (f"DS:{a:04X}", a, {1: "byte", 2: "integer", 4: "longint"}[n], n)
    for a, n in [
        (0x5D3C, 1), (0x5D3D, 1), (0x5D3E, 1), (0x5D3F, 1), (0x5D40, 2), (0x5D42, 2),
        (0x5D44, 2), (0x5D46, 2), (0x5D48, 1), (0x5D4A, 2), (0x5D4C, 1), (0x5D4E, 2),
        (0x5D50, 1), (0x5D52, 2), (0x5D54, 2), (0x5D56, 1), (0x5D58, 2), (0x5D5A, 1),
        (0x5D5C, 2), (0x5D5E, 1), (0x5D60, 2), (0x5D62, 1), (0x5D64, 2), (0x5D66, 1),
        (0x5D67, 1), (0x5D68, 1), (0x5D6A, 2), (0x5D6C, 1), (0x5D6D, 1), (0x5D6E, 2),
        (0x5D70, 1), (0x5D71, 1), (0x5D72, 2), (0x5D74, 1), (0x5D75, 1), (0x5D76, 2),
        (0x5D78, 1), (0x5D7A, 2), (0x5D7C, 1), (0x5D7E, 2), (0x5D80, 1), (0x5D82, 2),
        (0x5D84, 1), (0x5D86, 2), (0x5D88, 1), (0x5D8A, 2), (0x5D8C, 1), (0x5D8E, 2),
        (0x5D90, 1), (0x5D91, 1), (0x5D92, 2), (0x5D94, 1), (0x5D95, 1), (0x5D96, 2),
        (0x5D98, 1), (0x5D99, 1), (0x5D9A, 1), (0x5D9C, 2), (0x5D9E, 2), (0x5DA0, 2),
        (0x5DA2, 4),
    ]
]

# SAVE/INIT (1a3e:0bb1, called by Main for a new game): only the buildings.
INIT_BLOCKS = [
    ("Game::BuildingCount", 0xA2C0, "integer", 2),
    ("Game::Buildings", "[A2BC]", "array[1..1000] of TBuilding", 14000),
]

# Arrays the game only uses up to a count stored in another block.
TRIM = {"Message::Log": "Message::Count", "Game::Buildings": "Game::BuildingCount"}


class SaveError(Exception):
    pass


# --- Pascal type subset (see the header of ghidra/types.pas) -----------------

SCALARS = {
    "byte": ("<B", 1), "shortint": ("<b", 1), "char": ("<c", 1), "boolean": ("<B", 1),
    "word": ("<H", 2), "integer": ("<h", 2), "longint": ("<i", 4), "pointer": ("<I", 4),
}


class Types:
    def __init__(self, *sources):
        self.records = {}
        for src in sources:
            self._parse(src)

    def _parse(self, text):
        text = re.sub(r"\{.*?\}", "", text, flags=re.S)
        for m in re.finditer(r"(\w+)\s*=\s*record\b(.*?)\bend\s*;", text, flags=re.S):
            fields = []
            for decl in m.group(2).split(";"):
                if not decl.strip():
                    continue
                names, typ = decl.split(":", 1)
                for name in names.split(","):
                    fields.append((name.strip(), typ.strip()))
            self.records[m.group(1)] = fields

    def size(self, typ):
        typ = typ.strip()
        low = typ.lower()
        if low in SCALARS:
            return SCALARS[low][1]
        if low.startswith("^"):
            return 4
        m = re.fullmatch(r"string\s*\[\s*(\d+)\s*\]", low)
        if m:
            return int(m.group(1)) + 1
        if low == "string":
            return 256
        arr = self._array(typ)
        if arr:
            (lo, hi), rest = arr
            return (hi - lo + 1) * self.size(rest)
        if typ in self.records:
            return sum(self.size(t) for _, t in self.records[typ])
        raise SaveError(f"unknown type {typ!r}")

    @staticmethod
    def _array(typ):
        """array[a..b, c..d] of T -> ((a, b), 'array[c..d] of T')."""
        m = re.fullmatch(r"array\s*\[(.*?)\]\s*of\s+(.+)", typ.strip(), flags=re.I | re.S)
        if not m:
            return None
        ranges = [r.strip() for r in m.group(1).split(",")]
        lo, hi = (int(x) for x in ranges[0].split(".."))
        rest = m.group(2) if len(ranges) == 1 else f"array[{','.join(ranges[1:])}] of {m.group(2)}"
        return (lo, hi), rest

    def decode(self, typ, data, off):
        typ = typ.strip()
        low = typ.lower()
        if low in SCALARS:
            fmt, _ = SCALARS[low]
            v = struct.unpack_from(fmt, data, off)[0]
            if low == "char":
                return v.decode("cp437")
            if low == "boolean":
                return bool(v) if v in (0, 1) else v   # keep odd values visible
            if low == "pointer":
                return f"{v >> 16:04X}:{v & 0xFFFF:04X}"
            return v
        if low.startswith("^"):
            v = struct.unpack_from("<I", data, off)[0]
            return f"{v >> 16:04X}:{v & 0xFFFF:04X}"
        m = re.fullmatch(r"string\s*\[\s*(\d+)\s*\]", low)
        if m or low == "string":
            cap = int(m.group(1)) if m else 255
            n = min(data[off], cap)
            return data[off + 1:off + 1 + n].decode("cp437")
        arr = self._array(typ)
        if arr:
            (lo, hi), rest = arr
            step = self.size(rest)
            return [self.decode(rest, data, off + i * step) for i in range(hi - lo + 1)]
        if typ in self.records:
            out = {}
            for name, t in self.records[typ]:
                out[name] = self.decode(t, data, off)
                off += self.size(t)
            return out
        raise SaveError(f"unknown type {typ!r}")


def load_types():
    return Types(TYPES_PAS.read_text(encoding="utf-8"), EXTRA_TYPES)


def layout_for(data, types):
    """Picks the block list from the file size; checks the sizes against the types."""
    for kind, blocks in (("save", SAVE_BLOCKS), ("init", INIT_BLOCKS)):
        for name, _, typ, n in blocks:
            if types.size(typ) != n:
                raise SaveError(f"{name}: {typ} is {types.size(typ)} bytes, game uses {n}")
        if len(data) == sum(b[3] for b in blocks):
            return kind, blocks
    raise SaveError(f"{len(data)} bytes: not a save "
                    f"({sum(b[3] for b in SAVE_BLOCKS)}) or INIT "
                    f"({sum(b[3] for b in INIT_BLOCKS)})")


def parse(data, types, keep_all=False):
    kind, blocks = layout_for(data, types)
    values, notes, off = {}, [], 0
    for name, _, typ, n in blocks:
        values[name] = types.decode(typ, data, off)
        values[name + "@"] = off     # temporary, for trimming below
        off += n
    assert off == len(data)
    for name, count_name in TRIM.items():
        if name not in values:
            continue
        count, lst = values[count_name], values[name]
        step = types.size(dict((b[0], b[2]) for b in blocks)[name]) // len(lst)
        if not 0 <= count <= len(lst):
            notes.append(f"{count_name} = {count} is out of range; {name} kept whole")
            continue
        start = values[name + "@"] + count * step
        slack = data[start:values[name + "@"] + len(lst) * step]
        if any(slack):
            notes.append(f"{name}: {sum(1 for b in slack if b)} non-zero bytes after "
                         f"entry {count} (stale data, unused by the game)")
        if not keep_all:
            values[name] = lst[:count]
    for name, *_ in blocks:
        del values[name + "@"]
    return {"kind": kind, "size": len(data), "notes": notes, "blocks": values}


def print_layout(blocks, out):
    off = 0
    out.write(f"{'offset':>6}  {'size':>5}  {'address':8}  name / type\n")
    for name, addr, typ, n in blocks:
        a = "-" if addr is None else (f"^{addr[1:-1]}" if isinstance(addr, str) else f"{addr:04X}")
        out.write(f"{off:6X}  {n:5d}  {a:8}  {name}: {typ}\n")
        off += n
    out.write(f"{off:6X}  total {off} bytes\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("file", type=Path)
    ap.add_argument("-o", "--output", type=Path, help="write JSON here (default stdout)")
    ap.add_argument("--all", action="store_true", help="keep unused log/building slots")
    ap.add_argument("--layout", action="store_true", help="print the block table instead")
    args = ap.parse_args()

    types = load_types()
    data = args.file.read_bytes()
    try:
        if args.layout:
            _, blocks = layout_for(data, types)
            print_layout(blocks, sys.stdout)
            return 0
        result = parse(data, types, keep_all=args.all)
    except SaveError as e:
        print(f"{args.file}: {e}", file=sys.stderr)
        return 1
    result = {"file": args.file.name, **result}
    text = json.dumps(result, indent=1, ensure_ascii=False)
    if args.output:
        args.output.write_text(text + "\n", encoding="utf-8")
    else:
        print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
