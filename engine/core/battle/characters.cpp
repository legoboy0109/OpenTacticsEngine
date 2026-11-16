// Characters in a battle (docs/SRPG.md "Characters"): a unit's identity and its personal growths. The rest of a
// character (starting stats, items, name, level) is resolved when a map unit is loaded (map_loader.cpp), so a
// battle only needs to know WHO a unit is, which comes from its story id and is never saved.
#include "core/battle/battle.h"

namespace ote {

int Battle::characterOf(const Unit& u) const {
  if (u.storyId.empty() || db->characters.empty()) return -1;
  return db->findCharacter(u.storyId);
}

void Battle::characterDress(Unit& u) const { u.character = characterOf(u); }

// The chance in percent: the unit type's growth + the character's (a negative total counts as 0).
// Standing skills add their "growths" bonus (K1). The total is only clamped for a character's unit (see below).
int Battle::growthOf(const Unit& u, int stat) const {
  if (stat < 0) return 0;
  const std::vector<int>& growths = typeOf(u).growths;
  int g = (size_t)stat < growths.size() ? growths[(size_t)stat] : 0;
  if (u.character >= 0 && (size_t)u.character < db->characters.size()) {
    const std::vector<int>& own = db->characters[(size_t)u.character].growths;
    if ((size_t)stat < own.size()) g += own[(size_t)stat];
  }
  g += skillGrowthBonus(u, stat);  // standing skills' "growths" (0 in a game without skills)
  // A game without characters or skills keeps the raw type growth exactly as before (it was never clamped).
  return (u.character >= 0 || !db->skills.empty()) && g < 0 ? 0 : g;
}

}  // namespace ote
