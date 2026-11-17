// Loading difficulties (docs/DIFFICULTY.md). Every refusal names the file, the entry and the problem.
#include "core/data/loader_difficulty.h"

#include <algorithm>
#include <cstdlib>

namespace ote {

namespace {

constexpr int kMaxDifficulties = 12;
constexpr int kMaxLevelShift = 98;      // maxLevel is at most 99 and a level never goes below 1
constexpr int kMaxFlatStat = 999;
constexpr int kMaxPercent = 1000;

std::string q(const std::string& s) { return "\"" + s + "\""; }

bool onlyKeys(const LoadCtx& c, const Json& o, std::initializer_list<const char*> keys, const std::string& where) {
  for (const Json::Member& m : o.members()) {
    bool ok = false;
    for (const char* k : keys) ok = ok || m.first == k;
    if (!ok) return c.fail(where, "unknown key " + q(m.first));
  }
  return true;
}

bool percent(const LoadCtx& c, const Json& o, const char* key, const std::string& where, int& out, bool& given) {
  given = o.has(key);
  if (!given) return true;
  if (!c.integer(o, key, where, out)) return false;
  if (out < 0 || out > kMaxPercent)
    return c.fail(where, q(key) + " must be from 0 to " + std::to_string(kMaxPercent) + " (percent)");
  return true;
}

// {"levels": n, "stats": {...}, "statsPercent": n}. Remembers what was named (a map's override needs that).
bool readEnemy(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, DifficultyEnemy& out) {
  if (!e.isObject()) return c.fail(where, "\"enemy\" must be an object");
  const std::string w = where + ": \"enemy\"";
  if (!onlyKeys(c, e, {"levels", "stats", "statsPercent"}, w)) return false;
  out.stats.assign(db.stats.size(), 0);
  out.hasStat.assign(db.stats.size(), 0);
  if (e.has("levels")) {
    if (!c.integer(e, "levels", w, out.levels)) return false;
    if (out.levels < -kMaxLevelShift || out.levels > kMaxLevelShift)
      return c.fail(w, "\"levels\" must be from -" + std::to_string(kMaxLevelShift) + " to " + std::to_string(kMaxLevelShift));
    out.hasLevels = true;
  }
  if (e.has("stats")) {
    if (!c.statMap(db, e, "stats", w, out.stats)) return false;
    for (const Json::Member& m : e["stats"].members()) {
      const int s = db.findStat(m.first);
      if (std::abs(out.stats[(size_t)s]) > kMaxFlatStat)
        return c.fail(w, "\"stats\"." + q(m.first) + " must be from -999 to 999");
      out.hasStat[(size_t)s] = 1;
    }
  }
  if (!percent(c, e, "statsPercent", w, out.statsPercent, out.hasPercent)) return false;
  if (!out.hasPercent) out.statsPercent = 100;
  return true;
}

}  // namespace

bool loadDifficulties(const LoadCtx& c, const Json& root, Database& db) {
  if (!root.has("difficulties")) return true;
  const Json* d;
  if (!c.object(root, "difficulties", "", d)) return false;
  const std::string at = "difficulties";
  if (!onlyKeys(c, *d, {"default", "change", "percentStats", "list"}, at)) return false;
  DifficultyRules& r = db.difficulties;
  const Json* list;
  if (!c.array(*d, "list", at, list, true)) return false;
  if (list->size() == 0)
    return c.fail(at, "\"list\" needs at least one difficulty (leave out \"difficulties\" for a game without any)");
  if ((int)list->size() > kMaxDifficulties)
    return c.fail(at, "\"list\" has more than " + std::to_string(kMaxDifficulties) + " difficulties");
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& e = (*list)[i];
    std::string where = at + ": list[" + std::to_string(i) + "]";
    if (!e.isObject()) return c.fail(where, "must be an object");
    DifficultyDef def;
    if (!c.str(e, "id", where, def.id, true)) return false;
    if (!isStoryId(def.id)) return c.fail(where, "\"id\" must be letters, digits, \"_\" and \"-\" (got " + q(def.id) + ")");
    where = at + " " + q(def.id);
    if (r.find(def.id) >= 0) return c.fail(where, "duplicate id");
    if (!onlyKeys(c, e, {"id", "name", "description", "enemy", "xpPercent", "resourcePercent"}, where)) return false;
    def.name = def.id;
    if (!c.str(e, "name", where, def.name) || !c.str(e, "description", where, def.description)) return false;
    if (def.name.empty()) return c.fail(where, "\"name\" must not be empty");
    bool given;
    if (!percent(c, e, "xpPercent", where, def.xpPercent, given)) return false;
    if (!given) def.xpPercent = 100;
    if (!percent(c, e, "resourcePercent", where, def.resourcePercent, given)) return false;
    if (!given) def.resourcePercent = 100;
    if (e.has("enemy")) {
      if (!readEnemy(c, db, e["enemy"], where, def.enemy)) return false;
    } else {
      def.enemy.stats.assign(db.stats.size(), 0);
    }
    r.list.push_back(def);
  }
  std::string def;
  if (!c.str(*d, "default", at, def, true)) return false;
  r.defaultIndex = r.find(def);
  if (r.defaultIndex < 0)
    return c.fail(at, "\"default\" names unknown difficulty " + q(def) + " (it must be one of the list's ids)");
  r.defaultId = def;
  std::string change = "lowerOnly";
  if (!c.str(*d, "change", at, change)) return false;
  if (change == "never") r.change = DifficultyChange::Never;
  else if (change == "lowerOnly") r.change = DifficultyChange::LowerOnly;
  else if (change == "free") r.change = DifficultyChange::Free;
  else return c.fail(at, "\"change\" must be \"never\", \"lowerOnly\" or \"free\", not " + q(change));
  // The stats "statsPercent" scales: the game's own list, else every stat except the movement stat.
  if (d->has("percentStats")) {
    std::vector<std::string> ids;
    if (!c.stringList(*d, "percentStats", at, ids)) return false;
    for (const std::string& id : ids) {
      const int s = db.findStat(id);
      if (s < 0) return c.fail(at, "\"percentStats\" names unknown stat " + q(id));
      if (std::find(r.percentStatIndex.begin(), r.percentStatIndex.end(), s) == r.percentStatIndex.end())
        r.percentStatIndex.push_back(s);
    }
    r.percentStatsGiven = true;
  } else {
    for (size_t s = 0; s < db.stats.size(); ++s)
      if ((int)s != db.rules.statMove) r.percentStatIndex.push_back((int)s);
  }
  return true;
}

