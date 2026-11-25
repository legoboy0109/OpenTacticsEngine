// The wording of ability_text.h. (Named unit_info_* so the unit tests' source glob picks it up.)
#include <algorithm>
#include <cctype>

#include "battle/ability_text.h"

namespace ote {
namespace {

std::string num(int v) { return std::to_string(v); }
std::string plural(int n, const char* one, const char* many) { return num(n) + " " + (n == 1 ? one : many); }
std::string join(const std::string& t, const std::string& more) { return t.empty() ? more : t + ", " + more; }

// "Sleep for 3 turns (60%)": the condition an ability puts on, its turns and its chance (docs/SRPG.md "Ability conditions"); "by a formula" when the
// chance depends on the two units. A list of conditions reads "Protect for 3 turns and Shell for 3 turns" (docs/SRPG.md "Areas and several effects
// for abilities").
std::string oneInflictText(const Database& db, const AbilityInflict& in, bool withChance) {
  std::string t = in.cond >= 0 && (size_t)in.cond < db.conditions.size() ? db.conditions[(size_t)in.cond].name : "a condition";
  if (in.turns > 0) t += " for " + plural(in.turns, "turn", "turns");
  if (!withChance) return t;  // (the pick hint names the chance against the highlighted target itself)
  if (in.chanceExpr >= 0) t += " (a chance)";
  else if (in.chance > 0) t += " (" + num(in.chance) + "%)";
  return t;
}

std::string inflictText(const Database& db, const AbilityDef& def, bool withChance) {
  std::string t;
  for (size_t k = 0; k < def.inflicts.size(); ++k)
    t += (k == 0 ? "" : k + 1 == def.inflicts.size() ? " and " : ", ") + oneInflictText(db, def.inflicts[k], withChance);
  return t.empty() ? "a condition" : t;
}

std::string cureText(const Database& db, const AbilityDef& def) {
  if (def.cureAll) return "Cures every condition";
  if (def.cureKind == 1) return "Cures every harmful condition";
  if (def.cureKind == 2) return "Takes away every helpful condition";
  std::string t;
  for (int c : def.cures)
    if (c >= 0 && (size_t)c < db.conditions.size()) t = join(t, db.conditions[(size_t)c].name);
  return "Cures " + t;
}

// "radius 2", "a burst", "a cross of 2", "a line of 3", "a ring of 1", "the whole map": the shape of an ability's area (placeholder wording).
std::string areaShapeText(const AreaDef& a) {
  switch (a.shape) {
    case AreaShape::Burst: return "a burst";
    case AreaShape::Radius: return "radius " + num(a.radius);
    case AreaShape::Cross: return "a cross of " + num(a.radius);
    case AreaShape::Line: return "a line of " + num(a.length);
    case AreaShape::Ring: return "a ring of " + num(a.radius);
    case AreaShape::Field: return "the whole map";
    case AreaShape::None: break;
  }
  return "";
}

std::string effectOnly(const Database& db, const AbilityDef& def, bool withChance);

}  // namespace

std::string abilityEffectText(const Database& db, const AbilityDef& def, bool withChance) {
  std::string t = effectOnly(db, def, withChance);
  if (!def.inflicts.empty() && def.effect != AbilityEffect::None) t += "; inflicts " + inflictText(db, def, withChance);  // a condition on top of the effect (a strike that poisons)
  if (def.area.on()) t += " (area: " + areaShapeText(def.area) + ")";  // docs/SRPG.md "Areas and several effects for abilities"
  return t;
}

namespace {

std::string effectOnly(const Database& db, const AbilityDef& def, bool withChance) {
  switch (def.effect) {
    case AbilityEffect::Push: return "Pushes it " + plural(def.amount, "square", "squares") + " away";
    case AbilityEffect::Pull: return "Pulls it next to you";
    case AbilityEffect::Swap: return "Swaps places with it";
    case AbilityEffect::MoveBehind: return "Moves it behind you";
    case AbilityEffect::Heal: {
      std::string t = def.healPercent > 0 ? "Heals " + num(def.healPercent) + "% of its HP"
                      : def.healExpr >= 0 ? "Heals HP"  // (by a formula: the amount depends on both units)
                      : def.amount >= kHealFull ? "Heals all HP" : "Heals " + num(def.amount) + " HP";
      if (!def.healCures) return t;
      std::string c = cureText(db, def);  // "Heals 30 HP and cures Poison, Blind"
      c[0] = (char)std::tolower((unsigned char)c[0]);
      return t + " and " + c;
    }
    case AbilityEffect::Revive:  // docs/SRPG.md "Downed units and revive" (placeholder words)
      return def.healPercent > 0 ? "Revives a downed unit with " + num(def.healPercent) + "% of its HP"
             : def.healExpr >= 0 ? "Revives a downed unit"
             : def.amount >= kHealFull ? "Revives a downed unit with all its HP" : "Revives a downed unit with " + num(def.amount) + " HP";
    case AbilityEffect::Strike: {  // docs/SRPG.md "Abilities that strike"
      const WeaponDef* w = def.strike >= 0 && (size_t)def.strike < db.weapons.size() ? &db.weapons[(size_t)def.strike] : nullptr;
      if (!w) return "Strikes";
      std::string t = "Strikes with " + w->name;
      if (w->damageType >= 0 && (size_t)w->damageType < db.damageTypes.list.size()) t += " (" + db.damageTypes.list[(size_t)w->damageType].name + ")";
      return w->area.on() ? t + ", hits an area" : t;
    }
    case AbilityEffect::Unlock: {
      std::string t = def.unlockDoors && def.unlockChests ? "Opens a door or chest" : def.unlockDoors ? "Opens a door" : "Opens a chest";
      if (def.unlockNeeds >= 0 && (size_t)def.unlockNeeds < db.items.size()) t += ", wears a " + db.items[(size_t)def.unlockNeeds].name;
      return t;
    }
    case AbilityEffect::Steal:
      return def.stealWeapons ? (def.stealEquipped ? "Takes any item it carries" : "Takes an item it carries, weapons too (not the one in hand)")
                              : "Takes an item it carries (not weapons)";
    case AbilityEffect::Refresh: return def.refreshPerTurn > 1 ? "Lets it act again (up to " + num(def.refreshPerTurn) + " times a turn)" : "Lets it act again";
    case AbilityEffect::MovePast: return "Moves you to the far side of it";
    case AbilityEffect::Cure: return cureText(db, def);
    case AbilityEffect::None: return "Inflicts " + inflictText(db, def, withChance);  // only "inflict": the condition is the whole doing
    case AbilityEffect::Summon: {
      const SummonDef& s = def.summon;
      std::string name = s.type >= 0 && (size_t)s.type < db.unitTypes.size() ? db.unitTypes[(size_t)s.type].name : s.typeId;
      std::string t = "Calls a " + name + " (" + (s.level > 0 ? "level " + num(s.level) : std::string("your level")) +
                      ", up to " + num(s.limit) + " at a time";
      if (s.replace) t += ", the oldest goes when there are too many";
      if (s.lifetime > 0) t += ", lasts " + plural(s.lifetime, "turn", "turns");
      if (!s.canAct) t += ", waits a turn";
      return t + ")";
    }
  }
  return "";
}

}  // namespace

std::string abilityTargetText(const AbilityDef& def) {
  if (def.area.on()) {  // the square it is aimed at, and who in the area it acts on
    switch (def.target) {
      case AbilityTarget::Ally: return "a square (allies in the area)";
      case AbilityTarget::Enemy: return "a square (enemies in the area)";
      default: return "a square (everyone in the area)";
    }
  }
  switch (def.target) {
    case AbilityTarget::Self: return "yourself";
    case AbilityTarget::Ally: return "an ally";
    case AbilityTarget::Enemy: return "an enemy";
    case AbilityTarget::Unit: return "any unit";
    case AbilityTarget::Cell: return "a square";
  }
  return "";
}

std::string abilityRangeText(const AbilityDef& def) {
  if (def.target == AbilityTarget::Self) return "";
  return def.minRange == def.maxRange ? "range " + num(def.minRange) : "range " + num(def.minRange) + "-" + num(def.maxRange);
}

std::string abilityCostText(const AbilityDef& def) { return def.free ? "free" : "ends the turn"; }

std::string abilityLimitText(const AbilityDef& def) {
  std::string t;
  if (def.uses > 0) t = plural(def.uses, "use", "uses") + " per battle";
  if (def.cooldown > 0) t = join(t, "ready again after " + plural(def.cooldown, "turn", "turns"));
  if (!def.afterMove) t = join(t, "only before moving");
  return t;
}

std::string abilityStatusText(const Battle& b, const Unit& u, int ability) {
  const AbilityDef& def = b.db->abilities[(size_t)ability];
  const AbilityUse* use = b.abilityUseOf(u, ability);
  std::string t;
  if (def.uses > 0) t = num(std::max(0, def.uses - (use ? use->used : 0))) + " of " + num(def.uses) + " uses left";
  if (use && use->wait > 0) t = join(t, "ready in " + plural(use->wait, "turn", "turns"));
  else if (use && use->turn) t = join(t, "used this turn");
  return t;
}

std::string stealReasonText(const std::string& why, bool shortForm) {
  if (why == "weapons cannot be stolen") return shortForm ? "no weapons" : "weapons cannot be taken";
  if (why == "the equipped weapon") return shortForm ? "in hand" : "the weapon in hand";
  if (why == "cannot be stolen") return shortForm ? "not takeable" : "cannot be taken";
  return why;
}

std::string abilityShortReason(const std::string& reason) {
  static const char* const kPairs[][2] = {
      {"can only be used before moving", "before moving"}, {"no uses left this battle", "no uses left"},
      {"already used this turn", "used this turn"},        {"cannot be used now", "not now"},
      {"nothing to target", "no target"}};
  for (const auto& p : kPairs)
    if (reason == p[0]) return p[1];
  if (reason.compare(0, 12, "already has ") == 0) return "limit reached";  // "already has 2 summoned Hounds"
  if (reason.compare(0, 13, "stand on the ") == 0) return "stand on it";  // an unlock ability of a "from": "on" door or chest
  if (reason.compare(0, 13, "step off the ") == 0) return "step off it";
  return reason;
}

std::string abilityMenuExtra(const Battle& b, const Unit& u, const AbilityOption& o) {
  if (!o.ok) return o.why;
  const AbilityDef& def = b.db->abilities[(size_t)o.ability];
  // "spendsItem": the uses the giving item has left count too; the smaller number is what is left.
  int left = -1;
  if (def.uses > 0) {
    const AbilityUse* use = b.abilityUseOf(u, o.ability);
    left = std::max(0, def.uses - (use ? use->used : 0));
  }
  if (o.itemUses >= 0) left = left < 0 ? o.itemUses : std::min(left, o.itemUses);
  std::string out = abilityPriceText(*b.db, def);  // "6 MP" (docs/SRPG.md "Unit resources")
  if (left >= 0) out += (out.empty() ? "" : "  ") + num(left) + AbilityWords::kLeft;
  return out;
}

std::string abilityPriceText(const Database& db, const AbilityDef& def) {
  std::string out;
  for (const AbilitySpend& s : def.spend)
    out += (out.empty() ? "" : " ") + num(s.amount) + " " + (s.unit ? db.unitResources[(size_t)s.index].name : db.resources[(size_t)s.index].name);
  return out;
}

}  // namespace ote
