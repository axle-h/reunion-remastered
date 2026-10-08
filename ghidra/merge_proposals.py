#!/usr/bin/env python3
"""Merge a file of proposed symbols into ghidra/symbols.txt and apply it.

    ghidra/merge_proposals.py PROPOSALS [--section TITLE]

Lines starting with `seg:off` are appended under a section heading; lines
whose address is already in symbols.txt are skipped. Then rebuild.sh --apply
runs; any line ApplySymbols rejects (unknown type, prototype that doesn't
match the routine's stack cleanup, ...) is reduced to its name and comment
and the apply is repeated, so a bad guess never breaks the build.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SYMBOLS = HERE / "symbols.txt"
ENTRY = re.compile(r"^([0-9a-f]{4}:[0-9a-f]{4})\s")


def apply():
    out = subprocess.run([str(HERE / "rebuild.sh"), "--apply"], capture_output=True, text=True)
    return out.stdout + out.stderr


def strip_signature(line, reason):
    """'addr Name(params): T  // c' -> 'addr Name  // c (prototype rejected: reason)'."""
    code, _, comment = line.partition("//")
    addr, _, decl = code.strip().partition(" ")
    name = re.match(r"[A-Za-z_]\w*(::[A-Za-z_]\w*)*", decl.strip())
    name = name.group() if name else "Unnamed"
    note = f"prototype rejected: {reason}"
    comment = f"{comment.strip()} ({note})" if comment.strip() else note
    return f"{addr} {name}  // {comment}"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("proposals", type=Path)
    ap.add_argument("--section", help="heading for the new block (default: file name)")
    args = ap.parse_args()

    text = SYMBOLS.read_text()
    have = {m.group(1) for m in map(ENTRY.match, text.splitlines()) if m}
    new, skipped = [], 0
    for line in args.proposals.read_text().splitlines():
        m = ENTRY.match(line.strip())
        if not m:
            continue
        if m.group(1) in have:
            skipped += 1
            continue
        have.add(m.group(1))
        # "Name: // comment" (an empty type) means a plain name.
        new.append(re.sub(r"^(\S+ [A-Za-z_][\w:]*?):\s*(//|$)", r"\1  \2", line.strip()))

    title = args.section or args.proposals.stem
    bar = "# " + "-" * 75
    SYMBOLS.write_text(text.rstrip("\n") + f"\n\n{bar}\n# {title}\n{bar}\n" + "\n".join(new) + "\n")

    for attempt in range(5):
        log = apply()
        errors = re.findall(r"symbols\.txt:(\d+): (.*?) \(GhidraScript\)", log)
        if not errors:
            break
        lines = SYMBOLS.read_text().split("\n")
        for num, reason in errors:
            i = int(num) - 1
            lines[i] = strip_signature(lines[i], reason.strip())
        SYMBOLS.write_text("\n".join(lines))
    summary = re.findall(r"ApplySymbols: .*?(?= \(GhidraScript\))", log)
    print(f"{len(new)} added, {skipped} already present; "
          f"{sum(1 for _ in errors) if errors else 0} still failing")
    print(summary[-1] if summary else log[-2000:])
    return 0


if __name__ == "__main__":
    sys.exit(main())
