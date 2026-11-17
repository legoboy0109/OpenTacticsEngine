// Loader of data/abilities.json and of the "abilities" key on unit types (docs/SRPG.md "Menu abilities and
// summoning": Data). Abilities load before weapons, items and unit types (they name abilities and get indices at
// once); what an ability itself names (a summoned unit type, the unit and terrain keys of its "if") is checked last,
// in resolveAbilityRefs.
#include "core/data/loader_events.h"
#include "core/data/loader_places.h"
#include "core/data/loader_srpg.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

bool onlyKeys(const LoadCtx& c, const Json& o, std::initializer_list<const char*> keys, const std::string& where) {
  for (const Json::Member& m : o.members()) {
    bool known = false;
    for (const char* k : keys) known = known || m.first == k;
    if (known) continue;
    std::string list;
    for (const char* k : keys) list += (list.empty() ? "" : ", ") + q(k);
    return c.fail(where, "unknown key " + q(m.first) + " (expected " + list + ")");
  }
  return true;
}

// "summon": { "type": "hound", "level": "summoner", "limit": 1, ... } (the unit type is resolved later).
bool readSummon(const LoadCtx& c, const Database& db, const Json& v, const std::string& where, SummonDef& out) {
  const std::string& at = where;
  if (!v.isObject()) return c.fail(at, "\"summon\" must be an object such as { \"type\": \"hound\" }");
  const std::string in = "\"summon\": ";
  for (const Json::Member& m : v.members()) {
    static const char* const keys[] = {"type", "level", "limit", "vanish", "lifetime", "canAct", "givesXp"};
    bool known = false;
    for (const char* k : keys) known = known || m.first == k;
    if (!known)
      return c.fail(at, in + "unknown key " + q(m.first) + " (expected \"type\", \"level\", \"limit\", \"vanish\", \"lifetime\", \"canAct\", \"givesXp\")");
  }
  if (!v.has("type") || !v["type"].isString() || v["type"].asString().empty())
    return c.fail(at, in + "\"type\" must be the id of a unit type");
  out.typeId = v["type"].asString();
  if (v.has("level")) {
    const Json& l = v["level"];
    if (l.isString() && l.asString() == "summoner") out.level = 0;
    else if (LoadCtx::isWhole(l) && l.asInt() >= 1 && l.asInt() <= db.rules.maxLevel) out.level = l.asInt();
    else return c.fail(at, in + "\"level\" must be \"summoner\" or a whole number from 1 to " + std::to_string(db.rules.maxLevel));
  }
  const auto range = [&](const char* key, int lo, int hi, int& dst) {
    if (!v.has(key)) return true;
    if (!LoadCtx::isWhole(v[key]) || v[key].asInt() < lo || v[key].asInt() > hi)
      return c.fail(at, in + q(key) + " must be from " + std::to_string(lo) + " to " + std::to_string(hi));
    dst = v[key].asInt();
    return true;
  };
  if (!range("limit", 1, 20, out.limit) || !range("lifetime", 0, 99, out.lifetime)) return false;
  if (!c.boolean(v, "vanish", at, out.vanish) || !c.boolean(v, "canAct", at, out.canAct) ||
      !c.boolean(v, "givesXp", at, out.givesXp))
    return false;
  return true;
}

