// Map weapons (docs/SRPG.md "Map weapons"): a structure kind's "weapon", "uses", "by", "ignoreRank" and "becomes", and a placed
// structure's own "uses". Read once every kind exists (loadStructureWeapons), so "becomes" can name a kind that comes later.
#include <algorithm>

#include "core/data/loader_map_weapon.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

const int kMaxMapWeaponUses = 9999;

std::string q(const std::string& s) { return "\"" + s + "\""; }

// Can some movement type enter the structure's squares at all? (A kind that sets cost 0 for every movement type is a wall.)
bool someTypeEnters(const Database& db, const StructureDef& s) {
  for (size_t mt = 0; mt < db.moveTypes.size(); ++mt)
    if (s.moveCost.size() <= mt || s.moveCost[mt] != 0) return true;
  return false;
}

bool readBy(const LoadCtx& c, const Database& db, const Json& by, const std::string& where, StructureDef& s) {
  MapWeaponDef& m = s.mapWeapon;
  if (!by.isObject()) return c.fail(where, "\"by\" must be { \"types\": [unit type ids], \"tags\": [unit tags] }");
  for (const Json::Member& k : by.members())
    if (k.first != "types" && k.first != "tags") return c.fail(where, "\"by\" has an unknown key " + q(k.first) + " (it has \"types\" and \"tags\")");
  std::vector<std::string> types, tags;
  if (!c.stringList(by, "types", where + ": \"by\"", types) || !c.stringList(by, "tags", where + ": \"by\"", tags)) return false;
  if (types.empty() && tags.empty()) return c.fail(where, "\"by\" is empty: list \"types\" or \"tags\", or leave \"by\" out for any unit");
  for (const std::string& id : types) {
    const int t = db.findUnitType(id);
    if (t < 0) return c.fail(where, "\"by\" names unknown unit type " + q(id));
    if (std::find(m.byTypes.begin(), m.byTypes.end(), t) == m.byTypes.end()) m.byTypes.push_back(t);
  }
  for (const std::string& tag : tags) {
    bool used = false;
    for (size_t t = 0; t < db.unitTypes.size(); ++t)
      if (std::find(db.unitTypes[t].tags.begin(), db.unitTypes[t].tags.end(), tag) != db.unitTypes[t].tags.end()) {
        used = true;
        if (std::find(m.byTypes.begin(), m.byTypes.end(), (int)t) == m.byTypes.end()) m.byTypes.push_back((int)t);
      }
    if (!used) return c.fail(where, "\"by\" tag " + q(tag) + " is not used by any unit type");
  }
  m.restricted = true;
  // At least one of them must be able to stand there, or nobody could ever use the weapon.
  bool any = false;
  for (int t : m.byTypes) {
    const size_t mt = (size_t)db.unitTypes[(size_t)t].moveType;
    any = any || s.moveCost.size() <= mt || s.moveCost[mt] != 0;
  }
  if (!any) return c.fail(where, "none of the unit types in \"by\" can stand on this structure (its \"moveCost\" is 0 for their movement types)");
  return true;
}

}  // namespace

bool loadStructureWeapons(const LoadCtx& c, const Json& list, Database& db) {
  for (size_t i = 0; i < list.size(); ++i) {
    const Json& e = list[i];
    StructureDef& s = db.structures[i];
    const std::string where = "structure " + q(s.id);
    MapWeaponDef& m = s.mapWeapon;
    if (!e.has("weapon")) {
      for (const char* k : {"uses", "by", "ignoreRank", "becomes"})
        if (e.has(k)) return c.fail(where, std::string("\"") + k + "\" belongs with a \"weapon\" (the item a unit standing here attacks with)");
      continue;
    }
    std::string id;
    if (!c.str(e, "weapon", where, id, true)) return false;
    m.item = db.findItem(id);
    if (m.item < 0) return c.fail(where, "\"weapon\" names unknown item " + q(id) + (db.items.empty() ? " (this game has no items, data/items.json)" : ""));
    const ItemDef& item = db.items[(size_t)m.item];
    if (item.kind != ItemKind::Weapon || item.weapon < 0) return c.fail(where, "\"weapon\" names item " + q(id) + ", which is not a weapon");
    m.weapon = item.weapon;
    if (e.has("uses")) {
      if (!LoadCtx::isWhole(e["uses"]) || e["uses"].asInt() < 1 || e["uses"].asInt() > kMaxMapWeaponUses)
        return c.fail(where, "\"uses\" must be a whole number from 1 to " + std::to_string(kMaxMapWeaponUses) + " (leave it out for unlimited)");
      m.uses = e["uses"].asInt();
    }
    if (!c.boolean(e, "ignoreRank", where, m.ignoreRank)) return false;
    if (e.has("by") && !readBy(c, db, e["by"], where, s)) return false;
    if (!someTypeEnters(db, s)) return c.fail(where, "has a \"weapon\" but its \"moveCost\" is 0 for every movement type, so no unit can stand on it");
    if (e.has("becomes")) {
      std::string to;
      if (!c.str(e, "becomes", where, to, true)) return false;
      m.becomes = db.findStructure(to);
      if (m.becomes < 0) return c.fail(where, "\"becomes\" names unknown structure " + q(to));
      if (m.becomes == (int)i) return c.fail(where, "\"becomes\" cannot name the structure itself");
      const StructureDef& t = db.structures[(size_t)m.becomes];
      if (t.rings != s.rings || t.width != s.width || t.height != s.height)
        return c.fail(where, "\"becomes\" " + q(to) + " is not the same size (" + std::to_string(t.width) + " x " + std::to_string(t.height) +
                                 ", rings " + std::to_string(t.rings) + "); a structure keeps its footprint when its kind changes");
    }
  }
  return true;
}

bool readPlacedWeapon(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, MapStructureDef& s) {
  if (!e.has("uses")) return true;
  const MapWeaponDef& m = db.structures[(size_t)s.type].mapWeapon;
  if (!m.has()) return c.fail(where, "\"uses\" only belongs to a structure whose kind has a \"weapon\"");
  return c.integerIn(e, "uses", where, 1, kMaxMapWeaponUses, s.weaponUses);
}

}  // namespace ote
