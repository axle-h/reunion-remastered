# Releases of the DOS game

Two MS-DOS releases are in circulation: the English floppy release and the
European CD with four languages (English, French, German, Italian). Both show
the Grandslam logo in the intro; some archives carry a readme naming Merit
Studios as the publisher. No Amiga release is known.

The port is the floppy's `REUNION.PRG` (`src/game/dsinit.c` holds its typed
constants), with the CD's font and its four languages, and
`vendor/Reunion_DOS_EN` is the floppy release. Everything below
was checked on 2026-10-12 against that folder, a DOSBox install of it
(`~/dos/c/REUNION`) and the CD image `Reunion (1995)(Grandslam)(M4).iso`
(CRC32 `81098dff`, 140,656,640 bytes; plain ISO 9660, 8.3 names, no Joliet or
Rock Ridge).

## The CD

The CD holds four complete games, `REUNION.ENG`, `.DEU`, `.FRA` and `.ITA`,
each runnable from the disc (`START.BAT` changes into one), with an
installer: `UNION_1.RNC` (an `RNCA` archive of RNC ProPack files, below),
`INSTALL.EXE` and `CDPATCH.EXE`.

`REUNION.ENG` and the floppy release have 892 files in common, byte for byte.
Only these differ:

| File | Floppy | CD |
|---|---|---|
| `GRWAR\REUNION.PRG` | 288,992 bytes, CRC32 `5edcb698` | 288,928 bytes, CRC32 `517f0c7b`, dated 1994-08-15 |
| `GRAFIKA\CHARSET1.PIC` | 3,173 bytes, CRC32 `2a6f4cab` | 3,985 bytes, CRC32 `ec1961ca` |
| `SAVE\SETUP.SPD` | byte 0 = 2 | byte 0 = 5 (no sound) |

