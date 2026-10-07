# Ghidra workflow

The Ghidra project (`ghidra/project/`, git-ignored) is disposable. Everything we
learn is recorded in **`symbols.txt`**, which a script applies to a freshly
imported executable, so the database can always be rebuilt and every change is
reviewable in git.

```sh
ghidra/rebuild.sh           # import vendor/.../REUNION.PRG, auto-analyse, apply symbols.txt (~20 s)
ghidra/rebuild.sh --apply   # re-apply symbols.txt to the existing project
```

Set `GHIDRA_HOME` if Ghidra isn't in `~/ghidra`. Open `ghidra/project/reunion.gpr`
in the Ghidra GUI to browse.

> Names entered in the GUI are lost on the next full rebuild unless they're also
> added to `symbols.txt`.

## Addresses

Ghidra loads the program at segment `1000h`, so a Ghidra address is the file's
segment + `1000h` (file `38c7:0000` = Ghidra `48c7:0000`). All docs and
`symbols.txt` use **Ghidra numbering**. In DOSBox-X the segments are offset again by
the PSP load segment.

## `symbols.txt` format

One entry per line; `#` starts a comment line, `//` a trailing comment that
becomes the plate comment in Ghidra.

```
@unit 48c7 System                           # names in segment 48c7 go in namespace System
48c7:0116 Halt                              # name only (creates the function if missing)
48c7:0c96 StrStore(const src: string; var dst: string; maxLen: word)
48c7:0b67 LongMul(a: longint @DX:AX; b: longint @BX:CX): longint @DX:AX
4a81:75d8 InOutRes: integer                 # data: label + type
```

Function declarations use Pascal syntax, so they can be copied as they'd appear
in the original source:

- Parameters are listed **in source order**; the script assigns the stack slots
  the way Borland Pascal does (pushed left to right, callee pops, first stack
  argument at `[SP+4]` for far procs or `[SP+2]` for near).
- `var`/`const` parameters, `string`s and values over 4 bytes (except `real`)
  are passed as far pointers.
- A function returning `string` gets a hidden first `result` pointer.
- `@REG` or `@HI:LO` puts a parameter or result in registers, for asm routines.
- Default results: 1 byte in `AL`, 2 in `AX`, 4 in `DX:AX`, `real` in `DX:BX:AX`.
- Near or far is detected from the routine's `RET`/`RETF`.

Types: `byte shortint char boolean word integer longint real string pointer`,
`^T` (far pointer), `array[lo..hi] of T`, and any type already in the program
(`TextRec`, `FileRec`, …).

## Language: `x86:LE:16:Borland Pascal Real Mode`

The program is imported with a custom language, built by `lang/install.sh`
from your Ghidra's own x86 module and installed as a user extension
(`~/.config/ghidra/ghidra_<version>_PUBLIC/Extensions/ReunionX86`).
`rebuild.sh` installs or refreshes it automatically. It differs from the stock
`x86:LE:16:Real Mode` in two ways:

1. **`CS` after far calls.** Stock SLEIGH sets `CS` to the target segment as
   part of a far `CALL`. Since `CS` survives the call, the decompiler then
   believes the caller's `CS` changed, so every later `PUSH CS` resolved to the
   wrong segment. That breaks string constants, procedure pointers and
   same-segment far calls (448 places in this program). The patch drops that
   assignment.
2. **Compiler spec `pascal`** (`lang/x86-16-pascal.cspec`) with Borland
   conventions:

   | Name | Use |
   |---|---|
   | `__pascal16far` | far procs (default) |
   | `__pascal16near` | near procs (nested and unit-private routines) |

   Both preserve only `BP SP SS DS`. Ghidra's built-in `__stdcall16far` also
   assumes `SI DI ES` survive a call, which hides real data flow. `ApplySymbols`
   switches near functions to `__pascal16near`.

Functions without a prototype in `symbols.txt` keep dynamic storage, so their
parameters appear in reverse order (`param_1` is the *last* Pascal argument).
They get the right order once they're prototyped.

## Scripts

| Script | Purpose |
|---|---|
| `ApplySymbols.java <symbols.txt>` | Apply names, prototypes, types, comments |
| `Decompile.java <out.c> <target>…` | Write decompiled C; target = address, segment, or `all` |
| `DumpSeg.java <out.txt> <seg>…` | Disassembly of every function in a segment, with caller counts |
| `SegmentReport.java <out.md>` | Per-segment table (size, functions, call counts, port I/O, INTs) |

Run any of them against the existing project with:

```sh
~/ghidra/support/analyzeHeadless ghidra/project reunion -process REUNION.PRG \
    -noanalysis -readOnly -scriptPath ghidra/scripts \
    -postScript Decompile.java /tmp/out.c 1b8a
```
