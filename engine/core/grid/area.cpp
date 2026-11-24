#include "core/grid/area.h"

#include <algorithm>

namespace ote {

int straightDirection(const Grid& g, Cell from, Cell to, int* steps) {
  if (!g.valid(from) || !g.valid(to) || from == to) return -1;
  const int limit = g.width() + g.height();  // a straight line cannot be longer than the map
  for (int dir = 0; dir < g.directionCount(); ++dir) {
    Cell c = from;
    for (int k = 1; k <= limit; ++k) {
      c = g.neighbor(c, dir);
      if (c == kNoCell) break;
      if (c == to) {
        if (steps) *steps = k;
        return dir;
      }
    }
  }
  return -1;
}

void areaCells(const Grid& g, Cell from, Cell target, const AreaDef& area, std::vector<Cell>& out) {
  out.clear();
  if (!g.valid(target)) return;
  switch (area.shape) {
    case AreaShape::None:
      out.push_back(target);
      return;
    case AreaShape::Burst:
      g.within(target, 1, out);
      return;
    case AreaShape::Radius:
      g.within(target, area.radius, out);
      return;
    case AreaShape::Ring:  // the squares around the aimed one, not the aimed one (within() is ascending, so erasing keeps the order)
      g.within(target, area.radius, out);
      out.erase(std::remove(out.begin(), out.end(), target), out.end());
      return;
    case AreaShape::Field:  // the whole map, in square order
      out.reserve((size_t)g.cellCount());
      for (Cell c = 0; c < (Cell)g.cellCount(); ++c) out.push_back(c);
      return;
    case AreaShape::Cross: {
      out.push_back(target);
      for (int dir = 0; dir < g.directionCount(); ++dir) {
        Cell c = target;
        for (int k = 0; k < area.radius; ++k) {
          c = g.neighbor(c, dir);
          if (c == kNoCell) break;
          out.push_back(c);
        }
      }
      break;
    }
    case AreaShape::Line: {
      const int dir = straightDirection(g, from, target);
      if (dir < 0) return;
      Cell c = target;
      out.push_back(c);
      for (int k = 1; k < area.length; ++k) {
        c = g.neighbor(c, dir);
        if (c == kNoCell) break;
        out.push_back(c);
      }
      break;
    }
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
}

}  // namespace ote
