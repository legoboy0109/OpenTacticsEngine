// Conditions in a battle (docs/SRPG.md section 9): what a unit has, taking hold and wearing off, the questions the rest of the
// rules ask ("may it act, move, use this staff"), and the start / end of a turn. A condition's blocks are the blocks of a hidden
// skill (ConditionDef::skill) that Battle::skillsOf adds to the unit's own while it has the condition, so stat changes, combat
// numbers, strike effects and "turnStart" blocks need no code of their own here. Berserk is conditions_berserk.cpp.
//
// Timing (one place, the doc says the same): at the start of each turn of a unit's side, units in id order, conditions in the order
// they were applied: (1) the early wear-off chance is rolled (a condition that goes is gone for the whole turn), (2) a counted
// condition has one fewer turn left; then the blocks run (ticks), a unit that cannot act is finished, a berserk unit takes its action.
// A counted condition lasts through the turn in which its count reached 0 and wears off when that turn ends.
#include <algorithm>

#include "core/battle/combat_mods.h"

namespace ote {

namespace {

Event cEvent(EventType t, int a, int b, int c = -1, int d = -1, int flags = 0) {
  Event e;
  e.type = t;
  e.a = a;
  e.b = b;
  e.c = c;
  e.d = d;
  e.flags = flags;
  return e;
}

}  // namespace

bool Battle::hasCondition(const Unit& u, int condition) const {
  for (const ConditionState& cs : u.conditions)
    if (cs.condition == condition) return true;
  return false;
}

int Battle::conditionLeft(const Unit& u, int condition) const {
  for (const ConditionState& cs : u.conditions)
    if (cs.condition == condition) return cs.left;
  return -1;
}

int Battle::conditionDuration(int condition, int turns) const {
  if (condition < 0 || (size_t)condition >= db->conditions.size()) return -1;
  const ConditionDef& def = db->conditions[(size_t)condition];
  if (def.until != ConditionUntil::Turns) return -1;
  return turns > 0 ? turns : def.turns;
}

void Battle::conditionSkills(const Unit& u, std::vector<int>& out) const {
  out.clear();
  for (const ConditionState& cs : u.conditions) out.push_back(db->conditions[(size_t)cs.condition].skill);
}

bool Battle::conditionImmune(const Unit& u, int condition) const {
  if (db->skills.empty()) return false;
  std::vector<const SkillEffect*> fx;
  standingEffects(u, false, fx);
  for (const SkillEffect* e : fx)
    if (e->kind == SkillEffect::Kind::Immune && std::find(e->conds.begin(), e->conds.end(), condition) != e->conds.end()) return true;
  return false;
}

// The standing effects of one condition's blocks that hold for the unit (conditions of a block are judged through the unit).
static bool blockEffects(const Battle& b, const Unit& u, const ConditionState& cs, std::vector<const SkillEffect*>& out) {
  out.clear();
  const bool judgeable = u.id >= 0 && u.id < (int)b.units.size();
  SkillCtx ctx;
  ctx.unit = u.id;
  for (const SkillBlock& blk : b.db->skills[(size_t)b.db->conditions[(size_t)cs.condition].skill].blocks) {
    if (blk.when != SkillWhen::Always) continue;
    if (!blk.conds.empty()) {
      if (!judgeable) continue;
      bool holds = true;
      for (const SkillCond& c : blk.conds) holds = holds && b.skillCondHolds(c, ctx);
      if (!holds) continue;
    }
    for (const SkillEffect& e : blk.effects) out.push_back(&e);
  }
  return !out.empty();
}

int Battle::conditionBlock(const Unit& u, ConditionBlock what) const {
  if (u.conditions.empty()) return -1;
  const SkillEffect::Kind want = what == ConditionBlock::Act ? SkillEffect::Kind::CannotAct
                                 : what == ConditionBlock::Move ? SkillEffect::Kind::CannotMove : SkillEffect::Kind::Berserk;
  std::vector<const SkillEffect*> fx;
  for (const ConditionState& cs : u.conditions) {
    if (!blockEffects(*this, u, cs, fx)) continue;
    for (const SkillEffect* e : fx)
      if (e->kind == want) return cs.condition;
  }
  return -1;
}

// What a unit that cannot act still does (the most restrictive of its conditions wins): answer a strike, dodge one.
bool Battle::mayCounterWhileStopped(const Unit& u) const { return stoppedMay(u, true); }
bool Battle::mayDodgeWhileStopped(const Unit& u) const { return stoppedMay(u, false); }

bool Battle::stoppedMay(const Unit& u, bool counter) const {
  if (u.conditions.empty()) return true;
  std::vector<const SkillEffect*> fx;
  for (const ConditionState& cs : u.conditions) {
    if (!blockEffects(*this, u, cs, fx)) continue;
    for (const SkillEffect* e : fx)
      if (e->kind == SkillEffect::Kind::CannotAct && !(counter ? e->counter : e->dodge)) return false;
  }
  return true;
}

bool Battle::unitControllable(const Unit& u, std::string* why) const {
  if (u.conditions.empty()) return true;
  int c = conditionBlock(u, ConditionBlock::Act);
  if (c >= 0) {
    if (why) *why = "that unit cannot act (" + db->conditions[(size_t)c].name + ")";
    return false;
  }
  c = conditionBlock(u, ConditionBlock::Berserk);
  if (c >= 0) {
    if (why) *why = "that unit acts on its own (" + db->conditions[(size_t)c].name + ")";
    return false;
  }
  return true;
}

int Battle::conditionForbidsGroup(const Unit& u, const std::string& group) const {
  if (u.conditions.empty() || group.empty()) return -1;
  std::vector<const SkillEffect*> fx;
  for (const ConditionState& cs : u.conditions) {
    if (!blockEffects(*this, u, cs, fx)) continue;
    for (const SkillEffect* e : fx)
      if (e->kind == SkillEffect::Kind::Forbid && std::find(e->names.begin(), e->names.end(), group) != e->names.end()) return cs.condition;
  }
  return -1;
}

int Battle::conditionForbidsKind(const Unit& u, ItemKind kind) const {
  if (u.conditions.empty()) return -1;
  const char* name = kind == ItemKind::Staff ? "staff" : kind == ItemKind::Usable ? "usable" : kind == ItemKind::Weapon ? "weapon" : "";
  if (!*name) return -1;
  std::vector<const SkillEffect*> fx;
  for (const ConditionState& cs : u.conditions) {
    if (!blockEffects(*this, u, cs, fx)) continue;
    for (const SkillEffect* e : fx)
      if (e->kind == SkillEffect::Kind::Forbid && std::find(e->kinds.begin(), e->kinds.end(), name) != e->kinds.end()) return cs.condition;
  }
  return -1;
}

int Battle::inflictWhy(const Unit& u, int condition, int turns) const {
  if (condition < 0 || (size_t)condition >= db->conditions.size()) return 1;
  if (conditionImmune(u, condition)) return 1;
  const int left = conditionDuration(condition, turns);
  for (const ConditionState& cs : u.conditions)
    if (cs.condition == condition) return (cs.left >= 0 && left > cs.left) ? 0 : 2;
  return 0;
}

bool Battle::cureWould(const Unit& u, const std::vector<int>& ids, bool all) const {
  for (const ConditionState& cs : u.conditions)
    if (all || std::find(ids.begin(), ids.end(), cs.condition) != ids.end()) return true;
  return false;
}

// ---- taking hold and going ----

bool Battle::inflictCondition(int unit, int condition, int turns, int source, std::vector<Event>& events, bool* resisted) {
  if (resisted) *resisted = false;
  if (unit < 0 || unit >= (int)units.size() || condition < 0 || (size_t)condition >= db->conditions.size()) return false;
  Unit& u = units[(size_t)unit];
  if (!u.alive) return false;
  if (conditionImmune(u, condition)) {
    events.push_back(cEvent(EventType::ConditionResisted, unit, condition, source));
    if (resisted) *resisted = true;
    return false;
  }
  const ConditionDef& def = db->conditions[(size_t)condition];
  for (int r : def.replaces) cureConditions(unit, {r}, false, kConditionReplaced, events);
  const int left = conditionDuration(condition, turns);
  bool refreshed = false;
  for (ConditionState& cs : u.conditions)
    if (cs.condition == condition) {
      cs.left = left;  // the duration starts over (never kept: a longer old one is cut too, one rule)
      refreshed = true;
    }
  if (!refreshed) {
    ConditionState cs;
    cs.condition = condition;
    cs.left = left;
    u.conditions.push_back(cs);
  }
  events.push_back(cEvent(EventType::ConditionApplied, unit, condition, left, source, refreshed ? kRefreshed : 0));
  // A unit that cannot act loses what is left of its own turn.
  if (u.side == activeSide && u.cell != kNoCell && conditionBlock(u, ConditionBlock::Act) == condition) u.done = true;
  clampHp(u);
  return true;
}

int Battle::cureConditions(int unit, const std::vector<int>& ids, bool all, int reason, std::vector<Event>& events) {
  if (unit < 0 || unit >= (int)units.size()) return 0;
  Unit& u = units[(size_t)unit];
  int removed = 0;
  for (size_t i = 0; i < u.conditions.size();) {
    const int c = u.conditions[i].condition;
    if (!all && std::find(ids.begin(), ids.end(), c) == ids.end()) {
      ++i;
      continue;
    }
    u.conditions.erase(u.conditions.begin() + (long)i);
    events.push_back(cEvent(EventType::ConditionEnded, unit, c, reason));
    ++removed;
  }
  if (removed) clampHp(u);
  return removed;
}

// ---- the turn ----

void Battle::conditionsTurnStart(std::vector<Event>& events) {
  if (db->conditions.empty()) return;
  for (size_t i = 0; i < units.size(); ++i) {
    if (units[i].side != activeSide || !units[i].alive || units[i].cell == kNoCell || units[i].conditions.empty()) continue;
    for (size_t k = 0; k < units[i].conditions.size();) {
      const int c = units[i].conditions[k].condition;
      if (db->conditions[(size_t)c].wearOff > 0 && rollActivation(rng, db->conditions[(size_t)c].wearOff)) {
        cureConditions((int)i, {c}, false, kConditionWoreOff, events);  // erases entry k: look at the one that moved up
        continue;
      }
      if (units[i].conditions[k].left > 0) --units[i].conditions[k].left;
      ++k;
    }
  }
}

void Battle::conditionsPrevent(std::vector<Event>& events) {
  if (db->conditions.empty()) return;
  for (size_t i = 0; i < units.size(); ++i) {
    Unit& u = units[i];
    if (u.side != activeSide || !u.alive || u.cell == kNoCell || u.conditions.empty()) continue;
    const int c = conditionBlock(u, ConditionBlock::Act);
    if (c < 0) continue;
    u.done = true;  // the AI and the player's menus both skip a finished unit
    u.moved = true;
    events.push_back(cEvent(EventType::ConditionTick, (int)i, c, kTickCannotAct));
  }
}

void Battle::conditionsTurnEnd(std::vector<Event>& events) {
  if (db->conditions.empty()) return;
  for (size_t i = 0; i < units.size(); ++i) {
    if (units[i].side != activeSide || !exists(units[i])) continue;
    for (size_t k = 0; k < units[i].conditions.size();) {
      if (units[i].conditions[k].left == 0) {
        const int c = units[i].conditions[k].condition;
        cureConditions((int)i, {c}, false, kConditionRanOut, events);
        continue;
      }
      ++k;
    }
  }
}

void Battle::conditionsStruck(int unit, std::vector<Event>& events) {
  Unit& u = units[(size_t)unit];
  if (!u.alive || u.conditions.empty()) return;
  std::vector<int> ending;
  for (const ConditionState& cs : u.conditions)
    if (db->conditions[(size_t)cs.condition].endsWhenStruck) ending.push_back(cs.condition);
  if (!ending.empty()) cureConditions(unit, ending, false, kConditionStruck, events);
}

void Battle::conditionsSettleDeaths(std::vector<Event>& events) {
  if (condDeaths_.empty()) return;
  std::vector<int> dead;
  dead.swap(condDeaths_);
  // As after burning out of fuel: the story hears of each fall after the sweep (it may add or move units), then the win checks.
  for (int id : dead) unitDefeated(id, -1, events);
  settle(events);
}

}  // namespace ote
