// Loader side of character-locked weapons (docs/SRPG.md "Character-locked weapons"). Messages name the file and the weapon.
#include "core/data/loader_weapon_locks.h"

#include "core/data/loader_util.h"

namespace ote {

bool readWeaponWielders(const LoadCtx& c, const Json& e, const std::string& where, WeaponDef& w) {
  if (!e.has("wieldedBy")) return true;
  if (!e["wieldedBy"].isArray()) return c.fail(where, "\"wieldedBy\" must be a list of character ids");
  return c.stringList(e, "wieldedBy", where, w.wielderIds);
}

bool resolveWeaponWielders(const std::string& root, Database& db, std::string& err) {
  LoadCtx c(err, root + "/data/weapons.json");
  for (WeaponDef& w : db.weapons) {
    w.wielders.clear();
    if (w.wielderIds.empty()) continue;
    const std::string where = "weapon \"" + w.id + "\"";
    if (db.characters.empty())
      return c.fail(where, "\"wieldedBy\" is given but this game has no characters (data/characters.json)");
    for (const std::string& id : w.wielderIds) {
      const int ch = db.findCharacter(id);
      if (ch < 0) return c.fail(where, "\"wieldedBy\" names unknown character \"" + id + "\" (data/characters.json)");
      for (int seen : w.wielders)
        if (seen == ch) return c.fail(where, "\"wieldedBy\" names character \"" + id + "\" twice");
      w.wielders.push_back(ch);
    }
  }
  return true;
}

}  // namespace ote
