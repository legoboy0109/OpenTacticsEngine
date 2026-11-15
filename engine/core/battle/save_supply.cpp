#include "core/battle/save_supply.h"

namespace ote {
namespace savejson {

namespace {

Json num(double v) { return Json::makeNumber(v); }

// Does the type have weapons with ammunition that a unit tracks (fixed weapons only)?
bool tracksAmmo(const Database& db, const UnitTypeDef& t) {
  if (db.rules.items.carriedWeapons) return false;
  for (int w : t.weapons)
    if (db.weapons[(size_t)w].ammo > 0) return true;
  return false;
}

}  // namespace

void writeUnitSupply(const Database& db, const Unit& u, Json& out) {
  if (u.fuel >= 0) out.set("fuel", num(u.fuel));
  if (u.ammo.empty()) return;
  const UnitTypeDef& t = db.unitTypes[(size_t)u.type];
  Json ammo = Json::makeObject();
  for (size_t i = 0; i < t.weapons.size() && i < u.ammo.size(); ++i) {
    const std::string& id = db.weapons[(size_t)t.weapons[i]].id;
    if (u.ammo[i] >= 0 && !ammo.has(id.c_str())) ammo.set(id, num(u.ammo[i]));
  }
  out.set("ammo", ammo);
}

bool readUnitSupply(Reader& rd, const Database& db, const Json& j, const std::string& where, Unit& u) {
  const UnitTypeDef& t = db.unitTypes[(size_t)u.type];
  u.fuel = -1;
  u.ammo.clear();

  if (t.fuel > 0) {
    if (!j.has("fuel")) return rd.fail(where + ".fuel", "missing (this unit type uses fuel)");
    if (!rd.integer(j, "fuel", where, 0, t.fuel, u.fuel)) return false;
  } else if (j.has("fuel")) {
    return rd.fail(where + ".fuel", "this unit type uses no fuel");
  }

  if (!tracksAmmo(db, t)) {
    if (j.has("ammo")) return rd.fail(where + ".ammo", "this unit type has no weapon with ammunition");
    return true;
  }
  const Json* ammo;
  if (!j.has("ammo")) return rd.fail(where + ".ammo", "missing (this unit type has weapons with ammunition)");
  if (!rd.object(j, "ammo", where, ammo)) return false;
  const std::string at = where + ".ammo";
  for (const Json::Member& m : ammo->members()) {
    int w = db.findWeapon(m.first);
    bool listed = false;
    for (int own : t.weapons) listed = listed || own == w;
    if (w < 0 || !listed) return rd.fail(at, "\"" + m.first + "\" is not a weapon of this unit type");
    if (db.weapons[(size_t)w].ammo <= 0) return rd.fail(at, "\"" + m.first + "\" has no ammunition in this game");
  }
  for (size_t i = 0; i < t.weapons.size(); ++i) {
    const WeaponDef& w = db.weapons[(size_t)t.weapons[i]];
    if (w.ammo <= 0) {
      u.ammo.push_back(-1);
      continue;
    }
    if (!ammo->has(w.id.c_str())) return rd.fail(at + "." + w.id, "missing");
    int n = 0;
    if (!rd.integer(*ammo, w.id.c_str(), at, 0, w.ammo, n)) return false;
    u.ammo.push_back(n);  // a weapon listed twice shares one count in the file
  }
  return true;
}

}  // namespace savejson
}  // namespace ote
