# `.PIC` images ("SpidyGfx")

458 of the 474 `.PIC` files in the DOS release are 8-bit indexed images in
this format: a small header, PCX-style run-length data and a PCX-style
256-colour palette at the end. Decoder: `tools/pic.py`.

The other 16 `.PIC` files aren't images:

| Files | Contents |
|---|---|
| `GRWAR/*.PIC` (14) | Obfuscated AdLib (OPL2) music for the player in segment `428e`. Loaded by `4265:000c`: read whole file (max 31,700 bytes), reverse the byte order, add `2Fh` to every byte |
| `CREDITS/PAL3.PIC`, `PAL4.PIC` | Bare 768-byte 8-bit RGB palettes (used by `CREDITS.PRG`) |

## Layout

| Offset | Size | Contents |
|---|---|---|
| 0 | 8 | Magic `SpidyGfx` |
| 8 | 2 | Width (little-endian) |
| 10 | 2 | Height |
| 12 | … | RLE pixel data |
| size − 769 | 1 | `0Ch` palette marker (as in PCX) |
| size − 768 | 768 | Palette: 256 × R, G, B, 8 bits per component |

The palette is located from the **end of the file**, not from where the pixel
data stops.

## Pixel data

Same encoding as PCX:

- byte `< C0h`: a single pixel of that value
- byte `≥ C0h`: run of `byte & 3Fh` pixels (0–63) of the value in the next byte

So pixel values `C0h–FFh` can only be stored as runs (of length 1).

The game's decoder counts columns per row but writes its output as one linear
stream:

```
dst = dest + y * stride + x            # start position only
for row in 1 .. min(rows, height):
    col = 1
    while col <= width:
        b = next byte
        if b >= C0h:
            n = b & 3Fh;  v = next byte
            write n × (v + colourOffset) at dst;  dst += n;  col += n
        else:
            write (b + colourOffset) at dst;  dst += 1;  col += 1
```

`stride`, `x` and `y` only set the start address. The decoder never skips to the
next screen line, so it always produces `width × rows` contiguous bytes. Callers
decode into an off-screen buffer and blit from there (`405f:0f7a` decodes a
320×151 picture, then copies it to the screen with `47da:0025`). A run that
crosses the end of a row spills into the next one, and that row's column count
starts from 1 regardless.

All 458 images encode exactly `width` pixels per row, except
`INFO/INFO26.PIC`: its header says 191 but every row holds 192 pixels, like
the other `INFO*.PIC` (192×127). `tools/pic.py` detects this and decodes it at
192 with a warning. How the game displays it hasn't been checked.

## Palette

`loader[i] = file[i] >> 2`: the 8-bit values are cut to the VGA DAC's 6 bits.
Only entries `firstColour..lastColour` are taken, and they are stored at
`colourOffset + i` in the caller's 256-entry palette. Pixels get the same
`colourOffset` added. This lets a picture share the screen palette with the
UI: most game screens load colours `0..B0h` at offset `40h`. Many images only
use their first 32 colours.

The PNGs from `tools/pic.py` use offset 0 and the file's whole palette, which
shows each picture with its own colours. `--palette` substitutes another
file's palette.

## Fade while loading

When the `fade` flag is set, the decoder darkens the caller's current palette
entries `fadeFirst..fadeLast` (offset by `colourOffset`) to black in 40 steps
spread over the rows being decoded. It sets each step on the DAC with
`405f:0000` and `4049:0160`, so the old picture fades out while the new one is
drawn off-screen.

## Code

| Address | Function |
|---|---|
| `405f:0bef` | Decoder (`LoadPic`, see signature below) |
| `405f:0856` | Reads a whole file into a new heap block (`LoadFile`) |
| `405f:0f23` | Wrapper: full-screen picture at offset `40h`, colours `0..B0h`, fade |
| `405f:0f7a` | Loads `grafika\<name>` (320×151) and shows it |

Decoder parameters, in Pascal order (all words unless noted; `retf 22h`):

| Param | `[bp+]` | Meaning |
|---|---|---|
| `name: string` | 24h | File name; `.pic` is appended |
| `dest: pointer` | 20h | Output buffer |
| — | 1Eh | Unused (always 1) |
| `rows` | 1Ch | Rows to decode (clipped to the picture height) |
| `x` | 1Ah | Start column in `dest` |
| `y` | 18h | Start row in `dest` |
| `stride` | 16h | Bytes per `dest` row (used only for the start address) |
| `fade: boolean` | 14h | Fade the old palette out while decoding |
| `pal: pointer` | 10h | Caller's 256×3 palette (6-bit) |
| `firstColour`, `lastColour` | 0Eh, 0Ch | File palette range to copy |
| `colourOffset` | 0Ah | Added to pixel values and palette positions |
| `fadeFirst`, `fadeLast` | 08h, 06h | Palette range to fade |
