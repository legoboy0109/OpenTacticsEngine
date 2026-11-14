#include "core/battle/save_items.h"

namespace ote {
namespace savejson {

Json itemsJson(const std::vector<ItemStack>& items) {
  Json list = Json::makeArray();
  for (const ItemStack& s : items) {
    Json j = Json::makeObject();
    j.set("item", Json::makeNumber(s.item));
    j.set("uses", Json::makeNumber(s.uses));
    if (s.drop) j.set("drop", Json::makeBool(true));
    list.push(j);
  }
  return list;
}

bool readItems(Reader& rd, const Database& db, const Json& o, const char* key, const std::string& where, int maxCount,
               std::vector<ItemStack>& out) {
  out.clear();
  if (!o.has(key)) return true;
  const Json* list;
  if (!rd.array(o, key, where, -1, maxCount, list)) return false;
  for (size_t i = 0; i < list->size(); ++i) {
    std::string at = where + "." + key + "[" + std::to_string(i) + "]";
    const Json& j = (*list)[i];
    if (!j.isObject()) return rd.fail(at, "not an object");
    ItemStack s;
    if (!rd.integer(j, "item", at, 0, (int)db.items.size() - 1, s.item)) return false;
    int newUses = db.items[(size_t)s.item].uses;
    if (!rd.integer(j, "uses", at, newUses < 0 ? -1 : 1, newUses < 0 ? -1 : newUses, s.uses)) return false;
    if (j.has("drop") && !rd.boolean(j, "drop", at, s.drop)) return false;
    out.push_back(s);
  }
  return true;
}

}  // namespace savejson
}  // namespace ote
