# Amiga AGA disks: music

`tools/amiga_music.py` extracts the music modules from the zipped ADFs of the
Amiga AGA version (`vendor/Reunion_Amiga_EN`) into `tools/out/amiga/`.

## Disks

The install disk (disk 9) is an ordinary OFS disk. Its `Install` program
holds the table of the data disks' files:

| Offset in `Install` | Contents |
|---|---|
| `3F44h` | 510 × u16: disk << 12 \| 512-byte block |
| `4340h` | 510 × u32: unpacked size |
| `4B38h` | 510 × u32: packed size |
| `5330h` | 510 × 18-character names (`MUSIC/MAIN0001.MOD`, ...) |

The data disks (1–8) have no filesystem: each file is one chunk starting on
its block, packed with Imploder. The chunk is an `IMP!` file with the 12-byte
header cut to 4 bytes (`AM3` + the disk number; the two lengths come from
the table), so it is packed size + 38 bytes long:

| Offset | Contents |
|---|---|
| 0 | `AM3`, disk number |
| 4 | packed data from byte 12 to the packed size |
| then | 12 bytes: packed bytes 0–11, as three longs in reverse order |
| +12 | u32: the first literal run |
| +16 | u8: bit 7 clear = the stream ends one byte before the packed size |
| +17 | u8: the first bit buffer |
| +18 | 8 × u16 match offset bases, 12 × u8 extra-bit counts |

Decompression is Imploder's explode, reading backwards from the end.

The English "cr Loons" dump lacks disks 6–8, where most of the music is; the
German "cr TKK" set has them, and the files are the same. The ECS version
packs differently (`2AM`) and does not keep its music as ProTracker modules.

## The modules

All are 4-channel `M.K.` ProTracker modules, like the DOS `.SPD` files.

| Amiga | DOS |
|---|---|
| `MUSIC/MAIN0001`, `MAIN0002`, `TALK0000`, `W-SPACE0`, `W-EARTH0`, `rfailure` | byte-identical to `ANIM\MAIN1`, `MAIN2`, `TALK`, `SPACE`, `EARTH`, `FAILURE` `.SPD` |
| `MUSIC/CHOISE00` | `ANIM\CHOISE.SPD` without its sixth sample (played once) |
| `INTRO/STEALTHE` "Steal the sky" | `CREDITS\STEAL2.MOD` with 9 bytes changed (pattern 6, rows 33–36) |
| `MUSIC/reu-wine` "reu-win" | `VICTORY\ENDSEQ.MOD` "win" extended: the same samples, 15 patterns where DOS has 13 (four more in the middle, the last three changed) |

So the Amiga music is the DOS GUS music. Only the victory tune is a
different (longer) arrangement, and the DOS victory cut-scene times its
pictures to cues in `ENDSEQ`, so it could not simply be swapped in.
