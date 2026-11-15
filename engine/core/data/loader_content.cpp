#include "core/data/loader_content.h"

#include <cctype>

#include "core/data/loader.h"
#include "core/data/loader_util.h"

namespace ote {

// Shared start of every entry: object shape, unique non-empty id, name.
template <class T>
static bool readEntryHead(const LoadCtx& c, const Json& e, const char* listKey, size_t index,
                          const std::vector<T>& existing, T& t, std::string& where) {
  where = std::string(listKey) + "[" + std::to_string(index) + "]";
  if (!e.isObject()) return c.fail(where, "must be an object");
  if (!c.str(e, "id", where, t.id, true)) return false;
  if (t.id.empty()) return c.fail(where, "\"id\" must not be empty");
  where = std::string(listKey) + " \"" + t.id + "\"";
  for (const T& o : existing)
    if (o.id == t.id) return c.fail(where, "duplicate id");
  t.name = t.id;
  return c.str(e, "name", where, t.name);
}

bool loadWeapons(const std::string& path, Database& db, std::string& err) {
  Json root;
  if (!loadObjectFile(path, root, err)) return false;
  LoadCtx c(err, path);
  const Json* list;
  if (!c.array(root, "weapons", "", list, true)) return false;
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& e = (*list)[i];
    WeaponDef w;
    std::string where;
    if (!readEntryHead(c, e, "weapon", i, db.weapons, w, where)) return false;

    if (e.has("range")) {
      const Json& r = e["range"];
      if (!r.isArray() || r.size() != 2 || !LoadCtx::isWhole(r[size_t(0)]) || !LoadCtx::isWhole(r[size_t(1)]))
        return c.fail(where, "\"range\" must be [min, max] with two whole numbers");
      w.minRange = r[size_t(0)].asInt();
      w.maxRange = r[size_t(1)].asInt();
      if (w.minRange < 0) return c.fail(where, "\"range\" min must not be negative");
      if (w.minRange > w.maxRange) return c.fail(where, "\"range\" min is greater than max");
    }
    if (!c.boolean(e, "counter", where, w.counter)) return false;
    if (!c.boolean(e, "afterMove", where, w.afterMove)) return false;
    if (!c.props(e, "props", where, w.props)) return false;
    if (!c.props(e, "versus", where, w.versus)) return false;
    if (e.has("versusDefault")) {
      if (!c.num(e, "versusDefault", where, w.versusDefault)) return false;
      w.hasVersusDefault = true;
    }
    if (!c.str(e, "damage", where, w.damageFormula)) return false;
    if (!c.str(e, "group", where, w.group)) return false;
    if (!readWeaponAmmo(c, e, where, w)) return false;
    db.weapons.push_back(w);
  }
  return true;
}

bool loadUnitTypes(const std::string& path, Database& db, std::string& err) {
  Json root;
  if (!loadObjectFile(path, root, err)) return false;
  LoadCtx c(err, path);
  const Json* list;
  if (!c.array(root, "units", "", list, true)) return false;
  if (list->size() == 0) return c.fail("", "\"units\" must not be empty");
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& e = (*list)[i];
    UnitTypeDef u;
    std::string where;
    if (!readEntryHead(c, e, "unit", i, db.unitTypes, u, where)) return false;

    std::string glyph;
    if (!c.str(e, "glyph", where, glyph)) return false;
    if (!glyph.empty()) u.glyph = glyph[0];

    std::string mt;
    if (!c.str(e, "moveType", where, mt, true)) return false;
    u.moveType = db.findMoveType(mt);
    if (u.moveType < 0) return c.fail(where, "unknown moveType \"" + mt + "\"");

    u.stats.assign(db.stats.size(), 0);
    u.growths.assign(db.stats.size(), 0);
    if (!c.statMap(db, e, "stats", where, u.stats)) return false;
    if (!c.statMap(db, e, "growths", where, u.growths)) return false;

    std::vector<std::string> weaponIds;
    if (!c.stringList(e, "weapons", where, weaponIds)) return false;
    for (const std::string& wid : weaponIds) {
      int w = db.findWeapon(wid);
      if (w < 0) return c.fail(where, "unknown weapon \"" + wid + "\"");
      u.weapons.push_back(w);
    }
    if (!c.stringList(e, "wields", where, u.wields)) return false;
    if (!readItemGrants(c, db, e, "items", where, true, db.rules.items.inventorySize, u.items)) return false;
    if (!c.amounts(db, e, "cost", where, u.cost)) return false;
    if (!c.stringList(e, "tags", where, u.tags)) return false;
    if (!c.boolean(e, "canCapture", where, u.canCapture)) return false;
    if (!readUnitSupply(c, e, where, u) || !readUnitVision(c, e, where, u)) return false;
    db.unitTypes.push_back(u);
  }
  return true;
}

