// What a promotion would give, as plain text (docs/SRPG.md "Player UI > Promotion", step U2). Free of SDL so a unit
// test can check it: the question box, the promotion screen and the animation's banner all read from here.
#pragma once
#include <string>
#include <vector>

#include "core/battle/battle.h"

namespace ote {

struct PromotionPreview {
  int to = -1;                          // the unit type it would become
  std::string name;                     // that type's name
  std::vector<std::string> changes;     // stat changes that are not zero, "+3 HP" / "-1 Mov" (level-up box wording)
  std::string level;                    // "Back to level 1" when the level resets, else empty
  std::vector<std::string> skills;      // skills the new type brings (its own, and what it learns by the new level)
  std::vector<std::string> cannotWield; // carried weapons the new type cannot wield
};

// `slot` = the promotion item in the unit's inventory, or -1 for an offer at a level. Follows the same rules as the
// core (Battle::promote): the entry of the unit type's `promotions` for `to`, its `gains` or the base-stat difference,
// stats stop at 0, `levelAfter`.
PromotionPreview previewPromotion(const Battle& b, int unit, int slot, int to);

// "Promote Kell to Captain?" and the changes under it.
std::string promotionQuestion(const Battle& b, int unit, const PromotionPreview& p);
// "Kell is now a Captain" (the banner of the animation).
std::string promotionBannerText(const std::string& unitName, const std::string& typeName);
// "+3 HP, +2 Str" or "No stat changes".
std::string joinChanges(const std::vector<std::string>& changes);

}  // namespace ote
