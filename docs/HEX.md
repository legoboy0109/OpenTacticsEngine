# Hexagonal grids

Contract for hexagon maps. Owner decisions (2026-11-14):

- The game's maker chooses which way the hexagons point: **pointy-top** or
  **flat-top**.
- **One grid per game**: a game is a square game or a hexagon game; its maps
  cannot mix.
- Hexagon games work in the **top-down view first**. 2D isometric, 3D
  isometric and full 3D follow as later steps; until then a hexagon game
  cannot have styles of those kinds.
- On hexagon maps a structure covers **one hexagon, or a hexagon and its
  ring of six** ("big structures" from the start). Roads, rivers and coasts
  join with a hexagon set of pieces.

Rules never look at coordinates: everything in `engine/core` goes through
`Grid::neighbors / distance / within` (engine/core/grid/grid.h). Hexagons are
a new case there plus drawing, input, art and the editor.

Status: step 1 (core) done: grid, loader, structures with rings, join mask, `ote-sim`, tests and two fixture games.
Nothing is drawn yet (the player draws a hexagon map as if it were squares until step 2). Build order at the end;
what step 1 built is under "Implementation notes (core)".

## Data

### game.json

```jsonc
"grid": { "type": "hex", "layout": "pointy" }   // or "layout": "flat"
```

Optional. Missing, or `{ "type": "square" }`, is a square game (every existing
game). `layout` is required with `hex` and refused with `square`; unknown keys
are refused. A hexagon game may only have styles whose view is `top-down`
(the default); another view is refused:
`style "iso": the "isometric" view is not available for hexagon games yet (only "top-down")`.

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
- Styles: on a hexagon game only the top-down kind of view can be chosen.
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
  `game.json: style "iso": the "isometric" view is not available for hexagon games yet (only "top-down")` (the view name is
  the one in the file), `maps/x.json: grid: this is a hexagon game (game.json "grid"), but the map's type is "square"` (and
  the reverse), `structure "fort": "width" is for square games; a hexagon game uses "rings"` (same for `height`),
  `"rings" must be 0 or 1`, `"rings" is for hexagon games; a square game uses "width" and "height"`; an upgrade must keep
  the `rings`; map structures: `covers its hexagon and the six around it (rings 1) but they do not all fit inside the map`,
  `is outside the map`, `overlaps structures[n] (...) on hexagon x,y`. Tests: `tests/test_hex_*.cpp`.
- **Not touched on purpose**: story rectangles (`reach`, area conditions, `loader_story_refs.cpp`) stay column/row
  rectangles; `core/view/` (isometric and 3D projections) is square-only and untouched; the editor has not been taught
  `rings` yet (editor step; its server uses the engine loader, so hexagon games already validate there).
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
  pictures. The player draws hexagon games as squares until its own step lands; `preview_hex.py` is the reference for how
  the sheets are meant to be laid out.

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
  forces the top-down view (a warning if a style asks for another; the loader already refuses such styles).
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