// The one effect of an ability: exactly one of push, pull, swap, moveBehind, heal, summon.
bool readEffect(const LoadCtx& c, Database& db, const Json& e, const std::string& where, AbilityDef& a) {
  if (!e.isObject()) return c.fail(where, "\"effect\" must be an object such as { \"push\": 1 }");
  static const char* const keys[] = {"push", "pull", "swap", "moveBehind", "heal", "summon", "unlock", "steal", "refresh", "movePast", "cure"};
  int count = 0;
  for (const Json::Member& m : e.members()) {
    bool known = false;
    for (const char* k : keys) known = known || m.first == k;
    if (!known) return c.fail(where, "\"effect\": unknown key " + q(m.first) + " (expected push, pull, swap, moveBehind, heal, summon, unlock, steal, refresh, movePast, cure)");
    ++count;
  }
  if (count != 1) return c.fail(where, "\"effect\" needs exactly one of push, pull, swap, moveBehind, heal, summon, unlock, steal, refresh, movePast, cure");
  if (e.has("push")) {
    a.effect = AbilityEffect::Push;
    if (!LoadCtx::isWhole(e["push"]) || e["push"].asInt() < 1 || e["push"].asInt() > 5)
      return c.fail(where, "\"push\" must be a whole number from 1 to 5 (squares)");
    a.amount = e["push"].asInt();
  } else if (e.has("pull") || e.has("swap") || e.has("moveBehind") || e.has("movePast")) {
    const char* key = e.has("pull") ? "pull" : e.has("swap") ? "swap" : e.has("movePast") ? "movePast" : "moveBehind";
    a.effect = e.has("pull") ? AbilityEffect::Pull : e.has("swap") ? AbilityEffect::Swap
               : e.has("movePast") ? AbilityEffect::MovePast : AbilityEffect::MoveBehind;
    if (!e[key].isBool() || !e[key].asBool()) return c.fail(where, q(key) + " must be true");
  } else if (e.has("heal")) {
    a.effect = AbilityEffect::Heal;
    const Json& h = e["heal"];
    if (h.isString() && h.asString() == "full") a.amount = kHealFull;
    else if (LoadCtx::isWhole(h) && h.asInt() >= 1 && h.asInt() <= 999) a.amount = h.asInt();
    else return c.fail(where, "\"heal\" must be a whole number from 1 to 999 or \"full\"");
  } else if (e.has("steal")) {
    // { "steal": true } or { "steal": { "if": "a.spd > b.spd", "weapons": false, "equipped": false } }
    a.effect = AbilityEffect::Steal;
    const Json& v = e["steal"];
    if (v.isBool() && v.asBool()) return true;
    const std::string in = "\"steal\": ";
    if (!v.isObject()) return c.fail(where, in + "must be true or { \"if\": \"a.spd > b.spd\", \"weapons\": false, \"equipped\": false }");
    for (const Json::Member& m : v.members())
      if (m.first != "if" && m.first != "weapons" && m.first != "equipped")
        return c.fail(where, in + "unknown key " + q(m.first) + " (expected \"if\", \"weapons\", \"equipped\")");
    if (v.has("if")) {
      if (!v["if"].isString() || v["if"].asString().empty()) return c.fail(where, in + "\"if\" must be the text of a formula (a = the user, b = the foe)");
      a.stealIf = db.addExpr(v["if"].asString(), "data/abilities.json: " + where + ": \"steal\".\"if\"");
    }
    if (!c.boolean(v, "weapons", where, a.stealWeapons) || !c.boolean(v, "equipped", where, a.stealEquipped)) return false;
    if (a.stealEquipped && !a.stealWeapons) return c.fail(where, in + "\"equipped\": true needs \"weapons\": true (the equipped weapon is a weapon)");
  } else if (e.has("refresh")) {
    a.effect = AbilityEffect::Refresh;
    const Json& v = e["refresh"];
    if (v.isBool() && v.asBool()) return true;
    if (!v.isObject() || v.size() != 1 || !v.has("perTurn") || !LoadCtx::isWhole(v["perTurn"]) || v["perTurn"].asInt() < 1 || v["perTurn"].asInt() > 9)
      return c.fail(where, "\"refresh\" must be true or { \"perTurn\": 1 } (a whole number from 1 to 9)");
    a.refreshPerTurn = v["perTurn"].asInt();
  } else if (e.has("cure")) {
    a.effect = AbilityEffect::Cure;
    const Json& v = e["cure"];
    if (v.isString() && v.asString() == "all") {
      a.cureAll = true;
      return true;
    }
    if (!v.isArray() || v.size() == 0) return c.fail(where, "\"cure\" must be \"all\" or a list of condition ids");
    for (size_t i = 0; i < v.size(); ++i) {
      if (!v[i].isString() || v[i].asString().empty()) return c.fail(where, "\"cure\" must be \"all\" or a list of condition ids");
      a.cureIds.push_back(v[i].asString());
    }
  } else if (e.has("unlock")) {
    a.effect = AbilityEffect::Unlock;
    if (!readUnlockKinds(c, e["unlock"], where, true, a.unlockDoors, a.unlockChests, a.unlockNeedsId)) return false;
  } else {
    a.effect = AbilityEffect::Summon;
    if (!readSummon(c, db, e["summon"], where, a.summon)) return false;
  }
  return true;
}

