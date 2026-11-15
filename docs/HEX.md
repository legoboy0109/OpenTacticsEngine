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

Status: contract written; nothing built yet. Build order at the end.

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
