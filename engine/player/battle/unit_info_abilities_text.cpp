// The wording of ability_text.h. (Named unit_info_* so the unit tests' source glob picks it up.)
#include <algorithm>

#include "battle/ability_text.h"

namespace ote {
namespace {

std::string num(int v) { return std::to_string(v); }
std::string plural(int n, const char* one, const char* many) { return num(n) + " " + (n == 1 ? one : many); }
std::string join(const std::string& t, const std::string& more) { return t.empty() ? more : t + ", " + more; }

}  // namespace

std::string abilityEffectText(const Database& db, const AbilityDef& def) {
  switch (def.effect) {
    case AbilityEffect::Push: return "Pushes it " + plural(def.amount, "square", "squares") + " away";
    case AbilityEffect::Pull: return "Pulls it next to you";
    case AbilityEffect::Swap: return "Swaps places with it";
    case AbilityEffect::MoveBehind: return "Moves it behind you";
    case AbilityEffect::Heal:
      return def.amount >= kHealFull ? "Heals all HP" : "Heals " + num(def.amount) + " HP";
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
    case AbilityEffect::Cure: {
      if (def.cureAll) return "Cures every condition";
      std::string t;
      for (int c : def.cures)
        if (c >= 0 && (size_t)c < db.conditions.size()) t = join(t, db.conditions[(size_t)c].name);
      return "Cures " + t;
    }
    case AbilityEffect::None: {  // only "inflict": the condition is the whole doing
      const std::string name = def.inflictCond >= 0 && (size_t)def.inflictCond < db.conditions.size() ? db.conditions[(size_t)def.inflictCond].name : "a condition";
      return def.inflictTurns > 0 ? "Inflicts " + name + " for " + plural(def.inflictTurns, "turn", "turns") : "Inflicts " + name;
    }
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

std::string abilityTargetText(const AbilityDef& def) {
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
  return left < 0 ? "" : num(left) + AbilityWords::kLeft;
}

}  // namespace ote
