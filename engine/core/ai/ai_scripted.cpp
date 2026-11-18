// Computer opponent: the smaller pieces of the scripted-style options (docs/AI.md "Scripted-style computer players"): the acting order,
// "ignores", the patrol walk and guardArea's movement-cost measure. Nothing here runs for a unit or game that uses none of the keys.
#include <algorithm>

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

int orderedUnit(const Battle& b, int side) {
  if (!b.db->rules.ai.orderMode) return -1;
  int pick = -1;
  for (const Unit& u : b.units) {
    if (!u.alive || u.side != side || u.done) continue;
    // Strictly lower only: a tie keeps the earlier unit (the lower id = placement order: the map's list, then units that arrived later).
    if (pick < 0 || u.behaviour.order < b.units[(size_t)pick].behaviour.order) pick = u.id;
  }
  return pick;
}

namespace {

bool matchesId(const Battle& b, const Unit& o, const std::string& id) {
  if (!o.storyId.empty() && o.storyId == id) return true;
  return o.character >= 0 && o.character < (int)b.db->characters.size() && b.db->characters[(size_t)o.character].id == id;
}

}  // namespace

void fillIgnores(const Battle& b, const Unit& u, Stance& st) {
  const AiBehaviour& beh = u.behaviour;
  if (beh.ignores.empty()) return;
  std::vector<char> ign(b.units.size(), 0);
  int hostile = 0, ignoredCount = 0;
  for (const Unit& o : b.units) {
    if (!o.alive || !b.isEnemy(u, o) || !knownEnemy(b, u.side, o)) continue;
    ++hostile;
    for (const std::string& id : beh.ignores)
      if (matchesId(b, o, id)) {
        ign[(size_t)o.id] = 1;
        ++ignoredCount;
        break;
      }
  }
  if (ignoredCount == 0 || ignoredCount == hostile) return;  // nobody to ignore, or only ignored units are left: it goes for them after all
  st.ignored = ign;
  std::vector<Cell> goals, kept;
  enemyGoals(b, u.side, goals);
  for (Cell g : goals) {
    const int id = b.unitAt(g);
    if (id >= 0 && ign[(size_t)id]) continue;
    kept.push_back(g);
  }
  distanceField(b, b.typeOf(u).moveType, kept, st.goalField);
  st.ownGoals = true;
}

bool ignoredTarget(const Stance& st, int target) { return target >= 0 && target < (int)st.ignored.size() && st.ignored[(size_t)target]; }

void fillPatrol(const Battle& b, const Unit& u, Stance& st) {
  const AiBehaviour& beh = u.behaviour;
  if (beh.route.empty()) return;
  const std::pair<int, int>& goal = beh.route[(size_t)(u.patrolAt % (int)beh.route.size())];
  if (goal.first >= b.grid.width() || goal.second >= b.grid.height()) return;
  distanceField(b, b.typeOf(u).moveType, std::vector<Cell>(1, b.grid.at(goal.first, goal.second)), st.field);
  st.patrolling = true;
}

void considerPatrol(const Stance& st, const Battle& b, const Unit& u, const std::vector<Cell>& stops, Order& best) {
  if (!st.patrolling || stops.size() < 2) return;
  int bestVal = st.field[(size_t)u.cell];
  Cell bestCell = kNoCell;
  double bestDef = -1;
  for (Cell s : stops) {
    if (s == u.cell || st.field[(size_t)s] >= kFar) continue;
    const int val = st.field[(size_t)s];
    const double def = terrainDefense(b, s);
    // The least remaining cost wins; equal: the better terrain, then the square found first in the move search.
    if (val < bestVal || (val == bestVal && bestCell != kNoCell && def > bestDef)) {
      bestVal = val;
      bestCell = s;
      bestDef = def;
    }
  }
  if (bestCell == kNoCell) return;
  const double score = 100 - std::min(bestVal, 99);
  if (score > best.score) {
    best = Order();
    best.score = score;
    best.moveTo = bestCell;
    best.kind = ActionType::Wait;
  }
}

bool hostileWithinMoveCost(const Battle& b, const Unit& u, int within) {
  std::vector<Cell> goals;
  for (const Unit& o : b.units)
    if (o.alive && b.isEnemy(u, o) && knownEnemy(b, u.side, o)) goals.push_back(o.cell);
  if (goals.empty()) return false;
  std::vector<int> field;
  distanceField(b, b.typeOf(u).moveType, goals, field);
  return field[(size_t)u.cell] <= within;
}

}  // namespace ai
}  // namespace ote
