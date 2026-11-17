// Computer opponent: breaking a wall (docs/AI.md "Walls", docs/SRPG.md section 10).
//
// Only a unit the maker MARKED (map unit / addUnit "goesFor": ["walls"]) does this; an unmarked unit never attacks a door. A door with hit
// points that can be broken is a wall in every distance field, so a marked unit that finds no way at all to any enemy (every path is
// blocked by such a door) attacks the door it can reach, and otherwise walks toward the nearest square from which it could. It never
// breaks a door while an enemy can be reached: fighting comes first, and a door that is only a shortcut is left alone. Scores are those of
// doors and chests (docs/AI.md): above advancing, below attacking, so a marked unit that can fight, fights.
#include <algorithm>

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

namespace {

constexpr double kBreak = 150;    // attacking the door
constexpr double kHeadFor = 120;  // walking toward it

// The door `u` could hit from `from` and the weapon that does the most damage there; false when it cannot hit any.
bool bestHit(const Battle& b, const Unit& u, Cell from, int& structure, int& weapon) {
  std::vector<PlaceAttack> options;
  b.placeAttackOptions(u.id, from, options);
  int bestDamage = 0;
  structure = weapon = -1;
  for (const PlaceAttack& a : options) {
    PlaceForecast f;
    if (!b.placeForecast(u.id, a.structure, a.weapon, from, f)) continue;
    const int total = f.damage * f.strikes;
    if (total > bestDamage || (total == bestDamage && structure >= 0 && a.structure < structure)) {
      bestDamage = total;
      structure = a.structure;
      weapon = a.weapon;
    }
  }
  return structure >= 0;
}

}  // namespace

void considerWalls(Context& cx, const Unit& u, const Reach& reach, const std::vector<Cell>& stops, Order& best) {
  const Battle& b = cx.b;
  const int mt = b.typeOf(u).moveType;
  if (cx.enemyFieldFor(mt)[(size_t)u.cell] < kFar) return;  // an enemy can be reached: nothing to break
  bool any = false;
  for (size_t s = 0; s < b.structures.size() && !any; ++s) any = b.doorBreakable((int)s) && b.structureVisibleTo(b.teamOf(u), (int)s);
  if (!any) return;

  // 1. Hit it now, from a square the unit can stop on.
  for (Cell s : stops) {
    int structure, weapon;
    if (!bestHit(b, u, s, structure, weapon)) continue;
    const int travel = s == u.cell ? 0 : reach.cost[(size_t)s];
    const double score = kBreak - 0.5 * travel;
    if (score <= best.score) continue;
    best = Order();
    best.score = score;
    best.moveTo = s == u.cell ? kNoCell : s;
    best.kind = ActionType::BreakPlace;
    best.structure = structure;
    best.weapon = weapon;
  }

  // 2. Head for the nearest square from which a door could be hit (only when nothing better is known this turn).
  if (u.moved || stops.size() < 2) return;
  std::vector<Cell> goals, around, ring;
  for (size_t si = 0; si < b.structures.size(); ++si) {
    if (!b.doorBreakable((int)si) || !b.structureVisibleTo(b.teamOf(u), (int)si)) continue;
    b.structureCells((int)si, ring);
    for (Cell c : ring) {
      b.grid.within(c, 3, around);  // far enough for any weapon a door can be hit with in these games
      for (Cell a : around) {
        if (b.enterCost(a, mt) <= 0 || (b.unitAt(a) != -1 && b.unitAt(a) != u.id)) continue;
        int structure, weapon;
        if (bestHit(b, u, a, structure, weapon)) goals.push_back(a);
      }
    }
  }
  if (goals.empty()) return;
  std::sort(goals.begin(), goals.end());
  goals.erase(std::unique(goals.begin(), goals.end()), goals.end());
  std::vector<int> field;
  distanceField(b, mt, goals, field);
  int bestVal = field[(size_t)u.cell];
  Cell bestCell = kNoCell;
  for (Cell s : stops) {
    if (s == u.cell || field[(size_t)s] >= kFar) continue;
    if (field[(size_t)s] < bestVal) {
      bestVal = field[(size_t)s];
      bestCell = s;
    }
  }
  if (bestCell == kNoCell) return;  // nothing closer than here (or no way): do not shuffle
  const double score = kHeadFor - 0.2 * std::min(bestVal, 99);
  if (score > best.score) {
    best = Order();
    best.score = score;
    best.moveTo = bestCell;
    best.kind = ActionType::Wait;
  }
}

}  // namespace ai
}  // namespace ote
