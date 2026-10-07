# `.MAP` planet-surface maps

47 files in `MAP/`, named `MAP<planet>_<n>.MAP` (planet 1-11; `n` = which surface
map of that planet, 0-9). Each is one planet-surface screen's terrain grid.
Decoder and PNG renderer: `tools/map.py`.

## Layout

| Offset | Size | Contents |
|---|---|---|
| 0 | 1 | Width W (cells) |
| 1 | 1 | Height H |
| 2 | W*H | Cell bytes, row-major (row 0 first, W cells per row) |

File size is always exactly `2 + W*H`. Sizes range from 20x20 to 86x60 (MAP8_0).
Header `28 1E` (40x30) is just the common size (MAP10_x); other files differ.

## Cell meaning

A cell is a tile index into a 16x16 tile sheet, 20 tiles per row
(`col = n % 20`, `row = n / 20`). The sheet is `PLANETS/FELSZ<t>.PIC` (320 wide), where
`t` is the *terrain set*, not the planet number:

| planet | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| terrain `t` | 1 | 8 | 2 | 7 | 10 | 3 | 9 | 5 | 4 | 6 | 11 |

(word table at `4a81:1a38`, index = planet number.) Tiles `n >= count[t]` (FELSZ tile
count: 80,120,240,200,240,240,120,40,120,160,240 for t=1..11; table `4a81:1810`) are
animated tiles taken from `PLANETS/FANIM<t>.PIC` at `n - count[t]`
(only t = 1, 5, 10 have one). Animation frame k starts `(k-1) * 256 * T` bytes into
FANIM, T from table `4a81:1826` (80, 20, 80 tiles): FANIM is a stack of frames.
All 47 maps stay within these ranges; max cell value is 255.

Cell 0 is ordinary ground. Water, rocks, trees etc. are other tiles.

## Runtime layers

The loader expands the map into three arrays indexed `[x*60 + y]` (x = column, 1-based;
max 90 wide by 60 high, 0x1518 bytes each; MAP8_0 is 86x60):

* `78ec` terrain: the cell byte.
* `78f0` overlay: 0, or `FF` when the *blocked* table says so. The table is
  `DS:0C92 + terrain*240 + n + 20` (nonzero = blocked/unbuildable). Later code stores
  building ids here; the drawer shows `overlay-1` from `PLANETS/EPUL[T]<t>.PIC`
  (the buildings sheet; `T` variant when `*DAT_7b08 > 1`) instead of the terrain tile.
* `78f4`: word array, zeroed (per-cell object index).

The loader stores W and H in `78f8`/`78fa`; maps under 46x31 set flag `7918`.

## Code

| Address | Role |
|---|---|
| `2ef2:03e7` | Loader: builds `MAP\MAP<planet>_<n>.MAP` (planet = `*(7b08+15h)`, n = `78e4`), reads W, H, the body, fills the layers |
| `2ef2:1c3c` | Draws a building footprint: terrain tile or overlay per cell |
| `2ef2:1da3`, `2ef2:1924`, `2ef2:198a` | Map view redraw (15x10 cells visible, 16x16 blits at screen (5Dh, 35h)); 1924 = tile blit, 198a = 14x14 inset blit |
| `2ef2` screen init (before call to `03e7`) | Loads FELSZ / FANIM / EPUL / RADAR sheets |

## Oddities

* The viewport is 10 columns by 15 rows of cells, scrolled by `78fe`/`78fc`.
* MAP1_x rivers and water use tiles >= 80, i.e. FANIM1 (animated), not FELSZ1.
* The blocked table (`DS:0C92`) is not decoded here.
