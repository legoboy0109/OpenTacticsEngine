# Hexagonal grids

Contract for hexagon maps. Owner decisions (2026-11-14):

- The game's maker chooses which way the hexagons point: **pointy-top** or
  **flat-top**.
- **One grid per game**: a game is a square game or a hexagon game; its maps
  cannot mix.
- Hexagon games work in **all four kinds of view**: top-down, 2D isometric,
  3D isometric and full 3D.
- On hexagon maps a structure covers **one hexagon, or a hexagon and its
  ring of six** ("big structures" from the start). Roads, rivers and coasts
  join with a hexagon set of pieces.

Rules never look at coordinates: everything in `engine/core` goes through
`Grid::neighbors / distance / within` (engine/core/grid/grid.h). Hexagons are
a new case there plus drawing, input, art and the editor.

Status: built for all four kinds of view: rules core, player, starter art with two templates and
`games/demo-hex`, and the editor (the "Implementation notes" sections at the end). Nobody has tried it in a real window yet.

## Data

### game.json

```jsonc
"grid": { "type": "hex", "layout": "pointy" }   // or "layout": "flat"
```

Optional. Missing, or `{ "type": "square" }`, is a square game (every existing
game). `layout` is required with `hex` and refused with `square`; unknown keys
are refused. A hexagon game may have styles of every `view`:
every kind is allowed (`hexGameAllowsView` in core/data/database.h is the one list; a kind added later is refused with
`style "iso": the "<kind>" view is not available for hexagon games yet` until it is drawn).
An `isometric` block on a hexagon game may not have `tileHeight` (see "2D isometric").

### maps/*.json

`"grid": { "type": "hex", "width": 12, "height": 9 }`. The map's `type` must be
the game's: `maps/ford.json: grid: this is a hexagon game (game.json "grid"),
but the map's type is "square"` (and the other way round). Everything else in
a map file is unchanged: `tiles` is `height` rows of `width` characters,
`heights`, units and structures use the same `x`, `y` (column, row).

### Coordinates ("offset")

Maps stay rectangles of `width` x `height` cells, stored column and row.

- **pointy** (a corner points up; rows run straight across): odd rows (y = 1,
  3, ...) sit half a hexagon to the **right**.
  Neighbours of (x, y), in this fixed order: upper right, right, lower right,
  lower left, left, upper left.
  - even row: (x, y-1) (x+1, y) (x, y+1) (x-1, y+1) (x-1, y) (x-1, y-1)
  - odd row:  (x+1, y-1) (x+1, y) (x+1, y+1) (x, y+1) (x-1, y) (x, y-1)
- **flat** (a flat side on top; columns run straight down): odd columns sit
  half a hexagon **down**.
  Neighbours in this fixed order: up, upper right, lower right, down, lower
  left, upper left.
  - even column: (x, y-1) (x+1, y-1) (x+1, y) (x, y+1) (x-1, y) (x-1, y-1)
  - odd column:  (x, y-1) (x+1, y) (x+1, y+1) (x, y+1) (x-1, y+1) (x-1, y)

The order is part of the contract (path ties, the AI and the join pieces
depend on it). `distance` is the number of steps on an empty map (cube
distance); `within(c, r)` is every cell at distance <= r, ascending index.

Rectangles in story data (`reach`, area conditions, docs/EVENTS.md) stay
rectangles of columns and rows on hexagon maps.

### Structures (data/structures.json)

On a hexagon game `width` / `height` other than 1 are refused (`structure
"fort": "width" is for square games; a hexagon game uses "rings"`). New key
`"rings": 0 | 1` (default 0): 0 covers one hexagon, 1 covers it and its six
neighbours. `rings` is refused in a square game. In a map, `x`, `y` of a
structure with rings is its **middle** hexagon; all seven must be inside the
map and free of other structures. Upgrades keep the size (same `rings`).
Everything a structure does (costs, props, capture, income, production,
vision, hiding) applies to every hexagon it covers, as for squares.

## Top-down picture

`T` = `view.tileSize`. `L` = the multiple of 4 nearest to `T * 1.1547`
(24 -> 28, 16 -> 20, 32 -> 36).

- pointy: a hexagon is `T` wide and `L` tall; rows are `3L/4` apart. Cell
  (x, y) has its box at left `x*T + (y odd ? T/2 : 0)`, top `y * 3L/4`.
  Map size in pixels: `width*T + T/2` by `(height-1) * 3L/4 + L`.
- flat: a hexagon is `L` wide and `T` tall; columns are `3L/4` apart. Box at
  left `x * 3L/4`, top `y*T + (x odd ? T/2 : 0)`. Map size:
  `(width-1) * 3L/4 + L` by `height*T + T/2`.

The hexagon inside a box touches the box on all four sides (pointy: corners
at top and bottom middle, upright sides from 1/4 to 3/4 of the height; flat:
the same turned a quarter).

- **Terrain pictures** are sheet cells of the box size (`T x L` pointy,
  `L x T` flat) with the art's own transparent corners; the engine does not
  cut them. Terrain without art is a filled hexagon in its colour.
