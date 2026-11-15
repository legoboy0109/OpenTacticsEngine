#include "core/data/footprint.h"

namespace ote {

bool structureFootprint(const Grid& grid, const StructureDef& def, int x, int y, std::vector<Cell>& out) {
  out.clear();
  if (grid.isHex()) return grid.ringCells(grid.at(x, y), def.rings, out);
  if (x < 0 || y < 0 || x + def.width > grid.width() || y + def.height > grid.height()) return false;
  for (int dy = 0; dy < def.height; ++dy)
    for (int dx = 0; dx < def.width; ++dx) out.push_back(grid.at(x + dx, y + dy));
  return true;
}

}  // namespace ote
