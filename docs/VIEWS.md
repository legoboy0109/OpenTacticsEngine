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

Status: all four kinds are built. The 3D kinds draw through the 3D drawing
layer (docs/RENDER3D.md). Still to come for full 3D: separate close-up
battle scenes.

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
all four today. A style of a kind a build cannot draw is left out of
the options list and is never chosen: the game's default style is its first
style the player can draw, and a remembered choice that cannot be drawn
falls back to that default. A platform without a 3D backend would leave the
3D kinds out here.

## 3D isometric (`"view": "isometric-3d"`)

The picture of the 2D isometric view, made from real blocks by a camera
without perspective that turns about the map in quarter steps.

**Measures.** The same optional `"isometric"` block with the same meanings
and defaults: `tileWidth` x `tileHeight` is the diamond of a square at a
quarter-turn position, `heightStep` the pixels one level raises it,
`unitWidth` the width of unit pictures. So a style can change between the
two isometric kinds and keep its proportions; at turn 0 the squares are in
exactly the same places.

**Geometry** (engine/core/view/iso3d.h, `Iso3DGeometry`; tests
tests/test_views3d.cpp). 3D space in squares: square (x, y) spans
X = x - gridW/2 .. +1, Z = y - gridH/2 .. +1, Y up, one level =
`stepY`. The camera looks at the map's middle; quarter turn n has
yaw (n + 0.5) x 90 degrees; its height angle comes from the diamond
(`sin = tileHeight / tileWidth`). World pixels are a square box around
everything the map can show at any turn, so the scrolling camera's limits do
not change while turning. `pick` follows the line of sight through a pixel
down to the first block it meets (top or side); `hidden` says whether blocks
nearer the camera cover the middle of a square.

**Art** (a style of this kind starts with no art, like every kind but
top-down):
- **Terrain**: the picture is laid flat on the top of the square's block,
  picture north = map north, so a top-down tile fits as it is. Variants,
  frames and joining pieces work as in the top-down view. The block's sides
  take the terrain's colour, darkened.
- **Units**: the picture stands upright on the middle of the square and
  always faces the camera, `unitWidth` wide, height in proportion. With
  `unitMarker` a flat square in the side's colour lies under the feet.
- **Structures**: the picture stands upright in the middle of the footprint,
  1.15 x the average of the footprint's sides wide (in square edges), on the
  highest of its squares. A unit on one of its squares is drawn just in
  front of the picture; a unit behind it is covered (turn the view).
- Sheets become textures of the 3D layer: sides are padded to powers of two
  and a sheet larger than the backend's limit (1024) is refused there with a
  logged error; what uses it keeps the placeholder look in this view.
- Units and structures can be 3D models instead of pictures, chosen per
  thing: docs/GRAPHICS.md "Models". A unit standing on a model structure is
  drawn in front of it.

**Drawing** (engine/player/battle/map3d*.cpp, `Map3D`; `Sheets3D`,
`Batch3D`):
- Blocks are one fixed lit mesh (light fixed to the map; tops are lit at
  exactly full strength so pictures there look as drawn). Everything that
  changes is collected per material each frame.
- Without art: a square's top is its terrain colour with its letter; a
  structure is a low lit box in its colour with its letter; a unit is an
  upright block in its side's colour with its letter. Letters, HP bars,
  leader marks and capture numbers are drawn with the 2D renderer over the
  finished 3D picture.
- Move and attack highlights lie on the tops (on a placeholder structure: on
  its box). The owner's frame lies around the footprint. The cursor is a
  frame on the square.
- Blocks cover what stands behind them. A unit or the cursor behind higher
  blocks is drawn again faintly over everything (`Material3D::depthTest`
  off), as in the 2D isometric view; turning the view is the real answer.

**Turning.** `Button::TurnLeft` / `TurnRight`: keys `,` and `.` (also `[`
and `]`), gamepad west and north face buttons, and two on-screen buttons in
the map's bottom-left corner for pointer and touch. Allowed at any time,
also during animations and other sides' turns. A quarter turn takes a
quarter of a second; the square under the cursor keeps its place on screen.
Arrow keys keep their directions on screen in every turn (up = up-right, as
in the 2D isometric view). The turn is not saved.

