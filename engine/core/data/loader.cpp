#include "core/data/loader.h"

#include <cctype>
#include <cstdio>

#include "core/base/fileio.h"
#include "core/data/campaign.h"
#include "core/data/events.h"
#include "core/data/loader_action_points.h"
#include "core/data/loader_facing.h"
#include "core/data/loader_height.h"
#include "core/data/loader_jobs.h"
#include "core/data/loader_teaching.h"
#include "core/data/loader_unit_pools.h"
#include "core/data/loader_unit_ranks.h"
#include "core/data/loader_content.h"
#include "core/data/loader_research_branch.h"
#include "core/data/loader_conditions.h"
#include "core/data/loader_damage_types.h"
#include "core/data/loader_difficulty.h"
#include "core/data/loader_commanders.h"
#include "core/data/loader_factions.h"
#include "core/data/loader_places.h"
#include "core/data/loader_ranks.h"
#include "core/data/loader_speed.h"
#include "core/data/loader_srpg.h"
#include "core/data/loader_staves.h"
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

bool fileExists(const std::string& path) {
  FILE* f = openFile(path, "rb");
  if (!f) return false;
  std::fclose(f);
  return true;
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

// readBlock plus the unknown-key check of the block (`keys` = every key it takes, "enabled" included).
static bool readCheckedBlock(const LoadCtx& c, const Json& rules, const char* key, std::initializer_list<const char*> keys,
                             const Json*& blk, bool& enabled) {
  return readBlock(c, rules, key, blk, enabled) && c.onlyKeys(*blk, keys, std::string("rules.") + key);
}

static bool loadRules(const LoadCtx& c, const Json& root, Database& db) {
  RulesConfig& r = db.rules;
  const Json* rules;
  if (!c.object(root, "rules", "", rules)) return false;
  if (!c.onlyKeys(*rules, {"counterAttacks", "defeat", "economy", "production", "capture", "items", "supply", "research", "fog", "display",
                           "hitRoll", "rescue", "promotion", "places", "actionPoints", "turnOrder", "facing", "jobs", "height",
                           "progression", "ranks", "ai", "abilityPoints"},
                  "rules"))
    return false;

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
  if (!readCheckedBlock(c, *rules, "economy", {"enabled"}, blk, r.economy)) return false;
  if (!readCheckedBlock(c, *rules, "production", {"enabled", "builtUnitsCanAct"}, blk, r.production)) return false;
  if (!c.boolean(*blk, "builtUnitsCanAct", "rules.production", r.builtUnitsCanAct)) return false;

  if (!readCheckedBlock(c, *rules, "capture", {"enabled", "amount"}, blk, r.capture)) return false;
  if (r.capture) r.fCaptureAmount = "a.hp";
  if (!c.str(*blk, "amount", "rules.capture", r.fCaptureAmount)) return false;

  // Items (docs/ITEMS.md): no "enabled" flag; a game has items when data/items.json lists some.
  if (!c.object(*rules, "items", "rules", blk)) return false;
  // ("freeBattleStorage" is read by loadFreeBattleStock, after the items exist.)
  if (!c.onlyKeys(*blk, {"weapons", "storage", "inventorySize", "sellPercent", "freeBattleStorage", "drops"}, "rules.items")) return false;
  std::string weapons = "fixed", storage = "off", drops = "marked";
  if (!c.str(*blk, "weapons", "rules.items", weapons) || !c.str(*blk, "storage", "rules.items", storage) || !c.str(*blk, "drops", "rules.items", drops) ||
      !c.integer(*blk, "inventorySize", "rules.items", r.items.inventorySize))
    return false;
  if (weapons != "fixed" && weapons != "carried")
    return c.fail("rules.items", "\"weapons\" must be \"fixed\" or \"carried\", not \"" + weapons + "\"");
  r.items.carriedWeapons = weapons == "carried";
  if (drops != "marked" && drops != "last")  // docs/ITEMS.md "Drops"
    return c.fail("rules.items", "\"drops\" must be \"marked\" (every item marked to drop) or \"last\" (only the last item that is not the weapon in hand), not \"" + drops + "\"");
  r.items.dropLast = drops == "last";
  if (storage == "off") r.items.storage = ItemStorage::Off;
  else if (storage == "anywhere") r.items.storage = ItemStorage::Anywhere;
  else if (storage == "structure") r.items.storage = ItemStorage::Structure;
  else if (storage == "leader") r.items.storage = ItemStorage::Leader;
  else return c.fail("rules.items", "\"storage\" must be \"off\", \"anywhere\", \"structure\" or \"leader\"");
  if (r.items.inventorySize < 1 || r.items.inventorySize > kMaxInventory)
    return c.fail("rules.items", "\"inventorySize\" must be from 1 to " + std::to_string(kMaxInventory));
  if (!c.integer(*blk, "sellPercent", "rules.items", r.items.sellPercent)) return false;  // a buying shop's price
  if (r.items.sellPercent < 0 || r.items.sellPercent > 100)
    return c.fail("rules.items", "\"sellPercent\" must be from 0 to 100 (the share of an item's price a shop pays for it)");

  if (!loadSupplyRules(c, *rules, db)) return false;
  if (!loadOptionRules(c, *rules, db)) return false;
  // Strategy-RPG features (docs/SRPG.md): "rules.rescue" and "rules.promotion", each optional.
  if (!loadRescueRules(c, *rules, db) || !loadPromotionRules(c, *rules, db)) return false;
  if (!loadPlaceRules(c, *rules, db)) return false;  // "rules.places" (docs/SRPG.md section 10)
  if (!loadActionPointRules(c, *rules, db)) return false;  // "rules.actionPoints" (docs/SRPG.md "Action points")
  if (!loadSpeedRules(c, *rules, db)) return false;  // "rules.turnOrder" (docs/TURN_ORDER.md); after actionPoints and production, which it cannot be combined with
  if (!loadFacingRules(c, *rules, db)) return false;  // "rules.facing" (docs/SRPG.md "Facing"); before the maps whose units may say a "facing"
  if (!loadJobRules(c, *rules, db)) return false;  // "rules.jobs" (docs/JOBS.md); data/jobs.json is read after the unit types
  if (!loadAbilityPointRules(c, *rules, db)) return false;  // "rules.abilityPoints" (docs/JOBS.md "Learning from equipment")
  if (!loadHeightRules(c, *rules, db)) return false;  // "rules.height" (docs/SRPG.md "Height rules"); before the weapons and unit types that use its keys

  // ("levels" is the rank table, read by loadUnitRankRules; "computerGains" and "xpNoDamage" by loadProgressionExtras.)
  if (!readCheckedBlock(c, *rules, "progression", {"enabled", "xpPerLevel", "maxLevel", "xpHit", "xpKill", "levels", "computerGains", "xpNoDamage"},
                        blk, r.progression))
    return false;
  if (r.progression) { r.fXpHit = "10"; r.fXpKill = "30"; }
  if (!c.integer(*blk, "xpPerLevel", "rules.progression", r.xpPerLevel)) return false;
  if (!c.integer(*blk, "maxLevel", "rules.progression", r.maxLevel)) return false;
  if (!c.str(*blk, "xpHit", "rules.progression", r.fXpHit)) return false;
  if (!c.str(*blk, "xpKill", "rules.progression", r.fXpKill)) return false;
  if (!loadProgressionExtras(c, *blk, db) || !loadUnitRankRules(c, *blk, db)) return false;  // the rank table (docs/SRPG.md "Unit ranks") excludes xpPerLevel / maxLevel
  if (!loadMeritRankRules(c, *rules, db)) return false;  // "rules.ranks": the same table on its own counter, beside levelling (refused with the table above)
  if (r.xpPerLevel < 1) return c.fail("rules.progression", "\"xpPerLevel\" must be at least 1");
  if (r.maxLevel < 1) return c.fail("rules.progression", "\"maxLevel\" must be at least 1");
  if (r.xpPerLevel > 1000000) return c.fail("rules.progression", "\"xpPerLevel\" must be at most 1000000");
  // Growth is rolled once per level a unit gains: the cap keeps a typo ("maxLevel": 100000000) from freezing a battle.
  if (r.maxLevel > 9999) return c.fail("rules.progression", "\"maxLevel\" must be at most 9999");
  if (r.promotion.testAt > r.maxLevel)
    return c.fail("rules.promotion", "\"testAt\" must be from 1 to the game's top level (" + std::to_string(r.maxLevel) + ", rules.progression.maxLevel)");
  if (r.promotion.testAt > 0 && r.unitRanks.replacesLevels())
    return c.fail("rules.promotion", "\"testAt\" needs levels, and this game uses the experience rank table (rules.progression \"levels\") instead");

  const Json* f;
  if (!c.object(root, "formulas", "", f)) return false;
  if (!c.onlyKeys(*f, {"damage", "hit", "crit", "critMultiplier", "strikes"}, "formulas")) return false;
  if (!c.str(*f, "damage", "formulas", r.fDamage, true)) return false;
  if (r.fDamage.empty()) return c.fail("formulas", "\"damage\" must not be empty");
  if (!c.str(*f, "hit", "formulas", r.fHit)) return false;
  if (!c.str(*f, "crit", "formulas", r.fCrit)) return false;
  if (!c.str(*f, "strikes", "formulas", r.fStrikes)) return false;
  if (!c.num(*f, "critMultiplier", "formulas", r.critMultiplier)) return false;
  if (r.critMultiplier < 0 || r.critMultiplier > 1000)
    return c.fail("formulas", "\"critMultiplier\" must be a number from 0 to 1000 (what a critical hit multiplies the damage by)");
  return true;
}

static bool loadGameFile(const std::string& path, Database& db, std::string& err) {
  Json root;
  if (!loadObjectFile(path, root, err)) return false;
  LoadCtx c(err, path);

  // "editor": the web editor keeps its own bookkeeping there (which formula preset a text came from; editor/web model/formulas.ts);
  // nothing in the game reads it. Quiet: accepted, never listed in a message.
  if (!c.onlyKeys(root, {"formatVersion", "name", "description", "author", "saving", "battleScene", "battleBackgroundFrom", "battlePlatformFrom",
                         "battleSceneSides", "battleSceneStaves", "quotesPerFight", "menuUnavailable", "attackWeaponChoice", "grid", "view", "font", "stats",
                         "resources", "unitResources", "moveTypes", "switches", "variables", "roles", "rules", "formulas", "damageTypes", "resistRange",
                         "difficulties", "styles", "scripts", "maps"},
                  "", {"editor"}))
    return false;
  if (!root.has("formatVersion") || !root["formatVersion"].isNumber() ||
      root["formatVersion"].asNumber() != 1)
    return c.fail("", "\"formatVersion\" must be 1");
  if (!c.str(root, "name", "", db.name)) return false;
  if (!c.str(root, "description", "", db.description)) return false;
  if (!c.str(root, "author", "", db.author)) return false;
  std::string saving = "any";
  if (!c.str(root, "saving", "", saving)) return false;
  if (!parseSavingMode(saving, db.saving))
    return c.fail("", "\"saving\" must be \"any\", \"continue\" or \"off\" (got \"" + saving + "\")");
  std::string scene = "map";
  if (!c.str(root, "battleScene", "", scene)) return false;
  if (scene != "map" && scene != "scene")
    return c.fail("", "\"battleScene\" must be \"map\" (quick fights on the map) or \"scene\" (the side-view battle scene), got \"" + scene + "\"");
  db.battleSceneDefault = scene == "scene";
  std::string bgFrom = "defender";  // docs/BATTLE_SCENE.md "Which background and platform"
  if (!c.str(root, "battleBackgroundFrom", "", bgFrom)) return false;
  if (bgFrom != "defender" && bgFrom != "attacker")
    return c.fail("", "\"battleBackgroundFrom\" must be \"defender\" (the square of the fighter that is struck, the default) or \"attacker\", got \"" + bgFrom + "\"");
  db.battleBackgroundFromAttacker = bgFrom == "attacker";
  std::string platFrom = "same";  // docs/BATTLE_SCENE.md "Platforms"
  if (!c.str(root, "battlePlatformFrom", "", platFrom)) return false;
  if (platFrom != "same" && platFrom != "own")
    return c.fail("", "\"battlePlatformFrom\" must be \"same\" (both fighters stand on the platform of the square \"battleBackgroundFrom\" names, the default) or \"own\" (each on the platform of its own square), got \"" + platFrom + "\"");
  db.battlePlatformOwn = platFrom == "own";
  std::string sides = "map";  // docs/BATTLE_SCENE.md "Which side each fighter stands on"
  if (!c.str(root, "battleSceneSides", "", sides)) return false;
  if (sides != "map" && sides != "playerRight" && sides != "playerLeft")
    return c.fail("", "\"battleSceneSides\" must be \"map\" (the unit nearer the left edge of the map stands on the left, the default), \"playerRight\" or \"playerLeft\", got \"" + sides + "\"");
  db.battleSceneSides = sides == "playerRight" ? 1 : sides == "playerLeft" ? 2 : 0;
  db.battleSceneStaves = false;  // docs/BATTLE_SCENE.md "Staves in the scene"
  if (!c.boolean(root, "battleSceneStaves", "", db.battleSceneStaves)) return false;
  std::string perFight = "both";  // docs/EVENTS.md "Battle and defeat quotes"
  if (!c.str(root, "quotesPerFight", "", perFight)) return false;
  if (perFight != "both" && perFight != "one")
    return c.fail("", "\"quotesPerFight\" must be \"both\" (each fighter may say its battle quote, the default) or \"one\" (at most one battle quote per fight), got \"" + perFight + "\"");
  db.quotesOnePerFight = perFight == "one";
  // Menu options of the player (docs/SRPG.md "Menu options"): presentation of the action menu and the attack flow; no rule changes.
  std::string unavailable = "grey";
  if (!c.str(root, "menuUnavailable", "", unavailable)) return false;
  if (unavailable != "grey" && unavailable != "hide")
    return c.fail("", "\"menuUnavailable\" must be \"grey\" (an action with nothing to act on stays in the menu, greyed; the default) or \"hide\" (it is left out), got \"" + unavailable + "\"");
  db.menuHideUnavailable = unavailable == "hide";
  std::string weaponChoice = "equipped";
  if (!c.str(root, "attackWeaponChoice", "", weaponChoice)) return false;
  if (weaponChoice != "equipped" && weaponChoice != "pick")
    return c.fail("", "\"attackWeaponChoice\" must be \"equipped\" (Attack goes straight to the targets, the default) or \"pick\" (Attack asks which weapon to use first), got \"" + weaponChoice + "\"");
  db.attackWeaponPick = weaponChoice == "pick";

  if (!readGameGrid(c, root, db)) return false;

  const Json* view;
  if (!c.object(root, "view", "", view)) return false;
  if (!c.onlyKeys(*view, {"width", "height", "tileSize"}, "view")) return false;
  if (!c.integer(*view, "width", "view", db.view.width)) return false;
  if (!c.integer(*view, "height", "view", db.view.height)) return false;
  if (!c.integer(*view, "tileSize", "view", db.view.tileSize)) return false;
  if (db.view.width < 1 || db.view.height < 1 || db.view.tileSize < 1)
    return c.fail("view", "width, height and tileSize must be positive");
  if (db.view.width > 16384 || db.view.height > 16384 || db.view.tileSize > 1024)
    return c.fail("view", "\"width\" and \"height\" must be at most 16384 and \"tileSize\" at most 1024 (logical pixels)");

  const Json* font;
  if (!c.object(root, "font", "", font)) return false;
  if (!c.onlyKeys(*font, {"name", "file", "size", "smooth"}, "font")) return false;
  if (!c.str(*font, "name", "font", db.font.name)) return false;
  if (!c.str(*font, "file", "font", db.font.file)) return false;
  if (!c.integer(*font, "size", "font", db.font.size)) return false;
  bool smooth = false;
  if (font->has("smooth")) {
    if (!c.boolean(*font, "smooth", "font", smooth)) return false;
    db.font.smooth = smooth ? 1 : 0;
  }
  if (db.font.size < 0 || db.font.size > 200) return c.fail("font", "\"size\" must be between 0 (the font's own size) and 200");
  if (font->has("name") && db.font.name.empty()) return c.fail("font", "\"name\" must not be empty");
  if (!db.font.file.empty()) {
    // Keep font paths inside the game folder so games stay self-contained.
    if (db.font.file[0] == '/' || db.font.file.find("..") != std::string::npos)
      return c.fail("font", "\"file\" must be a path inside the game folder");
    std::string probe;
    if (!readTextFile(db.rootPath + "/" + db.font.file, probe))
      return c.fail("font", "cannot read font file \"" + db.font.file + "\"");
  }

  if (!readIdList(c, root, "stats", true, db.stats)) return false;
  if (!readIdList(c, root, "resources", false, db.resources, {"refill"})) return false;
  if (!loadResourceRefills(c, root, db)) return false;  // "refill" of a resource (docs/ECONOMY.md "Refilling resources")
  if (!loadUnitResources(c, root, db)) return false;  // per-unit pools (docs/SRPG.md "Unit resources")
  if (!readIdList(c, root, "moveTypes", true, db.moveTypes, {"sounds"})) return false;
  // Story switches and variables (docs/EVENTS.md); ids are what events refer to.
  if (!readIdList(c, root, "switches", false, db.switches)) return false;
  if (!readIdList(c, root, "variables", false, db.variables)) return false;
  for (const SwitchDef& sw : db.switches)
    if (!isStoryId(sw.id)) return c.fail("switches \"" + sw.id + "\"", "\"id\" must be letters, digits, \"_\" and \"-\"");
  for (const VariableDef& v : db.variables)
    if (!isStoryId(v.id)) return c.fail("variables \"" + v.id + "\"", "\"id\" must be letters, digits, \"_\" and \"-\"");

  const Json* roles;
  if (!c.object(root, "roles", "", roles, true)) return false;
  if (!c.onlyKeys(*roles, {"hp", "move"}, "roles")) return false;
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
  if (!loadDamageTypes(c, root, db)) return false;  // docs/SRPG.md "Damage types"; before the weapons that name one
  if (!loadDifficulties(c, root, db)) return false;  // docs/DIFFICULTY.md
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
    if (!c.onlyKeys(m, {"id", "name", "file"}, where)) return false;
    ref.name = ref.id;
    if (!c.str(m, "name", where, ref.name)) return false;
    if (!c.str(m, "file", where, ref.file, true)) return false;
    db.maps.push_back(ref);
  }
  return true;
}

