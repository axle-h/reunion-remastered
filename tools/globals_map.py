#!/usr/bin/env python3
"""Group data-segment globals by the code segments that use them.

    globals_map.py globals.csv [--limit A544]

Reads the CSV written by ghidra/scripts/GlobalsReport.java and prints runs of
consecutive globals that share the same main user. Borland Pascal lays out
each unit's variables contiguously, so runs owned by one segment show where a
unit's variables live; runs used by many segments are shared game state.
"""

import argparse
import collections
import csv


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("csv")
    ap.add_argument("--limit", default="a544",
                    help="ignore offsets at or above this (stack/heap), hex")
    args = ap.parse_args()
    limit = int(args.limit, 16)

    users = collections.defaultdict(collections.Counter)
    labels = {}
    with open(args.csv) as f:
        for row in csv.DictReader(f):
            off = int(row["offset"], 16)
            if off < limit:
                users[off][row["from_seg"]] += 1
                labels[off] = row["label"]

    runs = []  # [first, last, owner, count, all users, named]
    for off in sorted(users):
        owner = users[off].most_common(1)[0][0]
        named = not labels[off].startswith("DAT_")
        if runs and runs[-1][2] == owner:
            run = runs[-1]
            run[1] = off
            run[3] += 1
            run[4] |= set(users[off])
            run[5] += named
        else:
            runs.append([off, off, owner, 1, set(users[off]), int(named)])

    for first, last, owner, n, segs, named in runs:
        shared = f"shared by {len(segs)}" if len(segs) > 1 else ""
        print(f"{first:04x}-{last:04x}  {owner}  {n:3d} vars  {named:3d} named  {shared}")


if __name__ == "__main__":
    main()
