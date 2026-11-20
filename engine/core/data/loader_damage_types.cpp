// Loader of damage types (docs/SRPG.md "Damage types"). Every refusal names the file, the entry and the key.
#include "core/data/loader_damage_types.h"

namespace ote {

namespace {

constexpr int kMaxDamageTypes = 16;
std::string q(const std::string& s) { return "\"" + s + "\""; }

}  // namespace

bool loadDamageTypes(const LoadCtx& c, const Json& root, Database& db) {
  db.damageTypes = DamageTypes();
  DamageTypes& dt = db.damageTypes;
  if (root.has("resistRange") && !root.has("damageTypes"))
    return c.fail("", "\"resistRange\" needs \"damageTypes\"");
  if (!root.has("damageTypes")) return true;
  const Json* list;
  if (!c.array(root, "damageTypes", "", list, true)) return false;
  if (list->size() == 0) return c.fail("", "\"damageTypes\" must not be empty (leave the key out for a game without damage types)");
  if ((int)list->size() > kMaxDamageTypes)
    return c.fail("", "\"damageTypes\" has more than " + std::to_string(kMaxDamageTypes) + " entries");
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& e = (*list)[i];
    const std::string at = "damageTypes[" + std::to_string(i) + "]";
    if (!e.isObject()) return c.fail(at, "must be an object");
    DamageTypeDef d;
    if (!c.str(e, "id", at, d.id, true)) return false;
    if (d.id.empty()) return c.fail(at, "\"id\" must not be empty");
    const std::string where = "damageType " + q(d.id);
    for (const Json::Member& m : e.members())
      if (m.first != "id" && m.first != "name" && m.first != "ignoresTerrain")
        return c.fail(where, "unknown key " + q(m.first) + " (expected \"id\", \"name\" or \"ignoresTerrain\")");
    if (dt.find(d.id) >= 0) return c.fail(where, "duplicate id");
    d.name = d.id;
    if (!c.str(e, "name", where, d.name) || !c.boolean(e, "ignoresTerrain", where, d.ignoresTerrain)) return false;
    dt.list.push_back(d);
  }
  if (root.has("resistRange")) {
    const Json& r = root["resistRange"];
    if (!r.isArray() || r.size() != 2 || !LoadCtx::isWhole(r[size_t(0)]) || !LoadCtx::isWhole(r[size_t(1)]))
      return c.fail("", "\"resistRange\" must be [min, max] with two whole numbers");
    dt.resistMin = r[size_t(0)].asInt();
    dt.resistMax = r[size_t(1)].asInt();
    if (dt.resistMin > dt.resistMax) return c.fail("", "\"resistRange\" min is greater than max");
    if (dt.resistMin > 0 || dt.resistMax < 0) return c.fail("", "\"resistRange\" must include 0 (a unit without an entry resists 0)");
    dt.hasRange = true;
  }
  return true;
}

bool readWeaponDamageType(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, WeaponDef& w) {
  w.damageType = -1;
  const DamageTypes& dt = db.damageTypes;
  if (!dt.on()) {
    if (e.has("damageType")) return c.fail(where, "\"damageType\" needs \"damageTypes\" in game.json");
    return true;
  }
  w.damageType = 0;  // the first listed type is the default
  if (e.has("damageType")) {
    std::string id;
    if (!c.str(e, "damageType", where, id)) return false;
    w.damageType = dt.find(id);
    if (w.damageType < 0) return c.fail(where, "\"damageType\" names unknown damage type " + q(id) + " (game.json \"damageTypes\")");
  } else if (!w.damageKind.empty() && dt.find(w.damageKind) >= 0) {
    w.damageType = dt.find(w.damageKind);  // damageKind written as a declared type's id counts as that type
  }
  return true;
}

bool readResist(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, int unset, std::vector<int>& out) {
  out.clear();
  if (!e.has("resist")) return true;
  const DamageTypes& dt = db.damageTypes;
  if (!dt.on()) return c.fail(where, "\"resist\" needs \"damageTypes\" in game.json");
  const Json* m;
  if (!c.object(e, "resist", where, m)) return false;
  out.assign(dt.list.size(), unset);
  for (const Json::Member& r : m->members()) {
    const int t = dt.find(r.first);
    if (t < 0) return c.fail(where, "\"resist\" names unknown damage type " + q(r.first) + " (game.json \"damageTypes\")");
    if (!LoadCtx::isWhole(r.second)) return c.fail(where, "\"resist\"." + q(r.first) + " must be a whole number");
    const int v = r.second.asInt();
    if (dt.hasRange && (v < dt.resistMin || v > dt.resistMax))
      return c.fail(where, "\"resist\"." + q(r.first) + " must be from " + std::to_string(dt.resistMin) + " to " + std::to_string(dt.resistMax) +
                               " (game.json \"resistRange\")");
    out[(size_t)t] = v;
  }
  return true;
}

}  // namespace ote
