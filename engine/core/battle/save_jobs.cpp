// Jobs in saves (docs/JOBS.md "Saves", docs/SAVES.md): a unit's and a roster entry's job state as
//   "jobs": { "job": 1, "progress": [ { "job": 0, "earned": 120, "spent": 40, "learned": [2] } ], "equipped": [5] }
// with indices as everywhere in a save: the job into data/jobs.json's list, the learned and equipped numbers into the flat list of everything the
// jobs teach (file order). Written only for a unit that has a job in a game with jobs; the key in a game without jobs is refused.
#include "core/battle/save_srpg.h"
#include "core/jobs/job_rules.h"

namespace ote {
namespace savejson {

namespace {

Json indexList(const std::vector<int>& v) {
  Json a = Json::makeArray();
  for (int i : v) a.push(Json::makeNumber(i));
  return a;
}

Json stateJson(const JobState& s) {
  Json o = Json::makeObject();
  o.set("job", Json::makeNumber(s.job));
  Json list = Json::makeArray();
  for (const JobProgress& p : s.progress) {
    Json e = Json::makeObject();
    e.set("job", Json::makeNumber(p.job));
    e.set("earned", Json::makeNumber(p.earned));
    if (p.spent > 0) e.set("spent", Json::makeNumber(p.spent));
    if (!p.learned.empty()) e.set("learned", indexList(p.learned));
    list.push(e);
  }
  if (list.size() > 0) o.set("progress", list);
  if (!s.equipped.empty()) o.set("equipped", indexList(s.equipped));
  return o;
}

bool readIndices(Reader& rd, const Json& j, const char* key, const std::string& where, int hi, std::vector<int>& out) {
  out.clear();
  if (!j.has(key)) return true;
  const Json* list;
  if (!rd.array(j, key, where, -1, 1000, list)) return false;
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& v = (*list)[i];
    if (!v.isNumber() || v.asNumber() != std::floor(v.asNumber()) || v.asNumber() < 0 || v.asNumber() > hi)
      return rd.fail(where + "." + key + "[" + std::to_string(i) + "]", "not a whole number from 0 to " + std::to_string(hi));
    out.push_back((int)v.asNumber());
  }
  return true;
}

bool readState(Reader& rd, const Database& db, const Json& j, const std::string& where, JobState& out) {
  out = JobState();
  if (!j.has("jobs")) return true;
  const std::string at = where + ".jobs";
  if (!db.jobsOn()) return rd.fail(at, "this game has no jobs");
  const Json* o;
  if (!rd.object(j, "jobs", where, o)) return false;
  if (!rd.integer(*o, "job", at, 0, (int)db.jobs.size() - 1, out.job)) return false;
  if (o->has("progress")) {
    const Json* list;
    if (!rd.array(*o, "progress", at, -1, kMaxJobs, list)) return false;
    for (size_t i = 0; i < list->size(); ++i) {
      const Json& e = (*list)[i];
      const std::string pa = at + ".progress[" + std::to_string(i) + "]";
      if (!e.isObject()) return rd.fail(pa, "not an object");
      JobProgress p;
      p.spent = 0;
      if (!rd.integer(e, "job", pa, 0, (int)db.jobs.size() - 1, p.job) || !rd.integer(e, "earned", pa, 0, kMaxJobTotal, p.earned)) return false;
      if (e.has("spent") && !rd.integer(e, "spent", pa, 0, kMaxJobTotal, p.spent)) return false;
      if (!readIndices(rd, e, "learned", pa, (int)db.jobEntries.size() - 1, p.learned)) return false;
      out.progress.push_back(p);
    }
  }
  if (!readIndices(rd, *o, "equipped", at, (int)db.jobEntries.size() - 1, out.equipped)) return false;
  const std::string problem = jobStateProblem(db, out);
  if (!problem.empty()) return rd.fail(at, problem);
  return true;
}

}  // namespace

void writeUnitJobs(const Battle& b, const Unit& u, Json& j) {
  if (b.db->jobsOn() && u.jobs.on()) j.set("jobs", stateJson(u.jobs));
}

bool readUnitJobs(Reader& rd, const Battle& b, const Json& j, const std::string& where, Unit& u) { return readState(rd, *b.db, j, where, u.jobs); }

void writeRosterJobs(const Database& db, const RosterUnit& r, Json& j) {
  if (db.jobsOn() && r.jobs.on()) j.set("jobs", stateJson(r.jobs));
}

bool readRosterJobs(Reader& rd, const Database& db, const Json& j, const std::string& where, RosterUnit& r) { return readState(rd, db, j, where, r.jobs); }

}  // namespace savejson
}  // namespace ote
