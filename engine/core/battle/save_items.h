// Item lists in save files (a unit's inventory, a side's storage), shared by the battle save
// (save.cpp) and the campaign save (core/campaign/campaign_save.cpp).
#pragma once
#include <string>
#include <vector>

#include "core/base/json.h"
#include "core/battle/battle.h"
#include "core/battle/save_reader.h"

namespace ote {
namespace savejson {

// [ { "item": index, "uses": n, "drop": true } ]  ("drop" only when set)
Json itemsJson(const std::vector<ItemStack>& items);

// Reads such a list from o[key]; a missing key is an empty list (saves of games without items do not
// carry it). Checks the item index and that the uses fit the item.
bool readItems(Reader& rd, const Database& db, const Json& o, const char* key, const std::string& where, int maxCount,
               std::vector<ItemStack>& out);

}  // namespace savejson
}  // namespace ote
