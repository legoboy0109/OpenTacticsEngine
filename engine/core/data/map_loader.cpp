#include <algorithm>
#include <map>

#include "core/data/loader.h"
#include "core/data/footprint.h"
#include "core/data/loader_content.h"
#include "core/data/loader_difficulty.h"
#include "core/data/loader_facing.h"
#include "core/data/loader_jobs.h"
#include "core/data/loader_unit_ranks.h"
#include "core/data/loader_commanders.h"
#include "core/data/loader_factions.h"
#include "core/data/loader_graphics_art.h"
#include "core/data/loader_places.h"
#include "core/data/loader_map_weapon.h"
#include "core/data/loader_prep.h"
#include "core/data/loader_ranks.h"
#include "core/data/events.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

bool loadGrid(const LoadCtx& c, const Database& db, const Json& root, MapDef& m) {
  const Json* g;
  if (!c.object(root, "grid", "", g, true)) return false;
  // "layout" belongs to game.json "grid" (docs/HEX.md); a map that repeats it (hand-made hexagon maps do) is not wrong, the game's counts, so it is
  // accepted quietly.
  if (!c.onlyKeys(*g, {"type", "width", "height"}, "grid", {"layout"})) return false;
  std::string type;
  if (!c.str(*g, "type", "grid", type, true)) return false;
  if (type != "square" && type != "hex")
    return c.fail("grid", "type \"" + type + "\" is not supported (use \"square\" or \"hex\")");
  if ((type == "hex") != (db.gridType == GridType::Hex))
    return c.fail("grid", std::string("this is a ") + (db.gridType == GridType::Hex ? "hexagon" : "square") +
                              " game (game.json \"grid\"), but the map's type is \"" + type + "\"");
  m.gridType = db.gridType;
  m.hexLayout = db.hexLayout;
  if (!c.integer(*g, "width", "grid", m.width, true)) return false;
  if (!c.integer(*g, "height", "grid", m.height, true)) return false;
  if (m.width < 1 || m.width > 256 || m.height < 1 || m.height > 256)
    return c.fail("grid", "width and height must be between 1 and 256");
  return true;
}

bool loadTiles(const LoadCtx& c, const Database& db, const Json& root, MapDef& m) {
  const Json* legend;
  if (!c.object(root, "legend", "", legend, true)) return false;
  std::map<char, int> lookup;
  for (const Json::Member& e : legend->members()) {
    if (e.first.size() != 1) return c.fail("legend", "key \"" + e.first + "\" must be a single character");
    if (!e.second.isString()) return c.fail("legend", "\"" + e.first + "\" must name a terrain id");
    int t = db.findTerrain(e.second.asString());
    if (t < 0) return c.fail("legend", "\"" + e.first + "\" names unknown terrain \"" + e.second.asString() + "\"");
    lookup[e.first[0]] = t;
  }

  const Json* tiles;
  if (!c.array(root, "tiles", "", tiles, true)) return false;
  if (static_cast<int>(tiles->size()) != m.height)
    return c.fail("tiles", "has " + std::to_string(tiles->size()) + " rows but grid height is " +
                               std::to_string(m.height));
  m.terrain.assign(static_cast<size_t>(m.width * m.height), 0);
  for (int y = 0; y < m.height; ++y) {
    const Json& row = (*tiles)[static_cast<size_t>(y)];
    if (!row.isString()) return c.fail("tiles", "row " + std::to_string(y) + " must be a string");
    std::string s = row.asString();
    if (static_cast<int>(s.size()) != m.width)
      return c.fail("tiles", "row " + std::to_string(y) + " has " + std::to_string(s.size()) +
                                 " characters but grid width is " + std::to_string(m.width));
    for (int x = 0; x < m.width; ++x) {
      auto it = lookup.find(s[static_cast<size_t>(x)]);
      if (it == lookup.end())
        return c.fail("tiles", std::string("character '") + s[static_cast<size_t>(x)] +
                                   "' at row " + std::to_string(y) + ", column " + std::to_string(x) +
                                   " is not in the legend");
      m.terrain[static_cast<size_t>(y * m.width + x)] = it->second;
    }
  }

  m.heights.assign(m.terrain.size(), 0);
  if (root.has("heights")) {
    const Json* h;
    if (!c.array(root, "heights", "", h)) return false;
    if (static_cast<int>(h->size()) != m.height)
      return c.fail("heights", "must have one row per grid row (" + std::to_string(m.height) + ")");
    for (int y = 0; y < m.height; ++y) {
      const Json& row = (*h)[static_cast<size_t>(y)];
      if (!row.isArray() || static_cast<int>(row.size()) != m.width)
        return c.fail("heights", "row " + std::to_string(y) + " must be a list of " + std::to_string(m.width) + " numbers");
      for (int x = 0; x < m.width; ++x) {
        if (!LoadCtx::isWhole(row[static_cast<size_t>(x)]))
          return c.fail("heights", "row " + std::to_string(y) + ", column " + std::to_string(x) + " must be a whole number");
        m.heights[static_cast<size_t>(y * m.width + x)] = row[static_cast<size_t>(x)].asInt();
      }
    }
  }
  return true;
}