- **Units** are drawn as on squares (their sheet cell, centred on the
  hexagon's middle, feet at the same height as on a square of size `T`).
- **Highlights** (move and attack range, cursor, fog, owner frames, the
  editor's selection) are hexagon shaped.
- **Picking**: the hexagon whose middle is nearest to the pointer.
- **Direction buttons** move the cursor by column and row exactly as on
  squares (on pointy maps Up/Down zigzag half a hexagon, on flat maps
  Left/Right do); nothing else changes in the controls.

### Join pieces on hexagons

`"join": [col, row]` is the top-left cell of an **8 x 8 block of 64 pieces**.
The piece for a cell is the number made of six bits, bit `i` set when
neighbour `i` (order above) counts as the same; it sits at column `n % 8`,
row `n / 8` of the block. `joinWith`, `joinExcept`, `joinEdge` and animated
joins (several blocks as frames) work as on squares (docs/GRAPHICS.md).

### Structures

One hexagon: the picture is drawn in the hexagon's box (taller pictures rise
above it, as on squares). With rings: across the box around all seven
hexagons (pointy: `3T` wide, `3L/2 + L` tall; flat: turned), bottom edge on
the box's bottom. Placeholder (no art): each covered hexagon filled in the
structure's colour, the letter on the middle one. Owner frame: the outline
around the covered hexagons.

## The other three views (owner, 2026-11-15: go ahead)

Hexagon games may have styles of every kind of view. Built in two steps, both done: **2D isometric** ("Implementation
notes (2D isometric)" at the end), then the **two 3D kinds** together ("Implementation notes (3D)").

Common to all three: rules, picking by nearest hexagon, direction buttons by column and row, highlights shaped like
the hexagon as that view shows it, units drawn as that view draws them on squares, heights raise a hexagon as a
**six-sided block**, fog, structures of one hexagon and of seven, both layouts. The editor's map canvas stays top-down.

### 2D isometric

The top-down picture seen from the view's one fixed angle: every top-down position (x, y) of "Top-down picture"
is drawn at (x * sx, y * sy) with the vertical squash `sy = sx / 2`, and a height level lifts a hexagon by the
style's `heightStep`. `isometric.tileWidth` is the drawn width of one top-down `T` (default `2 * T`, so `sx = 2`);
`tileHeight` is not used on hexagon games (refused if present: the squash is fixed); `heightStep` defaults to
`tileWidth / 4`; `unitWidth` as on squares (default `tileWidth / 2`). All positions are whole pixels.

**Rounding rule** (built, tools/art/hex_iso_geom.py, engine/core/view/hex_iso.h). The squashed hexagons are a hexagon
lattice of their own with whole-pixel boxes, laid out exactly like the top-down one (rows or columns 3/4 of the long
side apart, odd ones half a box over). With `W` = `tileWidth`, `T` = `view.tileSize`, `L` as in "Top-down picture":

- pointy: the box is `W` wide and `Hs` tall, `Hs` = the multiple of 4 nearest to `L * W / (2T)` (halves go up);
- flat: the box is `Wf` wide, the multiple of 4 nearest to `L * W / T`, and `W / 2` tall (rounded up to an even number);
- if that long side would leave a one pixel seam between neighbours (the slanted edge's number of pixel rows odd and its
  rise odd as well; only unusual widths such as 36 over a tile of 24), the nearer multiple of 4 that does not is used;
- a hexagon is filled by the same row spans as in the top-down picture (`HexGeo::rowSpan`), so neighbouring hexagons
  share their edges exactly (no gap, no overlap, for every `tileWidth`: `tests/test_hex_iso.cpp`,
  `tests/hex_content_test.py`, `tests/hex_iso_player_test.py`);
- a cell's middle is exactly its top-down middle scaled by (box width / top-down box width, box height / top-down box
  height), so a unit standing on a hexagon is on the middle of its top face, and units walking between hexagons are placed
  by the same straight scaling (their height eases from one hexagon's level to the next's).

At the defaults (`T` = 24, `tileWidth` 48, `heightStep` 12): pointy boxes 48 x 28, rows 21 apart; flat boxes 56 x 24,
columns 42 apart. World pixels: x as in the lattice; y = lattice y - level * `heightStep` + (highest level) * `heightStep` +
headroom, headroom = the largest of the box height, `tileWidth` and 2 x `unitWidth`; the map is the box around the lattice plus that
room above and `heightStep` rows of side faces below the lowest hexagons.

- A hexagon's **top face** is the squashed hexagon; its **side faces** are the ones a viewer in front sees under the
  lower edges of the six-sided block: pointy the two slanted ones (lower left, lower right; the vertical edges are seen
  edge-on and show nothing), flat the three lower ones (lower left, bottom, lower right). A face is drawn only where the
  neighbour in that direction is lower (or off the map: the rim shows the side of the lowest level), from the neighbour's
  level up to the hexagon's; an equal or higher neighbour hides it.
- **Terrain art cell** (decided here): as wide as the top face's box (`W` pointy, `Wf` flat) and tall enough for, from the top,
  anything that sticks up (trees, peaks: any number of rows), the top face's box (`Hs` / `W/2` rows) and `heightStep` rows of
  front faces. The picture is drawn that wide, as tall as its proportions make it, its bottom edge `heightStep` rows under the top
  face's box (as on squares). So the smallest cell is `W x (Hs + heightStep)` = 48 x 40 pointy, 56 x 36 flat at the
  defaults; the starter art has 20 rows of room above: 48 x 60 and 56 x 56. The art's own transparent pixels cut it; the
  engine does not. Where a hexagon is higher than the neighbour in front, the bottom `box height / 2 + heightStep` rows of
  the same picture are repeated once per level under that face's columns (so a face's texture runs down to the neighbour).
  Without art: the top in the terrain's colour (its lowest pixel row a shade darker), the faces darker, lit on the left
  (0.70), shaded on the right (0.52), the flat bottom in between (0.61), as on squares. Join pieces: the same 8 x 8 block of
  64 cells of that size, animated joins as frames.
