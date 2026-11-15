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
  The player's existing square code (`sprites.cpp joinMaskOf`) still calls the four-neighbour version: harmless on a
  hexagon game (the piece stays inside the block) but wrong; step 2 must switch to `joinMaskHex`.
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
