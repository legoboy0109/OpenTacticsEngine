// Distance fields and capture-target assignment.
#include <algorithm>
#include <queue>

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

void distanceField(const Battle& b, int moveType, const std::vector<Cell>& goals,
                   std::vector<int>& out) {
  out.assign(b.grid.cellCount(), kFar);
  typedef std::pair<int, Cell> Node;
  std::priority_queue<Node, std::vector<Node>, std::greater<Node> > open;
  for (Cell g : goals) {
    out[g] = 0;
    open.push(Node(0, g));
  }
  // Reverse Dijkstra: stepping from p INTO c costs c's entry cost, so the
  // field value of neighbour p is value(c) + cost(c). A goal cell (value 0,
  // the only cell with value 0) may be impassable for the mover, e.g. an
  // enemy on a mountain: the mover only needs to reach a cell next to it,
  // so goals use a step cost of 1 instead.
  while (!open.empty()) {
    Node node = open.top();
    open.pop();
    Cell c = node.second;
    if (node.first != out[c]) continue;
    int enter = b.terrainAt(c).moveCost[moveType];
    if (enter <= 0) {
      if (node.first != 0) continue;  // cannot pass through it
      enter = 1;
    }
    Cell nb[Grid::kMaxNeighbors];
    int n = b.grid.neighbors(c, nb);
    for (int i = 0; i < n; ++i) {
      int total = node.first + enter;
      if (total < out[nb[i]]) {
        out[nb[i]] = total;
        open.push(Node(total, nb[i]));
      }
    }
  }
}

Context::Context(const Battle& battle) : b(battle), side(battle.activeSide) {
  enemyField.resize(b.db->moveTypes.size());
}

const std::vector<int>& Context::enemyFieldFor(int moveType) {
  std::vector<int>& f = enemyField[moveType];
  if (f.empty()) {
    std::vector<Cell> goals;
    for (const Unit& u : b.units)
      if (u.alive && b.teamOf(u) != b.sides[side].team) goals.push_back(u.cell);
    distanceField(b, moveType, goals, f);
  }
  return f;
}

const std::vector<int>& Context::goalFieldFor(int moveType, Cell goal) {
  std::vector<int>& f = goalFields[std::make_pair(moveType, goal)];
  if (f.empty()) distanceField(b, moveType, std::vector<Cell>(1, goal), f);
  return f;
}

namespace {
struct Pair {
  int dist, unit;
  Cell goal;
  bool operator<(const Pair& o) const {
    if (dist != o.dist) return dist < o.dist;
    if (unit != o.unit) return unit < o.unit;
    return goal < o.goal;
  }
};
}  // namespace

void assignCaptureGoals(Context& cx) {
  const Battle& b = cx.b;
  cx.assigned.assign(b.units.size(), kNoCell);
  cx.captureGoals.clear();
  if (!b.db->rules.capture) return;

  // Friendly capturers standing on a capturable cell have claimed it.
  std::vector<int> walkers;  // ready capturers without a claim
  std::vector<char> claimed(b.grid.cellCount(), 0);
  for (const Unit& u : b.units) {
    if (!u.alive || u.side != cx.side || !b.typeOf(u).canCapture) continue;
    if (b.canCaptureAt(u.id, u.cell)) {
      claimed[u.cell] = 1;
      cx.assigned[u.id] = u.cell;
    } else if (!u.done) {
      walkers.push_back(u.id);
    }
  }
  if (walkers.empty()) return;

  for (Cell c = 0; c < b.grid.cellCount(); ++c)
    if (!claimed[c] && b.canCaptureAt(walkers[0], c)) cx.captureGoals.push_back(c);

  // Greedy matching, shortest trip first, so the nearest unit takes each
  // city and two capturers never queue for the same one.
  std::vector<Pair> pairs;
  for (int w : walkers) {
    const Unit& u = b.units[w];
    int mt = b.typeOf(u).moveType;
    for (Cell g : cx.captureGoals) {
      int d = cx.goalFieldFor(mt, g)[u.cell];
      if (d < kFar) pairs.push_back(Pair{d, w, g});
    }
  }
  std::sort(pairs.begin(), pairs.end());
  std::vector<char> goalTaken(b.grid.cellCount(), 0);
  for (const Pair& p : pairs) {
    if (cx.assigned[p.unit] != kNoCell || goalTaken[p.goal]) continue;
    cx.assigned[p.unit] = p.goal;
    goalTaken[p.goal] = 1;
  }
}

}  // namespace ai
}  // namespace ote
