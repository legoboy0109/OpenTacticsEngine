#include "core/battle/save_mapstate.h"

#include <set>

namespace ote {
namespace savejson {

void writeMapState(const Battle& b, Json& out) {
  Json state = Json::makeObject();
  // Squares that differ from what the map placed (their terrain number), in square order.
  if (b.baseTerrain) {
    Json list = Json::makeArray();
    for (size_t c = 0; c < b.cells.size() && c < b.baseTerrain->size(); ++c)
      if (b.cells[c].terrain != (*b.baseTerrain)[c]) {
        Json pair = Json::makeArray();
        pair.push(Json::makeNumber((double)c));
        pair.push(Json::makeNumber(b.cells[c].terrain));
        list.push(pair);
      }
    if (list.size() > 0) state.set("terrain", list);
  }
  if (!b.appliedChanges.empty() && b.mapChanges) {
    Json list = Json::makeArray();
    for (const AppliedChange& a : b.appliedChanges) {
      Json j = Json::makeObject();
      j.set("change", Json::makeString((*b.mapChanges)[(size_t)a.change].id));
      Json before = Json::makeArray();
      for (int t : a.before) before.push(Json::makeNumber(t));
      j.set("before", before);
      if (!a.beforeKinds.empty()) {
        Json kinds = Json::makeArray();
        for (int k : a.beforeKinds) kinds.push(Json::makeNumber(k));
        j.set("kinds", kinds);
      }
      list.push(j);
    }
    state.set("applied", list);
  }
  if (b.fog != b.startFog) state.set("fog", Json::makeBool(b.fog));
  if (b.fogVision != b.startFogVision) state.set("fogVision", Json::makeNumber(b.fogVision));
  if (state.size() > 0) out.set("mapState", state);
}

bool readMapState(Reader& rd, const Json& saved, Battle& b) {
  if (!saved.has("mapState")) return true;
  const std::string top = "battle.mapState";
  const Json& s = saved["mapState"];
  if (!s.isObject()) return rd.fail(top, "not an object");
  for (const Json::Member& m : s.members())
    if (m.first != "terrain" && m.first != "applied" && m.first != "fog" && m.first != "fogVision")
      return rd.fail(top, "unknown key \"" + m.first + "\"");
  if (s.has("fog")) {
    if (!rd.boolean(s, "fog", top, b.fog)) return false;
    b.refreshSight();
  }
  if (s.has("fogVision") && !rd.integer(s, "fogVision", top, -kMaxFogVision, kMaxFogVision, b.fogVision)) return false;
  if (s.has("terrain")) {
    const Json* list;
    if (!rd.array(s, "terrain", top, -1, (int)b.cells.size(), list)) return false;
    std::set<int> seen;
    for (size_t i = 0; i < list->size(); ++i) {
      const std::string where = top + ".terrain[" + std::to_string(i) + "]";
      const Json& p = (*list)[i];
      if (!p.isArray() || p.size() != 2 || !p[0].isNumber() || !p[1].isNumber()) return rd.fail(where, "not a pair [square, terrain]");
      const double c = p[0].asNumber(), t = p[1].asNumber();
      if (c != (double)(int)c || c < 0 || c >= (double)b.cells.size()) return rd.fail(where, "not a square of the map");
      if (t != (double)(int)t || t < 0 || t >= (double)b.db->terrains.size()) return rd.fail(where, "not a terrain number");
      if (!seen.insert((int)c).second) return rd.fail(where, "the square is listed twice");
      b.restoreTerrain((Cell)c, (int)t);
    }
  }
  if (s.has("applied")) {
    const Json* list;
    if (!rd.array(s, "applied", top, -1, 1000, list)) return false;
    if (!b.mapChanges) return rd.fail(top + ".applied", "this map has no changes");
    b.appliedChanges.clear();
    for (size_t i = 0; i < list->size(); ++i) {
      const std::string where = top + ".applied[" + std::to_string(i) + "]";
      const Json& j = (*list)[i];
      if (!j.isObject()) return rd.fail(where, "not an object");
      if (!j["change"].isString()) return rd.fail(where + ".change", "missing or not a change id");
      AppliedChange a;
      for (size_t k = 0; k < b.mapChanges->size(); ++k)
        if ((*b.mapChanges)[k].id == j["change"].asString()) a.change = (int)k;
      if (a.change < 0) return rd.fail(where + ".change", "\"" + j["change"].asString() + "\" is not a change of this map");
      for (const AppliedChange& o : b.appliedChanges)
        if (o.change == a.change) return rd.fail(where + ".change", "applied twice");
      const MapChangeDef& def = (*b.mapChanges)[(size_t)a.change];
      const Json* before;
      if (!rd.array(j, "before", where, (int)def.cells.size(), 0, before)) return false;
      for (size_t k = 0; k < before->size(); ++k) {
        const Json& v = (*before)[k];
        if (!v.isNumber() || v.asNumber() != (double)(int)v.asNumber() || v.asNumber() < 0 || v.asNumber() >= (double)b.db->terrains.size())
          return rd.fail(where + ".before[" + std::to_string(k) + "]", "not a terrain number");
        a.before.push_back((int)v.asNumber());
      }
      if (!def.structures.empty()) {
        const Json* kinds;
        if (!rd.array(j, "kinds", where, (int)def.structures.size(), 0, kinds)) return false;
        for (size_t k = 0; k < kinds->size(); ++k) {
          const Json& v = (*kinds)[k];
          if (!v.isNumber() || v.asNumber() != (double)(int)v.asNumber() || v.asNumber() < 0 || v.asNumber() >= (double)b.db->structures.size())
            return rd.fail(where + ".kinds[" + std::to_string(k) + "]", "not a structure kind number");
          a.beforeKinds.push_back((int)v.asNumber());
        }
      } else if (j.has("kinds")) {
        return rd.fail(where + ".kinds", "this change turns no structure into another kind");
      }
      b.appliedChanges.push_back(a);
    }
  }
  return true;
}

}  // namespace savejson
}  // namespace ote
