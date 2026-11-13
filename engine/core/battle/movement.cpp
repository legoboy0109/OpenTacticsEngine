// Movement search (Dijkstra over the grid topology).
#include <algorithm>
#include <queue>

#include "core/battle/battle.h"

namespace ote {

void Battle::reachable(int unit, Reach& out) const {
  int n = grid.cellCount();
  out.cost.assign(n, -1);
  out.parent.assign(n, kNoCell);
  out.stop.assign(n, 0);
  out.stops.clear();
  if (unit < 0 || unit >= (int)units.size() || !units[unit].alive) return;

  const Unit& u = units[unit];
  int moveType = typeOf(u).moveType;
  int budget = movePoints(u);

  // (cost, cell) min-heap. Ties resolve by cell index, keeping results
  // identical across platforms/standard libraries.
  typedef std::pair<int, Cell> Node;
  std::priority_queue<Node, std::vector<Node>, std::greater<Node> > open;
  out.cost[u.cell] = 0;
  open.push(Node(0, u.cell));

  while (!open.empty()) {
    Node node = open.top();
    open.pop();
    Cell c = node.second;
    if (node.first != out.cost[c]) continue;  // stale entry

    Cell nb[Grid::kMaxNeighbors];
    int count = grid.neighbors(c, nb);
    for (int i = 0; i < count; ++i) {
      Cell next = nb[i];
      int step = terrainAt(next).moveCost[moveType];
      if (step <= 0) continue;  // impassable
      int occ = occupant[next];
      if (occ != -1 && isEnemy(u, units[occ])) continue;  // enemies block
      int total = node.first + step;
      if (total > budget) continue;
      if (out.cost[next] != -1 && out.cost[next] <= total) continue;
      out.cost[next] = total;
      out.parent[next] = c;
      open.push(Node(total, next));
    }
  }

  // May pass through allies but only stop on free cells (or stay put).
  for (Cell c = 0; c < n; ++c) {
    if (out.cost[c] < 0) continue;
    if (occupant[c] == -1 || occupant[c] == unit) {
      out.stop[c] = 1;
      out.stops.push_back(c);
    }
  }
}

bool Battle::pathTo(const Reach& reach, Cell dest, std::vector<Cell>& path) const {
  path.clear();
  if (dest < 0 || dest >= (Cell)reach.cost.size() || reach.cost[dest] < 0) return false;
  for (Cell c = dest; c != kNoCell; c = reach.parent[c]) path.push_back(c);
  std::reverse(path.begin(), path.end());
  return true;
}

}  // namespace ote
