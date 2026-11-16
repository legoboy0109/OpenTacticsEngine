#include <map>

#include "core/data/loader.h"
#include "core/data/footprint.h"
#include "core/data/loader_content.h"
#include "core/data/events.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

bool loadGrid(const LoadCtx& c, const Database& db, const Json& root, MapDef& m) {
  const Json* g;
  if (!c.object(root, "grid", "", g, true)) return false;
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
    if (!c.str(e, "color", where, color)) return false;
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
  if (!c.str(e, "character", where, character)) return false;
  const bool isCharacter = e.has("character");
  const CharacterDef* ch = nullptr;
  if (isCharacter) {
    if (db.characters.empty()) return c.fail(where, "\"character\" is given but the game has no characters (data/characters.json)");
    int ci = db.findCharacter(character);
    if (ci < 0) return c.fail(where, "unknown character \"" + character + "\" (data/characters.json)");
    ch = &db.characters[(size_t)ci];
  }
  if (!c.str(e, "type", where, type, !isCharacter) || !c.str(e, "side", where, side, true) ||
      !c.integer(e, "x", where, u.x, true) || !c.integer(e, "y", where, u.y, true))
    return false;
  if (!c.str(e, "name", where, u.name) || !c.integer(e, "level", where, u.level) ||
      !c.boolean(e, "leader", where, u.leader) || !c.str(e, "id", where, u.id))
    return false;
  if (isCharacter) {
    if (type.empty()) type = db.unitTypes[(size_t)ch->type].id;
    if (!e.has("level")) u.level = ch->level;
    if (u.name.empty()) u.name = ch->name;
  }
  u.type = db.findUnitType(type);
  if (u.type < 0) return c.fail(where, "unknown unit type \"" + type + "\"");
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
  return true;
}

// The second half: stats and items.
bool readUnitBody(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, MapUnitDef& u) {
  u.stats = db.unitTypes[static_cast<size_t>(u.type)].stats;
  const CharacterDef* ch = e.has("character") ? &db.characters[(size_t)db.findCharacter(e["character"].asString())] : nullptr;
  if (ch)  // the unit type of THIS map plus the character's own stats (never below 0); the map's "stats" replace single values
    for (size_t s = 0; s < u.stats.size() && s < ch->stats.size(); ++s) u.stats[s] = std::max(0, u.stats[s] + ch->stats[s]);
  if (!c.statMap(db, e, "stats", where, u.stats)) return false;
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
  std::map<int, int> occupied;  // cell -> unit index
  std::map<std::string, int> ids;  // story id -> unit index
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& e = (*list)[i];
    std::string where = "units[" + std::to_string(i) + "]";
    if (!e.isObject()) return c.fail(where, "must be an object");
    MapUnitDef u;
    if (!readUnitHead(c, db, m, e, where, u)) return false;
    if (!u.id.empty()) {
      if (ids.count(u.id) && e.has("character"))
        return c.fail(where, "\"character\" \"" + u.id + "\" is already placed by units[" + std::to_string(ids[u.id]) + "]");
      if (ids.count(u.id))
        return c.fail(where, "\"id\" \"" + u.id + "\" is already used by units[" + std::to_string(ids[u.id]) + "]");
      ids[u.id] = static_cast<int>(i);
    }
    int cell = u.y * m.width + u.x;
    if (occupied.count(cell))
      return c.fail(where, "shares a cell with units[" + std::to_string(occupied[cell]) + "]");
    occupied[cell] = static_cast<int>(i);

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

bool loadMap(const Database& db, const std::string& mapId, MapDef& out, std::string& err) {
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
  if (!loadGrid(c, db, root, m) || !loadTiles(c, db, root, m) || !loadSides(c, db, root, m) ||
      !readMapFog(c, root, m) || !loadStructures(c, db, root, m, structureAt) ||
      !loadUnits(c, db, root, m, structureAt) || !loadMapStory(db, root, m, path, err))
    return false;
  out = m;
  err.clear();
  return true;
}

}  // namespace ote
