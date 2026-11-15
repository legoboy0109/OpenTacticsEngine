#include "core/battle/save_fog.h"

namespace ote {
namespace savejson {

void writeSeenOwners(const Battle& b, Json& out) {
  if (!b.fogOn()) return;
  Json teams = Json::makeArray();
  for (int t = 0; t < b.teamCount; ++t) {
    Json list = Json::makeArray();
    // seenOwner() brings the memory up to date with the present sight first.
    for (size_t s = 0; s < b.structures.size(); ++s) list.push(Json::makeNumber(b.seenOwner(t, (int)s)));
    teams.push(list);
  }
  out.set("seenOwners", teams);
}

bool readSeenOwners(Reader& rd, const Json& saved, Battle& b) {
  const std::string top = "battle";
  if (!b.fogOn()) {
    if (saved.has("seenOwners")) return rd.fail(top + ".seenOwners", "this battle has no fog of war");
    return true;
  }
  const Json* teams;
  if (!rd.array(saved, "seenOwners", top, b.teamCount, 0, teams)) return false;
  std::vector<std::vector<int>> owners((size_t)b.teamCount);
  for (int t = 0; t < b.teamCount; ++t) {
    const std::string where = top + ".seenOwners[" + std::to_string(t) + "]";
    const Json& list = (*teams)[(size_t)t];
    if (!list.isArray()) return rd.fail(where, "not a list");
    if ((int)list.size() != (int)b.structures.size())
      return rd.fail(where, "has " + std::to_string(list.size()) + " entries, expected " + std::to_string(b.structures.size()));
    for (size_t s = 0; s < list.size(); ++s) {
      const Json& v = list[s];
      double d = v.isNumber() ? v.asNumber() : 0;
      if (!v.isNumber() || d != (double)(int)d || d < -1 || d > (double)b.sides.size() - 1)
        return rd.fail(where + "[" + std::to_string(s) + "]", "not a side number (-1 = nobody)");
      owners[(size_t)t].push_back((int)d);
    }
  }
  b.setSeenOwners(owners);
  return true;
}

}  // namespace savejson
}  // namespace ote
