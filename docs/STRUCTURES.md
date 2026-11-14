# Structures

Contract for the structure layer. Owner decisions (2026-11-14):

- Structures are their own layer placed **on top of** terrain squares; they
  are no longer kinds of terrain.
- A structure **replaces only the rules it sets** (movement cost per
  movement type, formula numbers such as defense); everything it does not
  set comes from the terrain underneath.
- Capturing, income, building units and "counts as headquarters" live on
  **structures only**. Terrain loses those settings.
- A structure can cover **several squares** (a rectangle).
- Structures are **fixed for the battle**; only their owner changes.
  Destroying/building them comes with the events and economy steps.

## Data

### data/structures.json (optional; missing file = the game has none)

```jsonc
{ "structures": [
  { "id": "city", "name": "City",
    "color": "#b7b7b7", "glyph": "c",          // placeholder look
    "width": 1, "height": 1,                    // squares, 1..8, default 1
    "moveCost": { "treads": 2, "ship": 0 },    // optional. Listed movement types replace
                                                // the terrain's cost; 0 = cannot enter
    "props": { "defense": 3 },                 // optional. Listed numbers replace the
                                                // terrain's numbers of the same name
    "capturePoints": 20,                        // > 0 = can be captured
    "income": { "funds": 1000 },                // per turn to the owner
    "produces": ["infantry", "tank"],           // unit types the owner can build here
    "tags": ["hq"] } ] }                        // "hq" = headquarters
```

Ids are unique among structures. `name` defaults to the id.

### Terrain (data/terrain.json)
`capturePoints`, `income`, `produces` and the tag `hq` are no longer
allowed on terrain. The loader refuses them with a message that says where
they went, for example: `terrain "city": "capturePoints" now belongs to
structures (data/structures.json), see docs/STRUCTURES.md`.

### Maps
```jsonc
"structures": [ { "type": "city", "x": 3, "y": 2, "side": "blue" } ]   // side optional = neutral
```
`x`, `y` is the structure's top-left square. Rules, each with a clear
loader message: the whole rectangle must be inside the map; structures may
not overlap; `type` and `side` must exist. The old `owners` list is refused
("ownership now belongs to structures"). A unit may not start on a square
it cannot enter, judged with the structure's movement costs applied.

### Art (data/graphics.json)
`"structures": { "<id>": look }` at the top level and inside each
`styles.<id>`, exactly like `units`: `cell` or `frames`, optional
`sides.<side id>` for an owner's own picture. The picture is one sheet cell
drawn across the structure's whole footprint: width = `width` squares,
height keeps the cell's proportions, bottom edge on the footprint's bottom
edge, so it may stick out above (towers). `GraphicsSet` gains
`std::vector<UnitLook> structures` (per structure type index).

## In memory (core)

```cpp
struct StructureDef {            // database.h, Database::structures, findStructure(id)
  std::string id, name;
  uint32_t color = 0x808080;
  char glyph = ' ';
  int width = 1, height = 1;
  std::vector<int> moveCost;     // per move type: -1 = not set (use terrain), 0 = cannot enter, >0 = cost
  Props props;                   // only the numbers it sets
  int capturePoints = 0;
  std::vector<Amount> income;
  std::vector<int> produces;
  std::vector<std::string> tags;
};
struct MapStructureDef { int type = 0; int x = 0, y = 0; int side = -1; };   // MapDef::structures

struct StructureState {          // battle.h, Battle::structures (index = id, never reordered)
  int type = 0;
  int x = 0, y = 0;              // top-left square
  int owner = -1;                // side index, -1 neutral
  int capture = 0;               // capture points remaining
  int capturer = -1;             // unit currently capturing
};
```
`Battle::structureAt` holds, per cell, the structure index or -1.
`CellState` keeps only `terrain` and `height`. `TerrainDef` loses
`capturePoints`, `income`, `produces`. `MapDef::owner` is removed.

Queries on `Battle` (use these everywhere; nothing else should combine
terrain and structure by hand):
- `int structureIndexAt(Cell)`, `const StructureDef* structureDefAt(Cell)`.
- `int enterCost(Cell, int moveType)`: the structure's cost if it sets one
  for that movement type, otherwise the terrain's. <= 0 = cannot enter.
- Formula numbers of a square (what formulas see as `ta` / `tb`): the
  terrain's numbers with the structure's numbers of the same name replacing
  them and its other numbers added.
- A way to list a structure's cells.

## Rules

- **Movement and combat numbers**: per square, as above. All squares of a
  structure behave the same.
- **Capture**: a unit that can capture, standing on any square of a
  capturable structure not owned by its team, may use Capture. Progress
  belongs to the structure and to one unit: if a different unit captures,
  progress starts again from full for that unit. Progress resets when the
  capturing unit dies or ends up on a square that is not part of that
  structure (moving between squares of the same structure keeps it). When
  points reach 0 the whole structure changes owner.
- **Income**: once per owned structure per turn (not per square).
- **Building units**: on any free square of a structure the active side
  owns whose `produces` lists the unit, provided the new unit could enter
  that square. The new unit appears on the chosen square.
- **Headquarters**: a side "has an HQ" while it owns at least one structure
  tagged `hq`; the lost-HQ defeat rule works as before.
- **Defeated side**: its structures become neutral (as its squares did).
- Events: `CellCaptured` becomes `StructureCaptured` (a = structure index,
  b = new owner, c = previous owner). `CaptureProgress` keeps a = unit,
  b = the unit's cell, c = points remaining, and gains d = structure index.
- Computer player: capture targets are structures (it walks to the nearest
  square of one that it can enter and that is free; one claim per
  structure); it builds on any free square of its producing structures.

## Drawing (player; the editor canvas matches)

- Order: all terrain first; then structures and units together, sorted by
  the row of their bottom edge, structures before units in the same row. A
  unit standing on a structure is therefore drawn over it, and a tall
  structure covers things behind it.
- Without a picture: a block in the structure's colour over its whole
  footprint (inset one pixel, 1-pixel dark outline) with its letter centred.
- With a picture: as described under "Art".
- Owner: a frame in the owner's colour around the whole footprint (the
  frame previously drawn around owned squares).
- Capture points remaining are shown once per structure, on the capturing
  unit's square as today.
- The info panel names the structure under the cursor (with its owner) and
  shows the effective numbers of the square.

## Editor helper

`GET /api/projects/:id` gains `"structures"`: the `structures` list of
`data/structures.json`, or `null` when the file does not exist (not a
problem). `PUT` accepts `"structures"` (a list: written as
`{ "structures": [ … ] }`; absent or `null`: untouched). Problems come from
the real loader with `file: "data/structures.json"`.

## Shipped games

Both demos and the editor templates are converted: every building-like
terrain (city, factory, headquarters, fort…) becomes a structure standing
on ordinary ground, with its ownership moved from `owners` to the map's
`structures`. Bridges stay terrain (they join with the river). At least one
shipped map uses a structure larger than one square.