bool loadSides(const LoadCtx& c, const Database& db, const Json& root, MapDef& m) {
  const Json* sides;
  if (!c.array(root, "sides", "", sides, true)) return false;
  if (sides->size() < 2) return c.fail("", "\"sides\" needs at least two sides");
  for (size_t i = 0; i < sides->size(); ++i) {
    const Json& e = (*sides)[i];
    std::string where = "sides[" + std::to_string(i) + "]";
    if (!e.isObject()) return c.fail(where, "must be an object");
    MapSideDef s;
    if (!c.str(e, "id", where, s.id, true) || s.id.empty()) return c.fail(where, "\"id\" is required");
    where = "side \"" + s.id + "\"";
    if (!c.onlyKeys(e, {"id", "name", "team", "controller", "aiSeesAll", "color", "difficultyShift", "faction", "commander", "resources", "storage"}, where))
      return false;
    for (const MapSideDef& o : m.sides)
      if (o.id == s.id) return c.fail(where, "duplicate id");
    s.name = s.id;
    s.team = s.id;
    if (!c.str(e, "name", where, s.name)) return false;
    if (!c.str(e, "team", where, s.team)) return false;
    std::string controller = "human", color;
    if (!c.str(e, "controller", where, controller)) return false;
    if (controller != "human" && controller != "ai")
      return c.fail(where, "\"controller\" must be \"human\" or \"ai\", not \"" + controller + "\"");
    s.ai = controller == "ai";
    if (e.has("aiSeesAll")) {  // fog (docs/ECONOMY.md "Fog of war"): this computer side sees everything / is bound by fog, whatever the map says
      bool all = false;
      if (!c.boolean(e, "aiSeesAll", where, all)) return false;
      s.aiSeesAll = all ? 1 : 0;
    }
    if (!c.str(e, "color", where, color)) return false;
    if (!c.boolean(e, "difficultyShift", where, s.difficultyShift)) return false;  // docs/DIFFICULTY.md
    if (!readSideFaction(c, db, e, where, s)) return false;  // "faction": an id or "choose" (docs/ECONOMY.md "Factions")
    if (!readSideCommander(c, db, e, where, s)) return false;  // "commander": an id or "choose" (docs/COMMANDERS.md)
    s.color = parseColor(color, 0xffffff);
    if (!c.amounts(db, e, "resources", where, s.resources)) return false;
    if (!readItemGrants(c, db, e, "storage", where, false, -1, s.storage)) return false;
    if (!s.storage.empty() && db.rules.items.storage == ItemStorage::Off)
      return c.fail(where, "\"storage\" is given but the game has no shared storage (rules.items.storage)");
    m.sides.push_back(s);
  }
  return true;
}

int findSide(const MapDef& m, const std::string& id) {
  for (size_t i = 0; i < m.sides.size(); ++i)
    if (m.sides[i].id == id) return static_cast<int>(i);
  return -1;
}

