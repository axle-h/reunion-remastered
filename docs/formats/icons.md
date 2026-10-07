# `ICON/ICON.ALL`

68 icons of 40 x 24 pixels in one file. Decoder: `tools/icons.py` (PNG per
icon and `sheet.png` in `tools/out/icons/`).

The file is a sequence of chunks `u16 length` + `length` bytes, with no header
or index (68 chunks cover the file exactly; the game finds chunk n by reading
all of them at start-up, `3a1b:0810`, which records each chunk's offset
in a table at `DS:7D72`). Each chunk is PCX-style RLE as in `pic.md` (byte
`>= C0h`: run of `b & 3Fh` of next byte) and expands to 960 pixels (40 x 24; the
last run overshoots to 961 in every chunk, harmless).

The blitter `3a1b:0864(x, seg, n)` adds **2** to every pixel (mod 256), so
stored 00h is palette index 2 and stored FFh is index 1 (a border colour).
Icons are drawn with `47da:00de` as 40 x 24 blocks. The palette is the
shared game palette; the PNGs use `ICON/ICONMAIN.PIC`'s, which looks right
(grey background) apart from a few stray red pixels from the low indices 0/1,
which the game sets elsewhere. Loaders: size `405f:095a`, open `405f:09ce`,
read chunk `405f:09f3`, close `405f:0a57`; `GfxPath::Init` allocates
`size + 5500h + DAT_54cc` bytes for them.
