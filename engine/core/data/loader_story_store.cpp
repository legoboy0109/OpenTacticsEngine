// The storeItems command of a map event (docs/EVENTS.md "Sending a unit's items to storage"):
//   { "storeItems": <unit filter>, "keep": ["item id", ...] }      the unit's items go to the side's storage, except the kept ones
// The campaign scene version is read in loader_campaign_extras.cpp.
#include "core/data/loader_events.h"

namespace ote {

bool StoryLoader::storeItemsCommand(const Json& e, const std::string& at, Command& out) {
  if (!onlyKeys(e, {"storeItems", "keep"}, at)) return false;
  if (db_.rules.items.storage == ItemStorage::Off)
    return fail(at, "\"storeItems\" needs a shared storage, and this game has none (game.json rules.items \"storage\" is \"off\"): "
                    "turn the storage on, or leave this command out");
  if (!filter(e["storeItems"], at, "\"storeItems\"", out.filter)) return false;
  if (!e.has("keep")) return true;
  if (!e["keep"].isArray()) return fail(at, "\"keep\" must be a list of item ids");
  for (size_t i = 0; i < e["keep"].size(); ++i) {
    const Json& v = e["keep"][i];
    const int item = v.isString() ? db_.findItem(v.asString()) : -1;
    if (item < 0) return fail(at, "\"keep\" names unknown item \"" + (v.isString() ? v.asString() : std::string("?")) + "\"");
    out.keep.push_back(item);
  }
  return true;
}

}  // namespace ote
