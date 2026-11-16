// Staves in a battle, carrying a use out (docs/SRPG.md section 8 "How it is used"). The questions are staves.cpp.
//
// Order of the events: StaffUsed; per unit in the area (ascending id) UnitHealed or StaffMissed; ItemBroke when the use was
// the staff's last; the user's XpGained / LevelUp; RankUp; then the user's order ends and supports earn for each unit healed.
#include <algorithm>
#include <cmath>

#include "core/battle/battle.h"

namespace ote {

namespace {
bool refuse(std::string* why, const char* reason) {
  if (why) *why = reason;
  return false;
}
Event staffEvent(EventType t, int a, int b, int c = -1, int d = -1) {
  Event e;
  e.type = t;
  e.a = a;
  e.b = b;
  e.c = c;
  e.d = d;
  return e;
}
}  // namespace

bool Battle::validateStaff(const Action& action, std::string* why) const {
  if (db->staves.empty()) return refuse(why, "this game has no staves");
  const Unit& u = units[(size_t)action.unit];  // validate() has checked the unit
  if (action.slot < 0 || action.slot >= (int)u.items.size()) return refuse(why, "the unit does not carry that item");
  const int item = u.items[(size_t)action.slot].item;
  if (db->items[(size_t)item].kind != ItemKind::Staff) return refuse(why, "that item is not a staff");
  if (!canWieldStaff(u, item, why)) return false;
  const StaffDef& d = db->staves[(size_t)db->items[(size_t)item].staff];
  std::vector<int> legal;
  staffTargets(action.unit, action.slot, u.cell, legal);
  if (!d.needsPick()) return !legal.empty() || refuse(why, "nobody in reach needs that");
  if (std::find(legal.begin(), legal.end(), action.target) != legal.end()) return true;
  // Say why this unit is not one (the list above is the truth; these are only the words).
  if (action.target < 0 || action.target >= (int)units.size() || !units[(size_t)action.target].alive)
    return refuse(why, "that is not a unit on the board");
  const Unit& t = units[(size_t)action.target];
  if (t.id != u.id && !unitVisibleTo(teamOf(u), t)) return refuse(why, "you cannot see that unit");
  const bool mine = teamOf(t) == teamOf(u);
  if ((d.target == StaffTarget::Ally && (!mine || t.id == u.id)) || (d.target == StaffTarget::Team && !mine) ||
      (d.target == StaffTarget::Foe && mine) || (d.target == StaffTarget::Any && t.id == u.id))
    return refuse(why, "that staff does not work on that unit");
  int minR = 1, maxR = 1;
  staffRange(u, item, minR, maxR);
  const int dist = grid.distance(u.cell, t.cell);
  if (dist < minR || dist > maxR) return refuse(why, "that unit is out of range");
  return refuse(why, "nobody in reach needs that");
}

void Battle::applyStaff(const Action& action, std::vector<Event>& events) {
  const int uid = action.unit;
  const int slot = action.slot;
  const int item = units[(size_t)uid].items[(size_t)slot].item;
  const StaffDef& d = db->staves[(size_t)db->items[(size_t)item].staff];  // validated
  const int center = d.needsPick() ? action.target : (d.target == StaffTarget::Self ? uid : -1);

  std::vector<int> affected;
  staffAffected(units[(size_t)uid], item, center, units[(size_t)uid].cell, affected);
  events.push_back(staffEvent(EventType::StaffUsed, uid, item, center, (int)affected.size()));

  std::vector<int> healed;
  for (int id : affected) {
    Unit& t = units[(size_t)id];
    if (d.canMiss()) {
      // One roll per unit, in ascending id, with the battle's own dice (docs/SRPG.md "hit").
      const int percent = std::max(0, std::min(100, (int)std::lround(staffEval(d.hitExpr, units[(size_t)uid], &t, d, kNoCell, 100))));
      if (!rng.chance(percent)) {
        events.push_back(staffEvent(EventType::StaffMissed, id, uid, item));
        continue;
      }
    }
    int total = 0;
    for (const StaffEffect& fx : d.effects) {
      if (fx.kind != StaffEffect::Kind::Heal) continue;
      total += std::max(0, std::min(staffHealAmount(units[(size_t)uid], d, fx, t, kNoCell), maxHp(t) - t.hp - total));
    }
    if (total <= 0) continue;
    t.hp += total;
    events.push_back(staffEvent(EventType::UnitHealed, id, total));
    if (id != uid) healed.push_back(id);
  }

  // The use is spent (the staff may break: the slot is gone after this), then what the user earns.
  Unit& user = units[(size_t)uid];
  const Unit* centerUnit = center >= 0 ? &units[(size_t)center] : nullptr;
  const int xp = d.xpExpr >= 0 ? (int)std::max(0.0, std::min<double>(kMaxStaffXp, std::floor(staffEval(d.xpExpr, user, centerUnit, d, kNoCell, 0)))) : d.xp;
  const int group = d.gear.rankGroup;
  const int rankXp = d.gear.rankXp;
  spendUse(uid, slot, events);
  if (xp > 0) awardXp(uid, xp, events, false);
  // Growth "use": the staff's own experience in its group, like a weapon after a fight (docs/SRPG.md section 7).
  if (db->ranks.on() && db->ranks.growByUse && group >= 0 && units[(size_t)uid].alive) {
    int before = 0, after = 0;
    addRankXp(units[(size_t)uid], group, rankXp, before, after);
    if (after > before) {
      Event up;
      up.type = EventType::RankUp;
      up.a = uid;
      up.b = group;
      up.c = after;
      up.d = before;
      events.push_back(up);
    }
  }
  endOrder(uid);
  // Supports earn after the order ends: their story events may add units (items do the same).
  for (int id : healed) supportsHealed(uid, id, events);
}

}  // namespace ote
