# `.VEC` vector models

`VECTORS/V1.VEC` … `V35.VEC` are small flat-shaded 3D models. The number in
the name is the model number the game passes to the loader, and it also
selects per-model view constants (zoom, spin) from tables in the data segment.
Decoder: `tools/vec.py`. It writes JSON, Wavefront OBJ and two PNG previews
rendered the way the game renders them.

The models are shown in two places:

- **Solid view**: `VecLoad.LoadVec` (`3d38:084a`), called from the screen
  handler at `32b4:0057` with an entry of the list at `DS:7AB4`. That handler
  also indexes the 35h-byte records at `DS:5D94` with the same number, which
  look like the invention table. Faces are drawn in a blue shade ramp, the
  model spins, and the 115×114 viewport is copied to the screen each frame
  (`3d38:01aa`, called from `Main`).
- **Wireframe view**: `LoadWireVec` (`3d38:0cd6`), called from `3abd:04b4`.
  That routine is called from `1b8a:04d8` with what looks like the number of a
  finished invention, and with 9 from `3abd:005c`. Only the edges are drawn,
  in green (colour 41h). It loops until a key or mouse button is pressed.

All 35 files parse cleanly: 254 objects, 3,327 vertices, 5,409 edges and 2,593
faces.

## Layout

A file is a list of **objects** (parts), ended by a word `FFFFh`. All values
are little-endian.

| Size | Contents |
|---|---|
| 2 | `size`: byte length of the rest of this object. `-1` (`FFFFh`) ends the file |
| 2 | `nVerts` |
| 6 × nVerts | Vertices: `x, y, z` signed words |
| 2 | `nEdges` |
| 2 × nEdges | Edges: two vertex-index bytes each |
| 2 | `nFaces` |
| 20 × nFaces | Faces: vertex-index bytes, ended by `FFh` |

`size` always equals `6 + 6·nVerts + 2·nEdges + 20·nFaces`. The loader only
compares it with -1. Vertex indices in edges and faces count from 1.

A face record holds up to 19 indices before an `FFh`. The renderer reads at
most 19 bytes, so a 20th byte is never used: one face in `V27.VEC` has no
terminator and its 19 indices are all used. After the `FFh`, 682 records keep
leftover bytes from an earlier edit, such as old index lists and further `FFh`s.
They are never read. Face sizes in the data run from 3 to 19 vertices. 2,013
faces are quads and 360 are triangles.

Faces carry no colour or winding information. Every face gets its colour from
its normal at draw time, and the renderer works out which side is outside
itself (see below).

### Buffer limits

The game allocates fixed heap blocks for up to **17 objects** per model
(`3d38:051c`, `3d38:0a00`):

| Data | Block | Holds |
|---|---|---|
| Vertices | 4B0h bytes | 200 vertices |
| Normals (one per face) | 4B0h bytes | 200 faces |
| Edges | 600 bytes | 300 edges |
| Faces (solid view) | 960h bytes | 120 faces |

Only one object breaks these limits: the 183-vertex sphere that is object 5
of `V34.VEC` and the only object of `V35.VEC`. It has 380 edges and 200
faces, so its edge and face reads run 160 and 1,600 bytes past their blocks.
The blocks are allocated in order, so the overflow lands in the next object's
block. That block is unused in both files. The wireframe loader reads faces
into the shared 8980h-byte screen buffer instead of the face blocks.

## Coordinates

The loader (`3d38:0696`) divides every coordinate by 10 (Pascal `div`,
truncating toward zero). File values reach ±1024, so loaded values stay within
±102. The projection reads only the **low byte** of each loaded word, as a
signed byte, so models must stay within ±127 after the division.

The projection matrix has determinant −1. Its screen frame is x right, y down
and z into the screen (z is depth), so the file's axes are effectively
left-handed. `vec.py` writes OBJ vertices as `(x, y, −z)`, which shows the
model the right way round in y-up, right-handed viewers. The JSON keeps the
raw file values.

## Derived data (`3e17:04d2`)

After loading, `PrepareObject` runs once per object:

1. **Centroid**: the mean of the (÷10) vertices, computed with Reals. It is
   rounded and stored at `DS:8582 + 6·slot`, for depth sorting only. A long
   hard-coded list then overrides centroid components of particular objects
   in models 6, 9, 11, 12, 14, 15, 16, 19, 21, 23, 24, 25, 26 and 32. These
   are hand fixes to the drawing order.
