# `.SPD` music modules and `SETUP.SPD`

## `ANIM/*.SPD` (13 files, not 14)

Plain **ProTracker modules** (4-channel `M.K.`), despite the extension. They
are music, not GUS sample data and not tied to `ATV*.PIC`; the names are just
track names (ATV1-5, CHOISE, EARTH, FAILURE, MAIN1, MAIN2, NO, SPACE, TALK).
Copying one to `.mod` plays in any tracker. Tool: `tools/spd.py`
(`--batch` writes the `.mod` copies and a summary to `tools/out/spd/`).

Loader `43d5:000d(name: PString)` (only if the GUS/music driver is active,
`DAT_89f0`): opens `anim\<name>.spd`, seeks to `3B8h` (952, the order table),
reads 128 bytes and takes the highest pattern number P. Buffer size is
`(P+1)*400h + 30h` - this is the player's own memory sizing, then the player
(`43d5:2a26..2c5d`) loads the file itself. Nothing else is parsed by the game.

Layout is standard: 20-byte title, 31 x 30-byte sample headers (the first two
sample names are credit text, "music_composed_by:", "tamas_kreiner."), song
length @950, restart @951, orders[128] @952, `M.K.` @1080, patterns of 1024
bytes, then 8-bit signed sample data. In all 13 files size = 1084 +
patterns*1024 + sum(sample lengths) exactly.

Oddities: `NO.SPD` has the internal title `main1` (probably a copy); MAIN1/MAIN2
have song length 61/66 but only 54/60 distinct patterns.

## `SAVE/SETUP.SPD` (7 bytes)

Written by the external setup program, read by `Sound::Init` (`431a:09c1`)
and `SoundDrv::Init` (`43d5:04b7`, first byte only).

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | card type: 1 = GUS (`43d5` enables the module player on 1), 2, 3, 6 = Sound Blaster family (`431a` accepts 1, 2, 3, 6 as "sound present"); anything else = no sound |
| 1 | 2 | base port (shipped: `0220h`) |
| 3 | 2 | IRQ (shipped: 7) |
| 5 | 2 | DMA (shipped: 1) |

Shipped file: `02 20 02 07 00 01 00` = type 2, port 220h, IRQ 7, DMA 1. For
types 2, 3, 6 `431a` allocates 9C4h bytes, runs the SB detect in `474f` and
prints "Sound Blaster detected / not detected". The meaning of 3 and 6 vs 2 is
not determined.