bool readAbility(const LoadCtx& c, Database& db, const Json& e, const std::string& where, AbilityDef& a) {
  if (!onlyKeys(c, e, {"id", "name", "description", "target", "range", "effect", "cost", "uses", "cooldown", "afterMove", "if"}, where))
    return false;
  a.name = a.id;
  if (!c.str(e, "name", where, a.name) || !c.str(e, "description", where, a.description)) return false;

  std::string target;
  if (!c.str(e, "target", where, target, true)) return false;
  if (target == "self") a.target = AbilityTarget::Self;
  else if (target == "ally") a.target = AbilityTarget::Ally;
  else if (target == "enemy") a.target = AbilityTarget::Enemy;
  else if (target == "unit") a.target = AbilityTarget::Unit;
  else if (target == "cell") a.target = AbilityTarget::Cell;
  else return c.fail(where, "\"target\" must be \"self\", \"ally\", \"enemy\", \"unit\" or \"cell\"");

  if (e.has("range")) {
    const Json& r = e["range"];
    if (a.target == AbilityTarget::Self) return c.fail(where, "\"range\" cannot be used with \"target\": \"self\"");
    if (!r.isArray() || r.size() != 2 || !LoadCtx::isWhole(r[0]) || !LoadCtx::isWhole(r[1]) || r[0].asInt() < 1 ||
        r[1].asInt() < r[0].asInt() || r[1].asInt() > 20)
      return c.fail(where, "\"range\" must be [min, max]: whole numbers from 1 to 20, min not above max");
    a.minRange = r[0].asInt();
    a.maxRange = r[1].asInt();
  }

  if (!e.has("effect")) return c.fail(where, "missing required \"effect\"");
  if (!readEffect(c, db, e["effect"], where, a)) return false;

  // Target against effect.
  const char* name = a.effect == AbilityEffect::Push ? "push" : a.effect == AbilityEffect::Pull ? "pull"
                     : a.effect == AbilityEffect::Swap ? "swap" : a.effect == AbilityEffect::MovePast ? "movePast" : "moveBehind";
  switch (a.effect) {
    case AbilityEffect::Steal:
      if (a.target != AbilityTarget::Enemy) return c.fail(where, "\"steal\" needs \"target\": \"enemy\"");
      if (a.minRange != 1 || a.maxRange != 1) return c.fail(where, "\"steal\" takes from a neighbour: \"range\" must be [1, 1]");
      break;
    case AbilityEffect::Refresh:
      if (a.target != AbilityTarget::Ally) return c.fail(where, "\"refresh\" needs \"target\": \"ally\"");
      break;
    case AbilityEffect::Cure:
      if (a.target != AbilityTarget::Self && a.target != AbilityTarget::Ally)
        return c.fail(where, "\"cure\" needs \"target\": \"self\" or \"ally\"");
      break;
    case AbilityEffect::Push:
    case AbilityEffect::Pull:
    case AbilityEffect::Swap:
    case AbilityEffect::MovePast:
    case AbilityEffect::MoveBehind:
      if (a.target == AbilityTarget::Self || a.target == AbilityTarget::Cell)
        return c.fail(where, q(name) + " needs \"target\" \"ally\", \"enemy\" or \"unit\"");
      if (a.minRange != 1 || a.maxRange != 1) return c.fail(where, q(name) + " moves a neighbour: \"range\" must be [1, 1]");
      break;
    case AbilityEffect::Heal:
      if (a.target == AbilityTarget::Enemy || a.target == AbilityTarget::Cell)
        return c.fail(where, "\"heal\" cannot target \"enemy\" or \"cell\"");
      break;
    case AbilityEffect::Summon:
      if (a.target != AbilityTarget::Cell) return c.fail(where, "\"summon\" needs \"target\": \"cell\"");
      break;
    case AbilityEffect::Unlock:
      if (a.target != AbilityTarget::Cell) return c.fail(where, "\"unlock\" needs \"target\": \"cell\" (a square of the door or chest)");
      break;
  }

  if (e.has("cost")) {
    if (!e["cost"].isString() || (e["cost"].asString() != "action" && e["cost"].asString() != "free"))
      return c.fail(where, "\"cost\" must be \"action\" or \"free\"");
    a.free = e["cost"].asString() == "free";
  }
  if (e.has("uses") && (!LoadCtx::isWhole(e["uses"]) || e["uses"].asInt() < 1 || e["uses"].asInt() > 99))
    return c.fail(where, "\"uses\" must be from 1 to 99");
  if (e.has("cooldown") && (!LoadCtx::isWhole(e["cooldown"]) || e["cooldown"].asInt() < 1 || e["cooldown"].asInt() > 99))
    return c.fail(where, "\"cooldown\" must be from 1 to 99");
  if (e.has("uses")) a.uses = e["uses"].asInt();
  if (e.has("cooldown")) a.cooldown = e["cooldown"].asInt();
  if (!c.boolean(e, "afterMove", where, a.afterMove)) return false;

  if (e.has("if")) {
    // A condition may judge the target unit ("who": "foe"), but not where there is none (a square). There is no
    // combat, so "initiating" and "distance" are refused; a formula is fine.
    SkillCondScope scope = {a.target != AbilityTarget::Cell, false, true,
                            a.target == AbilityTarget::Cell ? "in an ability that targets a square (there is no target unit)" : "in an ability"};
    if (!readSkillConds(c, db, e["if"], where, scope, "data/abilities.json: ", a.conds)) return false;
  }
  return true;
}

}  // namespace

