// Computer opponent: doors, chests and places (docs/SRPG.md section 10 "Computer opponent").
//
// Only a unit the maker MARKED (map unit / addUnit "goesFor": chests, villages, doors) does any of this; an unmarked unit never
// opens, visits or walks toward them. Closed doors are walls in every distance field already (their movement cost is 0), so a
// unit that cannot get through simply finds no way and waits; it never oscillates.
//   chests:   open one it can reach (unlocked, or it carries a key, or its trait fits: an "on" chest is opened from its own square), and walk toward the nearest one it could open.
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

// The trait: from `from`, the first unlock ability of `u` that opens structure `s` (a locked one the unit has no usable key for).
// unlockTargetOk reads the kind's "from" (an "on" chest is opened from its own square), so the walk to a square
// where this is true makes the unit step ONTO such a chest first (approachCells asks interaction() for every square near it).
bool traitOpens(const Battle& b, const Unit& u, int s, Cell from, Order& out) {
  std::vector<AbilityOption> menu;
  b.abilityOptions(u.id, from, menu);  // the ability's own state (uses, cooldown, afterMove) and "has a door or chest to open from here"
  std::vector<Cell> cells;
  b.structureCells(s, cells);
  for (const AbilityOption& o : menu) {
    const AbilityDef& d = b.db->abilities[(size_t)o.ability];
    if (!o.ok || d.effect != AbilityEffect::Unlock || b.abilityCharges(o.ability) || (d.free && from != u.cell)) continue;
    std::vector<int> units;
    std::vector<Cell> targets;
    b.abilityTargets(u.id, o.ability, from, units, targets);  // asks the kind's "from" and the range, as the player's menu does
    for (Cell c : cells) {
      if (std::find(targets.begin(), targets.end(), c) == targets.end()) continue;
      out.kind = ActionType::Ability;
      out.ability = o.ability;
      out.cell = c;
      out.structure = s;
      return true;
    }
  }
  return false;
}

// Can `u` do something useful to structure `s` from `from`? Fills the order's verb and key slot.
bool interaction(const Battle& b, const Unit& u, int s, Cell from, bool enemyBlocked, Order& out) {
  const PlaceDef& p = believed(b, u, s);
  const StructureState& st = b.structures[(size_t)s];
  if (p.isChest() || p.isDoor()) {
    if (!(u.aiGoals & (p.isChest() ? kAiChests : kAiDoors))) return false;
    if (p.isDoor() && !enemyBlocked) return false;
    if (!b.isClosedChest(s) && !b.isClosedDoor(s)) return false;  // it was opened already
    if (b.placeOf(s).isChest() && !b.roomForLoot(u, s)) return false;
    std::vector<int> keys;
    b.placeKeySlots(u.id, s, keys);
    if (b.placeInReach(s, from) && (!st.locked || !keys.empty())) {  // by hand, or with a key
      out.kind = ActionType::OpenPlace;
      out.slot = st.locked ? keys[0] : -1;
      out.structure = s;
      return true;
    }
    // Locked and no key that works from here: the unit's trait, if it has one (reach as the ability and the kind's "from" say).
    return st.locked && traitOpens(b, u, s, from, out);
  }
  if (!b.placeInReach(s, from)) return false;
  if (p.isVisit() && (u.aiGoals & kAiPlaces) && b.placeOf(s).isVisit() && (!st.used || b.placeOf(s).repeat)) {
    if (!b.visitAllowed(u, s) && !b.destroyAllowed(u, s)) return false;
    out.kind = ActionType::VisitPlace;
    out.structure = s;
    return true;
  }
  return false;
}

// The squares from which `u` could interact with one of the structures it goes for (free, enterable, not claimed by another unit; with
// `anyUnits` a square another unit stands on counts too: lootLeft asks whether anything is left at all, not what is free this turn).
// `hostileBlocks`: a square a hostile unit stands on is no approach square at all ("lootBlockedBy": "units").
void approachCells(const Battle& b, const Unit& u, bool enemyBlocked, std::vector<Cell>& out, bool anyUnits = false, bool hostileBlocks = false) {
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
      if (b.enterCost(c, mt) <= 0 || (!anyUnits && b.unitAt(c) != -1 && b.unitAt(c) != u.id)) continue;
      if (hostileBlocks && b.unitAt(c) != -1 && b.isEnemy(u, b.units[(size_t)b.unitAt(c)])) continue;
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
  // A looter (docs/AI.md "Looting and escaping") opens the doors it goes for whether or not an enemy can be reached: they are part of its loot.
  const bool enemyBlocked = u.behaviour.mode == AiMode::Loot || cx.enemyFieldFor(mt)[(size_t)u.cell] >= kFar;
  const bool automatic = b.db->rules.places.automatic;

  // 1. Do it now: from a square it can stop on.
  for (Cell s : stops) {
    Order o;
    for (size_t si = 0; si < b.structures.size(); ++si) {
      if (!interaction(b, u, (int)si, s, enemyBlocked, o)) continue;
      const int travel = s == u.cell ? 0 : reach.cost[(size_t)s];
      const double score = kInteract - 0.5 * travel;
      if (score <= best.score) continue;
      if (automatic && o.kind != ActionType::Ability) {  // (the trait is always an explicit ability use)
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

bool lootLeft(const Battle& b, const Unit& u) {
  if (u.aiGoals == 0 || b.structures.empty()) return false;
  if (u.behaviour.lootBlockedByUnits) {  // "lootBlockedBy": "units": walk out from the unit; a square a hostile unit stands on is a wall
    std::vector<Cell> goals;
    approachCells(b, u, true, goals, true, true);
    if (goals.empty()) return false;
    const int mt = b.typeOf(u).moveType;
    std::vector<char> seen((size_t)b.grid.cellCount(), 0), isGoal((size_t)b.grid.cellCount(), 0);
    for (Cell g : goals) isGoal[(size_t)g] = 1;
    std::vector<Cell> open(1, u.cell);
    seen[(size_t)u.cell] = 1;
    while (!open.empty()) {
      const Cell c = open.back();
      open.pop_back();
      if (isGoal[(size_t)c]) return true;
      Cell nb[Grid::kMaxNeighbors];
      const int n = b.grid.neighbors(c, nb);
      for (int i = 0; i < n; ++i) {
        const Cell d = nb[i];
        if (seen[(size_t)d] || b.enterCost(d, mt) <= 0) continue;
        if (b.unitAt(d) != -1 && b.isEnemy(u, b.units[(size_t)b.unitAt(d)])) continue;
        seen[(size_t)d] = 1;
        open.push_back(d);
      }
    }
    return false;
  }
  std::vector<Cell> goals;
  approachCells(b, u, true, goals, true);
  if (goals.empty()) return false;
  std::vector<int> field;
  distanceField(b, b.typeOf(u).moveType, goals, field);  // closed doors are walls here: loot behind one it cannot open is not "left"
  return field[(size_t)u.cell] < kFar;
}

}  // namespace ai
}  // namespace ote