bool loadTerrain(const std::string& path, Database& db, std::string& err) {
  Json root;
  if (!loadObjectFile(path, root, err)) return false;
  LoadCtx c(err, path);
  const Json* list;
  if (!c.array(root, "terrain", "", list, true)) return false;
  if (list->size() == 0) return c.fail("", "\"terrain\" must not be empty");
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& e = (*list)[i];
    TerrainDef t;
    std::string where;
    if (!readEntryHead(c, e, "terrain", i, db.terrains, t, where)) return false;

    std::string color, glyph;
    if (!c.str(e, "color", where, color)) return false;
    if (e.has("color")) {
      bool ok = color.size() == 7 && color[size_t(0)] == '#';
      for (size_t k = 1; ok && k < 7; ++k) ok = std::isxdigit(static_cast<unsigned char>(color[k])) != 0;
      if (!ok) return c.fail(where, "\"color\" must look like \"#rrggbb\"");
      t.color = parseColor(color, t.color);
    }
    if (!c.str(e, "glyph", where, glyph)) return false;
    if (!glyph.empty()) t.glyph = glyph[0];

    t.moveCost.assign(db.moveTypes.size(), 1);
    const Json* mc;
    if (!c.object(e, "moveCost", where, mc)) return false;
    for (const Json::Member& m : mc->members()) {
      int k = db.findMoveType(m.first);
      if (k < 0) return c.fail(where, "\"moveCost\" names unknown move type \"" + m.first + "\"");
      if (!LoadCtx::isWhole(m.second))
        return c.fail(where, "\"moveCost\".\"" + m.first + "\" must be a whole number");
      t.moveCost[static_cast<size_t>(k)] = m.second.asInt();
    }

    if (!c.props(e, "props", where, t.props)) return false;
    // Buildings are structures now: say where each old setting went.
    for (const char* old : {"capture", "capturePoints", "income", "produces"})
      if (e.has(old))
        return c.fail(where, std::string("\"") + old +
                                 "\" now belongs to structures (data/structures.json), see docs/STRUCTURES.md");
    if (!c.stringList(e, "tags", where, t.tags)) return false;
    for (const std::string& tag : t.tags)
      if (tag == "hq")
        return c.fail(where, "the tag \"hq\" now belongs to structures (data/structures.json), see docs/STRUCTURES.md");
    if (!readSquareVision(c, e, where, t.visionBonus, t.hides)) return false;
    db.terrains.push_back(t);
  }
  return true;
}