// "structures": buildings standing on top of the terrain (docs/STRUCTURES.md).
// `at` receives, per cell, the index of the structure covering it (-1 none).
bool loadStructures(const LoadCtx& c, const Database& db, const Json& root, MapDef& m, std::vector<int>& at) {
  at.assign(m.terrain.size(), -1);
  // An empty list assigns nothing (older editor versions always wrote one), so it is harmless.
  if (root.has("owners") && !(root["owners"].isArray() && root["owners"].size() == 0))
    return c.fail("", "\"owners\" is no longer used: ownership now belongs to structures "
                      "(give each entry of \"structures\" a \"side\"), see docs/STRUCTURES.md");
  const Json* list;
  if (!c.array(root, "structures", "", list)) return false;
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& e = (*list)[i];
    std::string where = "structures[" + std::to_string(i) + "]";
    if (!e.isObject()) return c.fail(where, "must be an object");
    MapStructureDef s;
    if (!c.onlyKeys(e, {"type", "x", "y", "side", "id", "contents", "locked", "visitSides", "visitUnits", "uses"}, where)) return false;
    std::string type, side;
    if (!c.str(e, "type", where, type, true) || !c.integer(e, "x", where, s.x, true) ||
        !c.integer(e, "y", where, s.y, true) || !c.str(e, "side", where, side))
      return false;
    s.type = db.findStructure(type);
    if (s.type < 0)
      return c.fail(where, "unknown structure type \"" + type + "\"" +
                               (db.structures.empty() ? " (this game has no data/structures.json)" : ""));
    const StructureDef& def = db.structures[(size_t)s.type];
    where += " (" + type + " at " + std::to_string(s.x) + "," + std::to_string(s.y) + ")";
    if (!side.empty()) {
      s.side = findSide(m, side);
      if (s.side < 0) return c.fail(where, "unknown side \"" + side + "\"");
    }
    const Grid grid(m.gridType, m.width, m.height, m.hexLayout);
    std::vector<Cell> cells;
    if (!structureFootprint(grid, def, s.x, s.y, cells)) {
      if (grid.isHex())
        return c.fail(where, def.rings ? "covers its hexagon and the six around it (rings 1) but they do not all fit inside the map (" +
                                             std::to_string(m.width) + " x " + std::to_string(m.height) + ")"
                                       : "is outside the map (" + std::to_string(m.width) + " x " + std::to_string(m.height) + ")");
      return c.fail(where, "needs " + std::to_string(def.width) + " x " + std::to_string(def.height) +
                               " squares from its top-left corner but does not fit inside the map (" +
                               std::to_string(m.width) + " x " + std::to_string(m.height) + ")");
    }
    for (Cell cell : cells)
      if (at[(size_t)cell] >= 0) {
        const MapStructureDef& o = m.structures[(size_t)at[(size_t)cell]];
        return c.fail(where, "overlaps structures[" + std::to_string(at[(size_t)cell]) + "] (" +
                                 db.structures[(size_t)o.type].id + " at " + std::to_string(o.x) + "," +
                                 std::to_string(o.y) + ") on " + (grid.isHex() ? "hexagon " : "square ") +
                                 std::to_string(grid.x(cell)) + "," + std::to_string(grid.y(cell)));
      }
    if (!readPlacedPlace(c, db, m, e, where, cells, s)) return false;  // id, contents, locked, visitors (docs/SRPG.md section 10)
    if (!readPlacedWeapon(c, db, e, where, s)) return false;  // "uses" of a map weapon (docs/SRPG.md "Map weapons")
    for (Cell cell : cells) at[(size_t)cell] = (int)m.structures.size();
    m.structures.push_back(s);
  }
  return true;
}

}  // namespace

int findMapSide(const MapDef& m, const std::string& id) { return findSide(m, id); }

