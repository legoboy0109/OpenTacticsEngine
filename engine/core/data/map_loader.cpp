#include <map>

#include "core/data/loader.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

bool loadGrid(const LoadCtx& c, const Json& root, MapDef& m) {
  const Json* g;
  if (!c.object(root, "grid", "", g, true)) return false;
  std::string type;
  if (!c.str(*g, "type", "grid", type, true)) return false;
  if (type != "square") return c.fail("grid", "type \"" + type + "\" is not supported (only \"square\")");
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
  if (root.has("owners"))
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
    if (s.x < 0 || s.y < 0 || s.x + def.width > m.width || s.y + def.height > m.height)
      return c.fail(where, "needs " + std::to_string(def.width) + " x " + std::to_string(def.height) +
                               " squares from its top-left corner but does not fit inside the map (" +
                               std::to_string(m.width) + " x " + std::to_string(m.height) + ")");
    for (int dy = 0; dy < def.height; ++dy)
      for (int dx = 0; dx < def.width; ++dx) {
        int cell = (s.y + dy) * m.width + s.x + dx;
        if (at[(size_t)cell] >= 0) {
          const MapStructureDef& o = m.structures[(size_t)at[(size_t)cell]];
          return c.fail(where, "overlaps structures[" + std::to_string(at[(size_t)cell]) + "] (" +
                                   db.structures[(size_t)o.type].id + " at " + std::to_string(o.x) + "," +
                                   std::to_string(o.y) + ") on square " + std::to_string(s.x + dx) + "," +
                                   std::to_string(s.y + dy));
        }
        at[(size_t)cell] = (int)m.structures.size();
      }
    m.structures.push_back(s);
  }
  return true;
}

bool loadUnits(const LoadCtx& c, const Database& db, const Json& root, MapDef& m, const std::vector<int>& structureAt) {
  const Json* list;
  if (!c.array(root, "units", "", list)) return false;
  std::map<int, int> occupied;  // cell -> unit index
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& e = (*list)[i];
    std::string where = "units[" + std::to_string(i) + "]";
    if (!e.isObject()) return c.fail(where, "must be an object");
    MapUnitDef u;
    std::string type, side;
    if (!c.str(e, "type", where, type, true) || !c.str(e, "side", where, side, true) ||
        !c.integer(e, "x", where, u.x, true) || !c.integer(e, "y", where, u.y, true))
      return false;
    if (!c.str(e, "name", where, u.name) || !c.integer(e, "level", where, u.level) ||
        !c.boolean(e, "leader", where, u.leader))
      return false;
    u.type = db.findUnitType(type);
    if (u.type < 0) return c.fail(where, "unknown unit type \"" + type + "\"");
    u.side = findSide(m, side);
    if (u.side < 0) return c.fail(where, "unknown side \"" + side + "\"");
    where += " (" + type + (u.name.empty() ? "" : " \"" + u.name + "\"") + " at " +
             std::to_string(u.x) + "," + std::to_string(u.y) + ")";
    if (u.level < 1) return c.fail(where, "\"level\" must be at least 1");
    if (u.x < 0 || u.y < 0 || u.x >= m.width || u.y >= m.height)
      return c.fail(where, "position is outside the map");
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
    u.stats = ut.stats;
    if (!c.statMap(db, e, "stats", where, u.stats)) return false;
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
  if (!loadGrid(c, root, m) || !loadTiles(c, db, root, m) || !loadSides(c, db, root, m) ||
      !loadStructures(c, db, root, m, structureAt) || !loadUnits(c, db, root, m, structureAt))
    return false;
  out = m;
  err.clear();
  return true;
}

}  // namespace ote
