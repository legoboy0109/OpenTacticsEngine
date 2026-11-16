// Loader side of character-locked weapons (docs/SRPG.md "Character-locked weapons": "wieldedBy" and "wieldedByTypes"). Messages name the file and the weapon.
#include "core/data/loader_weapon_locks.h"

#include "core/data/loader_util.h"

namespace ote {

bool readWeaponWielders(const LoadCtx& c, const Json& e, const std::string& where, WeaponDef& w) {
  if (e.has("wieldedBy")) {
    if (!e["wieldedBy"].isArray()) return c.fail(where, "\"wieldedBy\" must be a list of character ids");
    if (!c.stringList(e, "wieldedBy", where, w.wielderIds)) return false;
  }
  if (e.has("wieldedByTypes")) {
    if (!e["wieldedByTypes"].isArray()) return c.fail(where, "\"wieldedByTypes\" must be a list of unit type ids");
    if (!c.stringList(e, "wieldedByTypes", where, w.wielderTypeIds)) return false;
  }
  return true;
}

bool resolveGearWielders(const LoadCtx& c, const Database& db, const std::string& where, WeaponDef& w) {
  w.wielders.clear();
  w.wielderTypes.clear();
  if (!w.wielderIds.empty() && db.characters.empty())
    return c.fail(where, "\"wieldedBy\" is given but this game has no characters (data/characters.json)");
  for (const std::string& id : w.wielderIds) {
    const int ch = db.findCharacter(id);
    if (ch < 0) return c.fail(where, "\"wieldedBy\" names unknown character \"" + id + "\" (data/characters.json)");
    for (int seen : w.wielders)
      if (seen == ch) return c.fail(where, "\"wieldedBy\" names character \"" + id + "\" twice");
    w.wielders.push_back(ch);
  }
  for (const std::string& id : w.wielderTypeIds) {
    const int t = db.findUnitType(id);
    if (t < 0) return c.fail(where, "\"wieldedByTypes\" names unknown unit type \"" + id + "\" (data/units.json)");
    for (int seen : w.wielderTypes)
      if (seen == t) return c.fail(where, "\"wieldedByTypes\" names unit type \"" + id + "\" twice");
    w.wielderTypes.push_back(t);
  }
  return true;
}

bool resolveWeaponWielders(const std::string& root, Database& db, std::string& err) {
  LoadCtx c(err, root + "/data/weapons.json");
  for (WeaponDef& w : db.weapons)
    if (!resolveGearWielders(c, db, "weapon \"" + w.id + "\"", w)) return false;
  return true;
}

}  // namespace ote