bool loadStructures(const std::string& path, Database& db, std::string& err) {
  db.structures.clear();
  if (!fileExists(path)) return true;
  Json root;
  if (!loadObjectFile(path, root, err)) return false;
  LoadCtx c(err, path);
  const Json* list;
  if (!c.array(root, "structures", "", list, true)) return false;
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& e = (*list)[i];
    StructureDef s;
    std::string where;
    if (!readEntryHead(c, e, "structure", i, db.structures, s, where)) return false;

    std::string color, glyph;
    if (!c.str(e, "color", where, color)) return false;
    if (e.has("color")) {
      bool ok = color.size() == 7 && color[size_t(0)] == '#';
      for (size_t k = 1; ok && k < 7; ++k) ok = std::isxdigit(static_cast<unsigned char>(color[k])) != 0;
      if (!ok) return c.fail(where, "\"color\" must look like \"#rrggbb\"");
      s.color = parseColor(color, s.color);
    }
    if (!c.str(e, "glyph", where, glyph)) return false;
    if (!glyph.empty()) s.glyph = glyph[0];

    if (!c.integer(e, "width", where, s.width) || !c.integer(e, "height", where, s.height)) return false;
    if (s.width < 1 || s.width > kMaxStructureSize || s.height < 1 || s.height > kMaxStructureSize)
      return c.fail(where, "\"width\" and \"height\" must be between 1 and " + std::to_string(kMaxStructureSize) +
                               " squares");

    s.moveCost.assign(db.moveTypes.size(), -1);  // -1 = not set: the terrain's cost applies
    const Json* mc;
    if (!c.object(e, "moveCost", where, mc)) return false;
    for (const Json::Member& m : mc->members()) {
      int k = db.findMoveType(m.first);
      if (k < 0) return c.fail(where, "\"moveCost\" names unknown move type \"" + m.first + "\"");
      if (!LoadCtx::isWhole(m.second) || m.second.asInt() < 0)
        return c.fail(where, "\"moveCost\".\"" + m.first + "\" must be a whole number, 0 or more (0 = cannot enter)");
      s.moveCost[static_cast<size_t>(k)] = m.second.asInt();
    }

    if (!c.props(e, "props", where, s.props)) return false;
    if (e.has("capture"))
      return c.fail(where, "\"capture\" is called \"capturePoints\" on structures");
    if (!c.integer(e, "capturePoints", where, s.capturePoints)) return false;
    if (s.capturePoints < 0) return c.fail(where, "\"capturePoints\" must not be negative");
    if (!c.amounts(db, e, "income", where, s.income)) return false;

    std::vector<std::string> produces;
    if (!c.stringList(e, "produces", where, produces)) return false;
    for (const std::string& uid : produces) {
      int u = db.findUnitType(uid);
      if (u < 0) return c.fail(where, "\"produces\" names unknown unit type \"" + uid + "\"");
      s.produces.push_back(u);
    }
    std::vector<std::string> sells;
    if (!c.stringList(e, "sells", where, sells)) return false;
    for (const std::string& iid : sells) {
      int it = db.findItem(iid);
      if (it < 0) return c.fail(where, "\"sells\" names unknown item \"" + iid + "\"");
      if (db.items[(size_t)it].price.empty())
        return c.fail(where, "\"sells\" lists \"" + iid + "\", which has no \"price\" (data/items.json)");
      s.sells.push_back(it);
    }
    if (!c.stringList(e, "tags", where, s.tags)) return false;
    if (!c.boolean(e, "research", where, s.research)) return false;
    if (!readStructureRepairs(c, db, e, where, s) || !readSquareVision(c, e, where, s.visionBonus, s.hides)) return false;
    db.structures.push_back(s);
  }
  return loadStructureUpgrades(c, *list, db);
}

bool checkVersusKeys(const std::string& weaponsPath, const Database& db, std::string& err) {
  LoadCtx c(err, weaponsPath);
  for (const WeaponDef& w : db.weapons) {
    for (const NamedNumber& v : w.versus) {
      bool known = db.findUnitType(v.key) >= 0;
      for (size_t u = 0; u < db.unitTypes.size() && !known; ++u)
        for (const std::string& tag : db.unitTypes[u].tags)
          if (tag == v.key) { known = true; break; }
      if (!known)
        return c.fail("weapon \"" + w.id + "\"",
                      "\"versus\" key \"" + v.key + "\" is neither a unit type id nor a tag used by any unit type");
    }
  }
  return true;
}

}  // namespace ote