bool readMapDifficulty(const LoadCtx& c, const Database& db, const Json& root, MapDef& m) {
  if (!root.has("difficulty")) return true;
  const Json* d;
  if (!c.object(root, "difficulty", "", d)) return false;
  for (const Json::Member& e : d->members()) {
    const std::string where = "difficulty " + q(e.first);
    if (!db.difficulties.any())
      return c.fail(where, "the map names a difficulty but the game has none (game.json \"difficulties\")");
    MapDifficulty md;
    md.difficulty = db.difficulties.find(e.first);
    if (md.difficulty < 0) return c.fail(where, "unknown difficulty " + q(e.first) + " (game.json \"difficulties\")");
    if (!e.second.isObject()) return c.fail(where, "must be an object such as { \"enemy\": { \"levels\": 3 } }");
    if (!onlyKeys(c, e.second, {"enemy", "xpPercent", "resourcePercent"}, where)) return false;
    md.enemy.stats.assign(db.stats.size(), 0);
    md.enemy.hasStat.assign(db.stats.size(), 0);
    if (e.second.has("enemy") && !readEnemy(c, db, e.second["enemy"], where, md.enemy)) return false;
    bool given;
    if (!percent(c, e.second, "xpPercent", where, md.xpPercent, given)) return false;
    if (!given) md.xpPercent = -1;
    if (!percent(c, e.second, "resourcePercent", where, md.resourcePercent, given)) return false;
    if (!given) md.resourcePercent = -1;
    m.difficulty.push_back(md);
  }
  return true;
}

bool readDifficultyCondition(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, int& index, CmpOp& op) {
  if (!onlyKeys(c, e, {"difficulty"}, where)) return false;
  if (!db.difficulties.any())
    return c.fail(where, "\"difficulty\" is a condition but the game has no difficulties (game.json \"difficulties\")");
  const Json& v = e["difficulty"];
  std::string id;
  op = CmpOp::Eq;
  if (v.isString()) {
    id = v.asString();
  } else if (v.isObject() && v.size() == 1 && (v.has("atLeast") || v.has("atMost"))) {
    const char* key = v.has("atLeast") ? "atLeast" : "atMost";
    op = v.has("atLeast") ? CmpOp::Ge : CmpOp::Le;
    if (!v[key].isString()) return c.fail(where, std::string("\"") + key + "\" must be a difficulty id");
    id = v[key].asString();
  } else {
    return c.fail(where, "\"difficulty\" must be a difficulty id, or { \"atLeast\": id } / { \"atMost\": id }");
  }
  index = db.difficulties.find(id);
  if (index < 0) return c.fail(where, "\"difficulty\" names unknown difficulty " + q(id) + " (game.json \"difficulties\")");
  return true;
}

bool readUnitDifficulty(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, MapUnitDef& u) {
  if (e.has("difficultyShift") && !c.boolean(e, "difficultyShift", where, u.difficultyShift)) return false;
  if (!e.has("difficulty")) return true;
  if (!db.difficulties.any())
    return c.fail(where, "\"difficulty\" is given but the game has no difficulties (game.json \"difficulties\")");
  std::vector<std::string> ids;
  if (!c.stringList(e, "difficulty", where, ids)) return false;
  if (ids.empty()) return c.fail(where, "\"difficulty\" must list at least one difficulty (leave it out to appear on all)");
  for (const std::string& id : ids) {
    const int i = db.difficulties.find(id);
    if (i < 0) return c.fail(where, "\"difficulty\" names unknown difficulty " + q(id) + " (game.json \"difficulties\")");
    if (std::find(u.onlyOn.begin(), u.onlyOn.end(), i) == u.onlyOn.end()) u.onlyOn.push_back(i);
  }
  std::sort(u.onlyOn.begin(), u.onlyOn.end());
  return true;
}

}  // namespace ote