## Full 3D (`"view": "3d"`)

The block map of the 3D isometric view under a camera with perspective that
the player turns, tilts and zooms freely. Fights happen on the map; separate
close-up battle scenes come later (owner decision).

**Data and art.** Nothing but `"view": "3d"`; the `"isometric"` block is not
used. One height level is half a square. Art is exactly that of the 3D
isometric kind (a style of this kind starts with no art): terrain pictures
flat on the block tops, units and structures as upright pictures or as glTF
models (docs/GRAPHICS.md "Models"). A unit picture is 0.75 squares wide; an
upright picture is drawn taller the more the camera looks from above (up to
twice), so it does not look squashed.

**Geometry** (engine/core/view/free3d.h, `Free3DGeometry`; tests
tests/test_free3d.cpp). The 3D space of the 3D isometric kind. The camera
circles a focus point on the map: `yaw` (0 = standing south, looking north:
grid y up the screen, x to the right), `pitch` above the ground (25 to 75
degrees, starts at 50), `distance` (4 to 30 squares, starts at 11), field of
view 35 degrees. Pixels are viewport pixels: this kind has no 2D scrolling
(`MapProj::worldW/H` are the viewport's size, so the 2D camera stays at 0);
the focus moves instead. `pick` walks the sight line through a pixel square
by square to the first block it meets; `hidden` does the same from a square
to the eye. `shiftIntoView` says how far the focus must move for a spot to
be inside the picture's margins; the focus is kept a little inside the
map's outline so the picture is never half empty.

**Player** (`battle/free_cam.*`, `FreeCam`; `MapProj::free3d()`; the drawer
is `Map3D`, shared with the 3D isometric kind: `MapProj` hides which camera
looks at the blocks). The camera follows the cursor, or what the battle
shows, as soon as that leaves the middle of the picture (margins of 22% /
24% of the viewport), and rises and sinks with the ground. Letters, HP bars
and capture numbers are 2D things over the 3D picture, sized by how far
away their spot is. What stands behind higher blocks is drawn again faintly
on top, as in the other raised kinds.

**Controls.** A tap or press is one step, holding keeps going:

| | keys | gamepad | pointer |
|---|---|---|---|
| turn (15 degrees a step) | `,` `.` (`[` `]`) | camera stick sideways, west / north face buttons | on-screen `<` `>`, drag with the right mouse button |
| tilt (7.5 degrees) | `R` `F` (Page Up / Down) | camera stick up / down | on-screen `^` `v`, right-button drag up / down |
| zoom (15%) | `=` `-` (keypad `+` `-`) | right / left trigger | on-screen `+` `-`, mouse wheel |

`Button::TiltUp/TiltDown/ZoomIn/ZoomOut` and `InputEvent::CameraDrag` are
the platform side; `--input` knows `tiltup tiltdown zoomin zoomout`. Arrow
keys keep their directions on screen: they follow the camera in quarter
turns. The camera is not saved.

**Not yet**: close-up battle scenes; slopes or modelled terrain (the map is
blocks); remembering the camera; touch gestures (pinch, two-finger turn;
the on-screen buttons work); drawing 3D at the display's full resolution
(it is drawn at the game's logical size like everything else); per-style
camera settings (start angle, limits, field of view).

## Shipped games

Both demos have an "Isometric" style with generated art
(`tools/art/art_iso.py`: terrain blocks, the buildings on stone plinths,
the unit sprites cut under their feet) and a "3D Isometric" style that
reuses the top-down tiles on the block tops, the standing unit sprites and
the top-down structure pictures, and a "Full 3D" style with the same art as
the 3D isometric one (in `demo-srpg`: the unit and fort models).
`demo-srpg`'s map has heights (hills under
the mountains, a sunken river bed).

## Not in this step

Side pictures for blocks, remembering the
turn, free turning or zooming; rotating the 2D isometric view (owner: fixed), cliff art separate from the
terrain picture, joining isometric starter art (roads and water are single
pieces), slopes and stairs, the editor's map canvas in isometric (it stays
top-down; Play shows the real thing), unit facing.
