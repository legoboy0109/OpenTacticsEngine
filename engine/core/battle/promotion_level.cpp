// The level a unit needs for a promotion (docs/SRPG.md "A promotion level per character"). Most specific first: the promotion
// entry's "fromLevelFor" for the unit's character, then the character's own "promotion": { "fromLevel" }, then the entry's "fromLevel".
// A unit with no character (or a character that says nothing) gets the entry's level exactly as before.
#include "core/battle/battle.h"

namespace ote {

int Battle::promotionFromLevel(const Unit& u, const PromotionDef& d) const {
  if (u.character < 0 || (size_t)u.character >= db->characters.size()) return d.fromLevel;
  for (const auto& f : d.fromLevelFor)
    if (f.first == u.character) return f.second;
  const int own = db->characters[(size_t)u.character].promotionFromLevel;
  return own > 0 ? own : d.fromLevel;
}

}  // namespace ote