bool loadAbilities(const std::string& path, Database& db, std::string& err) {
  db.abilities.clear();
  if (!fileExists(path)) return true;  // a game without the file: nothing to load
  Json root;
  if (!loadObjectFile(path, root, err)) return false;
  LoadCtx c(err, path);
  for (const Json::Member& m : root.members())
    if (m.first != "abilities") return c.fail("", "unknown key \"" + m.first + "\" (expected \"abilities\")");
  const Json* list;
  if (!c.array(root, "abilities", "", list, true)) return false;
  if ((int)list->size() > kMaxAbilities) return c.fail("", "too many abilities (at most " + std::to_string(kMaxAbilities) + ")");
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& e = (*list)[i];
    std::string where = "abilities[" + std::to_string(i) + "]";
    if (!e.isObject()) return c.fail(where, "must be an object");
    AbilityDef a;
    if (!c.str(e, "id", where, a.id, true)) return false;
    if (a.id.empty()) return c.fail(where, "\"id\" must not be empty");
    where = "ability \"" + a.id + "\"";
    for (const AbilityDef& o : db.abilities)
      if (o.id == a.id) return c.fail(where, "duplicate id");
    if (!readAbility(c, db, e, where, a)) return false;
    db.abilities.push_back(a);
  }
  return true;
}

bool readUnitAbilities(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, UnitTypeDef& u) {
  std::vector<std::string> ids;
  if (!c.stringList(e, "abilities", where, ids)) return false;
  for (const std::string& id : ids) {
    const int a = db.findAbility(id);
    if (a < 0) return c.fail(where, "\"abilities\" names unknown ability \"" + id + "\" (data/abilities.json)");
    u.abilities.push_back(a);
  }
  return true;
}

// What the abilities name, once unit types, weapons, terrain and structures are loaded.
bool resolveAbilityRefs(const std::string& root, Database& db, std::string& err) {
  LoadCtx c(err, root + "/data/abilities.json");
  db.refreshAbilities = false;
  for (AbilityDef& a : db.abilities) {
    const std::string where = "ability \"" + a.id + "\"";
    if (a.effect == AbilityEffect::Refresh) db.refreshAbilities = true;
    if (a.effect == AbilityEffect::Summon) {
      a.summon.type = db.findUnitType(a.summon.typeId);
      if (a.summon.type < 0) return c.fail(where, "\"summon\": unknown unit type \"" + a.summon.typeId + "\"");
    }
    if (a.effect == AbilityEffect::Unlock && !a.unlockNeedsId.empty()) {
      a.unlockNeeds = db.findItem(a.unlockNeedsId);
      if (a.unlockNeeds < 0) return c.fail(where, "\"unlock\": \"needs\" names unknown item \"" + a.unlockNeedsId + "\" (data/items.json)");
      if (db.items[(size_t)a.unlockNeeds].uses < 0)
        return c.fail(where, "\"unlock\": \"needs\" names \"" + a.unlockNeedsId + "\", which has unlimited uses; a lockpick that wears out needs \"uses\"");
    }
    if (a.effect == AbilityEffect::Cure) {
      if (db.conditions.empty()) return c.fail(where, "\"cure\" needs conditions (data/conditions.json); this game has none");
      for (const std::string& id : a.cureIds) {
        const int k = db.findCondition(id);
        if (k < 0) return c.fail(where, "\"cure\" names unknown condition \"" + id + "\" (data/conditions.json)");
        a.cures.push_back(k);
      }
    }
    if (!resolveSkillConds(c, db, where, a.conds)) return false;
  }
  return true;
}

}  // namespace ote
