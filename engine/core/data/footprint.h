// The cells a structure covers: the ONE place that turns a structure's size into cells, for squares
// (width x height from the top-left corner) and hexagons (rings around the middle hexagon).
// The loader, battle setup, fog, the AI and saves all go through it (docs/HEX.md, docs/STRUCTURES.md).
#pragma once
#include <vector>

#include "core/data/database.h"

namespace ote {

// Fills `out` with the cells of a `def` placed at (x, y) on `grid`: squares row by row from the top-left
// corner (x, y); hexagons the middle hexagon (x, y) first, then its six neighbours in direction order
// when def.rings is 1. Returns false (and clears `out`) when any covered cell is off the map.
bool structureFootprint(const Grid& grid, const StructureDef& def, int x, int y, std::vector<Cell>& out);

}  // namespace ote
