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

void writeLights(const Battle& b, Json& out) {
  if (b.lights.empty()) return;
  Json list = Json::makeArray();
  for (const LightArea& l : b.lights) {
    Json j = Json::makeObject();
    j.set("side", Json::makeNumber(l.side));
    j.set("cell", Json::makeNumber(l.cell));
    j.set("radius", Json::makeNumber(l.radius));
    j.set("turns", Json::makeNumber(l.turns));
    if (l.shrink > 0) j.set("shrink", Json::makeNumber(l.shrink));  // only a torch has it: saves of other lights stay as they were
    list.push(j);
  }
  out.set("lights", list);
}

bool readLights(Reader& rd, const Json& saved, Battle& b) {
  const std::string top = "battle";
  b.lights.clear();
  if (!saved.has("lights")) return true;
  if (!b.fogOn()) return rd.fail(top + ".lights", "this battle has no fog of war");
  const Json* list;
  if (!rd.array(saved, "lights", top, -1, 1000, list)) return false;
  for (size_t i = 0; i < list->size(); ++i) {
    const std::string where = top + ".lights[" + std::to_string(i) + "]";
    const Json& j = (*list)[i];
    if (!j.isObject()) return rd.fail(where, "not an object");
    LightArea l;
    if (!rd.integer(j, "side", where, 0, (int)b.sides.size() - 1, l.side) ||
        !rd.integer(j, "cell", where, 0, b.grid.cellCount() - 1, l.cell) || !rd.integer(j, "radius", where, 1, kMaxStaffRadius, l.radius) ||
        !rd.integer(j, "turns", where, 1, kMaxStaffLightTurns, l.turns) || (j.has("shrink") && !rd.integer(j, "shrink", where, 1, kMaxStaffRadius, l.shrink)))
      return false;
    b.lights.push_back(l);
  }
  return true;
}

}  // namespace savejson
}  // namespace ote
