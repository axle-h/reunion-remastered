# `TEXT\` files

The 50 files in `TEXT\` come in two kinds. Decoder: `tools/text.py`.

| Kind | Files | Storage |
|---|---|---|
| Line files | 20 `.AT`, 18 `.LOC`, 4 `.SP`, 5 `.TXT` | CR LF text lines, each line obfuscated |
| String tables | 3 `.RAW` | Fixed-size Pascal string slots, not obfuscated |

All decoded text is 7-bit ASCII. Some lines are leftover Hungarian developer
notes (see [Oddities](#oddities)).

## Line files

Every line file is read through one routine, `398f:03ae`:

```
assign(t, 'text\' + name); reset(t)
for i := 1 to line do readln(t, s)      { s: string[255] }
Decode(s)                               { 398f:0864 }
close(t)
```

So callers ask for a line by its 1-based number, and the file is reopened and
read from the start on each call. A line past the end gives an empty string
(`readln` at end of file returns `''`). Line 0 reads nothing and decodes
whatever the caller's buffer already held. The files have no header and no
line count. Each file ends with CR LF. No line is longer than 247 bytes.

### Obfuscation

Each line is obfuscated on its own. The key depends only on the character's
position in the line, `i` (1-based):

```
decode:  s[i] := (s[i] + 5780h - i * 4Fh) mod E0h - 20h     { 398f:0864 }
encode:  s[i] := (s[i] + i * 4Fh) mod E0h + 20h
```

5780h = 100 × E0h, so the first term only keeps the value positive. The
arithmetic is 16-bit signed, but `i ≤ 255` keeps the sum positive anyway. The
encoder above reproduces every line of the shipped files byte for byte.
Encoded bytes are in 20h–FFh, so they never contain CR, LF or ^Z, and `readln`
splits the lines correctly. The CR LF line ends themselves aren't encoded.

### Formatting codes

- `|` is a line break. The renderers split on it. Leading spaces centre each
  line by hand.
- Dialogue lines (`KERDES*`/`VALASZ*` `.AT`/`.LOC`, `VALASZ1.SP`) start with a
  two-digit decimal number and a space: `NN text`. The game takes the first two
  characters with `Val`, then deletes three.

### Dialogue (`.LOC`: pub, `.AT`: aliens)

`.LOC` files are conversations with the 10 pub characters (`1fd2`, `Pub`).
`.AT` files are conversations with the 10 alien races (`2e4b`, `AlienTalk`).
`N` in the file name is the character or race number as a string (DS:7760 for
the pub, DS:78B8 for aliens). There is no `KERDES8.LOC`/`VALASZ8.LOC`, so pub
character 8 must never open a conversation (not checked).

The two kinds work the same way:

| File | Line | Number `NN` | Read by |
|---|---|---|---|
| `KERDESn` (questions: player's choices) | `NN text` | Line in `VALASZn` with the answer | `1fd2:0544` / `2e4b:0675` read the number; `1fd2:0788` / `2e4b:0780` load the menu |
| `VALASZn` (answers) | `NN text` | Next menu state; `00` ends the conversation | `1fd2:05fe` / `2e4b:0700` |

The menu of questions for a state is **not in the files**. It comes from tables
in the data segment, one table per character or race:

- Pub: far pointer at `DS:03D6 + 4 × char` → entries of 6 bytes,
  `[count, q1..q5]` (lines of `KERDESn.LOC`), indexed by state.
- Aliens: far pointer at `DS:0CDE + 4 × race` → entries of 4 bytes,
  `[count, q1..q3]`.

The pub adds questions at run time for character 2 in states 6 and 7
(question lines 10–14, 26 and 15–18, depending on flags DS:774B–7754).

Line 1 of every `VALASZ` file is a `01 -main` placeholder (`01 Main` in
`VALASZ4.LOC`, `01 x` in `VALASZ8.AT`): state 1 is the opening menu. The game overrides the answer line for many questions in code
(the switch after `1fd2:0544` in `Main`), so the `NN` in `KERDES` is only the
default.

Answers are drawn by `398f:0552`: it takes `NN` as the next state, then splits
the rest on `|` into at most 7 lines of at most 50 characters (longer pieces are
cut). `398f:07fe` counts the `|`-separated lines of a question so the menu
can map screen rows to questions.

For pub character 2, answer lines 10–13 of `VALASZ2.LOC` are replaced
in code with `09 It will be <n> credits` or `09 My work will cost <n> credits`.

### Advisor (`.SP`)

Read by segment `1991` (`Advisor`). The structure is like the dialogue files
but the files are split differently:

| File | Lines | Contents | Read by |
|---|---|---|---|
| `KERDES1.SP` | 11 | Full question (no `NN`), drawn as the player's line | `1991:0487`, line DS:771E |
| `RKERDES1.SP` | 11 | Short menu labels, same line numbers as `KERDES1.SP` | `1991:08de` into 61-byte (`string[60]`) slots |
| `VALASZ1.SP` | 15 | `NN text\|text`: next state, then up to 2 lines | `1991:0580`, line DS:7720 |
| `AJANLAS.SP` | 9 | "Offers": reasons to build a thing, no `NN` | `1991:0580` |

The menu for each state is the 6-byte table at `DS:00BC + 6 × state`
(`[count, q1..q5]`). The answer line is chosen in code (`1991:0000`), not from
the question. `1991:0580` (Ghidra fails to decompile it) overrides some answers:

- answer 7 (`-mit gyartsanak`): `"The most important thing would be a " + name`
  or `"We could develop a " + name`, plus an `AJANLAS.SP` line picked by
  `1991:02c9(0)`. With nothing to suggest: `I don't need anything right now`
- answer 8 (`-mit fejlesszenek`): the same with `"We really need more " + name + "s"` /
  `"We might need " + name + "."` and `1991:02c9(1)`
- answer 10 (`-ok, ennyi lesz ...`): `It will be <n> credits`, where n is a
  random cost × 1000

### Messages and other `.TXT`

| File | Lines | Contents | Read by |
|---|---|---|---|
| `MESSAGE.TXT` | 48 | Event message boxes, `\|` breaks | `34b0:011a(line)`, box via `34b0:0237` |
| `KITALAL.TXT` | 35 | "X is invented" messages; lines 1–2 are placeholders (empty, `hello`) | `34b0:01df(line)`, wraps the text in spaces |
| `PIRATE.TXT` | 20 | Lines 1–10: intercepted-transmission message for pirate job k. Lines 11–20: the same job offered in the pub | `1b8a:006e` (message, line k when the job starts); `1fd2:0788` (pub character 9, state 4: line 10 + active job, 0 if none) |
| `SZ_FELSZ.TXT` | 155 | Building descriptions, blocks of 6 lines: title, then 5 text lines | `398f:046f(skip)` from `2ef2` (`Surface`): skips `(building − 1) × 6` lines, draws 6 |
| `VALAMI.TXT` | 14 | History timeline events | `2693:0085` (`Protect2`), line = longint DS:0876, in a branch guarded by the copy-protection values (not analysed) |

`SZ_FELSZ.TXT` has 25 full blocks plus 5 lines. The 26th block's missing 6th
line reads as empty. `398f:046f` doesn't use `398f:03ae`; it opens the file
itself and decodes with `398f:0864`.

## String tables (`.RAW`)

Each file is an array of Pascal `string[n]` values, read with one `BlockRead`
of the exact file size into a heap block (segment `4049`, `RawText`). A slot is
a length byte plus `n` bytes. The bytes after the string are uninitialised junk
from the editor that wrote the file, not padding. Nothing is obfuscated.

| File | Bytes | Slot | Layout | Loader | Used by |
|---|---|---|---|---|---|
| `SZ_FACE.RAW` | 1968 | `string[40]` (41) | 4 professions × 3 people × 4 slots: `"<name> says:"`, quote, (always empty), `Salary: … Cr` | `4049:0011` | `3fab:048f` (`Hire`): slot at `(prof−1) × 492 + (person−1) × 164`; draws slots 1, 2, 4 |
| `SZ_TALAL.RAW` | 7595 | `string[30]` (31) | 35 items × 7 slots: centred title, 6 text lines | `4049:0083` | `32b4:0b57` (`Shop`): record `(item−1) × 217` |
| `SZ_FAJ.RAW` | 5184 | `string[35]` (36) | 12 races × 12 slots: title, 11 text lines | `4049:00f3` | `357b:1515` (`PlanetInfo`): record `(race−1) × 432` |

`Main` frees the three blocks.

## Decoder output

`tools/text.py` writes UTF-8 `.txt` files to `tools/out/text/<NAME>.<EXT>.txt`
(the extension stays because `KERDES1.AT`, `.LOC` and `.SP` share a stem):

- Line files: output line N is game line N, decoded. `|` and `NN ` are kept.
- `.RAW`: a `[n]` header per record, then one line per slot (empty slots
  included). The junk after each string is dropped.

All 50 files decode: 1,126 output lines.

## Oddities

- `VALASZ1.AT` is a copy of `VALASZ1.SP` (14 of its 15 lines, one with
  `|hello|hello` appended), but `KERDES1.AT` is empty. Race 1 seems to have no
  conversation.
- `KERDES8.AT`/`VALASZ8.AT` hold only `x` placeholders (`02 x`, `00 x`).
- Hungarian placeholders that the code replaces: `-main` (opening state),
  `-mit gyartsanak` ("what should they produce"), `-mit fejlesszenek` ("what
  should they develop"), `-ok, ennyi lesz` ("ok, that will be").
- `MESSAGE.TXT` and `PIRATE.TXT` contain `|` pieces of 51–52 characters. They
  are drawn by the message box (`34b0:0237`), not `398f:0552`, so the 50-character
  limit doesn't apply.
- The `SZ_FACE.RAW` third slot is empty in every record and isn't drawn.
