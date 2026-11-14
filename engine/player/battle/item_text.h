// Words about items shared by the item screens, the info panel and the forecast.
#pragma once
#include <string>
#include <vector>

#include "core/battle/battle.h"

namespace ote {

// Uses left over the most the item has ("3/3"); empty for an item without a limit.
std::string usesText(const Battle& b, const ItemStack& s);
// The line that explains an item: an accessory's bonus ("Defense +2"), else its description, else a weapon's range.
std::string itemDetail(const Battle& b, const ItemStack& s);
// "+2 Strength +1 Defense": per-stat numbers (index = stat) written out; zeros are left out.
std::string gainsText(const Database& db, const std::vector<int>& perStat);
// A price as "300 Gold" (several amounts joined by a space); empty for free.
std::string priceText(const Database& db, const std::vector<Amount>& price);

}  // namespace ote
