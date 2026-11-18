// Named map changes (contract: docs/EVENTS.md "Changing the map"). Included by database.h before MapDef.
//
// A map may define "changes": prepared sets of squares that get another terrain (and structures that become another kind) when a
// story command applies them: a bridge appears, a wall opens, a cave-in. Applying remembers what the squares held, so reverting
// puts that back.
#pragma once
#include <string>
#include <vector>

namespace ote {

// One square of a change: its position and the terrain index it gets (heights are never changed).
struct MapChangeCell {
  int x = 0, y = 0;
  int terrain = 0;  // index into Database::terrains
  // The map's tile layer (docs/GRAPHICS.md "Tile layer"): the tile the square gets (a tile number, -1 = no tile), or kTileUnset = none
  // given. Unset: the square's tile is cleared when its terrain really changes, unless `keepTile` (the loader resolves the change's
  // and the entry's "keepTile" into this one).
  int tile = -2;
  bool keepTile = false;
};
const int kTileUnset = -2;  // (and kTileKeep = -3, used inside the battle: leave the square's tile as it is)
const int kTileKeep = -3;

// A structure the map placed (named by its "id") that becomes another kind of the same size.
struct MapChangeStructure {
  std::string id;   // the placed structure's story id
  int index = -1;   // its index in MapDef::structures (resolved by the loader)
  int kind = -1;    // index into Database::structures
};

struct MapChangeDef {
  std::string id;
  std::vector<MapChangeCell> cells;
  std::vector<MapChangeStructure> structures;
};

const int kMaxMapChanges = 64;
const int kMaxChangeCells = 4096;
const int kMaxFogVision = 10;  // a map's / a story's vision adjustment: -10..10

}  // namespace ote