// The first half of a unit entry (map "units" and the addUnit command): what it is and where.
// `where` ends up naming the unit for later messages.
bool readUnitHead(const LoadCtx& c, const Database& db, const MapDef& m, const Json& e, std::string& where,
                  MapUnitDef& u) {
  // A unit that is a character (docs/SRPG.md "Characters") is resolved here into the plain unit it stands for: its
  // story id is the character id, and type / name / level default to the character's. Nothing else downstream
  // needs to know about the "character" key (readUnitBody adds its stats and items).
  std::string type, side, character;
  // (the keys of a unit that a story command adds are listed in loader_story_commands.cpp: the same without "facing")
  if (!c.onlyKeys(e, {"character", "type", "side", "x", "y", "name", "level", "leader", "id", "job", "jobs", "equipped", "learned", "goesFor",
                      "behaviour", "xp", "merit", "facing", "difficulty", "difficultyShift", "stats", "ranks", "skills", "items"},
                  where))
    return false;
  if (!c.str(e, "character", where, character)) return false;
  const bool isCharacter = e.has("character");
  const CharacterDef* ch = nullptr;
  if (isCharacter) {
    if (db.characters.empty()) return c.fail(where, "\"character\" is given but the game has no characters (data/characters.json)");
    int ci = db.findCharacter(character);
    if (ci < 0) return c.fail(where, "unknown character \"" + character + "\" (data/characters.json)");
    ch = &db.characters[(size_t)ci];
  }
  if (!c.str(e, "type", where, type, !isCharacter && !e.has("job")) || !c.str(e, "side", where, side, true) ||
      !c.integer(e, "x", where, u.x, true) || !c.integer(e, "y", where, u.y, true))
    return false;
  if (!c.str(e, "name", where, u.name) || !c.integer(e, "level", where, u.level) ||
      !c.boolean(e, "leader", where, u.leader) || !c.str(e, "id", where, u.id))
    return false;
  if (isCharacter) {
    // The character's own class: with a class job its `type` is the job's, so a game with overlay jobs kept the original in the job state.
    if (type.empty()) type = db.unitTypes[(size_t)(ch->jobs.baseType >= 0 ? ch->jobs.baseType : ch->type)].id;
    if (!e.has("level")) u.level = ch->level;
    if (u.name.empty()) u.name = ch->name;
  }
  if (type.empty() && e.has("job") && e["job"].isString() && db.findJob(e["job"].asString()) >= 0 &&
      !db.jobs[(size_t)db.findJob(e["job"].asString())].overlay())  // a unit with a class job needs no "type": the job's (an overlay job keeps the unit's own)
    type = db.unitTypes[(size_t)db.jobs[(size_t)db.findJob(e["job"].asString())].unitType].id;
  u.type = db.findUnitType(type);
  if (u.type < 0 && !(type.empty() && e.has("job"))) return c.fail(where, "unknown unit type \"" + type + "\"");  // (an unknown job is named below)
  {  // Jobs (docs/JOBS.md): a character brings its own job; the unit's keys replace it. A unit with a job plays as the job's unit type.
    bool given = false;
    u.jobs = ch ? ch->jobs : JobState();
    if (!readJobKeys(c, db, e, where, u.jobs, given)) return false;
    if (u.jobs.on()) {
      const JobDef& jd = db.jobs[(size_t)u.jobs.job];
      // The unit's own class, remembered in a game with overlay jobs (docs/JOBS.md "Class jobs and overlay jobs").
      int own = jd.overlay() ? u.type : (ch ? (ch->jobs.baseType >= 0 ? ch->jobs.baseType : ch->type) : jd.unitType);
      if (jd.overlay()) {
        if (u.type < 0) return c.fail(where, "job \"" + jd.id + "\" keeps the unit's own class, so the unit needs a \"type\" (or a \"character\")");
      } else {
        if (e.has("type")) return c.fail(where, "\"type\" and a class job together: the job decides the unit type (remove \"type\"; a job without \"unitType\" keeps it)");
        u.type = jd.unitType;
      }
      u.jobs.baseType = db.jobsOverlay() ? own : -1;
    }
  }
  u.side = findSide(m, side);
  if (u.side < 0) return c.fail(where, "unknown side \"" + side + "\"");
  where += " (" + type + (u.name.empty() ? "" : " \"" + u.name + "\"") + " at " +
           std::to_string(u.x) + "," + std::to_string(u.y) + ")";
  if (isCharacter) {
    if (e.has("id"))
      return c.fail(where, "a unit with a \"character\" is known to events by the character's id; remove \"id\"");
    u.id = character;
  } else if (e.has("id") && db.findCharacter(u.id) >= 0) {
    return c.fail(where, "\"id\" \"" + u.id + "\" is a character (data/characters.json); write \"character\": \"" + u.id + "\"");
  }
  if (e.has("id") && !isStoryId(u.id))
    return c.fail(where, "\"id\" must be letters, digits, \"_\" and \"-\" (got \"" + u.id + "\")");
  if (u.level < 1) return c.fail(where, "\"level\" must be at least 1");
  if (u.x < 0 || u.y < 0 || u.x >= m.width || u.y >= m.height)
    return c.fail(where, "position is outside the map");
  return readUnitGoals(c, m, e, where, u) &&  // "goesFor" (docs/SRPG.md section 10)
         readUnitBehaviour(c, db, m, e, where, u) &&  // "behaviour" (docs/AI.md)
         readUnitXp(c, db, e, where, u) &&  // "xp" (docs/SRPG.md "Unit ranks")
         readUnitMerit(c, db, e, where, u) &&  // "merit" (the same, "rules.ranks")
         readUnitFacing(c, db, m, e, where, u) &&  // "facing" (docs/SRPG.md "Facing")
         readUnitDifficulty(c, db, e, where, u) &&  // "difficulty", "difficultyShift" (docs/DIFFICULTY.md)
         (u.onlyOn.empty() || m.sides[(size_t)u.side].ai ||
          c.fail(where, "\"difficulty\" is for units of computer sides: a unit of a human side belongs to the party (roster, preparation screen), so it cannot come and go with a difficulty"));
}

