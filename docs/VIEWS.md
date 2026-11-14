# Kinds of view

Contract for how a visual style draws the battle. Owner decisions
(2026-11-14):

- A visual style is made separately by the game's maker and has a **kind of
  view**. The player switches between the styles a game ships **that the
  platform they are on can draw**.
- Kinds: the top-down 2D view, **2D isometric**, **3D isometric**, **full
  3D** (like the GameCube and Wii Fire Emblem games).
- The isometric kinds show map heights as **raised blocks**.
- 2D isometric has **one fixed angle**; 3D isometric rotates.
- In 3D isometric the maker chooses per game between 2D sprites and 3D
  models for units.
- Full 3D: 3D map, free camera, fights on the map first; separate close-up
  battle scenes later.
- The 3D renderer must run on the console ports later: an engine-owned 3D
  interface with swappable backends (OpenGL ES 3 / WebGL 2 first), no
  graphics-API or shader code in game data, fixed built-in materials, one
  directional light plus ambient, modest power-of-two textures, CPU
  skinning, glTF models.

Rules never depend on the style: a save, the AI and the editor's checks are
the same in every kind of view.

Status: top-down and 2D isometric are built. `isometric-3d` and `3d` are
accepted in game files already, so a game can be prepared for them; no
player draws them yet (see "What the player offers").

## Data (game.json `styles`)

```jsonc
"styles": [
  { "id": "classic", "name": "Classic" },                      // "view" defaults to "top-down"
  { "id": "iso", "name": "Isometric", "view": "isometric",
    "isometric": {            // optional, each key optional; logical pixels
      "tileWidth": 48,        // the diamond of one square; even, 8..512. Default 2 x view.tileSize
      "tileHeight": 24,       // even, 4..256. Default tileWidth / 2
      "heightStep": 12,       // pixels one height level raises a square, 0..128. Default tileHeight / 2
      "unitWidth": 24 } }     // width unit pictures are drawn at, 4..512. Default tileWidth / 2
]
```
`view`: `"top-down"`, `"isometric"`, `"isometric-3d"`, `"3d"`; anything else
is refused by the loader. `background`, `showGlyphs`, `terrainColors` and
`structureColors` work in every kind.

Core: `StyleDef::view` (`StyleView`), `StyleDef::iso` (`IsoConfig`);
`StyleInfo::view` in the cheap game preview.

## Art (data/graphics.json)

`styles.<id>` holds a style's pictures as before. **A style whose view is
not top-down starts with no art at all** instead of inheriting the general
(top-down) pictures: a picture made for one kind of view is wrong in
another. Whatever such a style does not define keeps its placeholder.

Isometric pictures (sheets, cells, `variants`, `frames`, `sides` all work
as usual; `join` pieces are chosen the same way but are not used by the
starter art):

- **Terrain**: drawn `tileWidth` wide, height in proportion to the cell.
  Its bottom edge lies `heightStep` pixels under the diamond's bottom
  corner, so the bottom `tileHeight + heightStep` pixels of the cell are the
  diamond and, under it, the side faces; everything above the diamond
  (trees, peaks) sticks up. A square higher than the squares in front of it
  shows its side faces down to them by repeating the bottom
  `tileHeight / 2 + heightStep` pixels of the same picture once per level.
- **Structures**: drawn across the footprint's full width
  (`(width + height) * tileWidth / 2`), height in proportion, bottom edge on
  the footprint's nearest corner. A structure stands at the height of its
  highest square.
- **Units**: drawn `unitWidth` wide, height in proportion, feet (bottom
  edge) a quarter diamond in front of the square's middle. With
  `unitMarker`, a small diamond in the side's colour lies under the feet.

## Geometry (engine/core/view/iso.h, `IsoGeometry`)

Grid x grows to the lower right of the screen, y to the lower left. With
`levels` = the highest elevation and elevations counted from the map's
lowest square (capped at 32):

```
wx = (gx - gy) * tileW/2 + gridH * tileW/2
wy = (gx + gy) * tileH/2 - elevation * step + levels * step + headroom
```
`pick(wx, wy)` gives the square nearest the viewer whose top or side faces
contain the pixel; `hidden(x, y)` says whether squares in front rise high
enough to cover the middle of a square. Both are unit-tested
(tests/test_views.cpp) and are what an editor port must match.

## Drawing (player: engine/player/battle/iso_draw.cpp)

- Squares back to front, one diagonal (x + y) at a time. After each
  diagonal: the structures whose nearest square is on it, then the units
  standing on it (a unit between two squares counts for the nearer one; a
  unit on a structure is drawn after that structure).
- Without art a square is a block in its terrain colour (lit left face,
  shaded right face) with its letter; a structure is a low box in its
  colour with its letter; a unit is an upright block in its side's colour.
- Move and attack highlights are diamonds on the squares' top faces (on a
  placeholder structure: on top of its box). The owner's frame is a diamond
  outline around the footprint. The cursor is a diamond outline.
- Shown through: a unit whose square is covered by higher ground in front
  is drawn again faintly on top of everything; so is the cursor.
- Units, effects and the camera keep working in "flat map pixels" (what
  `BattleView` animates); `MapProj` (battle/map_proj.h) turns those into
  screen positions for the kind of view in use. The top-down drawing code is
  untouched.
- Arrow keys move the cursor along the grid as before: up = up-right on
  screen, right = down-right, down = down-left, left = up-left.

## What the player offers

`viewSupported()` (engine/player/views.h) lists the kinds this build draws:
top-down and isometric. A style of another kind is left out of the options
list and is never chosen: the game's default style is its first style the
player can draw, and a remembered choice that cannot be drawn falls back to
that default. When the 3D layer exists this asks the platform.

## Shipped games

Both demos have an "Isometric" style with generated art
(`tools/art/art_iso.py`: terrain blocks, the buildings on stone plinths,
the unit sprites cut under their feet). `demo-srpg`'s map has heights
(hills under the mountains, a sunken river bed).

## Not in this step

Rotating the 2D isometric view (owner: fixed), cliff art separate from the
terrain picture, joining isometric starter art (roads and water are single
pieces), slopes and stairs, the editor's map canvas in isometric (it stays
top-down; Play shows the real thing), unit facing.
