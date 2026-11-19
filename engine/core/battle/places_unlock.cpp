// Opening without a key from a distance of one's own: the unlock ability (the thief's trait) and the unlock staff
// (docs/SRPG.md section 10 "Trait and staff"). Both open a door or chest through Battle::openPlace.
#include <algorithm>
#include <cmath>

#include "core/battle/battle.h"

namespace ote {

namespace {

bool refuse(std::string* why, const std::string& reason) {
  if (why) *why = reason;
  return false;
}

Event pev(EventType t, int a = -1, int b = -1, int c = -1, int d = -1) {
  Event e;
  e.type = t;
  e.a = a;
  e.b = b;
  e.c = c;
  e.d = d;
  return e;
}

}  // namespace

// The first stack of this item the unit carries (-1 = none).
static int slotOfItem(const Unit& u, int item) {
  for (size_t i = 0; i < u.items.size(); ++i)
    if (u.items[i].item == item) return (int)i;
  return -1;
}

// ---- the ability ----

bool Battle::unlockTargetOk(const Unit& u, int ability, Cell from, Cell cell, std::string* why) const {
  const AbilityDef& d = db->abilities[(size_t)ability];
  if (!grid.valid(cell)) return refuse(why, "that square is not on the map");
  const int s = structureAt[(size_t)cell];
  if (s < 0) return refuse(why, "there is nothing to unlock there");
  const PlaceDef& p = placeOf(s);
  if (!((p.isDoor() && d.unlockDoors) || (p.isChest() && d.unlockChests))) return refuse(why, "that cannot be unlocked");
  if (!structureVisibleTo(teamOf(u), s)) return refuse(why, "you cannot see that");
  if (!p.byTrait) return refuse(why, "a skill cannot open that");
  const int dist = placeDistance(s, from);
  if (dist < d.minRange || dist > d.maxRange) return refuse(why, "that is out of range");
  if (d.unlockNeeds >= 0 && slotOfItem(u, d.unlockNeeds) < 0)
    return refuse(why, "needs a " + db->items[(size_t)d.unlockNeeds].name);
  if (p.isChest() && !roomForLoot(u, s)) return refuse(why, "the unit cannot carry more: drop something first");
  return true;
}

void Battle::applyUnlockAbility(const Action& action, std::vector<Event>& events) {
  const int uid = action.unit;
  const AbilityDef& def = db->abilities[(size_t)action.ability];
  const int s = structureAt[(size_t)action.cell];
  const bool locked = structures[(size_t)s].locked;
  events.push_back(pev(EventType::AbilityUsed, uid, action.ability, -1, action.cell));
  if (def.unlockNeeds >= 0) {
    const int slot = slotOfItem(units[(size_t)uid], def.unlockNeeds);
    if (slot >= 0) spendUse(uid, slot, events);  // the lockpick wears
  }
  noteAbilityUse(uid, action.ability);
  spendAbilityItem(uid, action.ability, events);
  if (!def.free) endOrder(uid);
  openPlace(s, uid, locked ? kOpenTrait : kOpenFree, events);  // last: its story events may add units
}

// ---- the staff ----

// The squares of a door or chest the staff could open from `from`: those within its range of kinds it names (the picked square
// is any square of the structure; the structure is what opens). Ascending cell index.
void Battle::staffUnlockCells(int unit, int slot, Cell from, std::vector<Cell>& out) const {
  out.clear();
  if (unit < 0 || unit >= (int)units.size() || !grid.valid(from)) return;
  const Unit& u = units[(size_t)unit];
  if (!u.alive || slot < 0 || slot >= (int)u.items.size()) return;
  const int item = u.items[(size_t)slot].item;
  if (db->items[(size_t)item].kind != ItemKind::Staff || !canWieldStaff(u, item)) return;
  const StaffDef& d = db->staves[(size_t)db->items[(size_t)item].staff];
  const StaffEffect* fx = d.find(StaffEffect::Kind::Unlock);
  if (!fx) return;
  int minR = 1, maxR = 1;
  staffRange(u, item, minR, maxR);
  for (size_t s = 0; s < structures.size(); ++s) {
    const PlaceDef& p = placeOf((int)s);
    if (!((p.isDoor() && fx->unlockDoors) || (p.isChest() && fx->unlockChests))) continue;
    if (!structureVisibleTo(teamOf(u), (int)s)) continue;
    if (p.isChest() && !roomForLoot(u, (int)s)) continue;
    std::vector<Cell> structCells;
    structureCells((int)s, structCells);
    for (Cell c : structCells) {
      const int dist = grid.distance(from, c);
      if (dist >= minR && dist <= maxR) out.push_back(c);
    }
  }
  std::sort(out.begin(), out.end());
}

void Battle::applyStaffUnlock(const Action& action, std::vector<Event>& events) {
  const int uid = action.unit, slot = action.slot;
  const int item = units[(size_t)uid].items[(size_t)slot].item;
  const StaffDef& d = db->staves[(size_t)db->items[(size_t)item].staff];
  const int s = structureAt[(size_t)action.cell];
  events.push_back(pev(EventType::StaffUsed, uid, item, -1, 0));
  bool opened = true;
  if (d.canMiss()) {
    // One roll, with the battle's own dice, like a unit's (docs/SRPG.md "hit"). The formula's `b` is nobody.
    const int percent = std::max(0, std::min(100, (int)std::lround(staffEval(d.hitExpr, units[(size_t)uid], nullptr, d, kNoCell, 100))));
    opened = rng.chance(percent);
    if (!opened) events.push_back(pev(EventType::StaffMissed, -1, uid, item, action.cell));
  }
  const bool locked = structures[(size_t)s].locked;
  finishStaffUse(uid, slot, d, -1, events);  // the use, the experience and the end of the order, then the door
  if (opened) openPlace(s, uid, locked ? kOpenStaff : kOpenFree, events);
}

}  // namespace ote
