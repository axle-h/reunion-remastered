# `.SMP` sound effects

All 154 `.SMP` files (`SOUND/`, `SOUND2/WARSND*`, `SOUND3/GRSND*`; the
UI sounds are in `SOUND/`) are 8-bit unsigned mono PCM wrapped in the first two
blocks of a Creative Voice File (VOC) stream, without the usual
`Creative Voice File` signature. Converter: `tools/smp.py`.

## Layout

| Offset | Size | Contents |
|---|---|---|
| 0 | 1 | Block type `01h` (sound data) |
| 1 | 3 | Block length L (little-endian), counts everything after this field |
| 4 | 1 | Time constant TC |
| 5 | 1 | Codec `00h` (8-bit unsigned PCM) |
| 6 | L - 2 | Samples (`80h` = silence) |
| 4 + L | 1 | `00h`, terminator block |

File size is always L + 5 (verified for all 154 files).

## Sample rate

`rate = 1000000 / (256 - TC)`, the Sound Blaster time constant formula. The
GUS path in `43d5:02ab` computes exactly this from byte 4
(`1000000 div (0100h - buf[4])`), reads the whole file, XORs every byte with
`80h` (unsigned to signed) and uploads it to GUS DRAM. The Sound Blaster
driver (`474f`, via an indirect call) receives the raw buffer, so the VOC
header is interpreted by the SB side itself.

TC varies per file (35 distinct values, e.g. 174 = 12195 Hz is the most common,
80 = 5681 Hz); converted rates range 5000 to 20000 Hz (integer division).

## Loaders (segment 431a)

| Address | Role |
|---|---|
| `431a:0353` | Load `SOUND\<name>.SMP` (Pascal string arg) via `431a:0209` |
| `431a:0468` | Load `SOUND2\WARSND<n>.SMP`, cache by number in `89dc`, play |
| `431a:05ca` | Load `SOUND3\GRSND<n>.SMP`, same scheme |
| `431a:0712` | Preload `SOUND\SELECT<1..3>.SMP` into buffers |
| `43d5:027a` / `43d5:02ab` | GUS: read file, compute rate, XOR 80h, upload to DRAM |
| `43d5:0420` / `43d5:0438` | GUS: start playback of the uploaded sample |

Non-GUS mode reads the file into a 7594h-byte buffer (`89d6`) and passes it to
the SB driver (`474f:003a`).
