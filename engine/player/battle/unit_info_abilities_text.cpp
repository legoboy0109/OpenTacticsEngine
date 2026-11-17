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
    case AbilityEffect::Summon: {
      const SummonDef& s = def.summon;
      std::string name = s.type >= 0 && (size_t)s.type < db.unitTypes.size() ? db.unitTypes[(size_t)s.type].name : s.typeId;
      std::string t = "Calls a " + name + " (" + (s.level > 0 ? "level " + num(s.level) : std::string("your level")) +
                      ", up to " + num(s.limit) + " at a time";
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

std::string abilityShortReason(const std::string& reason) {
  static const char* const kPairs[][2] = {
      {"can only be used before moving", "before moving"}, {"no uses left this battle", "no uses left"},
      {"already used this turn", "used this turn"},        {"cannot be used now", "not now"},
      {"nothing to target", "no target"}};
  for (const auto& p : kPairs)
    if (reason == p[0]) return p[1];
  if (reason.compare(0, 12, "already has ") == 0) return "limit reached";  // "already has 2 summoned Hounds"
  return reason;
}

std::string abilityMenuExtra(const Battle& b, const Unit& u, const AbilityOption& o) {
  if (!o.ok) return o.why;
  const AbilityDef& def = b.db->abilities[(size_t)o.ability];
  if (def.uses <= 0) return "";
  const AbilityUse* use = b.abilityUseOf(u, o.ability);
  return num(std::max(0, def.uses - (use ? use->used : 0))) + AbilityWords::kLeft;
}

}  // namespace ote
