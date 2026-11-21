// Loaders of weapon ranks and stat caps (docs/SRPG.md section 7): data/ranks.json, data/caps.json, and the keys of
// weapons, unit types, characters and map units. Every message names the file and the entry.
#include "core/data/loader_content.h"
#include "core/data/loader_ranks.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

bool idChar(char ch) { return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-'; }

const char* const kNoRanks = "this game has no weapon ranks (data/ranks.json)";

}  // namespace

bool loadRanks(const std::string& path, Database& db, std::string& err) {
  db.ranks = RankRules();
  if (!fileExists(path)) return true;
  Json root;
  if (!loadObjectFile(path, root, err)) return false;
  LoadCtx c(err, path);
  for (const Json::Member& m : root.members())
    if (m.first != "growth" && m.first != "ranks") return c.fail("", unknownKeyText(m.first, {"growth", "ranks"}));
  std::string growth = "fixed";
  if (!c.str(root, "growth", "", growth)) return false;
  if (growth != "fixed" && growth != "use") return c.fail("", "\"growth\" must be \"fixed\" or \"use\"");
  const Json* list;
  if (!c.array(root, "ranks", "", list, true)) return false;
  if (list->size() == 0) return true;  // nothing in the file: the same game as no file
  if ((int)list->size() > kMaxRanks) return c.fail("", "too many ranks (at most " + std::to_string(kMaxRanks) + ")");
  RankRules rules;
  rules.growByUse = growth == "use";
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& e = (*list)[i];
    std::string where = "ranks[" + std::to_string(i) + "]";
    if (!e.isObject()) return c.fail(where, "must be an object");
    RankDef r;
    if (!c.str(e, "id", where, r.id, true)) return false;
    if (r.id.empty()) return c.fail(where, "\"id\" must not be empty");
    for (char ch : r.id)
      if (!idChar(ch)) return c.fail(where, "\"id\" must be lowercase letters, digits, \"_\" and \"-\" (got \"" + r.id + "\")");
    where = "rank \"" + r.id + "\"";
    for (const Json::Member& m : e.members())
      if (m.first != "id" && m.first != "name" && m.first != "xp") return c.fail(where, unknownKeyText(m.first, {"id", "name", "xp"}));
    if (rules.findRank(r.id) >= 0) return c.fail(where, "duplicate id");
    r.name = r.id;
    if (!c.str(e, "name", where, r.name) || !c.integer(e, "xp", where, r.xp, true)) return false;
    if (r.xp < 0 || r.xp > kMaxRankXp) return c.fail(where, "\"xp\" must be from 0 to " + std::to_string(kMaxRankXp));
    if (i == 0 && r.xp != 0) return c.fail(where, "the first rank needs \"xp\": 0 (every unit has at least that rank)");
    if (i > 0 && r.xp <= rules.ladder.back().xp)
      return c.fail(where, "\"xp\" must be higher than the one of \"" + rules.ladder.back().id + "\" before it");
    rules.ladder.push_back(r);
  }
  if (!db.rules.items.carriedWeapons)
    return c.fail("", "weapon ranks need carried weapons (game.json rules.items.weapons = \"carried\"): with fixed weapons there is nothing to restrict");
  db.ranks = rules;
  return true;
}

bool loadStatCaps(const std::string& path, Database& db, std::string& err) {
  db.statCaps.clear();
  db.computerCaps.clear();
  if (!fileExists(path)) return true;
  Json root;
  if (!loadObjectFile(path, root, err)) return false;
  LoadCtx c(err, path);
  for (const Json::Member& m : root.members())
    if (m.first != "caps" && m.first != "computerCaps") return c.fail("", unknownKeyText(m.first, {"caps", "computerCaps"}));
  const Json* caps;
  if (!c.object(root, "caps", "", caps, true)) return false;
  // One map of stat id -> cap, from "caps" or "computerCaps"; empty when it names nothing.
  const auto readMap = [&](const Json& map, const char* where, std::vector<int>& dest) {
    if (map.size() == 0) return true;
    std::vector<int> out(db.stats.size(), -1);
    for (const Json::Member& m : map.members()) {
      int s = db.findStat(m.first);
      if (s < 0) return c.fail(where, "names unknown stat \"" + m.first + "\"");
      if (!LoadCtx::isWhole(m.second) || m.second.asInt() < 1 || m.second.asInt() > kMaxStatCap)
        return c.fail(where, "\"" + m.first + "\" must be a whole number from 1 to " + std::to_string(kMaxStatCap));
      out[(size_t)s] = m.second.asInt();
    }
    dest = out;
    return true;
  };
  if (!readMap(*caps, "caps", db.statCaps)) return false;
  // "computerCaps": the same list for units of computer-controlled sides (it replaces the game-wide cap for them; a class's own
  // cap still wins). Docs: docs/SRPG.md "Stat caps".
  if (root.has("computerCaps")) {
    const Json* cc;
    if (!c.object(root, "computerCaps", "", cc)) return false;
    if (!readMap(*cc, "computerCaps", db.computerCaps)) return false;
  }
  return true;
}

