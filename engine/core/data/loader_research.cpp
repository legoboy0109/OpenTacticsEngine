// data/research.json (docs/ECONOMY.md "Research").
#include <initializer_list>

#include "core/data/loader_content.h"
#include "core/data/loader_research_branch.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

// An object may only have these keys: a typo in a rule file must not pass silently.
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

// Unit type ids in a list -> indices (no duplicates).
bool unitTypeList(const LoadCtx& c, const Database& db, const Json& o, const char* key, const std::string& where,
                  std::vector<int>& out) {
  std::vector<std::string> ids;
  if (!c.stringList(o, key, where, ids)) return false;
  for (const std::string& id : ids) {
    int t = db.findUnitType(id);
    if (t < 0) return c.fail(where, std::string("\"") + key + "\" names unknown unit type \"" + id + "\"");
    for (int seen : out)
      if (seen == t) return c.fail(where, std::string("\"") + key + "\" lists \"" + id + "\" twice");
    out.push_back(t);
  }
  return true;
}

// Depth-first search for a prerequisite loop; names the technologies on it.
bool findCycle(const Database& db, int at, std::vector<int>& state, std::vector<int>& path, std::string& loop) {
  state[(size_t)at] = 1;
  path.push_back(at);
  for (int next : db.research[(size_t)at].needs) {
    if (state[(size_t)next] == 1) {
      size_t from = 0;
      while (path[from] != next) ++from;
      for (size_t i = from; i < path.size(); ++i) loop += db.research[(size_t)path[i]].id + " -> ";
      loop += db.research[(size_t)next].id;
      return true;
    }
    if (state[(size_t)next] == 0 && findCycle(db, next, state, path, loop)) return true;
  }
  path.pop_back();
  state[(size_t)at] = 2;
  return false;
}

}  // namespace

bool loadResearch(const std::string& path, Database& db, std::string& err) {
  db.research.clear();
  if (!fileExists(path)) return true;
  Json root;
  if (!loadObjectFile(path, root, err)) return false;
  LoadCtx c(err, path);
  if (!onlyKeys(c, root, {"technologies"}, "")) return false;
  const Json* list;
  if (!c.array(root, "technologies", "", list, true)) return false;
  if (list->size() == 0) return c.fail("", "\"technologies\" is empty (delete the file if the game has no research)");
  std::vector<const Json*> entries;

  // Pass 1: everything except the links between technologies (they may name later ones).
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& e = (*list)[i];
    std::string where = "technologies[" + std::to_string(i) + "]";
    if (!e.isObject()) return c.fail(where, "must be an object");
    TechnologyDef t;
    if (!c.str(e, "id", where, t.id, true)) return false;
    if (t.id.empty()) return c.fail(where, "\"id\" must not be empty");
    where = "technology \"" + t.id + "\"";
    if (db.findTechnology(t.id) >= 0) return c.fail(where, "duplicate id");
    if (!onlyKeys(c, e, {"id", "name", "description", "cost", "turns", "requires", "unlocks", "bonus", "tree", "tier",
                           "exclusiveGroup", "requiresStructures", "grants"}, where)) return false;
    t.name = t.id;
    if (!c.str(e, "name", where, t.name) || !c.str(e, "description", where, t.description)) return false;
    if (!c.amounts(db, e, "cost", where, t.cost)) return false;
    for (const Amount& a : t.cost)
      if (a.amount < 0) return c.fail(where, "\"cost\" must not be negative");
    if (!c.integer(e, "turns", where, t.turns)) return false;
    if (t.turns < 0 || t.turns > kMaxResearchTurns)
      return c.fail(where, "\"turns\" must be from 0 to " + std::to_string(kMaxResearchTurns) + " (0 = finished at once)");
    if (e.has("unlocks")) {
      const Json* u;
      if (!c.object(e, "unlocks", where, u) || !onlyKeys(c, *u, {"units"}, where + ": \"unlocks\"")) return false;
      if (!unitTypeList(c, db, *u, "units", where + ": \"unlocks\"", t.unlocksUnits)) return false;
    }
    if (e.has("bonus")) {
      const Json* b;
      std::string at = where + ": \"bonus\"";
      if (!c.object(e, "bonus", where, b) || !onlyKeys(c, *b, {"types", "stats", "income"}, at)) return false;
      if (!unitTypeList(c, db, *b, "types", at, t.bonusTypes)) return false;
      if (!readBonusIncome(c, db, *b, where, t)) return false;
      // "stats" may be left out when the bonus is income only.
      if (b->has("stats") || !b->has("income")) {
        t.bonus.assign(db.stats.size(), 0);
        if (!b->has("stats")) return c.fail(where, "\"bonus\" needs \"stats\", for example { \"def\": 1 }");
        if (!c.statMap(db, *b, "stats", at, t.bonus)) return false;
        bool any = false;
        for (int v : t.bonus) any = any || v != 0;
        if (!any) return c.fail(where, "\"bonus\".\"stats\" must change at least one stat");
      } else if (b->has("types")) {
        return c.fail(where, "\"bonus\".\"types\" only narrows \"stats\"; income goes to the whole side");
      }
    }
    if (!readBranchKeys(c, db, e, where, t)) return false;
    db.research.push_back(t);
    entries.push_back(&e);
  }

  // Pass 2: prerequisites.
  for (size_t i = 0; i < db.research.size(); ++i) {
    TechnologyDef& t = db.research[i];
    std::string where = "technology \"" + t.id + "\"";
    std::vector<std::string> ids;
    if (!c.stringList(*entries[i], "requires", where, ids)) return false;
    for (const std::string& id : ids) {
      int r = db.findTechnology(id);
      if (r < 0) return c.fail(where, "\"requires\" names unknown technology \"" + id + "\"");
      for (int seen : t.needs)
        if (seen == r) return c.fail(where, "\"requires\" lists \"" + id + "\" twice");
      t.needs.push_back(r);
    }
  }
  std::vector<int> state(db.research.size(), 0), trail;
  for (size_t i = 0; i < db.research.size(); ++i) {
    std::string loop;
    if (state[i] == 0 && findCycle(db, (int)i, state, trail, loop))
      return c.fail("", "technologies require each other in a loop: " + loop);
  }
  return true;
}

}  // namespace ote
