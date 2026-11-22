// Learning from equipment and unit resources in saves (docs/SAVES.md, docs/JOBS.md "Learning from equipment", docs/SRPG.md "Unit resources"):
//   units[i] and a roster entry: "mastery": [ { "entry": 0, "points": 40 } ]   (entry = index into the list of what items teach, first-named order)
//   units[i]:                    "pools": [ 12, 3 ]                            (per game.json "unitResources" entry)
// Written only when not empty. A save without them loads as "no points yet" and "the start share" (older saves); the keys in a game without the feature
// are refused.
#include <cmath>

#include "core/battle/save_srpg.h"
#include "core/jobs/mastery.h"

namespace ote {
namespace savejson {

namespace {

const int kMaxSavedPool = 1000000;

Json masteryJson(const std::vector<MasteryPoints>& m) {
  Json a = Json::makeArray();
  for (const MasteryPoints& p : m) {
    Json e = Json::makeObject();
    e.set("entry", Json::makeNumber(p.entry));
    e.set("points", Json::makeNumber(p.points));
    a.push(e);
  }
  return a;
}

bool readMastery(Reader& rd, const Database& db, const JobState& jobs, const Json& j, const std::string& where, std::vector<MasteryPoints>& out) {
  out.clear();
  if (!j.has("mastery")) return true;
  const std::string at = where + ".mastery";
  if (!db.teachingOn()) return rd.fail(at, "this game has no items that teach");
  const Json* list;
  if (!rd.array(j, "mastery", where, -1, (int)db.teachEntries.size(), list)) return false;
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& e = (*list)[i];
    const std::string ea = at + "[" + std::to_string(i) + "]";
    if (!e.isObject()) return rd.fail(ea, "not an object");
    MasteryPoints p;
    if (!rd.integer(e, "entry", ea, 0, (int)db.teachEntries.size() - 1, p.entry) || !rd.integer(e, "points", ea, 1, kMaxTeachPoints, p.points)) return false;
    out.push_back(p);
  }
  const std::string problem = masteryProblem(db, jobs, out);
  return problem.empty() || rd.fail(at, problem);
}

}  // namespace

void writeUnitMastery(const Battle& b, const Unit& u, Json& j) {
  if (b.db->teachingOn() && !u.mastery.empty()) j.set("mastery", masteryJson(u.mastery));
  if (b.db->unitResources.empty() || u.pools.empty()) return;
  Json a = Json::makeArray();
  for (int v : u.pools) a.push(Json::makeNumber(v));
  j.set("pools", a);
}

bool readUnitMastery(Reader& rd, const Battle& b, const Json& j, const std::string& where, Unit& u) {
  if (!readMastery(rd, *b.db, u.jobs, j, where, u.mastery)) return false;  // (after "jobs": the check asks what its jobs learned)
  u.pools.clear();
  if (!j.has("pools")) return true;
  const std::string at = where + ".pools";
  const int n = (int)b.db->unitResources.size();
  if (n == 0) return rd.fail(at, "this game has no unit resources");
  const Json* list;
  if (!rd.array(j, "pools", where, n, n, list)) return false;
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& v = (*list)[i];
    if (!v.isNumber() || v.asNumber() != std::floor(v.asNumber()) || v.asNumber() < 0 || v.asNumber() > kMaxSavedPool)
      return rd.fail(at + "[" + std::to_string(i) + "]", "not a whole number from 0 to " + std::to_string(kMaxSavedPool));
    u.pools.push_back((int)v.asNumber());
  }
  return true;
}

void writeRosterMastery(const Database& db, const RosterUnit& r, Json& j) {
  if (db.teachingOn() && !r.mastery.empty()) j.set("mastery", masteryJson(r.mastery));
}

bool readRosterMastery(Reader& rd, const Database& db, const Json& j, const std::string& where, RosterUnit& r) {
  return readMastery(rd, db, r.jobs, j, where, r.mastery);
}

}  // namespace savejson
}  // namespace ote
