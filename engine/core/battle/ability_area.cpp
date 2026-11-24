// Abilities with an area, and what a heal / cure / inflict ability does to one unit (ability_area_api.h; docs/SRPG.md "Areas and several effects for
// abilities"). An area ability aims at a square in range; every unit in its area that the ability's "target" accepts and that the ability would
// change is acted on, in unit-id order: the heal, then the cure, then each "inflict" entry in list order (each with its own roll). Like an area
// attack it may be aimed only where at least one such unit is one the user's team sees; units in fog are acted on all the same.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

namespace {

bool refuse(std::string* why, const std::string& reason) {
  if (why) *why = reason;
  return false;
}

bool curesSomething(const AbilityDef& d, const Unit& t) {
  for (const ConditionState& cs : t.conditions)
    if (d.cureAll || std::find(d.cures.begin(), d.cures.end(), cs.condition) != d.cures.end()) return true;
  return false;
}

}  // namespace

bool Battle::abilityActsOn(const Unit& u, const AbilityDef& d, Cell from, const Unit& t, std::string* why) const {
  switch (d.effect) {
    case AbilityEffect::Heal: {
      const bool hurt = t.hp < maxHp(t);
      if (!d.healCures) return hurt ? true : refuse(why, "that unit is at full health");
      // Heal and cure: either part acting is enough.
      return hurt || curesSomething(d, t) ? true : refuse(why, t.id == u.id ? "nothing to heal or cure" : "that unit needs neither healing nor a cure");
    }
    case AbilityEffect::None: {  // only "inflict": at least one entry must be able to take hold (not immune, not had already for as long)
      bool immuneToAll = true;
      for (const AbilityInflict& in : d.inflicts) {
        const int w = inflictWhy(t, in.cond, in.turns);
        if (w == 0) return true;
        immuneToAll = immuneToAll && w == 1;
      }
      if (d.inflicts.empty()) return true;
      return refuse(why, immuneToAll ? "that unit is immune" : d.inflicts.size() > 1 ? "that unit already has them" : "that unit already has it");
    }
    case AbilityEffect::Steal:
    case AbilityEffect::Refresh:
    case AbilityEffect::Cure:
      return abilityEffectOk(u, d, from, t, why);
    default: return true;
  }
}

void Battle::abilityAreaUnits(const Unit& u, const AbilityDef& d, Cell from, Cell target, bool seenOnly, std::vector<int>& out,
                              std::vector<Cell>* covered) const {
  out.clear();
  std::vector<Cell> cells;
  areaCoverOf(d.area, from, target, cells);
  const int team = teamOf(u);
  for (Cell c : cells) {
    // The user stands on `from` when it is planned from another square (the AI, the menus of a move not made yet).
    int id = c == from ? u.id : occupant[(size_t)c];
    if (id == u.id && c != from) id = -1;
    if (id < 0) continue;
    const Unit& t = units[(size_t)id];
    if (!t.alive) continue;
    const bool ally = teamOf(t) == team;
    if ((d.target == AbilityTarget::Ally && !ally) || (d.target == AbilityTarget::Enemy && ally)) continue;
    if (seenOnly && !unitVisibleTo(team, t)) continue;
    if (!abilityActsOn(u, d, from, t, nullptr)) continue;
    out.push_back(id);
  }
  std::sort(out.begin(), out.end());  // unit-id order: the order it acts in
  if (covered) *covered = cells;
}

bool Battle::areaAbilityTargetOk(const Unit& u, int ability, Cell from, Cell cell, std::string* why) const {
  const AbilityDef& d = db->abilities[(size_t)ability];
  if (!grid.valid(cell)) return refuse(why, "that square is not on the map");
  const int dist = grid.distance(from, cell);
  if (dist < d.minRange || dist > d.maxRange) return refuse(why, "that square is out of range");
  std::vector<int> who;
  abilityAreaUnits(u, d, from, cell, true, who);
  return who.empty() ? refuse(why, "nobody there would be affected") : true;
}

bool Battle::abilityAreaForecast(int user, int ability, Cell from, Cell target, AbilityAreaForecast& out) const {
  out = AbilityAreaForecast();
  if (user < 0 || user >= (int)units.size() || ability < 0 || ability >= (int)db->abilities.size() || !grid.valid(from)) return false;
  const AbilityDef& d = db->abilities[(size_t)ability];
  const Unit& u = units[(size_t)user];
  if (!d.area.on() || !u.alive || !areaAbilityTargetOk(u, ability, from, target, nullptr)) return false;
  out.cell = target;
  std::vector<int> who;
  abilityAreaUnits(u, d, from, target, false, who, &out.cells);
  const int team = teamOf(u);
  for (int id : who) {
    const Unit& t = units[(size_t)id];
    AbilityAreaHit h;
    h.unit = id;
    h.seen = unitVisibleTo(team, t);
    if (d.effect == AbilityEffect::Heal) h.heal = std::max(0, std::min(abilityHealAmount(d, user, id), maxHp(t) - t.hp));
    if (d.effect == AbilityEffect::Cure || d.healCures)
      for (const ConditionState& cs : t.conditions)
        h.cures += d.cureAll || std::find(d.cures.begin(), d.cures.end(), cs.condition) != d.cures.end();
    for (const AbilityInflict& in : d.inflicts) h.inflicts += inflictWhy(t, in.cond, in.turns) == 0;
    out.hits.push_back(h);
  }
  return true;
}

void Battle::abilityHealCure(const AbilityDef& d, int user, int target, std::vector<Event>& events) {
  Unit& t = units[(size_t)target];
  if (d.effect == AbilityEffect::Heal) {
    const int amount = std::min(abilityHealAmount(d, user, target), maxHp(t) - t.hp);  // a number, "full", a percent or a formula
    if (amount > 0) {
      t.hp += amount;
      Event h;
      h.type = EventType::UnitHealed;
      h.a = target;
      h.b = amount;
      events.push_back(h);
      if (target != user) supportsHealed(user, target, events);  // supports: one healed the other
    }
  }
  if (d.effect == AbilityEffect::Cure || d.healCures) cureConditions(target, d.cures, d.cureAll, kConditionCured, events);
}

void Battle::applyAreaAbility(const Action& action, std::vector<Event>& events, bool resolving) {
  const int uid = action.unit;
  const int ability = action.ability;
  const AbilityDef& def = db->abilities[(size_t)ability];
  // The truth: units in fog are acted on as well (the use was judged by what the team sees; at least one of those is here).
  std::vector<int> who;
  abilityAreaUnits(units[(size_t)uid], def, units[(size_t)uid].cell, action.cell, false, who);
  Event used;
  used.type = EventType::AbilityUsed;
  used.a = uid;
  used.b = ability;
  used.c = -1;
  used.d = action.cell;
  used.flags = kArea;
  used.data = who;
  events.push_back(used);
  for (int t : who) {
    if (!units[(size_t)t].alive) continue;
    abilityHealCure(def, uid, t, events);
    if (!def.inflicts.empty()) abilityInflict(def, uid, t, events);  // each entry in list order, each with its own roll
  }
  abilityExperience(def, uid, who.empty() ? -1 : who[0], events);  // "xp" once per use (b = the first unit acted on)
  if (!resolving) {  // a charged ability paid for these when it was declared
    noteAbilityUse(uid, ability);
    spendAbilityItem(uid, ability, events);
    payAbilityPrice(uid, ability, events);
    if (!def.free) endOrder(uid);
  }
}

}  // namespace ote
