#include "core/battle/save_mapweapon.h"

namespace ote {
namespace savejson {

void writeStructureWeapon(const Battle& b, int index, Json& out) {
  const StructureState& st = b.structures[(size_t)index];
  const MapWeaponDef& m = b.defOf(st).mapWeapon;
  if (!m.has()) return;
  const int left = b.mapWeaponUses(index);
  // What a fresh structure of this kind holds is not written; a placement's own number is (it differs from the kind's).
  if (left != m.uses) out.set("weaponUses", Json::makeNumber(left));
}

bool readStructureWeapon(Reader& rd, const Battle& b, const Json& j, const std::string& where, StructureState& st) {
  const MapWeaponDef& m = b.defOf(st).mapWeapon;
  if (!m.has()) {
    if (j.has("weaponUses")) return rd.fail(where + ".weaponUses", "this structure has no weapon");
    return true;
  }
  st.weaponFor = st.type;
  st.weaponUses = m.uses;
  if (!j.has("weaponUses")) return true;
  return rd.integer(j, "weaponUses", where, 0, 9999, st.weaponUses);
}

}  // namespace savejson
}  // namespace ote
