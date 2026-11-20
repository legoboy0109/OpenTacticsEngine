// Jobs outside a battle (job_camp.h; docs/JOBS.md "Between chapters").
#include "core/jobs/job_camp.h"

#include <algorithm>

#include "core/campaign/prep.h"
#include "core/jobs/job_rules.h"

namespace ote {

namespace {

bool refuse(std::string& why, const std::string& reason) {
  why = reason;
  return false;
}

// The character `id` if it exists and uses jobs (a RosterUnit* for a Playthrough&, a const one for a const Playthrough&).
template <class P>
auto usable(const Database& db, P& pt, const std::string& id, std::string& why) -> decltype(&pt.roster[0]) {
  if (!db.jobsOn()) {
    why = "this game has no jobs";
    return nullptr;
  }
  decltype(&pt.roster[0]) r = nullptr;
  for (auto& e : pt.roster)
    if (e.id == id) r = &e;
  if (!r || r->gone) {
    why = "there is no such character";
    return nullptr;
  }
  if (!r->jobs.on()) {
    why = r->name + " does not use jobs";
    return nullptr;
  }
  return r;
}

}  // namespace

bool jobsRosterUses(const Database& db, const RosterUnit& r) { return db.jobsOn() && !r.gone && r.jobs.on(); }

std::vector<std::string> jobsRosterIds(const Database& db, const Playthrough& pt) {
  std::vector<std::string> out;
  for (const RosterUnit& r : pt.roster)
    if (jobsRosterUses(db, r)) out.push_back(r.id);
  return out;
}

bool jobsChangeAllowedInPrep(const Database& db) { return db.jobsOn() && db.rules.jobs.changeInPrep; }

bool jobsChangeReason(const Database& db, const Playthrough& pt, const std::string& id, int job, std::string& why) {
  const RosterUnit* r = usable(db, pt, id, why);
  if (!r) return false;
  if (job < 0 || job >= (int)db.jobs.size()) return refuse(why, "there is no such job");
  if (r->jobs.job == job) return refuse(why, r->name + " is a " + db.jobs[(size_t)job].name + " already");
  std::string lock;
  if (!jobUnlocked(db, r->jobs, job, &lock)) return refuse(why, lock);
  return true;
}

bool jobsChange(const Database& db, Playthrough& pt, const std::string& id, int job, std::string& why) {
  if (!jobsChangeReason(db, pt, id, job, why)) return false;
  std::string unused;
  RosterUnit& r = *usable(db, pt, id, unused);
  // The type it plays as under the new job: the class job's own, or (an overlay job) its base type. From an overlay job to another nothing changes.
  const int toType = jobPlayType(db, r.jobs, job, r.type);
  const UnitTypeDef& from = db.unitTypes[(size_t)r.type];
  const UnitTypeDef& to = db.unitTypes[(size_t)toType];
  if (!jobSwitch(db, r.jobs, job, why)) return false;  // cannot fail after the reason test
  r.stats.resize(db.stats.size(), 0);
  for (size_t s = 0; s < r.stats.size(); ++s) r.stats[s] = std::max(0, r.stats[s] + to.stats[s] - from.stats[s]);
  r.type = toType;
  return true;
}

bool jobsChangeInPrep(const Database& db, Playthrough& pt, const MapDef& map, const std::string& id, int job, std::string& why) {
  if (!jobsChangeAllowedInPrep(db)) return refuse(why, "jobs are changed in the camp, between chapters");
  if (!jobsChangeReason(db, pt, id, job, why)) return false;
  const RosterUnit* ru = pt.findUnit(id);
  const int type = jobPlayType(db, ru ? ru->jobs : JobState(), job, ru ? ru->type : 0);
  for (const PrepDeploy& d : pt.prep.deployed)
    if (d.id == id && !prepCanStand(db, map, type, d.slot))
      return refuse(why, "a " + db.jobs[(size_t)job].name + " could not stand on slot " + std::to_string(d.slot + 1));
  return jobsChange(db, pt, id, job, why);
}

bool jobsLearn(const Database& db, Playthrough& pt, const std::string& id, int flat, std::string& why) {
  RosterUnit* r = usable(db, pt, id, why);
  return r && jobLearn(db, r->jobs, flat, why);
}

bool jobsEquip(const Database& db, Playthrough& pt, const std::string& id, int flat, std::string& why) {
  RosterUnit* r = usable(db, pt, id, why);
  return r && jobEquip(db, r->jobs, flat, why);
}

bool jobsUnequip(const Database& db, Playthrough& pt, const std::string& id, int flat, std::string& why) {
  RosterUnit* r = usable(db, pt, id, why);
  return r && jobUnequip(db, r->jobs, flat, why);
}

}  // namespace ote