- **Structures**: a one-hexagon picture is drawn as wide as the hexagon's top-face box, as tall as its proportions make it, its
  bottom edge on the box's bottom (the front point of a pointy hexagon, the front edge of a flat one), standing at the height of
  its hexagon; with rings across the box of the seven (pointy `3W` wide, `5Hs/2` tall, flat `5Wf/2` wide, `3 * W/2` tall) at its
  own proportions, bottom edge on that box's bottom, at the height of the highest of the seven. The picture of the starter art
  stands on a stone plinth hexagon (3 rows deep) and is taller than the box (single: 15 rows, with rings: 26). Placeholder (no
  art): every covered hexagon a low block (3/4 of the box height) in the structure's colour, drawn back to front, the letter on the
  middle one. The owner frame is the outline of the covered hexagons on the ground (two pixels, outer edges only); the edges facing
  the viewer are drawn once more over the picture.
- **Draw order**: back to front by the pixel row of a hexagon's front point on the ground (the bottom of its top-face box before
  lifting; equal rows left to right, which cannot overlap). A structure is drawn after every hexagon up to its frontmost covered one,
  a unit after the hexagons up to the row of its feet's hexagon (a unit on a structure after that structure), so nearer
  hexagons, units and structures cover farther ones; units and the cursor behind higher ground are drawn again faintly on top.
- **Picking** (decided here): a pixel belongs to the hexagon nearest the viewer whose top face or side faces contain it, found as
  on squares: for every level the pixel is looked at as a spot of that level's plane (unsquashed back to the top-down picture) and
  the hexagon whose middle is nearest there counts if it is at least that high; the nearest to the viewer wins. On flat
  ground this is the nearest middle.
- **Units** are drawn `unitWidth` wide on the middle of the hexagon's top face, the feet a quarter of the box height in front of
  the middle; with `unitMarker` a small flat hexagon in the side's colour under the feet. **Highlights** (move and attack range)
  are the filled hexagon on the top face (on a placeholder structure: on top of its blocks), the cursor a two pixel outline; **fog**
  darkens the whole hexagon, sides included, as on squares.
- Starter art for both layouts from `tools/art/` (generated); `games/demo-hex` and the two hexagon templates gain an
  "Isometric" style (the demo's maps got heights: mountains 2, the hexagons around them 1).

### 3D isometric and full 3D

The ground is six-sided blocks in the 3D space of docs/RENDER3D.md (one top-down `T` = one unit, as for squares).

- The **top** of a block is textured with the terrain's **top-down hexagon picture** (the same sheet cell, mapped
  onto the hexagon), so a hexagon game's top-down art works in 3D unchanged; a 3D style may give its own pictures.
  Sides take the style's side material or the darkened terrain colour, as on squares.
- Join pieces, variants and animation as in the top-down view. Standing terrain (docs/VIEWS.md) works on hexagons.
- Units (pictures or models), structures (one hexagon, seven), highlights lying on the blocks' tops as hexagons.
- **3D isometric turns in sixths** (60 degrees) on hexagon maps, where square maps turn in quarters; the turn
  buttons and the camera rules are otherwise the same. Full 3D: the free camera, unchanged.
- Software and OpenGL / WebGL backends; `games/demo-hex` gains "3D Isometric" and "Full 3D" styles.

### Editor

The Kind of view choice on a hexagon game offers each kind as soon as the engine does (the editor asks the helper
which kinds a hexagon game may use, so it follows the build it runs with). The isometric settings hide
`tileHeight` on a hexagon game. Art screens preview pieces at the style's cell size.

## What does not change

Rules, formulas, AI, saves, fog, items, story, economy, sound. A save made on
a hexagon map is an ordinary save. The rules hash covers the grid, so a game
that changes its grid refuses its old saves like any other rules change.

## Editor

- A game's grid is chosen when it is made (the template decides) and shown on
  the Settings screen; it cannot be changed later.
- Map canvas, painting, filling, resizing, structure placing, unit placing,
  story area pickers and the sight aid work on hexagons.
- Structures screen: on a hexagon game the size control offers "1 hexagon" or
  "7 hexagons" instead of width and height.
- Graphics: join blocks are 8 x 8 on a hexagon game; previews are hexagons.
- Styles: on a hexagon game only the kinds of view the engine draws for hexagons can be chosen (top-down and 2D isometric
  today; the 3D kinds when their step lands).
- Play runs the real player as before.

## Starter content

- `tools/art/`: a generator for hexagon starter tiles, both layouts
  (terrain with variants, joined roads, animated joined water, a bridge,
  structures including one with rings).
- Two templates, one per layout, and one demo game (`games/demo-hex`, pointy)
  with an economy-style rule set, a structure with rings, fog on one map and
  the retro sound set.

