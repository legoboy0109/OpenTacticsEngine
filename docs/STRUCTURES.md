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
    // "rings": 1,                              // hexagon games only (docs/HEX.md): 0 = one hexagon, 1 = it and its six
                                                // neighbours. Replaces width/height there (they must be 1 or absent);
                                                // refused in square games. Upgrades keep the same rings.
    "moveCost": { "treads": 2, "ship": 0 },    // optional. Listed movement types replace
                                                // the terrain's cost; 0 = cannot enter
    "props": { "defense": 3 },                 // optional. Listed numbers replace the
                                                // terrain's numbers of the same name
    "capturePoints": 20,                        // > 0 = can be captured; 0 or left out = cannot be captured; negative is refused
    "income": { "funds": 1000 },                // per turn to the owner
    "produces": ["infantry", "tank"],           // unit types the owner can build here
    "tags": ["hq"],                             // "hq" = headquarters
    "research": true,                           // optional: owners may research here (docs/ECONOMY.md)
    "upgrades": [ { "to": "town", "cost": { "funds": 3000 }, "requires": "masonry" } ],
                                                // optional: kinds it can become (same size); docs/ECONOMY.md
    "repairs": ["air", "tank"] } ] }            // optional: unit type ids or tags it repairs (default all);
                                                // needs rules.supply.repair; docs/ECONOMY.md "Repair and supply"
                                                // fog of war (docs/ECONOMY.md "Fog of war"), both optional:
    "visionBonus": 3, "hides": true }           // 0..10 added to the vision of a unit on its squares (to the terrain's);
                                                // or { "amount": 3, "units": ["infantry", "<tag>"] } for those unit types only;
                                                // a unit on its squares is seen only from next to it
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
("ownership now belongs to structures") unless it is empty: older editor
versions wrote `"owners": []` into every map, which assigns nothing. A unit may not start on a square
it cannot enter, judged with the structure's movement costs applied.

### Art (data/graphics.json)
`"structures": { "<id>": look }` at the top level and inside each
`styles.<id>`, exactly like `units`: `cell` or `frames`, optional
`sides.<side id>` for an owner's own picture. The picture is one sheet cell
drawn across the structure's whole footprint: width = `width` squares,
height keeps the cell's proportions, bottom edge on the footprint's bottom
edge, so it may stick out above (towers). `GraphicsSet` gains
`std::vector<UnitLook> structures` (per structure type index).

### Fog and map changes (docs/ECONOMY.md "Fog options", docs/EVENTS.md "Changing the map")

A kind may carry `"light": 1..10` (a brazier: a fixed light in sight of every team) besides `blocksSight`; a door block `"word"`. A map's named change can turn a
placed structure (by its `id`) into another kind of the **same size**, which is an upgrade-like kind change that needs no payment and no research; saves accept
it as a reachable kind. Structures are never added or removed during a battle.

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
- **Doors, chests and visitable places** are structure kinds with a `"door"`, `"chest"` or `"visit"` block; opening, visiting or destroying one turns it
  into another kind (docs/SRPG.md section 10). `"blocksSight": true` hides what is behind a kind's squares in fog.
- **Map weapons**: a kind may carry a weapon that a unit standing on it attacks with (`"weapon"`, `"uses"`, `"by"`, `"ignoreRank"`, `"becomes"`; a placed structure may have its own
  `"uses"`): docs/SRPG.md "Map weapons". `StructureState` gains `weaponUses` / `weaponFor` (uses left, valid for the kind it was set for).
- **Upgrades**: a structure's kind can change during play (`UpgradeStructure`, docs/ECONOMY.md): everything above
  then follows the new kind. The kind is `StructureState::type`; `mapType` is the one the map placed.
- **Defeated side**: its structures become neutral (as its squares did).
- **Fog of war** (docs/ECONOMY.md): a structure sees for its owner's team, one square around every square of its
  footprint; `visionBonus` and `hides` are added to / combined with the terrain's for the unit standing on it. What a
  team believes about who owns a structure out of its sight is the last owner it saw (`Battle::seenOwner`) and the
  last kind it saw (`Battle::seenKind`: an upgrade done out of its sight stays hidden, the footprint never changes);
  the true owner, kind and capture progress are in `StructureState`. `visionBonus` may be limited to some unit types
  (`{ "amount": 2, "units": [...] }`, ids or tags, like `repairs`).
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

## Implementation notes

What the first implementation does where the contract was silent, and where it
differs from the contract above (everything else is as written).

Deviations
- **Drawing order for units on a multi-square structure.** "Sorted by the row of
  their bottom edge" alone would draw a unit standing on the upper row of a 2 x 2
  footprint *behind* the structure. A unit standing on a structure square therefore
  sorts with that structure's bottom edge (structures first on a tie), so it is always
  drawn over the structure it stands on. Everything else sorts by the pixel row of its
  bottom edge (units: `py + tile`, so a unit walking past a tall structure passes
  behind/in front of it smoothly).
- **Highlights.** Move/attack tint is painted again over a structure's squares right
  after the structure is drawn; without that the picture hides the tint of the terrain pass.
- **`TerrainDef::tags` stays** (only the tag `hq` is refused); the contract listed only
  `capturePoints`, `income`, `produces` as removed from the struct.