The floppy rip also has `CRACK\` and a few loose DOS tools; the CD has the
installer files above.

**The CD's executable** is a rebuild of the same program. Its data segment is
laid out slightly differently (variables move by 0 to -51 bytes), so almost
every variable address in the code changes, and byte locals are packed
without padding. Compared function by function (2026-10-13; all 813 of the
floppy's are accounted for), the only changes are:

- Four of the port's fixes (`src/game/fixes.h`): mining reads each system's
  own bodies (`mine_all_systems`); a body without a colony stores 10,000 t,
  in mining and on the cargo screen (`station_storage`, which the port
  applies to bodies without a planet force); loading ore no longer caps the
  ship's load at the colony's storage (`cargo_planet_cap`); the bartender
  keeps offering the League tip after the League's first attack
  (`league_tip`, taken from the CD).
- No copy protection: "the date" is never armed (`2693:0000` and its call
  are gone), nor is "page N" (`357b:3d77` and its call are gone, as is a
  small routine after `264a:034a` that draws `Random(14)`). The floppy's
  `REUNION.PRG` is a cracked copy: `Protect::CheckToken` has its test
  (`cmp ax,0D80h / je`, `24eb:0100`) replaced by NOPs.
- The font (below), and `Gfx_DrawText6x8` turns character 1 into `ß` (E1h)
  before drawing.
- The credits name a second musician, Krisztian Kornyei.

Nothing else changed: the CD's claimed stability fixes are not there.

**The CD's font** has the floppy's 78 glyphs, pixel for pixel, but packs them
40 to a row, 8 pixels apart, where the floppy's sheet has 20 to a row, 16
pixels apart. Glyph 78 is a ß, 79 is blank, and 80-99 are accented:
`ÄÖÜäöüß` then `êéèàÉÂÇçôûùòì`. The CD's executable finds glyph *g* at
`(g / 40) * 0A00h + (g % 40) * 8 + 80h` and has 87 characters in
`Gfx_CharsetChars`: the floppy's 78, two more spaces, and `ÄÖÜäöüß` (code
page 437), glyphs 80-86. `Gfx_DrawText6x8` scans 89 bytes of it, so a
character it lacks gets glyph 88 (é) where the floppy's gets the blank 78,
and it first turns character 1 into ß (E1h): the text files have 1 for ß,
the `.RAW` tables E1h. `Gfx_TypeText` (the hire screen) has neither: a
character the table lacks reads glyph 255, past the sheet. So German draws
right, but every French and Italian accent draws as é, or as garbage on the
hire screen. The port takes the CD's table, layout and text routines, and
draws each French and Italian accent with its own glyph (`â`, `ï` and `î`,
which have none, as `à`, `i` and `ì`; `gfx_set_cd_charset`). With the
floppy's layout the CD's sheet draws wrong or blank characters, and the
money and the date stay empty.

**The languages.** All four language folders share the same `REUNION.PRG`,
`INTRO.PRG`, `CREDITS.PRG` and `CHARSET1.PIC`, so the texts compiled into
the executable (menus, captions, names, the advisor's price) are English in
every language run from the disc (the port translates them: below). Compared with `REUNION.ENG`, each other language changes
161 of the files the port loads (`src/platform/gamefiles.c`): `TEXT\` (49),
`SOUND\` (63-64: the speech), the intro's modules (`INTRO\INTRO1-7.MOD`:
their samples speak) and stills with text in `INTRO\`, `INTROP\`,
`GRAFIKA\`, `WAR\` and `VICTORY\`. The text files are the same lines in
the same order (the game reads them by line number); German, French and
Italian text uses code page 437. `VICTORY.PRG` has no text of its own: the
German one leaves out `TX5` (its `TX5.PIC` is the English one), and the
French and Italian one (the same file, 16 bytes longer) plays the English
script; both only lengthen an out-of-memory message. The port plays each
language's script (`src/cutscene/victory.c`). Only English has
`CREDITS\STEAL3.MOD` and `INTRO\PAL13.PIC`, `PAL14.PIC` and `PAL21.PIC`; only
French has `INTROP\_BACKUP_.PIC` (`19X4.PIC` with a 640x1100 white rectangle
below it). `REUNION.ITA\SAVE` ships a `SPIDYSAV.1` (2927-08-14).
`GRAFIKA\DEATH.PIC` is 320x256 in English (a black field and a pink
separator below the picture) and 320x200 in the other three; the port reads
only its first 200 rows (`command.c`).

**The install archives.** Each language folder's `UNION_1.RNC` holds that
language's game for the hard disk, with an older program: its
`GRWAR\REUNION.PRG` is the floppy release's program (the English one is
the floppy's, uncracked) with the texts compiled into it translated in
German, French and Italian: button labels, hotspot captions, panels and
lists, names, and the messages the program puts together. Its character
table names the French and Italian accents (`ê é è à É Ç ç ô û ù ò ì`,
glyphs 87-99, `Â` by 8Fh). The port takes those texts, compiled in so
that building needs no copy of the game: `tools/cd_texts.py` pairs each
text of the English program with the other builds' by lining up their
code, and writes `src/game/lang_texts.c` (`src/game/lang.h`). The builds
mostly differ only in their texts. Where the code differs as well,
`src/game/lang.c` does what each build does: most leave out a piece of a
message that the translation words differently, the German trader quotes
a price one way only, and the German race buttons have no captions. The
Italian planet name line is put together in another order ("NOME DEL TUO
PIANETA : New Earth", `planetinfo.c`). Some translations are longer than
the program's room for them (the German "Planetenkräfte", the French
"Jaanosiens"); the data segment holds what fits, and the screens that
show them whole take them from the table. The French build captions the
command centre's PLANET MAIN hotspot "INFO VAISSEAU", as it does SHIP
INFO; the port keeps it.

## Saves

Save files are the same 41,670-byte format in both releases and every
language. The port loads both DOSBox saves (`~/dos/c/REUNION/SAVE`, written by
the floppy executable) and the CD's Italian save, plays on from them, and
writes them back byte for byte. The DOSBox saves are golden test cases
(`tests/golden/saves/DOS_1.sav`, `DOS_2.sav`).

## Files the game never loads

None of these needs to be in the player's copy; the import asks for every
other file of the floppy release but the executables, `CRACK\`, the
archives and the readme (`src/platform/gamefiles.c`):

- `CREDITS\STEAL3.MOD`, a slightly different mix of the credits theme: the
  credits play `STEAL2`.
- `CREDITS\ANIM28.ANI`, `CHARMED.PIC` and `GRANDSLA.PIC`, and
  `PLANETS\MASZK1.PIC` and `MASZK2.PIC`: never named.
- `WAR\PIRLOST.PIC` and `WAR\PIRWIN.PIC`, debriefing screens in the layout of
  `GRLOST`/`GRVICT`/`SPLOST`/`SPVICT`, probably for a pirate battle that was
  never built; `GRAFIKA\SHIP3.PIC` has a matching fourth (brown) set of fleet
  icons.
- `EPULET\EPULET26.PIC`, a placeholder: building help pictures go up to 25
  ("épület" is Hungarian for building).
- Most stills next to the animations are there only as the animation's
  palette; some, like `INTRO\PAL13/14/21.PIC`, are never named.
- What only the DOS program used: `GRWAR\ALFA1.PIC` and `ALFA2.PIC` (sound
  drivers, which the port replaces), `TEXT\VALAMI.TXT` (the copy
  protection's date question) and `SAVE\SETUP.SPD` (read when present, but
  not imported: the CD's switches the sound off).

`GRAFIKA\PIRATES.PIC` has one pub character (second from the left) who never
appears in the game. Gaps in the dialogue files were probably his lines. The
beetle-like alien on the same sheet is said to be a shapeshifter; everywhere
else, including the pub moments later, he is a red humanoid covered in
blisters, so the two may have started as separate characters.
