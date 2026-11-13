#include "core/data/loader.h"

#include <cctype>

#include "core/data/loader_content.h"
#include "core/data/loader_util.h"

namespace ote {

uint32_t parseColor(const std::string& text, uint32_t def) {
  if (text.size() != 7 || text[0] != '#') return def;
  uint32_t v = 0;
  for (size_t i = 1; i < 7; ++i) {
    char ch = text[i];
    uint32_t d;
    if (ch >= '0' && ch <= '9') d = static_cast<uint32_t>(ch - '0');
    else if (ch >= 'a' && ch <= 'f') d = static_cast<uint32_t>(ch - 'a' + 10);
    else if (ch >= 'A' && ch <= 'F') d = static_cast<uint32_t>(ch - 'A' + 10);
    else return def;
    v = (v << 4) | d;
  }
  return v;
}

bool loadObjectFile(const std::string& path, Json& out, std::string& err) {
  if (!loadJsonFile(path, out, err)) return false;
  if (!out.isObject()) {
    err = path + ": the file must contain a JSON object at the top level";
    return false;
  }
  return true;
}

// Optional "enabled" flag + the block must be an object when present.
static bool readBlock(const LoadCtx& c, const Json& rules, const char* key, const Json*& blk,
                      bool& enabled) {
  if (!c.object(rules, key, "rules", blk)) return false;
  return c.boolean(*blk, "enabled", std::string("rules.") + key, enabled);
}

static bool loadRules(const LoadCtx& c, const Json& root, Database& db) {
  RulesConfig& r = db.rules;
  const Json* rules;
  if (!c.object(root, "rules", "", rules)) return false;

  std::string order = "sides";
  if (!c.str(*rules, "turnOrder", "rules", order)) return false;
  if (order != "sides")
    return c.fail("rules", "turnOrder \"" + order + "\" is not supported yet (only \"sides\")");
  if (!c.boolean(*rules, "counterAttacks", "rules", r.counterAttacks)) return false;

  if (rules->has("defeat")) {
    std::vector<std::string> names;
    if (!c.stringList(*rules, "defeat", "rules", names)) return false;
    r.defeatNoUnits = r.defeatLeaderDead = r.defeatLostHq = false;
    for (const std::string& n : names) {
      if (n == "noUnits") r.defeatNoUnits = true;
      else if (n == "leaderDead") r.defeatLeaderDead = true;
      else if (n == "lostHq") r.defeatLostHq = true;
      else return c.fail("rules", "unknown defeat condition \"" + n +
                                      "\" (expected noUnits, leaderDead or lostHq)");
    }
  }

  const Json* blk;
  if (!readBlock(c, *rules, "economy", blk, r.economy)) return false;
  if (!readBlock(c, *rules, "production", blk, r.production)) return false;
  if (!c.boolean(*blk, "builtUnitsCanAct", "rules.production", r.builtUnitsCanAct)) return false;

  if (!readBlock(c, *rules, "capture", blk, r.capture)) return false;
  if (r.capture) r.fCaptureAmount = "a.hp";
  if (!c.str(*blk, "amount", "rules.capture", r.fCaptureAmount)) return false;

  if (!readBlock(c, *rules, "progression", blk, r.progression)) return false;
  if (r.progression) { r.fXpHit = "10"; r.fXpKill = "30"; }
  if (!c.integer(*blk, "xpPerLevel", "rules.progression", r.xpPerLevel)) return false;
  if (!c.integer(*blk, "maxLevel", "rules.progression", r.maxLevel)) return false;
  if (!c.str(*blk, "xpHit", "rules.progression", r.fXpHit)) return false;
  if (!c.str(*blk, "xpKill", "rules.progression", r.fXpKill)) return false;
  if (r.xpPerLevel < 1) return c.fail("rules.progression", "\"xpPerLevel\" must be at least 1");
  if (r.maxLevel < 1) return c.fail("rules.progression", "\"maxLevel\" must be at least 1");

  const Json* f;
  if (!c.object(root, "formulas", "", f)) return false;
  if (!c.str(*f, "damage", "formulas", r.fDamage, true)) return false;
  if (r.fDamage.empty()) return c.fail("formulas", "\"damage\" must not be empty");
  if (!c.str(*f, "hit", "formulas", r.fHit)) return false;
  if (!c.str(*f, "crit", "formulas", r.fCrit)) return false;
  if (!c.str(*f, "strikes", "formulas", r.fStrikes)) return false;
  return c.num(*f, "critMultiplier", "formulas", r.critMultiplier);
}

static bool loadGameFile(const std::string& path, Database& db, std::string& err) {
  Json root;
  if (!loadObjectFile(path, root, err)) return false;
  LoadCtx c(err, path);

  if (!root.has("formatVersion") || !root["formatVersion"].isNumber() ||
      root["formatVersion"].asNumber() != 1)
    return c.fail("", "\"formatVersion\" must be 1");
  if (!c.str(root, "name", "", db.name)) return false;

  const Json* view;
  if (!c.object(root, "view", "", view)) return false;
  if (!c.integer(*view, "width", "view", db.view.width)) return false;
  if (!c.integer(*view, "height", "view", db.view.height)) return false;
  if (!c.integer(*view, "tileSize", "view", db.view.tileSize)) return false;
  if (db.view.width < 1 || db.view.height < 1 || db.view.tileSize < 1)
    return c.fail("view", "width, height and tileSize must be positive");

  if (!readIdList(c, root, "stats", true, db.stats)) return false;
  if (!readIdList(c, root, "resources", false, db.resources)) return false;
  if (!readIdList(c, root, "moveTypes", true, db.moveTypes)) return false;

  const Json* roles;
  if (!c.object(root, "roles", "", roles, true)) return false;
  struct Role { const char* key; int* target; };
  Role list[] = {{"hp", &db.rules.statHp}, {"move", &db.rules.statMove}};
  for (const Role& role : list) {
    std::string id;
    if (!c.str(*roles, role.key, "roles", id, true)) return false;
    *role.target = db.findStat(id);
    if (*role.target < 0)
      return c.fail("roles", std::string("\"") + role.key + "\" names unknown stat \"" + id + "\"");
  }

  if (!loadRules(c, root, db)) return false;
  if (!c.stringList(root, "scripts", "", db.scripts)) return false;

  const Json* maps;
  if (!c.array(root, "maps", "", maps)) return false;
  for (size_t i = 0; i < maps->size(); ++i) {
    const Json& m = (*maps)[i];
    std::string where = "maps[" + std::to_string(i) + "]";
    if (!m.isObject()) return c.fail(where, "must be an object");
    MapRef ref;
    if (!c.str(m, "id", where, ref.id, true) || ref.id.empty()) return c.fail(where, "\"id\" is required");
    where = "map \"" + ref.id + "\"";
    if (db.findMap(ref.id) >= 0) return c.fail(where, "duplicate id");
    ref.name = ref.id;
    if (!c.str(m, "name", where, ref.name)) return false;
    if (!c.str(m, "file", where, ref.file, true)) return false;
    db.maps.push_back(ref);
  }
  return true;
}

bool loadGame(const std::string& gameDir, Database& out, std::string& err) {
  Database db;
  std::string root = gameDir;
  while (root.size() > 1 && (root.back() == '/' || root.back() == '\\')) root.pop_back();
  db.rootPath = root;

  const std::string weaponsPath = root + "/data/weapons.json";
  // Order matters: each step resolves ids defined by the previous ones.
  if (!loadGameFile(root + "/game.json", db, err)) return false;
  if (!loadWeapons(weaponsPath, db, err)) return false;
  if (!loadUnitTypes(root + "/data/units.json", db, err)) return false;
  if (!loadTerrain(root + "/data/terrain.json", db, err)) return false;
  if (!checkVersusKeys(weaponsPath, db, err)) return false;

  out = db;
  err.clear();
  return true;
}

}  // namespace ote
