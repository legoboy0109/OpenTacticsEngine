// Computer opponent: doors, chests and places (docs/SRPG.md section 10 "Computer opponent").
//
// Only a unit the maker MARKED (map unit / addUnit "goesFor": chests, villages, doors) does any of this; an unmarked unit never
// opens, visits or walks toward them. Closed doors are walls in every distance field already (their movement cost is 0), so a
// unit that cannot get through simply finds no way and waits; it never oscillates.
//   chests:   open one it can reach (unlocked, or it carries a key), and walk toward the nearest one it could open.
//   villages: visit one it may, destroy one that is the enemy's, and walk toward the nearest.
//   doors:    only while the enemy cannot be reached at all (a door is in the way): open a door with a key it carries (or an unlocked
//             door), and walk toward the nearest such door.
// Scores sit between advancing (at most 100) and attacking (200 and up): a marked unit that can fight, fights.
#include <algorithm>

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

namespace {

constexpr double kInteract = 150;  // opening, visiting
constexpr double kHeadFor = 120;   // walking toward a goal; minus a little for distance, still above plain advancing

// What the unit's side believes the structure is (an upgrade or opening it did not see does not count).
const PlaceDef& believed(const Battle& b, const Unit& u, int s) { return b.db->structures[(size_t)b.seenKindForSide(u.side, s)].place; }

// Can `u` do something useful to structure `s` from `from`? Fills the order's verb and key slot.
bool interaction(const Battle& b, const Unit& u, int s, Cell from, bool enemyBlocked, Order& out) {
  const PlaceDef& p = believed(b, u, s);
  const StructureState& st = b.structures[(size_t)s];
  if (!b.placeInReach(s, from)) return false;
  if (p.isChest() || p.isDoor()) {
    if (!(u.aiGoals & (p.isChest() ? kAiChests : kAiDoors))) return false;
    if (p.isDoor() && !enemyBlocked) return false;
    if (!b.isClosedChest(s) && !b.isClosedDoor(s)) return false;  // it was opened already
    std::vector<int> keys;
    b.placeKeySlots(u.id, s, keys);
    if (st.locked && keys.empty()) return false;
    if (b.placeOf(s).isChest() && !b.roomForLoot(u, s)) return false;
    out.kind = ActionType::OpenPlace;
    out.slot = st.locked ? keys[0] : -1;
    out.structure = s;
    return true;
  }
  if (p.isVisit() && (u.aiGoals & kAiPlaces) && b.placeOf(s).isVisit() && (!st.used || b.placeOf(s).repeat)) {
    if (!b.visitAllowed(u, s) && !b.destroyAllowed(u, s)) return false;
    out.kind = ActionType::VisitPlace;
    out.structure = s;
    return true;
  }
  return false;
}

// The squares from which `u` could interact with one of the structures it goes for (free, enterable, not claimed by another unit).
void approachCells(const Battle& b, const Unit& u, bool enemyBlocked, std::vector<Cell>& out) {
  out.clear();
  const int mt = b.typeOf(u).moveType;
  std::vector<Cell> ring;
  for (size_t si = 0; si < b.structures.size(); ++si) {
    Order probe;
    // A square next to the structure (or on it): test the interaction from the structure's own neighbourhood.
    b.structureCells((int)si, ring);
    std::vector<Cell> around;
    for (Cell c : ring) {
      around.push_back(c);
      Cell nb[Grid::kMaxNeighbors];
      const int n = b.grid.neighbors(c, nb);
      for (int i = 0; i < n; ++i) around.push_back(nb[i]);
    }
    for (Cell c : around) {
      if (b.enterCost(c, mt) <= 0 || (b.unitAt(c) != -1 && b.unitAt(c) != u.id)) continue;
      if (interaction(b, u, (int)si, c, enemyBlocked, probe)) out.push_back(c);
    }
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
}

}  // namespace

void considerPlaces(Context& cx, const Unit& u, const Reach& reach, const std::vector<Cell>& stops, Order& best) {
  const Battle& b = cx.b;
  if (u.aiGoals == 0 || b.structures.empty() || u.acted) return;
  if (u.aiGoals & kAiWalls) considerWalls(cx, u, reach, stops, best);
  const int mt = b.typeOf(u).moveType;
  const bool enemyBlocked = cx.enemyFieldFor(mt)[(size_t)u.cell] >= kFar;
  const bool automatic = b.db->rules.places.automatic;

  // 1. Do it now: from a square it can stop on.
  for (Cell s : stops) {
    Order o;
    for (size_t si = 0; si < b.structures.size(); ++si) {
      if (!interaction(b, u, (int)si, s, enemyBlocked, o)) continue;
      const int travel = s == u.cell ? 0 : reach.cost[(size_t)s];
      const double score = kInteract - 0.5 * travel;
      if (score <= best.score) continue;
      if (automatic) {
        // The move itself does it (rules.places "auto"); standing still does nothing.
        if (s == u.cell) continue;
        best = Order();
        best.score = score;
        best.moveTo = s;
        best.kind = ActionType::Wait;
      } else {
        best = o;
        best.score = score;
        best.moveTo = s == u.cell ? kNoCell : s;
      }
    }
  }

  // 2. Head for the nearest one (only when nothing better to do this turn is known).
  if (u.moved || stops.size() < 2) return;
  std::vector<Cell> goals;
  approachCells(b, u, enemyBlocked, goals);
  if (goals.empty()) return;
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