## Build order

1. **Core**: `Grid` hexagons, loader (game.json `grid`, maps, structures
   `rings`, styles check), structure footprints through the grid, join mask
   for six neighbours, `ote-sim`, tests. A hand-made fixture game without art.
2. **Player** (top-down): hexagon projection, drawing (placeholder and art,
   join pieces, structures), highlights, picking, fog, the editor's embed.
3. **Starter content**: art generator, the two templates, `games/demo-hex`.
4. **Editor**: everything under "Editor" above.

Steps 2, 3 and 4 only depend on step 1.

## Implementation notes (core)

What step 1 built, for the player, art and editor steps.

- **Grid** (`engine/core/grid/grid.h`): `GridType::Hex`, `HexLayout::{Pointy, Flat}`; `Grid(type, w, h, layout)`,
  `grid.isHex()`, `grid.layout()`. Rules and the player use only these, never offset arithmetic:
  - `directionCount()` (4 or 6), `neighbor(cell, dir)` (kNoCell off the map), `directionTo(from, to)` (-1 if not
    adjacent). Direction `i` is the contract's neighbour order; squares are N, E, S, W. Opposite of `i` is `(i + 3) % 6`
    on hexagons. Use these (not pixels) for join bits, edge drawing and anything that needs "the neighbour on the right".
  - `neighbors()`, `distance()`, `within()` as before (`neighbors` skips off-map cells, keeping the order).
  - `ringCells(middle, rings, out)`: the middle hexagon, then its six neighbours in direction order; false when any is
    off the map.
- **Where the grid comes from**: `Database::gridType` / `hexLayout` (game.json `grid`, read by `readGameGrid` in
  `data/loader_grid.cpp`), copied into every `MapDef` (`gridType`, `hexLayout`) and into `Battle::grid`. The launcher
  gets `GameInfo::gridType` / `hexLayout`. Games without `grid` are square, exactly as before.
