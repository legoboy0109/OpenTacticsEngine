#include "core/battle/save_research.h"

namespace ote {
namespace savejson {

namespace {
Json num(double v) { return Json::makeNumber(v); }
}  // namespace

Json techListJson(const std::vector<int>& techs) {
  Json a = Json::makeArray();
  for (int t : techs) a.push(num(t));
  return a;
}

bool readTechList(Reader& rd, const Database& db, const Json& o, const char* key, const std::string& where,
                  std::vector<int>& out) {
  out.clear();
  if (!o.has(key)) return true;
  if (db.research.empty()) return rd.fail(where + "." + key, "the game has no research");
  const Json* list;
  if (!rd.array(o, key, where, -1, (int)db.research.size(), list)) return false;
  int last = -1;
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& v = (*list)[i];
    std::string at = where + "." + key + "[" + std::to_string(i) + "]";
    if (!v.isNumber() || v.asNumber() != std::floor(v.asNumber()) || v.asNumber() < 0 || v.asNumber() >= (double)db.research.size())
      return rd.fail(at, "not a technology number (0.." + std::to_string(db.research.size() - 1) + ")");
    int t = (int)v.asNumber();
    if (t <= last) return rd.fail(at, "technologies must be listed once each, in ascending order");
    out.push_back(t);
    last = t;
  }
  return true;
}

bool researchJson(const SideState& s, Json& out) {
  std::vector<int> done;
  for (size_t t = 0; t < s.researched.size(); ++t)
    if (s.researched[t]) done.push_back((int)t);
  if (done.empty() && s.researching < 0) return false;
  out = Json::makeObject();
  out.set("done", techListJson(done));
  if (s.researching >= 0) {
    out.set("current", num(s.researching));
    out.set("left", num(s.researchLeft));
  }
  return true;
}

bool readSideResearch(Reader& rd, const Database& db, const Json& side, const std::string& where, SideState& s) {
  s.researched.assign(db.research.size(), 0);
  s.researching = -1;
  s.researchLeft = 0;
  if (!side.has("research")) return true;
  const std::string at = where + ".research";
  if (db.research.empty()) return rd.fail(at, "the game has no research");
  const Json* r;
  if (!rd.object(side, "research", where, r)) return false;
  std::vector<int> done;
  if (!r->has("done")) return rd.fail(at + ".done", "missing");
  if (!readTechList(rd, db, *r, "done", at, done)) return false;
  for (int t : done) s.researched[(size_t)t] = 1;
  if (r->has("current") != r->has("left")) return rd.fail(at, "\"current\" and \"left\" go together");
  if (r->has("current")) {
    int cur = 0, left = 0;
    if (!rd.integer(*r, "current", at, 0, (int)db.research.size() - 1, cur)) return false;
    if (s.researched[(size_t)cur]) return rd.fail(at + ".current", "that technology is finished already");
    // The turns count: at least one (it finishes when the last is used up), at most what it takes.
    if (!rd.integer(*r, "left", at, 1, db.research[(size_t)cur].turns > 0 ? db.research[(size_t)cur].turns : 1, left)) return false;
    s.researching = cur;
    s.researchLeft = left;
    for (int need : db.research[(size_t)cur].needs)
      if (!s.researched[(size_t)need]) return rd.fail(at + ".current", "its prerequisite \"" + db.research[(size_t)need].id + "\" is not finished");
  }
  for (int t : done)
    for (int need : db.research[(size_t)t].needs)
      if (!s.researched[(size_t)need])
        return rd.fail(at + ".done", "\"" + db.research[(size_t)t].id + "\" is finished but its prerequisite \"" +
                                         db.research[(size_t)need].id + "\" is not");
  return true;
}

void writeStructureKind(const StructureState& s, Json& out) {
  if (s.type != s.mapType) out.set("type", num(s.type));
}

bool checkStructureKind(Reader& rd, const Database& db, int type, int mapType, const std::string& at) {
  const StructureDef& base = db.structures[(size_t)mapType];
  const StructureDef& now = db.structures[(size_t)type];
  if (now.width != base.width || now.height != base.height)
    return rd.fail(at, "\"" + now.id + "\" does not have the size of \"" + base.id + "\", the kind the map placed");
  // Reachable from the map's kind through upgrades (a chain of them is fine).
  std::vector<char> seen(db.structures.size(), 0);
  std::vector<int> todo(1, mapType);
  seen[(size_t)mapType] = 1;
  for (size_t i = 0; i < todo.size(); ++i)
    for (const UpgradeDef& up : db.structures[(size_t)todo[i]].upgrades)
      if (!seen[(size_t)up.to]) {
        seen[(size_t)up.to] = 1;
        todo.push_back(up.to);
      }
  if (!seen[(size_t)type]) return rd.fail(at, "\"" + now.id + "\" cannot be reached from \"" + base.id + "\" by upgrades");
  return true;
}

bool readStructureKind(Reader& rd, const Database& db, const Json& j, const std::string& where, StructureState& st) {
  if (!j.has("type")) return true;  // st.type is the map's kind already
  int type = 0;
  if (!rd.integer(j, "type", where, 0, (int)db.structures.size() - 1, type)) return false;
  if (!checkStructureKind(rd, db, type, st.mapType, where + ".type")) return false;
  st.type = type;
  return true;
}

}  // namespace savejson
}  // namespace ote
