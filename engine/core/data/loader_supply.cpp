// Repair and supply data (docs/ECONOMY.md "Repair and supply"): game.json "rules.supply", structures.json
// "repairs", units.json "fuel" / "fuelPerTurn" / "supplies", weapons.json "ammo". Every key is optional;
// a game that uses none of them loads exactly as before.
#include "core/data/loader_content.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

bool inRange(const LoadCtx& c, const Json& o, const char* key, const std::string& where, int lo, int hi, int& out) {
  if (!c.integer(o, key, where, out)) return false;
  if (o.has(key) && (out < lo || out > hi))
    return c.fail(where, std::string("\"") + key + "\" must be from " + std::to_string(lo) + " to " + std::to_string(hi));
  return true;
}

// Refuses keys that are not in the list, naming the allowed ones (a typo must not silently do nothing).
bool onlyKeys(const LoadCtx& c, const Json& o, const std::string& where, const char* const* allowed, const char* names) {
  for (const Json::Member& m : o.members()) {
    bool ok = false;
    for (const char* const* k = allowed; *k; ++k) ok = ok || m.first == *k;
    if (!ok) return c.fail(where, "unknown key \"" + m.first + "\" (expected " + names + ")");
  }
  return true;
}

}  // namespace

bool loadSupplyRules(const LoadCtx& c, const Json& rules, Database& db) {
  SupplyRules& s = db.rules.supply;
  const Json* blk;
  if (!c.object(rules, "supply", "rules", blk)) return false;
  if (!rules.has("supply")) return true;
  static const char* const kKeys[] = {"repair", "structures", nullptr};
  if (!onlyKeys(c, *blk, "rules.supply", kKeys, "\"repair\" or \"structures\"")) return false;
  if (!c.boolean(*blk, "structures", "rules.supply", s.structures)) return false;
  if (blk->has("repair")) {
    const Json* rep;
    if (!c.object(*blk, "repair", "rules.supply", rep)) return false;
    static const char* const kRepairKeys[] = {"amount", "costShare", nullptr};
    if (!onlyKeys(c, *rep, "rules.supply.repair", kRepairKeys, "\"amount\" or \"costShare\"")) return false;
    if (!rep->has("amount")) return c.fail("rules.supply.repair", "missing required \"amount\" (percent of maximum HP per turn)");
    if (!inRange(c, *rep, "amount", "rules.supply.repair", 1, 100, s.repairAmount)) return false;
    if (!inRange(c, *rep, "costShare", "rules.supply.repair", 0, 100, s.repairCostShare)) return false;
    s.repair = true;
  }
  return true;
}

bool readWeaponAmmo(const LoadCtx& c, const Json& e, const std::string& where, WeaponDef& w) {
  return inRange(c, e, "ammo", where, 1, 99, w.ammo);
}

bool readUnitSupply(const LoadCtx& c, const Json& e, const std::string& where, UnitTypeDef& u) {
  if (!inRange(c, e, "fuel", where, 1, 999, u.fuel)) return false;
  if (e.has("fuelPerTurn")) {
    if (!e.has("fuel")) return c.fail(where, "\"fuelPerTurn\" needs \"fuel\"");
    if (!inRange(c, e, "fuelPerTurn", where, 1, 999, u.fuelPerTurn)) return false;
    // A unit that burns its whole tank at the first turn start would be destroyed before it ever moved.
    if (u.fuelPerTurn >= u.fuel)
      return c.fail(where, "\"fuelPerTurn\" (" + std::to_string(u.fuelPerTurn) + ") must be less than \"fuel\" (" +
                               std::to_string(u.fuel) + "): the unit would be destroyed at its first turn");
  }
  return c.boolean(e, "supplies", where, u.supplies);
}

bool readStructureRepairs(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, StructureDef& s) {
  if (!e.has("repairs")) return true;
  if (!c.stringList(e, "repairs", where, s.repairs)) return false;
  s.repairsAll = false;
  // Same rule as "versus" keys: a unit type id, or a tag some unit type carries.
  for (const std::string& key : s.repairs) {
    bool known = db.findUnitType(key) >= 0;
    for (size_t u = 0; u < db.unitTypes.size() && !known; ++u)
      for (const std::string& tag : db.unitTypes[u].tags)
        if (tag == key) { known = true; break; }
    if (!known)
      return c.fail(where, "\"repairs\" entry \"" + key + "\" is neither a unit type id nor a tag used by any unit type");
  }
  return true;
}

bool checkAmmoNotItems(const std::string& weaponsPath, const Database& db, std::string& err) {
  LoadCtx c(err, weaponsPath);
  for (const ItemDef& it : db.items) {
    if (it.kind != ItemKind::Weapon || it.weapon < 0) continue;
    const WeaponDef& w = db.weapons[(size_t)it.weapon];
    if (w.ammo > 0)
      return c.fail("weapon \"" + w.id + "\"",
                    "\"ammo\" is for fixed weapons; this weapon is carried as the item \"" + it.id +
                        "\", whose \"uses\" already limit it");
  }
  return true;
}

}  // namespace ote