- **Structures**: `StructureDef::rings` (0 or 1; `width`/`height` stay 1 on hexagon games). The cells a structure covers
  come from ONE function, `structureFootprint(grid, def, x, y, out)` (`core/data/footprint.h`): used by the map loader,
  `Battle::init`, `Battle::structureCells` (fog, AI, events all go through that) and tests. For squares the order is
  row by row from the top-left corner (as before); for hexagons the middle hexagon comes first (`out[0]`, which is the
  cell of the structure's `x`, `y`), then its neighbours 0..5. Anything that draws a structure needs `x`, `y` (middle)
  and `rings`; the covered cells are `Battle::structureCells(i, out)` or `structureAt`.
- **Join pieces** (`core/data/graphics.h`): `joinMaskHex(look, grid, cell, terrainAt)` gives the 6-bit mask (bit `i` =
  neighbour `i`; `terrainAt` takes a `Cell`); `pickCell(look, x, y, mask, timeMs, blockSide)` takes `blockSide` 8 for
  hexagon games (piece `n` at column `n % 8`, row `n / 8`; default 4 keeps squares as they were);
  `joinBlockSide(grid)` returns 4 or 8. The loader checks join blocks against the sheet as 8 x 8 on hexagon games.
  (`sprites.cpp joinMaskOf` used the four-neighbour version until step 2; it now calls `joinMaskHex` on hexagon games.)
- **Rules hash**: `game.json` is hashed key by key (rules keys only); `grid` was added to those keys, so a hexagon game
  hashes its kind and layout and an old save of a game whose grid changed is refused. `grid` missing and
  `{ "type": "square" }` hash alike (skipped), so square games keep their hash. `structures.json` (`rings`) and map files
  (`grid.type`) are hashed whole already.
- **Loader messages**: `game.json: grid: ...` (unknown key, `layout` with square, missing `layout`, bad `type`/`layout`),
  `game.json: style "iso": the "<view>" view is not available for hexagon games yet` (only for a kind the build cannot draw; none today), `maps/x.json: grid: this is a hexagon game (game.json "grid"), but the map's type is "square"` (and
  the reverse), `structure "fort": "width" is for square games; a hexagon game uses "rings"` (same for `height`),
  `"rings" must be 0 or 1`, `"rings" is for hexagon games; a square game uses "width" and "height"`; an upgrade must keep
  the `rings`; map structures: `covers its hexagon and the six around it (rings 1) but they do not all fit inside the map`,
  `is outside the map`, `overlaps structures[n] (...) on hexagon x,y`. Tests: `tests/test_hex_*.cpp`.
- **Not touched on purpose**: story rectangles (`reach`, area conditions, `loader_story_refs.cpp`) stay column/row
  rectangles; `core/view/` (isometric and 3D projections) is square-only and untouched; the editor's structure form has `rings`
  (docs/EDITOR_UI.md), and its server uses the engine loader, so hexagon games validate there.
- **Fixtures**: `tests/fixtures/hex-pointy` and `hex-flat` (no art; map `skirmish`, 12 x 9, 2 AI sides, an HQ with
  `"rings": 1`, factories, cities). `ote-sim tests/fixtures/hex-pointy skirmish` works; results are deterministic.

## Implementation notes (starter content)

What step 3 built: hexagon starter art (generated), two editor templates and a demo. Everything is top-down, T = 24
(pointy cells 24 x 28, flat cells 28 x 24).

- **Generator** (`tools/art/`, Python 3 + Pillow, deterministic): `hex_geom.py` (the one place with the geometry: cell size,
  `inside(x, y)`, corners, neighbour order and offsets, where a box sits on a map, the ring picture size), `art_hex_tiles.py`
  (terrain), `art_hex_structures.py` (structures), `make_hex_art.py` (sheets and `data/graphics.json`),
  `preview_hex.py` (composites a map from a game's sheets with the contract's geometry, for looking and for the test).
  Regenerate: `python3 tools/art/make_hex_art.py .` (all three games; extra arguments name game folders). It never touches the
  square games; `make_starter_art.py` still reports no changes.
- **Hexagon test**: a pixel belongs to a hexagon by its centre; no centre can lie on an edge, so neighbouring hexagons
  partition the pixel grid exactly (no gap, no overlap) and the art needs no clipping or overdraw. Corners outside the
  hexagon are transparent in every terrain cell; inside it every pixel is opaque.
- **`art/tiles.png`** (16 x 26 cells): row 0 plain variants (cols 0-2), forest (3), mountain (4); rows 2-9 the road block at
  column 0; rows 10-17 water, frame 0 at column 0 and frame 1 at column 8; rows 18-25 the bridge, same two frames. Join piece
  `n` (bit `i` = neighbour `i`, the order above) is at column `n % 8`, row `n / 8` of its block.
  - Roads: from the middle a 8 px wide arm runs toward every set bit and ends exactly on the shared edge; two pieces across an
    edge compute the same line, so roads meet without a seam. No bit set is a small round patch.
  - Water (`joinWith` the bridge, `joinEdge` true): a sand bank with a foam line on every side whose bit is clear; the water
    "core" is the hexagon cut back by 4 px on those sides and the bank is graded by distance from it (corners round off).
    Shores bend by 60 degrees at hexagon corners, which is how a hexagon coast looks. Bank and foam outline are identical in
    both frames; only the foam gaps and the shimmer move.
  - Bridge (`joinExcept` water, `joinEdge` false): water with banks on the sides that have an arm and a plank deck running
    toward every non-water neighbour (on a diagonal river the deck therefore forks toward both bank hexagons).
- **Structures**: `art/structures.png` (one-hexagon: city, factory, hq, cells box width x (box height + 4), the box on the
  bottom, the square set's buildings on a paving shaped like the lower part of the hexagon) and `art/structures_ring.png`
  (`fortress`, one cell 72 x 70 pointy / 70 x 72 flat across the seven hexagons: paving in exactly their shape, six towers on
  the outer hexagons joined by walls, a keep with a flag on the middle one). The picture's middle is the middle hexagon's
  middle; its box starts `(picture - box) / 2` up and left of the picture's corner. No per-side pictures: like the square
  starter set, the owner shows through the engine's owner frame.
- **Units**: `art/units.png` is the square set's sprites, unchanged (24 x 24, drawn on the hexagon's middle).
- **Games**: `editor/templates/hex-tactics-pointy` and `hex-tactics-flat` (the `economy-tactics` rules and roster; one 12 x 9 map
  `crossing` with two rivers, bridges, a fortress between them, two sides, funds 3000) and `games/demo-hex` (pointy; the same
  rules with mech and recon added; maps `fortress_ford`, 19 x 13, a river that splits around a fortress island with five
  bridges, mirrored sides, and `foggy_hills`, 13 x 9, `"fog": true`, forests that hide). All use the retro sound set
  (`tools/audio/targets.py`, `python3 tools/audio/make_starter_audio.py .`; the demo also ships `ambush`). `ote-sim` finishes
  every map without a stall (the sim is deterministic, so each map has one fixed winner, as with the square demos).
- **Tests**: `tests/hex_content_test.py REPO [OTE_SIM]` (generator determinism and "committed = generated", sheet sizes and
  cells, transparent corners, all 64 pieces of each block, seamless tiling, structure picture sizes, every file named exists,
  sims finish; `ctest -R hex_content`), `tests/test_demos.cpp` (`hex_demo_and_templates_load_and_start`), `tests/test_art.cpp`
  (complete art), plus the template lists in `tests/editor_api_test.py` and `tests/sound_player_test.py`.
- **Not done / for later**: no sea or beach terrain (the demo uses a river), no tinted style sheets, no per-side structure
  pictures. `preview_hex.py` is the reference for how the top-down sheets are meant to be laid out (the isometric ones: see the
  last section).

## Implementation notes (player)

What step 2 built (top-down view only), for the art, editor and later view steps.

- **Geometry** is one small module with no drawing, `engine/core/view/hex_geo.h|cpp` (`HexGeo`, tested in
  `tests/test_hex_view.cpp`): `HexGeo::of(grid, tile)`; `box(x, y)`, `middle`, `mapW/mapH`, `structureBox(x, y, rings)`,
  `pick(fx, fy)` (nearest middle, `kNoCell` when that position is off the map), `nearestCell` (clamped), and
  `rowSpan(row, l, r)`: the filled hexagon inside a box, row by row. `L` is `longSide(T)`; the quarter of it is the
  overlap of rows (pointy) or columns (flat). Helpers that work on both kinds of grid (squares keep their exact old
  arithmetic): `cellCentre`, `cellTopLeft`, `structureBox`, `cellNearPos`.
- **Flat map pixels on hexagon maps.** A unit's `UnitView::px/py` is still the top-left of its tile-sized box, but that box is
  centred on the hexagon's middle (`cellTopLeft`: middle minus `T/2`). So everything that adds `tile/2` to reach "the
  middle" (floats, camera focus, lunges, fog flood) works unchanged, units are drawn exactly as on squares (sheet cell
  `T` wide, feet at middle + `T/2`), and the walk glides between middles. Anything that needs a cell's position goes
  through `cellCentre` / `cellTopLeft` / `structureBox`, never `x * tile` (there are no such uses left in `engine/player`
  outside the isometric and 3D code, which never see a hexagon map).