2. **Face normals**: `n = (P2 − P1) × (P3 − P1)` from the face's first three
   vertices, using 32-bit integers. If `n · (C − P1) < 0` (C is the Real
   centroid), n is negated. Every normal therefore points **toward the
   object's centroid**. n is then scaled to length 50 and rounded, and stored
   as three words (`DS:7E5A[slot]`).

Because the orientation comes from the centroid, faces may be wound either
way in the file. The rule only works for convex objects, which is why models
are split into many convex parts.

## Projection (`3e17:1500`)

`Project(a, b, c, src, dst, scale, count, mode)` rotates and scales `count`
points (6 bytes each) from `src` to `dst`. There is **no perspective**: it is
an orthographic projection.

Angles are whole degrees. The view keeps them as 0…1079 in 1/3-degree units
(`DS:7E48`, `7E4A`, `7E4C`) and passes `units div 3`. A cosine table
`cos500[0..360] = Round(cos(i°) · 500)` is built by `3e17:18db`. Sines are
read from it as `cos500[i−90]` (or `cos500[i+270]` when i < 90). The matrix
entries are scaled by 500, and every product is divided by 500 (`div`) in the
order shown:

```
r1 = [cb·ca, cb·sa, sb]
r2 = [cc·sa + (sc·sb)·ca,  (sc·sb)·sa − cc·ca,  −sc·cb]
r3 = [sc·sa − (cc·sb)·ca, −sc·ca − (cc·sb)·sa,   cc·cb]
if mode = 2: swap r1 and r2
for each point p:
    X, Y, Z = int8(p.x)·scale, int8(p.y)·scale, int8(p.z)·scale   (8×8-bit imul)
    out = hiword(X·r + Y·r + Z·r) for r in r1, r2, r3              (>> 16)
```

`out.x` and `out.y` are screen offsets and `out.z` is depth: larger values are
further away. The projected point is `(out.x + 86h, out.y + 43h)` in the
solid view. The view's 320-wide off-screen buffer (`DS:8584`) has its
115×114 window at offset 0CCCh (x 76, y 10). In the wireframe view the point
is `(out.x + 46h, out.y + 32h)`.

| Setting | Solid view | Wireframe |
|---|---|---|
| Vertex `scale` | `zoom[n]` (`DS:56CD + n`, 50…78) | 30 (`DS:7E54`) |
| Normal `scale` | 40h | — |
| `mode` | `DS:56F1 + n` (2 for models 4 and 27, else 1) | 1 |
| Start angles (units) | (0, 0, 300); model 14: (215, 45, 54); model 24: (0, 21, 32) | (0, 0, 300) |
| Spin per frame (units) | 3 bytes at `DS:5713 + 3n` | (5, 0, 0) |

`DS:899A` is the vertex scale for the solid view. It starts at 200 and is
clamped to `zoom[n]` on every frame, so it is effectively always `zoom[n]`.

## Drawing a frame (`3d38:01aa`)

1. Clear the window to colour 40h.
2. Project every object's centroid (scale 40h). A few models add fixed
   offsets to the resulting depths (models 1, 13, 17, 21, 27, 29 and 32,
   some of them +1000 or −1000) to force an object to the back or front.
