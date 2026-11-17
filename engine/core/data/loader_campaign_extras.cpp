// Campaign scene version of storeItems (docs/EVENTS.md "Sending a unit's items to storage"):
//   { "storeItems": "eirik", "keep": ["oak_blade"] }       the unit is a roster id (as in a scene's takeItem)
// Same refusals as the event command, in the scene's words.
#include "core/data/loader_campaign.h"

namespace ote {

bool CampaignLoader::storeItemsCommand(const Json& e, const std::string& at, SceneCommand& out) {
  out.kind = SceneCommand::Kind::StoreItems;
  if (!onlyKeys(e, {"storeItems", "keep"}, at)) return false;
  if (db_.rules.items.storage == ItemStorage::Off)
    return fail(at, "\"storeItems\" needs a shared storage, and this game has none (game.json rules.items \"storage\" is \"off\"): "
                    "turn the storage on, or leave this command out");
  // The character may join the roster in a later chapter, so only the shape of the id is checked (as takeItem does).
  if (!e["storeItems"].isString() || !isStoryId(e["storeItems"].asString()))
    return fail(at, "\"storeItems\" must be a roster unit id (letters, digits, \"_\" and \"-\")");
  out.unit = e["storeItems"].asString();
  if (!e.has("keep")) return true;
  if (!e["keep"].isArray()) return fail(at, "\"keep\" must be a list of item ids");
  for (size_t i = 0; i < e["keep"].size(); ++i) {
    const Json& v = e["keep"][i];
    const int item = v.isString() ? db_.findItem(v.asString()) : -1;
    if (item < 0) return fail(at, "\"keep\" names unknown item " + quoted(v.isString() ? v.asString() : std::string("?")));
    out.keep.push_back(item);
  }
  return true;
}

}  // namespace ote
