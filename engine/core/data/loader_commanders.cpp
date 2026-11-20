// data/commanders.json (docs/COMMANDERS.md). Every refusal names the file, the entry and the key.
#include "core/data/loader_commanders.h"

#include <initializer_list>

#include "core/data/loader_research_branch.h"

namespace ote {

namespace {

const int kMaxMeter = 100000;
const int kMaxRate = 1000;  // meter points per hit point (or per turn): a bigger number is surely a slip

bool onlyKeys(const LoadCtx& c, const Json& o, std::initializer_list<const char*> keys, const std::string& where) {
  for (const Json::Member& m : o.members()) {
    bool known = false;
    std::string list;
    for (const char* k : keys) {
      known = known || m.first == k;
      list += (list.empty() ? "\"" : ", \"") + std::string(k) + "\"";
    }
    if (!known) return c.fail(where, "unknown key \"" + m.first + "\" (expected " + list + ")");
  }
  return true;
}

// A list of skill ids / { skill, filter } entries (the side-wide skill format), refusing the same skill and filter twice.
bool readGrants(const LoadCtx& c, const Database& db, const Json& e, const char* key, const std::string& where, std::vector<SkillGrant>& out) {
  if (!e.has(key)) return true;
  const Json* list;
  if (!c.array(e, key, where, list)) return false;
  for (size_t i = 0; i < list->size(); ++i) {
    SkillGrant g;
    if (!readSkillGrant(c, db, (*list)[i], where + ": \"" + key + "\"", g)) return false;
    for (const SkillGrant& o : out)
      if (o.skill == g.skill && o.filterType == g.filterType && o.filterTag == g.filterTag)
        return c.fail(where, std::string("\"") + key + "\" lists \"" + db.skills[(size_t)g.skill].id + "\" twice with the same filter");
    out.push_back(g);
  }
  return true;
}

// One instant effect: exactly one of heal / damage / giveResource / refresh, and "filter" (heal, damage) / "to" (damage).
bool readEffect(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, CommanderEffect& fx) {
  const std::string at = where + ": \"effects\"";
  if (!e.isObject()) return c.fail(at, "each entry must be an object such as { \"heal\": 3 }");
  static const char* const kinds[] = {"heal", "damage", "giveResource", "refresh"};
  int found = 0;
  const char* kind = nullptr;
  for (const char* k : kinds)
    if (e.has(k)) { ++found; kind = k; }
  if (found == 0) {
    const std::string keys = e.members().empty() ? "nothing" : "\"" + e.members()[0].first + "\"";
    return c.fail(at, "has " + keys + " but no effect (expected one of \"heal\", \"damage\", \"giveResource\", \"refresh\")");
  }
  if (found > 1) return c.fail(at, "an entry holds one effect only (\"heal\", \"damage\", \"giveResource\" or \"refresh\"), not several");
  const std::string name = kind;
  for (const Json::Member& m : e.members())
    if (m.first != name && !(m.first == "filter" && (name == "heal" || name == "damage")) && !(m.first == "to" && name == "damage"))
      return c.fail(at, "\"" + name + "\" does not take the key \"" + m.first + "\"");
  if (name == "heal" || name == "damage") {
    fx.kind = name == "heal" ? CommanderEffect::Kind::Heal : CommanderEffect::Kind::Damage;
    if (!c.integerIn(e, kind, at, 1, 9999, fx.amount, true)) return false;
    if (e.has("filter") && !readGrantFilter(c, db, e["filter"], at, fx.filterType, fx.filterTag)) return false;
    if (name == "damage") {
      std::string to = "enemies";
      if (!c.str(e, "to", at, to)) return false;
      if (to != "enemies") return c.fail(at, "\"damage\".\"to\" must be \"enemies\" (the only target there is), not \"" + to + "\"");
    }
  } else if (name == "giveResource") {
    fx.kind = CommanderEffect::Kind::GiveResource;
    if (!c.amounts(db, e, "giveResource", at, fx.resources)) return false;
    if (fx.resources.empty()) return c.fail(at, "\"giveResource\" names no resource");
    for (const Amount& a : fx.resources)
      if (a.amount < 1 || a.amount > 99999)
        return c.fail(at, "\"giveResource\".\"" + db.resources[(size_t)a.resource].id + "\" must be a whole number from 1 to 99999");
  } else {
    fx.kind = CommanderEffect::Kind::Refresh;  // "refresh": a filter object, or true for every unit
    const Json& f = e["refresh"];
    if (f.isBool() && f.asBool()) return true;
    if (!f.isObject()) return c.fail(at, "\"refresh\" must be a filter such as {}, { \"type\": id } or { \"tag\": text } (or true for every unit)");
    if (!readGrantFilter(c, db, f, at, fx.filterType, fx.filterTag)) return false;
  }
  return true;
}

bool readPower(const LoadCtx& c, const Database& db, const Json& e, const std::string& at, const CommanderDef& cmd, CommanderPower& p) {
  if (!e.isObject()) return c.fail(at, "must be an object");
  if (!c.str(e, "id", at, p.id, true) || p.id.empty()) return c.fail(at, "\"id\" is required");
  const std::string where = "commander \"" + cmd.id + "\" power \"" + p.id + "\"";
  if (!onlyKeys(c, e, {"id", "name", "description", "cost", "turns", "costGrowthPercent", "modifiers", "effects"}, where)) return false;
  for (const CommanderPower& o : cmd.powers)
    if (o.id == p.id) return c.fail(where, "duplicate power id");
  p.name = p.id;
  if (!c.str(e, "name", where, p.name) || !c.str(e, "description", where, p.description)) return false;
  if (e.has("name") && p.name.empty()) return c.fail(where, "\"name\" must not be empty text (leave the key out)");
  if (!c.integer(e, "cost", where, p.cost, true)) return false;
  if (p.cost < 1 || p.cost > cmd.meterMax)
    return c.fail(where, "\"cost\" must be from 1 to the meter's \"max\" (" + std::to_string(cmd.meterMax) + "), or the power could never be used");
  if (!c.integerIn(e, "turns", where, 1, 99, p.turns)) return false;
  if (!c.integerIn(e, "costGrowthPercent", where, 0, 1000, p.growthPercent)) return false;
  if (!readGrants(c, db, e, "modifiers", where, p.modifiers)) return false;
  if (e.has("turns") && p.modifiers.empty())
    return c.fail(where, "\"turns\" only means something with \"modifiers\" (a power of instant effects only is over as soon as it is used)");
  if (e.has("effects")) {
    const Json* list;
    if (!c.array(e, "effects", where, list)) return false;
    for (size_t i = 0; i < list->size(); ++i) {
      CommanderEffect fx;
      if (!readEffect(c, db, (*list)[i], where, fx)) return false;
      p.effects.push_back(fx);
    }
  }
  if (p.modifiers.empty() && p.effects.empty()) return c.fail(where, "has neither \"modifiers\" nor \"effects\": it would do nothing");
  return true;
}

bool readMeter(const LoadCtx& c, const Json& e, const std::string& where, CommanderDef& d) {
  const Json* m;
  if (!c.object(e, "meter", where, m, true)) return false;
  const std::string at = where + ": \"meter\"";
  if (!onlyKeys(c, *m, {"max", "perDamageDealt", "perDamageTaken", "perTurn"}, at)) return false;
  if (!c.integerIn(*m, "max", at, 1, kMaxMeter, d.meterMax, true)) return false;
  if (!c.integerIn(*m, "perDamageDealt", at, 0, kMaxRate, d.perDamageDealt) || !c.integerIn(*m, "perDamageTaken", at, 0, kMaxRate, d.perDamageTaken) ||
      !c.integerIn(*m, "perTurn", at, 0, kMaxRate, d.perTurn))
    return false;
  if (d.perDamageDealt == 0 && d.perDamageTaken == 0 && d.perTurn == 0)
    return c.fail(at, "fills by nothing (give \"perDamageDealt\", \"perDamageTaken\" or \"perTurn\" a number above 0)");
  return true;
}

bool readCommander(const LoadCtx& c, const Database& db, const Json& e, const std::string& at, CommanderDef& d) {
  if (!e.isObject()) return c.fail(at, "must be an object");
  if (!c.str(e, "id", at, d.id, true) || d.id.empty()) return c.fail(at, "\"id\" is required");
  const std::string where = "commander \"" + d.id + "\"";
  if (!onlyKeys(c, e, {"id", "name", "description", "portrait", "passive", "meter", "powers"}, where)) return false;
  d.name = d.id;
  if (!c.str(e, "name", where, d.name) || !c.str(e, "description", where, d.description) || !c.str(e, "portrait", where, d.portrait)) return false;
  if (e.has("name") && d.name.empty()) return c.fail(where, "\"name\" must not be empty text (leave the key out)");
  if (!readGrants(c, db, e, "passive", where, d.passive)) return false;
  if (!readMeter(c, e, where, d)) return false;
  const Json* list;
  if (!c.array(e, "powers", where, list)) return false;
  if (list->size() > (size_t)kMaxCommanderPowers)
    return c.fail(where, "\"powers\" lists " + std::to_string(list->size()) + " powers (at most " + std::to_string(kMaxCommanderPowers) + ")");
  for (size_t i = 0; i < list->size(); ++i) {
    CommanderPower p;
    if (!readPower(c, db, (*list)[i], where + ": \"powers\"[" + std::to_string(i) + "]", d, p)) return false;
    d.powers.push_back(p);
  }
  return true;
}

}  // namespace

bool loadCommanders(const std::string& path, Database& db, std::string& err) {
  db.commanders.clear();
  if (!fileExists(path)) return true;
  Json root;
  if (!loadObjectFile(path, root, err)) return false;
  LoadCtx c(err, path);
  if (!onlyKeys(c, root, {"commanders"}, "")) return false;
  const Json* list;
  if (!c.array(root, "commanders", "", list, true)) return false;
  if (list->size() == 0) return c.fail("", "\"commanders\" is empty (delete the file if the game has no commanders)");
  for (size_t i = 0; i < list->size(); ++i) {
    CommanderDef d;
    if (!readCommander(c, db, (*list)[i], "commanders[" + std::to_string(i) + "]", d)) return false;
    for (const CommanderDef& o : db.commanders)
      if (o.id == d.id) return c.fail("commander \"" + d.id + "\"", "duplicate id");
    db.commanders.push_back(d);
  }
  return true;
}

bool readSideCommander(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, MapSideDef& s) {
  if (!e.has("commander")) return true;
  std::string id;
  if (!c.str(e, "commander", where, id)) return false;
  if (db.commanders.empty()) return c.fail(where, "\"commander\" needs data/commanders.json (this game has no commanders)");
  if (id == "choose") {  // the person picks before the battle; a computer side takes one from the seed (Battle::init)
    s.commanderChoose = true;
    return true;
  }
  s.commander = db.findCommander(id);
  if (s.commander < 0) {
    std::string ids;
    for (const CommanderDef& d : db.commanders) ids += (ids.empty() ? "" : ", ") + d.id;
    return c.fail(where, "\"commander\" names unknown commander \"" + id + "\" (data/commanders.json has: " + ids + "; or \"choose\")");
  }
  return true;
}

}  // namespace ote
