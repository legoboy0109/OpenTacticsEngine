// Staves in a battle, carrying a use out (docs/SRPG.md section 8 "How it is used"). The questions are staves.cpp and
// staves_effects.cpp.
//
// Order of the events: StaffUsed; per unit in the area (ascending id) StaffMissed, or UnitHealed, ItemRepaired and last
// UnitRelocated (a move is always the last thing an effect does to a unit, whatever order the staff lists them in, so every
// formula sees the unit where it stood when it was picked); ItemBroke when the use was the staff's last; the user's XpGained /
// LevelUp; RankUp; then the user's order ends, supports earn for each unit healed, and last the story's "reach" events for the
// units that were moved (they may add units or end the battle, so nothing runs after them).
// A light staff: StaffUsed, AreaLit, then the same spending, experience and end of the order.
#include <algorithm>
#include <cmath>

#include "core/battle/battle.h"

namespace ote {

namespace {
bool refuse(std::string* why, const std::string& reason) {
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

  if (d.has(StaffEffect::Kind::Unlock)) {  // doors and chests (docs/SRPG.md section 10): the square of the one to open
    if (!grid.valid(action.cell)) return refuse(why, "pick a door or chest to unlock");
    std::vector<Cell> structCells;
    staffUnlockCells(action.unit, action.slot, u.cell, structCells);
    if (std::find(structCells.begin(), structCells.end(), action.cell) != structCells.end()) return true;
    int minR = 1, maxR = 1;
    staffRange(u, item, minR, maxR);
    const int dist = grid.distance(u.cell, action.cell);
    if (dist < minR || dist > maxR) return refuse(why, "that square is out of range");
    return refuse(why, "that cannot be unlocked");
  }
  if (d.picksSquare()) {  // a light: the square to light
    if (!fog) return refuse(why, "this map has no fog of war, so there is nothing to light");
    if (!grid.valid(action.cell)) return refuse(why, "pick a square to light");
    std::vector<Cell> structCells;
    staffLightCells(action.unit, action.slot, u.cell, structCells);
    if (std::find(structCells.begin(), structCells.end(), action.cell) != structCells.end()) return true;
    int minR = 1, maxR = 1;
    staffRange(u, item, minR, maxR);
    const int dist = grid.distance(u.cell, action.cell);
    if (dist < minR || dist > maxR) return refuse(why, "that square is out of range");
    return refuse(why, "nothing there is hidden from you");
  }

  std::vector<int> legal;
  staffTargets(action.unit, action.slot, u.cell, legal);
  int center = action.target;
  if (!d.needsPick()) {
    if (legal.empty()) return refuse(why, staffNobodyWhy(d));
    center = d.target == StaffTarget::Self ? action.unit : -1;  // (a staff of "self" that warps or repairs still needs its second pick)
  } else if (std::find(legal.begin(), legal.end(), action.target) == legal.end()) {
    const std::string reason = staffTargetWhy(action.unit, action.slot, action.target, u.cell);
    return refuse(why, reason.empty() ? "nobody in reach needs that" : reason);
  }
  if (center < 0) return true;
  // The second pick of a warp (the square) and of a repair (the item).
  if (d.has(StaffEffect::Kind::Warp)) {
    std::vector<Cell> structCells;
    staffWarpCells(action.unit, action.slot, center, u.cell, structCells);
    // With no square to send it to the warp part is skipped (the staff's other effects still work), so none is needed.
    if (!structCells.empty() && !grid.valid(action.cell)) return refuse(why, "pick the square to warp it to");
    if (!structCells.empty() && std::find(structCells.begin(), structCells.end(), action.cell) == structCells.end())
      return refuse(why, "it cannot be warped to that square (too far, not free, or it could not stand there)");
  }
  if (d.has(StaffEffect::Kind::Repair)) {
    std::vector<int> mendable;
    staffRepairSlots(action.unit, action.slot, center, mendable);
    if (!mendable.empty() && action.itemSlot < 0) return refuse(why, "pick the item to repair");
    if (action.itemSlot >= 0 && std::find(mendable.begin(), mendable.end(), action.itemSlot) == mendable.end())
      return refuse(why, "that item cannot be repaired");
  }
  return true;
}

// The use is spent (the staff may break: the slot is gone after this), then what the user earns, then the order ends.
void Battle::finishStaffUse(int uid, int slot, const StaffDef& d, int center, std::vector<Event>& events, int healedTotal, bool allMissed) {
  Unit& user = units[(size_t)uid];
  const Unit* centerUnit = center >= 0 ? &units[(size_t)center] : nullptr;
  // "xpMiss" instead of "xp" when the staff missed everyone (a staff without it earns the same either way).
  const bool useMiss = allMissed && (d.xpMiss >= 0 || d.xpMissExpr >= 0);
  const int exprIndex = useMiss ? d.xpMissExpr : d.xpExpr;
  const int xp = exprIndex >= 0 ? (int)std::max(0.0, std::min<double>(kMaxStaffXp, std::floor(staffEval(exprIndex, user, centerUnit, d, kNoCell, 0)))) : useMiss ? d.xpMiss : d.xp;
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
  skillsStaffUse(uid, d.gear.group, center, healedTotal, events);  // docs/SRPG.md "A staff used"
}

void Battle::applyStaffLight(const Action& action, std::vector<Event>& events) {
  const int uid = action.unit;
  const int slot = action.slot;
  const int item = units[(size_t)uid].items[(size_t)slot].item;
  const StaffDef& d = db->staves[(size_t)db->items[(size_t)item].staff];
  const StaffEffect& fx = *d.find(StaffEffect::Kind::Light);
  const int team = teamOf(units[(size_t)uid]);
  std::vector<Cell> area;
  grid.within(action.cell, fx.radius, area);
  int fresh = 0;
  for (Cell c : area) fresh += !visibleTo(team, c);
  events.push_back(staffEvent(EventType::StaffUsed, uid, item, -1, fresh));
  markSight();  // the events so far describe the world as it was; the light changes who sees what
  LightArea light;
  light.side = units[(size_t)uid].side;
  light.cell = action.cell;
  light.radius = fx.radius;
  light.turns = fx.turns;
  light.shrink = fx.shrink;
  lights.push_back(light);
  events.push_back(staffEvent(EventType::AreaLit, uid, action.cell, fx.radius, fx.turns));
  finishStaffUse(uid, slot, d, -1, events);
}

void Battle::applyStaff(const Action& action, std::vector<Event>& events) {
  const int uid = action.unit;
  const int slot = action.slot;
  const int item = units[(size_t)uid].items[(size_t)slot].item;
  const StaffDef& d = db->staves[(size_t)db->items[(size_t)item].staff];  // validated
  if (d.has(StaffEffect::Kind::Unlock)) {
    applyStaffUnlock(action, events);
    return;
  }
  if (d.picksSquare()) {
    applyStaffLight(action, events);
    return;
  }
  const int center = d.needsPick() ? action.target : (d.target == StaffTarget::Self ? uid : -1);

  std::vector<int> affected;
  staffAffected(units[(size_t)uid], item, slot, center, units[(size_t)uid].cell, affected);

  // What would move, worked out on the truth before anything changes. A warp whose square holds an enemy the team never saw
  // is blocked: the use is paid for, the other effects still happen, the unit stays where it was.
  AbilityPlan plan;
  bool blocked = false;
  if (d.movesUnit() && !affected.empty()) {
    const Unit& user = units[(size_t)uid];
    const Unit& t = units[(size_t)center];
    // An effect that cannot work on this unit (only possible next to a heal that can) is skipped.
    if (d.has(StaffEffect::Kind::Warp)) {
      if (grid.valid(action.cell) && occupant[(size_t)action.cell] >= 0 && occupant[(size_t)action.cell] != uid) {
        plan.blocker = occupant[(size_t)action.cell];
        blocked = true;
      } else if (grid.valid(action.cell)) {
        plan.moves = {{center, action.cell}};
      }
    } else if (d.has(StaffEffect::Kind::Pull)) {
      const Cell to = staffPullCell(uid, center, user.cell);
      if (to != kNoCell) plan.moves = {{center, to}};
    } else if (center != uid && staffCanStand(user, t.cell) && staffCanStand(t, user.cell)) {
      plan.moves = {{center, user.cell}, {uid, t.cell}};  // the target first: the events and "reach" follow that order
    }
  }
  Event used = staffEvent(EventType::StaffUsed, uid, item, center, (int)affected.size());
  if (blocked) {
    used.flags = kBlocked;
    used.data.push_back(plan.blocker);
  }
  events.push_back(used);

  std::vector<int> healed;
  int healedTotal = 0;  // everything the staff restored, for the user's "staffUse" blocks (`healed`)
  bool moves = false;
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
    if (total > 0) {
      t.hp += total;
      events.push_back(staffEvent(EventType::UnitHealed, id, total));
      healedTotal += total;
      if (id != uid) healed.push_back(id);
    }
    if (const StaffEffect* fx = d.find(StaffEffect::Kind::Repair)) {
      if (action.itemSlot >= 0 && action.itemSlot < (int)t.items.size()) {
        ItemStack& st = t.items[(size_t)action.itemSlot];
        const int top = db->items[(size_t)st.item].uses;
        const int amount = std::min(top - st.uses, fx->amount == StaffEffect::Amount::Full ? top : fx->value);
        if (amount > 0) {
          st.uses += amount;
          events.push_back(staffEvent(EventType::ItemRepaired, id, st.item, amount, st.uses));
        }
      }
    }
    // Conditions (docs/SRPG.md section 9): after heal and repair, before the move; a cure first, then an inflict (so "cure all and
    // inflict X" ends with X). An inflict that would change nothing (the unit has it for as long already) is skipped.
    if (const StaffEffect* fx = d.find(StaffEffect::Kind::Cure)) cureConditions(id, fx->conds, fx->cureAll, kConditionCured, events);
    if (const StaffEffect* fx = d.find(StaffEffect::Kind::Inflict))
      if (inflictWhy(t, fx->conds[0], fx->turns) != 2) inflictCondition(id, fx->conds[0], fx->turns, uid, events);
    moves = true;
  }
  if (moves) doAbilityMoves(plan, events);  // a miss leaves `moves` false for the one unit: nothing moves
  if (!moves) plan.moves.clear();

  finishStaffUse(uid, slot, d, center, events, healedTotal, d.canMiss() && !moves && !affected.empty());
  // Supports earn after the order ends: their story events may add units (items do the same).
  for (int id : healed) supportsHealed(uid, id, events);
  // A moved unit has stopped somewhere: events waiting for a unit to arrive on that square may run (as after an ability or a
  // drop). The same units' objectives ("reach a square") are judged by settle() after every action.
  for (const AbilityMove& m : plan.moves) {
    StoryOccurrence o;
    o.kind = Trigger::Kind::Reach;
    o.unit = m.unit;
    o.cell = m.to;
    fireStory(o, events);
  }
}

}  // namespace ote