- **Old terrain keys.** The key terrain files actually used is `capture`, not
  `capturePoints`: both are refused, naming the key as written
  (`terrain "city": "capture" now belongs to structures (data/structures.json), see docs/STRUCTURES.md`),
  as are `income`, `produces` and the tag `hq`. Structures use `capturePoints`; a structure
  that says `capture` gets `"capture" is called "capturePoints" on structures`.
- **`FormulaCtx::ta` / `tb` are `const Props*`** (the merged numbers) instead of
  `const TerrainDef*`; the Lua side is unchanged.

Decisions where the contract was silent
- **Styles.** Games had `styles.<id>.terrainColors` entries for buildings (night/parchment).
  Those names are no longer terrain, so a new `styles.<id>.structureColors` (structure id ->
  `#rrggbb`) recolors structure placeholder blocks; unknown structure ids are load errors.
  Both shipped games were moved over. (The editor's style screen does not know the key yet.)
- **Shared queries.** `enterCostOf()` and `squareNumbers()` (database.cpp) are the single
  place where terrain and structure combine; `Battle::enterCost` / `numbersAt` and the map
  loader's "may a unit start here" check call them. `Battle::structureCells`, `structureTypeAt`,
  `defOf`, `ownsHq` are the other helpers. `numbersAt` returns the terrain's own list without
  copying when no structure number applies.
- **Structure moveCost values** must be 0 or more (-1 is the in-memory "not set"); a
  negative number is a loader error. The file is optional; an empty list is fine; a file
  without `"structures"` is an error. A game without the file refuses map structures with
  "unknown structure type ... (this game has no data/structures.json)".
- **Map checks (messages).** `structures[i] (type at x,y): needs W x H squares from its
  top-left corner but does not fit inside the map (w x h)`; `overlaps structures[j] (type at
  x,y) on square x,y`; `unknown structure type`; `unknown side`. A unit starting on a
  structure whose cost is 0 gets `starts on structure "wall" (on terrain "plain") which is
  impassable for move type ...`; water with a structure that sets a cost for the unit's move
  type (a pier) is a legal start square. `Battle::init` also refuses overlapping, out-of-map and
  unknown structures (maps can be built in code).
- **Capture.** Progress is stored on the structure (`capture`, `capturer`). A different
  unit starts from full; leaving the structure (move to a square that is not part of it) or
  dying resets; a defeated side's structures also reset. `CaptureProgress.b` is the capturing
  unit's square; `d` is the structure index.
- **Building.** `buildOptions` lists a structure's `produces` filtered by "the new unit can
  enter this square" (per move type, with the structure's cost applied); `validate` gives
  separate reasons (not your structure / occupied / not buildable here / could not enter).
- **Income** once per owned structure per turn; hq = owning at least one structure tagged `hq`
  (`SideState::hadHq` is set at start from owned hq structures).
- **Computer player.** Capture targets are structures. Each ready capturer is matched (shortest
  trip first) with one structure and walks to the nearest of its squares that it can enter and
  that is free (own square counts). One claim per structure: if several friendly capturers
  stand on squares of the same structure only the one already capturing it (else the lowest id)
  captures; the others walk on. Building scans every cell, so it builds on the first free square
  of any producing structure it owns (a 2 x 2 factory can therefore take up to four builds a
  turn, money permitting). `terrainDefense` reads the merged `defense` number.
- **Player.** Placeholder letter size is `tile * 9/20 * min(width, height)`. The cursor stays one
  square. The info panel shows the terrain line, then a structure line with its own swatch (its
  picture squeezed in; the size `2x2` is appended only when it fits), then the merged numbers,
  Owner and Capture (structure's). `StructureCaptured` floats "Captured!" over the middle of the
  footprint's top edge. Capture numbers and everything else read the Battle state; nothing
  needed a refresh.
- **Art model.** `graphics.json` `structures` reuses the unit look code (`cell` / `frames`, `sides`),
  error messages say `structure "x"` / `structures can only use "cell" or "frames"`; `GraphicsSet::structures`
  is a `std::vector<UnitLook>`.
- **Shipped games.** demo-economy and the economy-tactics template: city, factory (1 x 1) and a
  2 x 2 `hq` (blue at 0,4 / red at 13,4 in demo-economy; demo-economy also has the city's upgrades town and metropolis, 1 x 1). Under each building the ground is
  plain, or road where a road led into the old building tile (roads no longer join buildings).
  demo-srpg: five 1 x 1 forts on plain, no cost overrides (they set defense and avoid only).
  `games/test` and the blank template have no buildings and kept their data (the strategy-rpg template has none of these either, but later got doors, chests and villages: docs/SRPG.md section 10); their
  art was regenerated (tile sheet no longer carries building tiles). `games/index.json` unchanged.
- **ote-sim** prints captures and builds per run and their averages in the summary.
- **Existing tests changed on purpose** (rules moved, intent kept): `test_battle` (capture, income and
  production tests give structures an owner with `World::own`; checks read `Battle::structures`),
  `test_ai` (same, `StructureCaptured`), `test_demos` (formula inputs via `numbersAt`),
  `test_loader` (demo maps now have structures: forts are structures on plain, the economy map's
  symmetry/ownership is checked on structures, terrain count 5), `battle_fixture.h` (city, hq,
  factory became structures; new 2 x 2 castle, 2 x 1 gate, mire, block types for the new tests).
  New: `test_structures.cpp` (rules) and `test_structure_loader.cpp` (loader messages, art section,
  shipped games).
