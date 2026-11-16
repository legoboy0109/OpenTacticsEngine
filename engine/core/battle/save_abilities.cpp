// Abilities and summons in saves (docs/SRPG.md "Menu abilities and summoning": Saves, events): the abilities a unit
// has used this battle, and what makes a unit a summon. A key is written only when it is not the default; a key in a
// game without abilities is refused. Every value is checked.
#include "core/battle/save_srpg.h"

namespace ote {
namespace savejson {

void writeUnitAbilities(const Battle&, const Unit& u, Json& j) {
  if (!u.abilityUses.empty()) {
    Json list = Json::makeArray();
    for (const AbilityUse& use : u.abilityUses) {
      Json e = Json::makeObject();
      e.set("ability", Json::makeNumber(use.ability));
      e.set("used", Json::makeNumber(use.used));
      e.set("wait", Json::makeNumber(use.wait));
      if (use.turn) e.set("turn", Json::makeBool(true));
      list.push(e);
    }
    j.set("abilities", list);
  }
  if (u.summoner >= 0) {
    Json s = Json::makeObject();
    s.set("by", Json::makeNumber(u.summoner));
    s.set("ability", Json::makeNumber(u.summonAbility));
    if (u.summonLeft >= 0) s.set("left", Json::makeNumber(u.summonLeft));
    j.set("summon", s);
  }
}

bool readUnitAbilities(Reader& rd, const Battle& b, const Json& j, const std::string& where, Unit& u) {
  const Database& db = *b.db;
  u.abilityUses.clear();
  u.summoner = u.summonAbility = u.summonLeft = -1;
  for (const char* key : {"abilities", "summon"})
    if (j.has(key) && db.abilities.empty()) return rd.fail(where + "." + key, "this game has no abilities");

  if (j.has("abilities")) {
    const Json* list;
    if (!rd.array(j, "abilities", where, -1, (int)db.abilities.size(), list)) return false;
    for (size_t i = 0; i < list->size(); ++i) {
      const Json& e = (*list)[i];
      const std::string at = where + ".abilities[" + std::to_string(i) + "]";
      if (!e.isObject()) return rd.fail(at, "not an object");
      AbilityUse use;
      if (!rd.integer(e, "ability", at, 0, (int)db.abilities.size() - 1, use.ability) ||
          !rd.integer(e, "used", at, 1, 999, use.used) || !rd.integer(e, "wait", at, 0, 99, use.wait))
        return false;
      if (e.has("turn") && !rd.boolean(e, "turn", at, use.turn)) return false;
      for (const AbilityUse& o : u.abilityUses)
        if (o.ability == use.ability) return rd.fail(at + ".ability", "ability " + std::to_string(use.ability) + " is listed twice");
      u.abilityUses.push_back(use);
    }
  }

  if (j.has("summon")) {
    const Json* s;
    const std::string at = where + ".summon";
    if (!rd.object(j, "summon", where, s)) return false;
    // `by` must name a unit with a lower id: only known once the units are read (checkUnitsAbilities).
    if (!rd.integer(*s, "by", at, 0, kMaxUnits, u.summoner) ||
        !rd.integer(*s, "ability", at, 0, (int)db.abilities.size() - 1, u.summonAbility))
      return false;
    const AbilityDef& def = db.abilities[(size_t)u.summonAbility];
    if (def.effect != AbilityEffect::Summon) return rd.fail(at + ".ability", "that ability does not summon");
    if (s->has("left")) {
      if (def.summon.lifetime <= 0) return rd.fail(at + ".left", "that ability's summons have no lifetime");
      if (!rd.integer(*s, "left", at, 0, 99, u.summonLeft)) return false;
    } else if (def.summon.lifetime > 0) {
      return rd.fail(at + ".left", "missing or not a number (that ability's summons have a lifetime)");
    }
  }
  return true;
}

bool checkUnitsAbilities(Reader& rd, const Battle& b) {
  for (const Unit& u : b.units)
    if (u.summoner >= 0 && u.summoner >= u.id)
      return rd.fail("units[" + std::to_string(u.id) + "].summon.by", "must be a unit with a lower id than " + std::to_string(u.id));
  return true;
}

}  // namespace savejson
}  // namespace ote
