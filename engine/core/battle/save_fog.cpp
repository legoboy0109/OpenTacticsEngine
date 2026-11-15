#include "core/battle/save_fog.h"

#include "core/battle/save_research.h"

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

void writeSeenKinds(const Battle& b, Json& out) {
  if (!b.fogOn()) return;
  bool any = false;
  for (int t = 0; t < b.teamCount && !any; ++t)
    for (size_t s = 0; s < b.structures.size(); ++s)
      if (b.seenKind(t, (int)s) != b.structures[s].mapType) {  // seenKind() brings the memory up to date first
        any = true;
        break;
      }
  if (!any) return;
  Json teams = Json::makeArray();
  for (int t = 0; t < b.teamCount; ++t) {
    Json list = Json::makeArray();
    for (size_t s = 0; s < b.structures.size(); ++s) list.push(Json::makeNumber(b.seenKind(t, (int)s)));
    teams.push(list);
  }
  out.set("seenKinds", teams);
}

bool readSeenKinds(Reader& rd, const Json& saved, Battle& b) {
  const std::string top = "battle";
  if (!b.fogOn()) {
    if (saved.has("seenKinds")) return rd.fail(top + ".seenKinds", "this battle has no fog of war");
    return true;
  }
  std::vector<std::vector<int>> kinds((size_t)b.teamCount);
  for (auto& list : kinds)
    for (const StructureState& st : b.structures) list.push_back(st.mapType);
  if (saved.has("seenKinds")) {
    const Json* teams;
    if (!rd.array(saved, "seenKinds", top, b.teamCount, 0, teams)) return false;
    for (int t = 0; t < b.teamCount; ++t) {
      const std::string where = top + ".seenKinds[" + std::to_string(t) + "]";
      const Json& list = (*teams)[(size_t)t];
      if (!list.isArray()) return rd.fail(where, "not a list");
      if ((int)list.size() != (int)b.structures.size())
        return rd.fail(where, "has " + std::to_string(list.size()) + " entries, expected " + std::to_string(b.structures.size()));
      for (size_t s = 0; s < list.size(); ++s) {
        const Json& v = list[s];
        const std::string at = where + "[" + std::to_string(s) + "]";
        double d = v.isNumber() ? v.asNumber() : 0;
        if (!v.isNumber() || d != (double)(int)d || d < 0 || d > (double)b.db->structures.size() - 1)
          return rd.fail(at, "not a structure kind number");
        if (!checkStructureKind(rd, *b.db, (int)d, b.structures[s].mapType, at)) return false;
        kinds[(size_t)t][s] = (int)d;
      }
    }
  }
  b.setSeenKinds(kinds);
  return true;
}

}  // namespace savejson
}  // namespace ote