static bool isHexColor(const std::string& t) {
  if (t.size() != 7 || t[0] != '#') return false;
  for (size_t i = 1; i < 7; ++i) {
    char ch = t[i];
    if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))) return false;
  }
  return true;
}

// "styles" refer to terrain and structure ids, so they are read after those files.
// The list always ends up with a default style at index 0.
static bool loadStyles(const std::string& path, Database& db, std::string& err) {
  Json root;
  if (!loadObjectFile(path, root, err)) return false;
  LoadCtx c(err, path);
  const Json* list;
  if (!c.array(root, "styles", "", list)) return false;

  for (size_t i = 0; i < list->size(); ++i) {
    const Json& j = (*list)[i];
    std::string where = "styles[" + std::to_string(i) + "]";
    if (!j.isObject()) return c.fail(where, "must be an object");
    StyleDef st;
    if (!c.str(j, "id", where, st.id, true)) return false;
    if (st.id.empty()) return c.fail(where, "\"id\" must not be empty");
    where = "style \"" + st.id + "\"";
    if (db.findStyle(st.id) >= 0) return c.fail(where, "duplicate id");
    if (!c.onlyKeys(j, {"id", "name", "background", "largeArt", "view", "isometric", "showGlyphs", "terrainColors", "structureColors"}, where)) return false;
    st.name = st.id;
    if (!c.str(j, "name", where, st.name)) return false;
    std::string bg;
    if (!c.str(j, "background", where, bg)) return false;
    if (!bg.empty()) {
      if (!isHexColor(bg)) return c.fail(where, "\"background\" must look like \"#rrggbb\"");
      st.hasBackground = true;
      st.background = parseColor(bg, 0);
    }
    if (!c.boolean(j, "largeArt", where, st.largeArt)) return false;
    std::string view;
    if (!c.str(j, "view", where, view)) return false;
    if (view.empty() || view == "top-down") st.view = StyleView::TopDown;
    else if (view == "isometric") st.view = StyleView::Isometric;
    else if (view == "isometric-3d") st.view = StyleView::Isometric3D;
    else if (view == "3d") st.view = StyleView::Full3D;
    else return c.fail(where, "\"view\" must be \"top-down\", \"isometric\", \"isometric-3d\" or \"3d\"");
    // hexGameAllowsView lists the kinds hexagon games may use (all four today; a future kind stays refused until built).
    if (db.gridType == GridType::Hex && !hexGameAllowsView(st.view))
      return c.fail(where, "the \"" + view + "\" view is not available for hexagon games yet");
    const Json* iso;
    if (!c.object(j, "isometric", where, iso)) return false;
    if (j.has("isometric")) {
      std::string iw = where + " \"isometric\"";
      if (!c.onlyKeys(*iso, {"tileWidth", "tileHeight", "heightStep", "unitWidth"}, iw)) return false;
      if (!c.integer(*iso, "tileWidth", iw, st.iso.tileWidth) || !c.integer(*iso, "tileHeight", iw, st.iso.tileHeight) ||
          !c.integer(*iso, "heightStep", iw, st.iso.heightStep) || !c.integer(*iso, "unitWidth", iw, st.iso.unitWidth))
        return false;
      if (iso->has("tileWidth") && (st.iso.tileWidth < 8 || st.iso.tileWidth > 512 || st.iso.tileWidth % 2))
        return c.fail(iw, "\"tileWidth\" must be an even number from 8 to 512");
      if (iso->has("tileHeight") && db.gridType == GridType::Hex)  // the squash of a hexagon is fixed (docs/HEX.md)
        return c.fail(iw, "\"tileHeight\" is not used on hexagon games (the squash is fixed; \"tileWidth\" sets the size)");
      if (iso->has("tileHeight") && (st.iso.tileHeight < 4 || st.iso.tileHeight > 256 || st.iso.tileHeight % 2))
        return c.fail(iw, "\"tileHeight\" must be an even number from 4 to 256");
      if (iso->has("heightStep") && (st.iso.heightStep < 0 || st.iso.heightStep > 128))
        return c.fail(iw, "\"heightStep\" must be from 0 to 128");
      if (iso->has("unitWidth") && (st.iso.unitWidth < 4 || st.iso.unitWidth > 512))
        return c.fail(iw, "\"unitWidth\" must be from 4 to 512");
    }
    if (j.has("showGlyphs")) {
      bool show = true;
      if (!c.boolean(j, "showGlyphs", where, show)) return false;
      st.showGlyphs = show ? 1 : 0;
    }
    st.terrainColors.assign(db.terrains.size(), -1);
    const Json* colors;
    if (!c.object(j, "terrainColors", where, colors)) return false;
    for (const Json::Member& m : colors->members()) {
      int t = db.findTerrain(m.first);
      if (t < 0) return c.fail(where, "\"terrainColors\" names unknown terrain \"" + m.first + "\"");
      if (!m.second.isString() || !isHexColor(m.second.asString()))
        return c.fail(where, "\"terrainColors\".\"" + m.first + "\" must look like \"#rrggbb\"");
      st.terrainColors[(size_t)t] = (int32_t)parseColor(m.second.asString(), 0);
    }
    st.structureColors.assign(db.structures.size(), -1);
    const Json* scolors;
    if (!c.object(j, "structureColors", where, scolors)) return false;
    for (const Json::Member& m : scolors->members()) {
      int s = db.findStructure(m.first);
      if (s < 0) return c.fail(where, "\"structureColors\" names unknown structure \"" + m.first + "\"");
      if (!m.second.isString() || !isHexColor(m.second.asString()))
        return c.fail(where, "\"structureColors\".\"" + m.first + "\" must look like \"#rrggbb\"");
      st.structureColors[(size_t)s] = (int32_t)parseColor(m.second.asString(), 0);
    }
    db.styles.push_back(st);
  }
  if (db.styles.empty()) {
    StyleDef def;
    def.id = "default";
    def.name = "Default";
    def.terrainColors.assign(db.terrains.size(), -1);
    def.structureColors.assign(db.structures.size(), -1);
    db.styles.push_back(def);
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
  // Strategy-RPG features (docs/SRPG.md "Load order"): skills and abilities first (ids and shapes only), because
  // weapons, items and unit types name them; what they name themselves is resolved at the very end.
  if (!loadSkills(root + "/data/skills.json", db, err)) return false;
  // Conditions (docs/SRPG.md section 9) come right after skills (their blocks are skill blocks, kept as hidden skills) and
  // before everything that names one: weapons ("inflict"), items and staves ("cure", "inflict").
  if (!loadConditions(root + "/data/conditions.json", db, err)) return false;
  if (!loadAbilities(root + "/data/abilities.json", db, err)) return false;
  // Weapon ranks and stat caps (docs/SRPG.md section 7): the ladder before the weapons that name a rank; the groups
  // after them, because unit types, characters and map units name groups.
  if (!loadRanks(root + "/data/ranks.json", db, err)) return false;
  if (!loadStatCaps(root + "/data/caps.json", db, err)) return false;
  if (!loadWeapons(weaponsPath, db, err)) return false;
  buildRankGroups(db);
  if (!loadItems(root + "/data/items.json", db, err)) return false;
  addStaffRankGroups(db);  // staff groups after the weapons' (docs/SRPG.md section 8)
  if (!loadFreeBattleStock(root + "/game.json", db, err)) return false;  // needs the items (and the resources of game.json)
  if (!checkAmmoNotItems(weaponsPath, db, err)) return false;
  if (!loadUnitTypes(root + "/data/units.json", db, err)) return false;
  if (!loadTerrain(root + "/data/terrain.json", db, err)) return false;
  if (!loadJobs(root + "/data/jobs.json", db, err)) return false;  // needs unit types, skills, abilities; characters and map units name jobs (docs/JOBS.md)
  if (!resolveTeaching(root, db, err)) return false;  // what items teach against the jobs (docs/JOBS.md "Learning from equipment")
  if (!loadCharacters(root + "/data/characters.json", db, err)) return false;  // needs unit types, items, skills, abilities
  if (!loadResearch(root + "/data/research.json", db, err)) return false;  // before structures: upgrades name technologies
  if (!loadStructures(root + "/data/structures.json", db, err)) return false;
  if (!resolveResearchStructures(root + "/data/research.json", db, err)) return false;  // "requiresStructures"
  if (!checkVersusKeys(weaponsPath, db, err)) return false;
  if (!loadStyles(root + "/game.json", db, err)) return false;
  if (!loadAiRules(root + "/game.json", db, err)) return false;  // "rules.ai" (docs/AI.md); needs the unit types
  if (!loadCommanders(root + "/data/commanders.json", db, err)) return false;  // needs skills, resources, unit types (docs/COMMANDERS.md); before the factions, which may list them
  if (!loadFactions(root + "/data/factions.json", db, err)) return false;  // needs unit types, structures, research, skills, resources, rules.ai (docs/ECONOMY.md "Factions")
  if (!loadGraphics(root + "/data/graphics.json", db, err)) return false;
  if (!loadCampaign(root + "/data/campaign.json", db, err)) return false;
  if (!loadSupports(root + "/data/supports.json", db, err)) return false;  // needs the characters
  if (!resolveSrpgRefs(root, db, err)) return false;  // last: every cross reference of the new data

  out = db;
  err.clear();
  return true;
}

}  // namespace ote