// The second half: stats and items.
bool readUnitBody(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, MapUnitDef& u) {
  u.stats = db.unitTypes[static_cast<size_t>(u.type)].stats;
  const CharacterDef* ch = e.has("character") ? &db.characters[(size_t)db.findCharacter(e["character"].asString())] : nullptr;
  if (ch)  // the unit type of THIS map plus the character's own stats (never below 0); the map's "stats" replace single values
    for (size_t s = 0; s < u.stats.size() && s < ch->stats.size(); ++s) u.stats[s] = std::max(0, u.stats[s] + ch->stats[s]);
  if (!c.statMap(db, e, "stats", where, u.stats)) return false;
  if (e.has("stats")) {  // docs/DIFFICULTY.md: a stat the placement wrote itself is exact and never shifted
    u.statExact.assign(u.stats.size(), 0);
    for (const Json::Member& m : e["stats"].members()) u.statExact[(size_t)db.findStat(m.first)] = 1;
  }
  if (!readRankMap(c, db, e, where, -1, u.weaponXp)) return false;  // "ranks" replaces the start of the groups it names
  std::vector<std::string> skillIds;  // "skills": added to those of its type and character
  if (!c.stringList(e, "skills", where, skillIds)) return false;
  for (const std::string& id : skillIds) {
    const int s = db.findSkill(id);
    if (s < 0) return c.fail(where, "\"skills\" names unknown skill \"" + id + "\" (data/skills.json)");
    if (std::find(u.skills.begin(), u.skills.end(), s) == u.skills.end()) u.skills.push_back(s);
  }
  u.hasItems = e.has("items");
  if (u.hasItems || !ch || !ch->hasItems) return readItemGrants(c, db, e, "items", where, true, db.rules.items.inventorySize, u.items);
  u.hasItems = true;  // the character's items replace the unit type's
  u.items = ch->items;
  return true;
}

namespace {

bool loadUnits(const LoadCtx& c, const Database& db, const Json& root, MapDef& m, const std::vector<int>& structureAt) {
  const Json* list;
  if (!c.array(root, "units", "", list)) return false;
  // Two placements conflict only if they can exist in one battle (docs/DIFFICULTY.md "One unit, different numbers per
  // difficulty"): units limited to disjoint difficulty lists may share a square and an id.
  std::map<int, std::vector<int>> occupied;  // cell -> unit indexes (same as m.units, which grows in step)
  std::map<std::string, std::vector<int>> ids;  // story id -> unit indexes
  auto conflict = [&](const std::vector<int>& others, const MapUnitDef& u, int& found, std::string& on) {
    for (int o : others) {
      const int d = sharedDifficulty(m.units[(size_t)o].onlyOn, u.onlyOn);
      if (d == kNoSharedDifficulty) continue;
      found = o;
      on = d >= 0 ? " on difficulty \"" + db.difficulties.list[(size_t)d].id + "\"" : "";
      return true;
    }
    return false;
  };
  int hit = -1;
  std::string on;
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& e = (*list)[i];
    std::string where = "units[" + std::to_string(i) + "]";
    if (!e.isObject()) return c.fail(where, "must be an object");
    MapUnitDef u;
    if (!readUnitHead(c, db, m, e, where, u)) return false;
    if (!u.id.empty()) {
      if (conflict(ids[u.id], u, hit, on) && e.has("character"))
        return c.fail(where, "\"character\" \"" + u.id + "\" is already placed by units[" + std::to_string(hit) + "]" + on);
      if (conflict(ids[u.id], u, hit, on))
        return c.fail(where, "\"id\" \"" + u.id + "\" is already used by units[" + std::to_string(hit) + "]" + on);
      ids[u.id].push_back(static_cast<int>(i));
    }
    int cell = u.y * m.width + u.x;
    if (conflict(occupied[cell], u, hit, on))
      return c.fail(where, "shares a cell with units[" + std::to_string(hit) + "]" + on);
    occupied[cell].push_back(static_cast<int>(i));

    const int terrain = m.terrain[static_cast<size_t>(cell)], structure = structureAt[static_cast<size_t>(cell)];
    const int structureType = structure < 0 ? -1 : m.structures[static_cast<size_t>(structure)].type;
    const UnitTypeDef& ut = db.unitTypes[static_cast<size_t>(u.type)];
    if (enterCostOf(db, terrain, structureType, ut.moveType) <= 0) {
      std::string what = "terrain \"" + db.terrains[static_cast<size_t>(terrain)].id + "\"";
      if (structure >= 0) {
        const MapStructureDef& ms = m.structures[static_cast<size_t>(structure)];
        what = "structure \"" + db.structures[static_cast<size_t>(ms.type)].id + "\" (on " + what + ")";
      }
      return c.fail(where, "starts on " + what + " which is impassable for move type \"" +
                               db.moveTypes[static_cast<size_t>(ut.moveType)].id + "\"");
    }
    if (!readUnitBody(c, db, e, where, u)) return false;
    m.units.push_back(u);
  }
  return true;
}

}  // namespace