- **`MapProj`**: `hex()`, `hexGeo()`; `worldW/H` are the real map size (camera limits, scrolling, centring follow);
  `pick` = nearest middle of the pixel's centre; `cellBox` = the hexagon's box (menus, the white flashes). A hexagon map
  is drawn top-down or 2D isometric (a warning and top-down for a 3D style; the loader already refuses such styles).
- **Drawing** is in `battle/hex_draw.h|cpp` and `structure_draw.cpp` (the square code paths are untouched): `fillHex`
  (rows of equal width merged: about 15 `fillRect` calls), `hexEdges(color, width, edgeMask)` (the border of the
  rows, only the edges whose direction bit is set), `outerEdges(grid, cell, covered)` (what an owner frame draws),
  `drawTerrainHex`, and `CellShape` (one call for "a mark over this cell" on both kinds of grid). They draw whole
  pixels, so any window scale or UI scale just magnifies them. The spans are built so that neighbouring hexagons
  share their edges exactly (no gap, no overlap; `hex_drawn_hexagons_tile_exactly_and_agree_with_picking`); use an
  **even `tileSize`** (an odd one may leave a one-pixel seam).
- **Terrain**: the colour as a filled hexagon, then the art cell (box size, drawn in the box, never cut), or a faint outline
  and the letter. Join pieces: `sprites.cpp` uses `joinMaskHex` + `pickCell(..., 8)` on hexagon games.
- **Order of drawing**: terrain row by row (cells at screen positions sort the same way), then structures and units in one
  pass sorted by the pixel row of their bottom edge (a structure: its box's bottom; a unit: its tile box's bottom; a unit
  on a structure sorts with the structure), then fog, capture numbers, cursor. Odd columns on flat maps sit lower, rows
  interleave on pointy maps: the pixel row decides, not the row index.
- **Highlights** (move range, attack ring, supply, fog veil, white flashes) are `fillHex` at the cell's box; the cursor is
  two rings (yellow outside, white inside). Capture numbers sit at the top right of the unit's tile box.
- **Structures**: a one-hexagon picture is drawn in the hexagon's box; with `rings` across `structureBox` (pointy `3T` x
  `5L/2`, flat `5L/2` x `3T`, bottom edge on the box's bottom) at its own aspect. The sheet cell of such a picture should
  have exactly that size (it is scaled to the box's width). Placeholder: each covered hexagon filled, a dark outline round
  the group, the letter on the middle hexagon. Owner frame: 2 pixels, on the outer edges of the covered hexagons only
  (not on seams inside the group). `structureDimmed` (fog) now looks at `Battle::structureCells`.
- **Input**: pointer and touch use `MapProj::pick`; the direction buttons move by column and row (unchanged code), the
  turn/tilt buttons do not exist (no 3D). The player has no minimap, no edge scrolling and no drawn move path or
  story-area markers, so there was nothing else to port; "next unit", dialogue focus, story camera moves and the
  floating numbers use the hexagon's middle through the unit positions.
- **Tests**: `tests/test_hex_view.cpp` (geometry, picking, tiling, `MapProj` on the fixtures), `tests/hex_player_test.py`
  (both layouts: terrain and box corners by pixel, cursor, move range and attack ring against the contract's neighbour
  order written again in Python, tap picking next to edges, rings frame, move and attack through the Continue save,
  scrolling to the real map size, fog veil, and a generated art sheet: box-sized cells, 8 x 8 join pieces for every mask,
  a ring structure's picture box), registered as `hex_player`; `tools/webtest/suites/hex.mjs` (a fixture through the
  embed page, with a click).
- **For art**: terrain cells are `T x L` (pointy) / `L x T` (flat), `L` = `longSide(T)`; join blocks 8 x 8 cells of that size;
  unit cells as on squares (`T` wide); a ring structure's cell is `3T x 5L/2` (pointy) / `5L/2 x 3T` (flat). Transparent
  corners show whatever was drawn before (the neighbour, or the background colour behind the map).

## Implementation notes (editor)

What step 4 built (details and wording in docs/EDITOR_UI.md "Hexagon games"). Code: `editor/web/src/model/hex.ts`, `gridSpec.ts`, `mapGeo.ts`,
`screens/maps/editor/drawHex.ts`, `drawUnits.ts`, `drawTypes.ts`.

- **Interpretations.** "Settings screen" is the editor's **Game** screen (it has no screen of that name): the grid is a read-only "Grid" card there. The
  map canvas measures everything in *tiles* (pixels of the game's tile size); `view.scale` is pixels per tile on both grids. A hexagon's unit is drawn
  as a tile-sized square centred on the hexagon's middle (docs/HEX.md "Units"). A structure's picture keeps the piece's proportions across the box
  (the rule squares use), so a ring picture of exactly 3T x 5L/2 fills the box and a taller one rises above it. The owner frame, selection and ghost are drawn
  inside the outline of the covered hexagons. Picking returns the cell whose middle is nearest within the map's picture; outside it, nothing.
- **Resize** works by column and row as on squares; anchors that shift by an odd number of rows (pointy) or columns (flat) change which shapes the odd/even
  shift gives, so contents move half a hexagon sideways (or down). Structures whose seven hexagons no longer all fit are removed on confirm (listed first).
- **Not changed on purpose.** Story rectangles stay columns and rows. The editor never changes a game's grid. The first-guess division of a new image uses the
  hexagon's box; other quick sizes stay available.
- **Server.** `GET /api/templates` reports each template's `grid`; everything else passes through (the loader judges hexagon games). No friendly editor
  checks were added beyond the existing ones, which now know rings (`misfits`, units on a structure, upgrade sizes).
- **Tests.** The editor's unit and e2e tests above, `tests/editor_api_hex_checks.py`. Play is checked only for "starts and reports ready" (the player draws
  hexagon games in its own step).

## Implementation notes (2D isometric)

What the second view step built, for the 3D step and for art makers.

- **Geometry** is `engine/core/view/hex_iso.h|cpp` (`HexIsoGeometry`, no drawing; tests `tests/test_hex_iso.cpp`): `td` is the top-down
  lattice, `sq` the squashed one (a `HexGeo` built with `HexGeo::ofBox(grid, shortSide, longSide)`), `topBox`, `project` /
  `unproject` (flat map pixels <-> world pixels at a level), `visibleFaces` / `faceSpan` (which edges show a face and the columns
  under them), `sortKey`, `pick`, `hidden`. Its twin for art and tests is `tools/art/hex_iso_geom.py` (`IsoGeom`); the player test
  checks the twin against the engine's real pixels. `hidden` is defined through `pick` (a cell is hidden when a pointer on the middle of
  its top face would pick another).
