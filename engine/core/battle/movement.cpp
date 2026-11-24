// Movement search (Dijkstra over the grid topology).
#include <algorithm>
#include <queue>

#include "core/battle/battle.h"

namespace ote {

void Battle::reachable(int unit, Reach& out) const { reachFor(unit, -1, out); }

// viewTeam >= 0 (the display aid that draws what an enemy could do next turn: enemy_ranges.cpp): the search of a fresh turn, as
// the viewer's team could work it out. The unit's turn flags are not used (during another side's turn they are last turn's), and
// nothing the viewer cannot see shapes the result: a unit it does not see counts as absent (no gap at its square, no wall around
// it) and a structure it does not see counts as the kind it last saw (an opened door shows no new path).
void Battle::reachFor(int unit, int viewTeam, Reach& out, bool limitByAp, bool ignoreHeight) const {
  const bool seen = viewTeam >= 0;
  int n = grid.cellCount();
  out.cost.assign(n, -1);
  out.parent.assign(n, kNoCell);
  out.stop.assign(n, 0);
  out.stops.clear();
  if (unit < 0 || unit >= (int)units.size() || !units[unit].alive) return;

  const Unit& u = units[unit];
  int moveType = typeOf(u).moveType;
  const int team = teamOf(u);
  // Standing skills (docs/SRPG.md "Evaluation and stacking"): a cap on what a square costs, passing through enemies.
  const int costCap = moveCostCap(u);
  const bool pass = passesEnemies(u);
  int budget = movePoints(u);
  // Height rules (docs/SRPG.md "Height rules"): the most the unit climbs / drops in one step (no limit without the rules, for a flier, or when asked to ignore it).
  int jumpUp = kNoHeightLimit, jumpDown = kNoHeightLimit;
  if (!ignoreHeight) jumpOf(u, jumpUp, jumpDown);
  // "Move again" (docs/SRPG.md "Turn flags"): after its action only the movement it has not used is left.
  if (db->rules.ap.on) {
    // Action points (docs/SRPG.md "Action points"): the Move stat is the most a unit walks per turn over all its moves, and the pool must pay for the
    // walk. The enemy-range display (`seen`) shows the unit's full Move whatever the foe's pool holds.
    if (!seen) budget = apMoveBudget(u, budget, limitByAp);
  } else if (u.acted && !seen) {
    budget = std::max(0, budget - u.moveSpent);
  }
  if (u.fuel >= 0 && u.fuel < budget) budget = u.fuel;  // fuel (docs/ECONOMY.md): the tank limits the reach
  if (!u.conditions.empty() && conditionBlock(u, ConditionBlock::Move) >= 0) budget = 0;  // "cannot move" (docs/SRPG.md section 9)

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
      int step = enterCost(next, moveType);
      if (seen && fog) {
        const int si = structureIndexAt(next);
        if (si >= 0) {
          const int base = enterCostOf(*db, cells[next].terrain, seenKind(viewTeam, si), moveType);
          step = db->anyWeatherCost ? weatherEnterCost(next, moveType, base) : base;
        }
      }
      if (step <= 0) continue;  // impassable
      if (jumpUp != kNoHeightLimit || jumpDown != kNoHeightLimit) {
        if (!stepAllowed(jumpUp, jumpDown, c, next)) continue;  // too steep: a cliff the unit cannot climb or drop down
      }
      if (costCap > 0 && step > costCap) step = costCap;
      int occ = occupant[next];
      // Enemies block (unless the unit may pass through them, and then they are never a stop either: see below);
      // with fog only the ones the unit's team sees (an unseen one is planned around as if absent, and may stop
      // the move when it is carried out: fog_rules.cpp).
      if (seen && occ != -1 && !occupantSeen(viewTeam, occ)) occ = -1;  // (no sight line to it: not part of the picture)
      // (A downed unit never blocks the way, being no enemy that stands: docs/SRPG.md "Downed units and revive". It is never a stop either, below.)
      if (!pass && occ != -1 && isEnemy(u, units[occ]) && unitVisibleTo(team, units[occ])) continue;
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
    // A square held by an enemy the team does not see looks free to it.
    if (seen && occupant[c] != -1 && !occupantSeen(viewTeam, occupant[c])) {
      out.stop[c] = 1;
      out.stops.push_back(c);
    } else if (occupant[c] == -1 || occupant[c] == unit || (isEnemy(u, units[occupant[c]]) && !occupantSeen(team, occupant[c]))) {
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