namespace {

// `story` false: the shape only (loadMapShape): no events, objectives or preparation are read.
bool loadMapFile(const Database& db, const std::string& mapId, MapDef& out, std::string& err, bool story) {
  int idx = db.findMap(mapId);
  if (idx < 0) {
    err = "game.json: no map with id \"" + mapId + "\"";
    return false;
  }
  const MapRef& ref = db.maps[static_cast<size_t>(idx)];
  std::string path = db.rootPath + "/" + ref.file;
  Json root;
  if (!loadObjectFile(path, root, err)) return false;
  LoadCtx c(err, path);

  MapDef m;
  m.id = ref.id;
  m.name = ref.name;
  if (!c.str(root, "name", "", m.name)) return false;
  std::vector<int> structureAt;
  if (!loadGrid(c, db, root, m) || !loadTiles(c, db, root, m) || !readMapTileLayer(c, db, root, m) || !readMapBackdrop(c, db, root, m) || !loadSides(c, db, root, m) ||
      !readMapFog(c, root, m) || !readMapWeather(c, root, m) || !readMapExits(c, root, m) || !readMapBattleBackground(c, db, root, m) || !readMapBattleTables(c, db, root, m) || !loadStructures(c, db, root, m, structureAt) ||
      !readMapChanges(c, db, root, m) || !readMapDifficulty(c, db, root, m) ||
      !loadUnits(c, db, root, m, structureAt) || !checkPlaceVisitors(c, db, m))
    return false;
  if (ref.cutScene)  // docs/EVENTS.md "Showing another map": a map only scenes show is never played
    for (const char* k : {"events", "objectives", "prep"})
      if (root.has(k))
        return c.fail("", std::string("\"") + k + "\" makes no sense on a cut-scene map (game.json \"cutScene\": true): it is only shown by "
                          "the showMap command of another map's events and never played; put the scene's commands in that event");
  if (story && (!loadMapStory(db, root, m, path, err) || !loadMapPrep(c, db, root, m, structureAt))) return false;
  // Last, so that a more specific message of the readers above (a "tileLayer" that is not an object, ...) is the one a maker sees first; a
  // typo in a key the readers never look at is what is left to find here.
  // Every key a map may have. The audio loader (engine/player/sound/audio_data.cpp) reads "music", "playerTurnMusic", "enemyTurnMusic",
  // "prepMusic", "phaseMusic" and "fewEnemies"; "owners" is the old list that older editors wrote empty (loadStructures refuses a full one).
  if (!c.onlyKeys(root, {"name", "grid", "legend", "tiles", "heights", "tileLayer", "backdrop", "sides", "structures", "units", "owners", "fog", "fogVision", "aiSeesAll",
                         "weather", "exits", "battleBackground", "battleBackgrounds", "battlePlatforms", "changes", "difficulty", "events", "objectives", "prep",
                         "music", "playerTurnMusic", "enemyTurnMusic", "prepMusic", "phaseMusic", "fewEnemies"},
                  ""))
    return false;
  out = m;
  err.clear();
  return true;
}

}  // namespace

bool loadMap(const Database& db, const std::string& mapId, MapDef& out, std::string& err) { return loadMapFile(db, mapId, out, err, true); }

bool loadMapShape(const Database& db, const std::string& mapId, MapDef& out, std::string& err) {
  return loadMapFile(db, mapId, out, err, false);
}

}  // namespace ote