- **Player**: `MapProj` takes the hexagon isometric case (`iso()` and `hex()` both true; `hexIso()`, `hexElevationAt(fx, fy)` blends the
  nearest hexagon's level with the next nearest's for walking units, `hexDrawOrder()`, `cellBox` = the top face's box, `pick`).
  Drawing is `battle/hex_iso_draw.cpp` (`drawMapHexIso`, called from `drawMapIso` for hexagon maps), placeholder blocks are
  `IsoTiles::hexBlock` (`iso_tiles_hex.cpp`), and the unit figure is shared with squares (`iso_unit.cpp`, `drawIsoUnitFigure`; the
  marker is the only difference). Floats start above the unit's hexagon; the story's white flash is a box over the unit.
- **Loader**: `hexGameAllowsView(StyleView)` in `core/data/database.h` is the one list of the kinds a hexagon game may use (top-down
  and isometric today); the loader refuses the rest with the message above and refuses `tileHeight` on a hexagon game
  (`style "iso" "isometric": "tileHeight" is not used on hexagon games ...`). The editor helper reports the list in `GET /api/info`
  (`hexViews`) and the editor offers exactly it; it hides `tileHeight` and shows a quarter of the width as the Height step default.
- **Starter art**: `tools/art/art_hex_iso.py` (called by `make_hex_art.py` for every style whose view is `isometric`; the art is drawn
  for the default measures, a game with other measures needs its own): `art/iso_tiles.png` (16 x 26 cells in the layout of `tiles.png`,
  48 x 60 pointy / 56 x 56 flat: the top-down piece of the same terrain squashed onto the squashed hexagon, so all 64 pieces of
  road, water and bridge keep their meaning and still meet across edges; forest and mountain are drawn standing up; earth, rock,
  water and plank side faces), `art/iso_structures.png`, `art/iso_structures_ring.png` (the fortress on its plinth with towers and
  walls), `art/iso_units.png` (the square isometric set's sprites). They are the style's own art (`styles.<id>` in graphics.json).
- **Tests**: `tests/test_hex_iso.cpp` (rounding rule, exact tiling for every tile width, faces, picking and hiding with heights,
  `MapProj` on the fixtures), `tests/hex_iso_player_test.py` (registered `hex_iso_player`: exact top faces by pixel, heights and faces,
  picking by taps through the Continue save, highlights, cursor, fog, structures, units on raised ground, and art cells drawn as
  documented), the isometric part of `tests/hex_content_test.py`, `editor/web/tests/unit/views.test.ts` and the Game screen's e2e
  spec `hex-screens.spec.ts`.
- **Flat hexagons at the defaults** are 56 x 24 and the rounding of the shared row spans leaves the leftmost and rightmost pixel
  column of the box empty (the tips are one pixel in); the lattice still tiles exactly and the art's cell is as wide as the box.
- **Decisions where the contract was silent**: the rounding rule and the terrain art cell (above); that the pointy block shows two
  side faces (the contract said three: the third, the vertical edge, is edge-on and has no width); that the starter art is
  drawn for the default measures; that the unit's marker is a flat hexagon; that the repeated faces use the bottom
  `box height / 2 + heightStep` rows of the cell; picking and draw order as above.
- **Not done**: the 3D kinds; art made for other `tileWidth` values; the editor's map canvas stays top-down by design (Play shows the
  real thing); nobody has looked at it in a real window.

## Implementation notes (3D)

What the third view step built (3D isometric and full 3D on hexagons), for art makers and later steps.

- **Space.** `core/view/hex3d.h|cpp` (`HexSpace`, tests `tests/test_hex3d.cpp`): the 3D space is the top-down picture in squares (one
  `tileSize` is one unit), so a spot in flat map pixels (what units animate in) is `X = fx / T - W/2`, `Z = fy / T - H/2`, with
  `W x H` the whole map picture over `T`. Nothing about units, effects or the camera follow-ups changed: they already live in flat
  map pixels. `HexGeo::corners` gives a hexagon's six corners (clockwise from above; edge `i` faces direction `i` of the neighbour order in both
  layouts), so neighbouring blocks share corners exactly and the ground has no cracks. A block is a six-sided prism from the top
  (`elevation * stepY`) down to one level under the lowest ground (the rim of the map shows that). `Iso3DGeometry` and `Free3DGeometry` got a
  `hex` member; with it on, `pick` / `hidden` / the extent of the ground work on hexagon blocks (a line of sight sampled every 0.05 squares,
  the hexagon being the nearest middle, as in the top-down picture), and `MapProj` (`cellMid`, `spaceW/H`, `turnCount`, `elevationAt` is the
  hexagon version) hides the difference from the drawer.
