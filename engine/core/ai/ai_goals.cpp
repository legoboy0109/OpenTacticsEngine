// Distance fields and capture-target assignment.
#include <algorithm>
#include <queue>

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

void distanceField(const Battle& b, int moveType, const std::vector<Cell>& goals,
                   std::vector<int>& out, int jumpUp, int jumpDown) {
  out.assign(b.grid.cellCount(), kFar);
  const bool limited = jumpUp != kNoHeightLimit || jumpDown != kNoHeightLimit;
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
    int enter = b.enterCost(c, moveType);
    if (enter <= 0) {
      if (node.first != 0) continue;  // cannot pass through it
      enter = 1;
    }
    Cell nb[Grid::kMaxNeighbors];
    int n = b.grid.neighbors(c, nb);
    for (int i = 0; i < n; ++i) {
      // Height rules: a walker with a limited jump cannot take the step nb -> c (a goal square, value 0, is only ever the target of an attack).
      if (limited && node.first != 0 && !b.stepAllowed(jumpUp, jumpDown, nb[i], c)) continue;
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
    enemyGoals(b, side, goals);
    f = goalsFieldIn(b, goalSetFor(moveType), goals);  // a copy: cheap next to a Dijkstra
  }
  return f;
}

GoalFieldSet& Context::goalSetFor(int moveType) {
  if (goalSets.size() <= (size_t)moveType) goalSets.resize((size_t)moveType + 1);
  std::shared_ptr<GoalFieldSet>& set = goalSets[(size_t)moveType];
  if (!set) set = goalFieldSetFor(b, moveType);  // once per Context: the battle's terrain cannot change inside one step
  return *set;
}

const std::vector<int>& Context::goalFieldFor(int moveType, Cell goal) {
  return goalFieldIn(b, goalSetFor(moveType), goal);
}

// Height rules (docs/AI.md "Height rules"): a unit that cannot climb every step needs fields that go around the cliffs it cannot take. A game
// without the rules, and a unit with no limit, take the plain fields above (so nothing changes for them).
bool Context::jumpKey(const Unit& u, std::array<int, 3>& key) const {
  int up, down;
  b.jumpOf(u, up, down);
  if (up == kNoHeightLimit && down == kNoHeightLimit) return false;
  key = {{b.typeOf(u).moveType, up, down}};
  return true;
}

const std::vector<int>& Context::enemyFieldFor(const Unit& u) {
  std::array<int, 3> key;
  if (!jumpKey(u, key)) return enemyFieldFor(b.typeOf(u).moveType);
  std::vector<int>& f = jumpEnemyField[key];
  if (f.empty()) {
    std::vector<Cell> goals;
    enemyGoals(b, side, goals);
    f = goalsFieldIn(b, jumpSetFor(key), goals);
  }
  return f;
}

const std::vector<int>& Context::goalFieldFor(const Unit& u, Cell goal) {
  std::array<int, 3> key;
  if (!jumpKey(u, key)) return goalFieldFor(b.typeOf(u).moveType, goal);
  return goalFieldIn(b, jumpSetFor(key), goal);
}

GoalFieldSet& Context::jumpSetFor(const std::array<int, 3>& key) {
  std::shared_ptr<GoalFieldSet>& set = jumpSets[key];
  if (!set) set = goalFieldSetFor(b, key[0], key[1], key[2]);
  return *set;
}

namespace {
struct Pair {
  int dist, unit, target;  // target = structure index
  Cell goal;               // the square of it to walk to
  bool operator<(const Pair& o) const {
    if (dist != o.dist) return dist < o.dist;
    if (unit != o.unit) return unit < o.unit;
    if (target != o.target) return target < o.target;
    return goal < o.goal;
  }
};
}  // namespace

void assignCaptureGoals(Context& cx) {
  const Battle& b = cx.b;
  cx.assigned.assign(b.units.size(), kNoCell);
  cx.captureTargets.clear();
  cx.claimant.assign(b.structures.size(), -1);
  if (!b.db->rules.capture) return;

  // A friendly capturer standing on any square of a capturable structure has claimed it.
  // If several stand on the same one, the unit already capturing it keeps the claim
  // (else the lowest id); the others walk on to other targets.
  for (const Unit& u : b.units) {
    if (!u.alive || u.side != cx.side || !b.typeOf(u).canCapture || !b.canCaptureAt(u.id, u.cell)) continue;
    int s = b.structureIndexAt(u.cell);
    if (cx.claimant[s] < 0 || b.structures[s].capturer == u.id) cx.claimant[s] = u.id;
  }
  std::vector<int> walkers;  // ready capturers without a claim
  for (const Unit& u : b.units) {
    if (!u.alive || u.side != cx.side || !b.typeOf(u).canCapture) continue;
    if (b.canCaptureAt(u.id, u.cell) && cx.claimant[b.structureIndexAt(u.cell)] == u.id) cx.assigned[u.id] = u.cell;
    else if (!u.done) walkers.push_back(u.id);
  }
  if (walkers.empty()) return;
  std::vector<char> claimed(b.structures.size(), 0);
  for (size_t s = 0; s < claimed.size(); ++s) claimed[s] = cx.claimant[s] >= 0;

  std::vector<Cell> footprint;
  for (int s = 0; s < (int)b.structures.size(); ++s) {
    if (claimed[s]) continue;
    b.structureCells(s, footprint);
    // Capturability depends on the structure and the team only, so any square will do for the test.
    // (Judged on the owner the team knows: it cannot plan on the owner of a structure it has not seen change hands.)
    if (knownCapturable(b, b.units[(size_t)walkers[0]], footprint[0])) cx.captureTargets.push_back(s);
  }

  // Greedy matching, shortest trip first, so the nearest unit takes each
  // structure and two capturers never queue for the same one. A unit aims
  // for the nearest square of the structure that it can enter and that is free.
  std::vector<Pair> pairs;
  for (int w : walkers) {
    const Unit& u = b.units[w];
    int mt = b.typeOf(u).moveType;
    for (int s : cx.captureTargets) {
      b.structureCells(s, footprint);
      int bestD = kFar;
      Cell bestG = kNoCell;
      for (Cell g : footprint) {
        if (b.enterCost(g, mt) <= 0 || (b.unitAt(g) != -1 && b.unitAt(g) != w)) continue;
        int d = cx.goalFieldFor(mt, g)[u.cell];
        if (d < bestD) { bestD = d; bestG = g; }
      }
      if (bestG != kNoCell) pairs.push_back(Pair{bestD, w, s, bestG});
    }
  }
  std::sort(pairs.begin(), pairs.end());
  std::vector<char> targetTaken(b.structures.size(), 0);
  for (const Pair& p : pairs) {
    if (cx.assigned[p.unit] != kNoCell || targetTaken[p.target]) continue;
    cx.assigned[p.unit] = p.goal;
    targetTaken[p.target] = 1;
  }
}

}  // namespace ai
}  // namespace ote