3. Sort the objects by depth, furthest first (selection sort, `3d38:00e5`)
   and draw them in that order (painter's algorithm, per object only). Model
   35 draws only object 1.
4. For each object (`3d38:0000`): project the vertices with `zoom[n]` and the
   normals with 40h, then call `DrawFaces` (`3e17:00cc`). There is no sorting
   within an object.
   - A face is drawn when its projected normal has `z ≥ 0`. The normal points
     inward, so this is back-face culling.
   - Colour = `C0h + clamp((nz − nx + 24) · 4 div 5, 0, 2Eh)`, from the
     projected normal (length about 24). The light comes from −x/+z. Palette
     entries C0h…EFh are a blue ramp, `(0, 0, 10 + i·40 div 48)` (`3d38:03b3`).
   - Six objects are drawn two-sided: model 7 object 1, 12/7, 18/5, 26/7,
     26/9 and 27/4. Their back faces are drawn first with the shading
     reversed (`nx − nz`). A few faces of 7/1, 12/7 and 18/5 are never drawn.
5. Advance the angles by the spin and wrap at 1080.

Polygons are filled by `FillPoly` (`3e17:11f9`). It splits the outline at its
lowest and highest vertex into two x-per-scanline tables (`DS:8676` and
`DS:8806`, filled by `3e17:1438`). `FillSpans` (`3e17:13d6`) then fills
`|xb − xa|` pixels per row from the smaller x. It writes words of
`colour+1, colour`, so even columns get `colour+1` (a two-colour dither).
Nothing is clipped.

The wireframe view (`3d38:0490` → `0413`) projects each object with zoom 30
and draws every edge with the line routine `405f:1c82` in colour 41h, set to
`(0, 38h, 0)` by `3d38:04f9`.

## `tools/vec.py`

```
python3 -I tools/vec.py VECTORS/V3.VEC -o outdir [--angles A,B,C] [--scale N]
python3 -I tools/vec.py --batch vendor/Reunion_DOS_EN/VECTORS   # -> tools/out/vec/
```

- `Vn.json` holds the raw objects (vertices, 1-based edges and faces), the
  game's normals and centroids (after ÷10), and the view constants.
- `Vn.obj` has one `o` per object, with faces wound counter-clockwise from
  outside (using the game's centroid test) and edges as `l` lines.
- `Vn.png` is the solid view at the start angles, and `Vn_wire.png` is the
  wireframe view. Both are indexed PNGs using the game's colour numbers, and
  colour 40h is shown as black.

The previews follow the projection, sorting, culling and shading above. They
leave out the per-model centroid and depth overrides, and the polygon edge
walk is an approximation of `3e17:1438`.

## Code

| Address | Function | Return |
|---|---|---|
| `3d38:0000` | `DrawObject(slot)`: project vertices and normals, `DrawFaces` | `ret 2` (near) |
| `3d38:00e5` | Depth sort of the draw order (nested in `3d38:01aa`) | `ret 2` (near) |
| `3d38:01aa` | Draw one solid frame and advance the angles | `retf` |
| `3d38:03b3` | Set the C0h–EFh blue ramp and draw the first frame | `retf` |
| `3d38:0413` | `DrawWireObject(slot)` | `ret 2` (near) |
| `3d38:0490` | Draw one wireframe frame and advance the angles | `retf` |
| `3d38:04f9` | Set colour 41h to green | `retf` |
| `3d38:051c` / `05d8` | Allocate / free the solid-view buffers | `retf` |
| `3d38:0696` | `ReadObject(model, slot): boolean` (nested in `084a`) | `ret 6` (near) |
| `3d38:084a` | `LoadVec(n)`: reads `vectors\v<n>.vec` and sets up the solid view | `retf 2` |
| `3d38:0a00` / `0a96` | Allocate / free the wireframe buffers | `retf` |
| `3d38:0b2e` | Wireframe `ReadObject` (faces go to the `DS:8584` buffer) | `ret 6` (near) |
| `3d38:0cd6` | `LoadWireVec(n)` | `retf 2` |
| `3e17:0000` | `DrawEdges(bufSeg, verts, edges, count, colour)` | `retf 0Eh` |
| `3e17:00cc` | `DrawFaces(bufSeg, verts, faces, normals, count)` | `retf 10h` |
| `3e17:04d2` | `PrepareObject(model, slot, verts, faces, normals, nVerts, nFaces)`: centroid and normals | `retf 14h` |
| `3e17:11f9` | `FillPoly(bufSeg, n, pts, colour)` | `retf 0Ah` |
| `3e17:13d6` | `FillSpans(bufSeg, yMin, yMax, colour)` | `retf 8` |
| `3e17:1438` | `EdgeToTable(x1, y1, x2, y2, table)` | `retf 0Ch` |
| `3e17:1500` | `Project(a, b, c, src, dst, scale, count, mode)` | `retf 14h` |
| `3e17:18db` | Build `cos500[0..360]` at `DS:7FD4` (longints) | `retf` |

Data: `DS:7E9E[slot]` vertices, `7EE4[slot]` vertex counts, `7F04[slot]`
edges, `7F4A[slot]` edge counts, `7F6A[slot]` faces, `7FB0[slot]` face
counts, `7E5A[slot]` normals (`7E5A[0]` holds the projected normals),
`7E56` projected vertices, `7E50` object count, `8996` current model.