- **Decisions where the contract was silent.**
  - *Turning*: 60 degrees per step, `yaw = turn * 60`. **Turn 0 is the 2D isometric picture** (the top-down map squashed 2:1, rows straight across),
    so a style can change between the two kinds and keep its look. The measures are the 2D isometric ones (`tileWidth` pixels per top-down
    `tileSize`, squash fixed at 2:1, `heightStep` pixels a level, `unitWidth`; defaults 2 x, quarter, half); `tileHeight` is refused as there.
    A step takes 1/6 s (a quarter turn on squares takes 1/4 s). Full 3D keeps its free camera, start distance and limits.
  - *Direction buttons while turned*: the arrows are rotated by the **nearest quarter turn** of the camera (turn 0..5 = 0, 1, 1, 2, 3, 3 quarters), then
    move by column and row as always. There is no exact mapping of four buttons onto six directions; this keeps them predictable.
  - *Top picture*: the terrain's top-down cell (or a 3D style's own picture) is laid over the hexagon's box, the hexagon cuts it (transparent
    corners never show); sides are the terrain colour mixed with dark (35 %), as on squares (no style side material exists on squares either).
  - *Highlights, fog*: filled hexagons a hair above the top with no inset (neighbours share edges exactly, so a translucent fill never doubles up);
    the fog veil also covers the side faces. The cursor is a ring 0.09 squares wide inside the hexagon (pulsing, faint again through blocks in
    front). A unit's marker is a small flat hexagon (0.6 squares across) with a dark rim.
  - *Structures*: a picture or model stands on the middle hexagon facing the camera, as wide as its top-down box (one hexagon: 1.15 x the box); with
    `rings` as wide as the seven-hexagon box. A placeholder is a low block (0.45 squares) per covered hexagon: a lone block pulled in 8 % from the edge with
    the owner's frame on the ground round it, a group of seven fills its hexagons with the frame on the outer edges of its top.
  - *Standing terrain*: as on squares. A picture stands a little in front of its hexagon's middle (its lower edge on the tile, the unit on that hexagon in front of it; structure pictures likewise stand in the front part of their footprint); a model stands on the middle and a unit on that
    hexagon moves in front of it; "units in front of a standing part" looks at the neighbours whose middles lie within about 63 degrees of the camera.
  - *Heights under units*: a walking unit's height is eased between the nearest hexagon and the next nearest (`hexElevationAt`), as in 2D isometric.
- **Starter art** (`tools/art/art_hex_3d.py`, called by `make_hex_art.py` for styles of the two 3D kinds): `art/ground3d.png` (leafy floor and rock floor as
  hexagon cells), `art/stand.png` (the trees and peaks of the square starter), `data/graphics.json` styles with the top-down terrain, structures and the
  isometric unit sprites, as `demo-economy` does for squares. The two templates have no 3D styles (not trivial: they would need their own look decisions).
- **Tests**: `tests/test_hex3d.cpp` (corners tile, space, sixth turns, pick and hidden on both cameras, `MapProj` on the fixtures), `tests/hex3d_player_test.py`
  (registered `hex3d_player`: terrain colours at the projected middles at all six turns and in full 3D, heights, taps, direction buttons, range, fog, cursor,
  structures of 1 and 7, units, standing terrain, software = hardware), `tests/render3d_backends_test.py` (three hexagon scenes), the 3D part of
  `tests/hex_content_test.py`, the editor's unit and e2e tests (all four kinds offered).
- **Not done / doubtful**: the editor's map canvas stays top-down; templates have no 3D styles; the free camera's `Up/Down` keys are unchanged; art made for
  other `tileWidth` values; nobody has looked at it in a real window.
