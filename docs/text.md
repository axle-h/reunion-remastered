# The game's texts (`TEXT\`)

What the 50 files in `TEXT\` hold and how the game uses them. Decoding is in
`src/assets/text.c` and `src/game/textfile.c`.

| Kind | Files | Storage |
|---|---|---|
| Line files | 20 `.AT`, 18 `.LOC`, 4 `.SP`, 5 `.TXT` | CR LF text lines, each line obfuscated on its own |
| String tables | 3 `.RAW` | Fixed-size Pascal string slots, not obfuscated |

English text is 7-bit ASCII; the CD's German, French and Italian files
use code page 437, with 1 for ß in the line files (docs/versions.md, "The
CD's font"). A line is asked for by its 1-based number;
`|` is a line break, and leading spaces centre lines by hand.

To edit a line file, encode each character at position `i` (1-based) with
`(c + i * 4Fh) mod E0h + 20h`, the inverse of `TextFile_DecodeLine`. This
reproduces every shipped line byte for byte. Encoded bytes are 20h–FFh, so
they never hold CR, LF or ^Z. The CR LF line ends are not encoded.

## Dialogues

| Files | Who | Read by |
|---|---|---|
| `KERDESn.LOC`, `VALASZn.LOC` | the 10 pub characters | `src/game/pub.c` |
| `KERDESn.AT`, `VALASZn.AT` | the alien races | `src/game/alientalk.c` |
| `KERDES1.SP`, `RKERDES1.SP`, `VALASZ1.SP`, `AJANLAS.SP` | the advisor | `src/game/advisor.c` |

`KERDES` lines are the player's questions, `VALASZ` lines the answers. Both
start with a two-digit number and a space (`NN text`). In a question, `NN`
is the line of the answer. In an answer, it is the next menu state, and
`00` ends the conversation. Line 1 of every `VALASZ` file is a placeholder
for state 1, the opening menu. Which questions a state offers is not in the
files but in `Pub_MenuLists`, `Alien_MenuLists` and `Advisor_Menus`
(`dseg.h`), and the code overrides many answers (prices, what to build or
develop), so the `NN` of a question is only the default.

The advisor's files are split differently:

| File | Lines | Contents |
|---|---|---|
| `KERDES1.SP` | 11 | Full questions, no `NN`, drawn as the player's line |
| `RKERDES1.SP` | 11 | Short menu labels for the same questions |
| `VALASZ1.SP` | 15 | `NN text\|text`: next state, then up to 2 lines |
| `AJANLAS.SP` | 9 | Reasons to build a thing, appended to a suggestion |

## Messages and descriptions

| File | Lines | Contents |
|---|---|---|
| `MESSAGE.TXT` | 48 | Event message boxes |
| `KITALAL.TXT` | 35 | "X is invented" messages; lines 1–2 are placeholders |
| `PIRATE.TXT` | 20 | Lines 1–10: the intercepted transmission of pirate job k. Lines 11–20: the same job offered in the pub |
| `SZ_FELSZ.TXT` | 155 | Building descriptions, 6 lines each (title, 5 text lines). The 26th block has only 5 lines |
| `VALAMI.TXT` | 14 | Historic dates, the questions of the removed copy protection (`src/game/protect.h`) |

## String tables (`.RAW`)

Arrays of Pascal strings, loaded whole. The bytes after each string are junk
from the editor that wrote the file.

| File | Slot | Layout | Used by |
|---|---|---|---|
| `SZ_FACE.RAW` | 164-byte records | 4 professions × 3 candidates: `string[40]` name line, `string[81]` quote, `string[40]` salary | the hire screen |
| `SZ_TALAL.RAW` | `string[30]` | 35 products × 7 slots: centred title, 6 text lines | the shop |
| `SZ_FAJ.RAW` | `string[35]` | 12 races × 12 slots: title, 11 text lines | planet info |

## Oddities

- `VALASZ1.AT` is a copy of `VALASZ1.SP`, and `KERDES1.AT` is empty: race 1
  has no conversation. `KERDES8.AT`/`VALASZ8.AT` hold only `x`
  placeholders, and there is no `KERDES8.LOC`/`VALASZ8.LOC`, so pub
  character 8 must never open a conversation.
- Hungarian placeholders the code replaces: `-main` (opening state),
  `-mit gyartsanak` ("what should they produce"), `-mit fejlesszenek`
  ("what should they develop"), `-ok, ennyi lesz` ("ok, that will be").
- `MESSAGE.TXT` and `PIRATE.TXT` have `|` pieces of 51–52 characters. The
  message box draws them, so the 50-character limit of replies doesn't
  apply.
