// Characters in a battle (docs/SRPG.md "Characters"): a unit's identity and personal growths.
// STUB (step F1): no game has characters yet (data/characters.json is refused by the loader), so a unit never has
// one and growths are the unit type's. Step C1 fills this file.
#include "core/battle/battle.h"

namespace ote {

int Battle::characterOf(const Unit&) const { return -1; }

void Battle::characterDress(Unit& u) const { u.character = characterOf(u); }

int Battle::growthOf(const Unit& u, int stat) const {
  const std::vector<int>& growths = typeOf(u).growths;
  return stat >= 0 && (size_t)stat < growths.size() ? growths[(size_t)stat] : 0;
}

}  // namespace ote