bool readWeaponRank(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, WeaponDef& w) {
  if (!e.has("rank") && !e.has("rankXp")) return true;
  const char* key = e.has("rank") ? "rank" : "rankXp";
  if (!db.ranks.on()) return c.fail(where, std::string("\"") + key + "\" is given but " + kNoRanks);
  if (w.group.empty()) return c.fail(where, std::string("\"") + key + "\" needs a \"group\" (ranks are kept per weapon group)");
  if (e.has("rank")) {
    std::string id;
    if (!c.str(e, "rank", where, id)) return false;
    w.rankNeed = db.ranks.findRank(id);
    if (w.rankNeed < 0) return c.fail(where, "\"rank\" names unknown rank \"" + id + "\" (data/ranks.json)");
  }
  if (!c.integer(e, "rankXp", where, w.rankXp)) return false;
  // 0 is allowed: a weapon that trains nothing (the key absent keeps the default 1). The message keeps the "1 to 99" words.
  if (w.rankXp < 0 || w.rankXp > kMaxRankUseXp)
    return c.fail(where, "\"rankXp\" must be from 1 to " + std::to_string(kMaxRankUseXp) + ", or 0 for a weapon that trains nothing");
  return true;
}

void buildRankGroups(Database& db) {
  db.ranks.groups.clear();
  if (!db.ranks.on()) return;
  for (WeaponDef& w : db.weapons) {
    if (w.group.empty()) continue;
    int g = db.ranks.findGroup(w.group);
    if (g < 0) {
      g = (int)db.ranks.groups.size();
      db.ranks.groups.push_back(w.group);
    }
    w.rankGroup = g;
  }
}

bool readRankMap(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, int unset,
                 std::vector<int>& out) {
  if (!e.has("ranks")) return true;
  if (!db.ranks.on()) return c.fail(where, std::string("\"ranks\" is given but ") + kNoRanks);
  const Json* m;
  if (!c.object(e, "ranks", where, m)) return false;
  out.assign(db.ranks.groups.size(), unset);
  for (const Json::Member& g : m->members()) {
    const int gi = db.ranks.findGroup(g.first);
    if (gi < 0)
      return c.fail(where, "\"ranks\" names unknown weapon group \"" + g.first + "\" (no weapon in data/weapons.json has that \"group\")");
    int xp = 0;
    if (g.second.isString()) {
      const int r = db.ranks.findRank(g.second.asString());
      if (r < 0) return c.fail(where, "\"ranks\".\"" + g.first + "\" names unknown rank \"" + g.second.asString() + "\" (data/ranks.json)");
      xp = db.ranks.ladder[(size_t)r].xp;
    } else if (LoadCtx::isWhole(g.second) && g.second.asInt() >= 0 && g.second.asInt() <= db.ranks.topXp()) {
      xp = g.second.asInt();
    } else {
      return c.fail(where, "\"ranks\".\"" + g.first + "\" must be a rank id or weapon experience from 0 to " +
                               std::to_string(db.ranks.topXp()));
    }
    out[(size_t)gi] = xp;
  }
  return true;
}

bool readUnitRanks(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, UnitTypeDef& u) {
  if (!readRankMap(c, db, e, where, 0, u.rankStart)) return false;
  if (!e.has("caps")) return true;
  u.caps.assign(db.stats.size(), -1);
  const Json* m;
  if (!c.object(e, "caps", where, m)) return false;
  for (const Json::Member& s : m->members()) {
    const int si = db.findStat(s.first);
    if (si < 0) return c.fail(where, "\"caps\" names unknown stat \"" + s.first + "\"");
    if (!LoadCtx::isWhole(s.second) || s.second.asInt() < 1 || s.second.asInt() > kMaxStatCap)
      return c.fail(where, "\"caps\".\"" + s.first + "\" must be a whole number from 1 to " + std::to_string(kMaxStatCap));
    u.caps[(size_t)si] = s.second.asInt();
  }
  return true;
}

bool readCharacterRanks(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, CharacterDef& ch) {
  if (!readRankMap(c, db, e, where, 0, ch.rankStart)) return false;
  if (!e.has("capAdjust")) return true;
  ch.capAdjust.assign(db.stats.size(), 0);
  return c.statMap(db, e, "capAdjust", where, ch.capAdjust);
}

}  // namespace ote
