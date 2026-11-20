// data/factions.json (docs/ECONOMY.md "Factions"). Every refusal names the file, the entry and the key.
#include "core/data/loader_factions.h"

#include <initializer_list>

#include "core/data/loader_content.h"
#include "core/data/loader_research_branch.h"

namespace ote {

namespace {

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

// A list of ids of one kind -> indices. The key may be left out (no restriction); a written list, even an empty one, is the
// whole set of what the faction may have. `find` maps an id to its index (-1 = unknown); `what` names the kind for messages.
template <class Find>
bool idList(const LoadCtx& c, const Json& e, const char* key, const std::string& where, const char* what, Find find,
            const char* file, std::vector<int>& out, bool& given) {
  given = e.has(key);
  if (!given) return true;
  std::vector<std::string> ids;
  if (!c.stringList(e, key, where, ids)) return false;
  for (const std::string& id : ids) {
    const int i = find(id);
    if (i < 0) return c.fail(where, std::string("\"") + key + "\" names unknown " + what + " \"" + id + "\" (" + file + ")");
    for (int seen : out)
      if (seen == i) return c.fail(where, std::string("\"") + key + "\" lists \"" + id + "\" twice");
    out.push_back(i);
  }
  return true;
}

bool readStart(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, FactionDef& f) {
  if (!e.has("start")) return true;
  const Json* s;
  if (!c.object(e, "start", where, s)) return false;
  const std::string at = where + ": \"start\"";
  if (!onlyKeys(c, *s, {"resources"}, at)) return false;
  if (!c.amounts(db, *s, "resources", at, f.start)) return false;
  for (const Amount& a : f.start)
    if (a.amount < 0)
      return c.fail(at, "\"resources\".\"" + db.resources[(size_t)a.resource].id + "\" must be 0 or more (it is added to what the side starts with)");
  return true;
}

bool readAi(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, FactionDef& f) {
  if (!e.has("ai")) return true;
  const Json* ai;
  if (!c.object(e, "ai", where, ai)) return false;
  const std::string at = where + ": \"ai\"";
  if (!onlyKeys(c, *ai, {"weights"}, at)) return false;
  if (!ai->has("weights")) return true;
  // The same keys as one set of game.json "rules.ai.weights"; what is not written keeps that set's default.
  if (!readAiWeightSet(c, db, at + ": \"weights\"", "faction " + f.id, (*ai)["weights"], f.weights)) return false;
  f.hasWeights = true;
  return true;
}

bool readFaction(const LoadCtx& c, const Database& db, const Json& e, const std::string& at, FactionDef& f) {
  if (!e.isObject()) return c.fail(at, "must be an object");
  if (!c.str(e, "id", at, f.id, true) || f.id.empty()) return c.fail(at, "\"id\" is required");
  const std::string where = "faction \"" + f.id + "\"";
  if (!onlyKeys(c, e, {"id", "name", "description", "units", "structures", "techs", "start", "modifiers", "commanders", "ai", "tags"}, where)) return false;
  f.name = f.id;
  if (!c.str(e, "name", where, f.name) || !c.str(e, "description", where, f.description)) return false;
  if (e.has("name") && f.name.empty()) return c.fail(where, "\"name\" must not be empty text (leave the key out)");
  if (!idList(c, e, "units", where, "unit type", [&](const std::string& id) { return db.findUnitType(id); }, "data/units.json", f.units, f.limitUnits) ||
      !idList(c, e, "structures", where, "structure", [&](const std::string& id) { return db.findStructure(id); }, "data/structures.json",
              f.structures, f.limitStructures) ||
      !idList(c, e, "techs", where, "technology", [&](const std::string& id) { return db.findTechnology(id); }, "data/research.json", f.techs,
              f.limitTechs))
    return false;
  if (!readStart(c, db, e, where, f)) return false;
  if (e.has("modifiers")) {
    const Json* list;
    if (!c.array(e, "modifiers", where, list)) return false;
    for (size_t i = 0; i < list->size(); ++i) {
      SkillGrant g;
      if (!readSkillGrant(c, db, (*list)[i], where + ": \"modifiers\"", g)) return false;
      for (const SkillGrant& o : f.modifiers)
        if (o.skill == g.skill && o.filterType == g.filterType && o.filterTag == g.filterTag)
          return c.fail(where, "\"modifiers\" lists \"" + db.skills[(size_t)g.skill].id + "\" twice with the same filter");
      f.modifiers.push_back(g);
    }
  }
  if (e.has("commanders")) {  // the commanders a "choose" side of this faction may pick (docs/COMMANDERS.md)
    if (db.commanders.empty()) return c.fail(where, "\"commanders\" needs data/commanders.json (this game has no commanders)");
    if (!idList(c, e, "commanders", where, "commander", [&](const std::string& id) { return db.findCommander(id); }, "data/commanders.json",
                f.commanders, f.limitCommanders))
      return false;
    if (f.commanders.empty()) return c.fail(where, "\"commanders\" is empty (a side of this faction could pick nobody; leave the key out for no limit)");
  }
  if (!readAi(c, db, e, where, f)) return false;
  if (!c.stringList(e, "tags", where, f.tags)) return false;
  for (const std::string& t : f.tags)
    if (t.empty()) return c.fail(where, "\"tags\" must not hold empty text");
  return true;
}

}  // namespace

bool loadFactions(const std::string& path, Database& db, std::string& err) {
  db.factions.clear();
  if (!fileExists(path)) return true;
  Json root;
  if (!loadObjectFile(path, root, err)) return false;
  LoadCtx c(err, path);
  if (!onlyKeys(c, root, {"factions"}, "")) return false;
  const Json* list;
  if (!c.array(root, "factions", "", list, true)) return false;
  if (list->size() == 0) return c.fail("", "\"factions\" is empty (delete the file if the game has no factions)");
  for (size_t i = 0; i < list->size(); ++i) {
    FactionDef f;
    if (!readFaction(c, db, (*list)[i], "factions[" + std::to_string(i) + "]", f)) return false;
    for (const FactionDef& o : db.factions)
      if (o.id == f.id) return c.fail("faction \"" + f.id + "\"", "duplicate id");
    db.factions.push_back(f);
  }
  return true;
}

bool readSideFaction(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, MapSideDef& s) {
  if (!e.has("faction")) return true;
  std::string id;
  if (!c.str(e, "faction", where, id)) return false;
  if (db.factions.empty()) return c.fail(where, "\"faction\" needs data/factions.json (this game has no factions)");
  if (id == "choose") {  // the person picks before the battle; a computer side takes one from the seed (Battle::init)
    s.factionChoose = true;
    return true;
  }
  s.faction = db.findFaction(id);
  if (s.faction < 0) {
    std::string ids;
    for (const FactionDef& f : db.factions) ids += (ids.empty() ? "" : ", ") + f.id;
    return c.fail(where, "\"faction\" names unknown faction \"" + id + "\" (data/factions.json has: " + ids + "; or \"choose\")");
  }
  return true;
}

}  // namespace ote
