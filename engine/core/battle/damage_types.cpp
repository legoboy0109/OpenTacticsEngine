// A unit's resistance per declared damage type (docs/SRPG.md "Damage types").
#include "core/battle/battle.h"

namespace ote {

// The character's own entry wins when it wrote one (like a character's stats it is personal), else the unit type's; 0 for a type
// nobody gave a number. Static data only: nothing here is saved, so a save is the same with or without damage types.
int Battle::resistOf(const Unit& u, int type) const {
  if (type < 0 || (size_t)type >= db->damageTypes.list.size()) return 0;
  if (u.character >= 0 && (size_t)u.character < db->characters.size()) {
    const std::vector<int>& own = db->characters[(size_t)u.character].resist;
    if ((size_t)type < own.size() && own[(size_t)type] != kResistUnset) return own[(size_t)type];
  }
  const std::vector<int>& base = db->unitTypes[u.type].resist;
  return (size_t)type < base.size() ? base[(size_t)type] : 0;
}

}  // namespace ote
