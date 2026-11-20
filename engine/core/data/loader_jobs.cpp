// Loaders of the job system (loader_jobs.h; docs/JOBS.md). Every refusal names the file (through the LoadCtx), the entry and the key.
#include "core/data/loader_jobs.h"

#include <algorithm>

#include "core/jobs/job_rules.h"

namespace ote {

namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

constexpr int kMaxSlotCount = 8;

bool onlyThese(const LoadCtx& c, const Json& e, const std::string& at, std::initializer_list<const char*> keys) {
  for (const Json::Member& m : e.members()) {
    bool known = false;
    for (const char* k : keys) known = known || m.first == k;
    if (known) continue;
    std::string list;
    for (const char* k : keys) list += (list.empty() ? "" : ", ") + q(k);
    return c.fail(at, "unknown key " + q(m.first) + " (expected " + list + ")");
  }
  return true;
}

// "jpLevels": [100, 300, 600]: total JP for job levels 2, 3, ...; rising, each from 1 to kMaxJobJp.
bool readLevelTable(const LoadCtx& c, const Json& e, const std::string& at, std::vector<int>& out) {
  if (!e.has("jpLevels")) return true;
  const Json* list;
  if (!c.array(e, "jpLevels", at, list)) return false;
  if ((int)list->size() > kMaxJobLevel - 1) return c.fail(at, "\"jpLevels\" lists at most " + std::to_string(kMaxJobLevel - 1) + " steps (job levels 2 to " + std::to_string(kMaxJobLevel) + ")");
  out.clear();
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& v = (*list)[i];
    if (!LoadCtx::isWhole(v) || v.asInt() < 1 || v.asInt() > kMaxJobJp)
      return c.fail(at, "\"jpLevels\" entry " + std::to_string(i + 1) + " must be a whole number from 1 to " + std::to_string(kMaxJobJp));
    if (!out.empty() && v.asInt() <= out.back())
      return c.fail(at, "\"jpLevels\" entry " + std::to_string(i + 1) + " must be higher than the one before it (" + std::to_string(out.back()) + "): the levels go up in order");
    out.push_back(v.asInt());
  }
  return true;
}

bool readSlot(const LoadCtx& c, const Json& e, const std::string& at, JobSlot& out) {
  if (!e.has("slot")) return c.fail(at, "missing required \"slot\" (\"action\", \"reaction\", \"support\" or \"movement\")");
  const std::string name = e["slot"].isString() ? e["slot"].asString() : std::string();
  for (int i = 0; i < kJobSlotKinds; ++i)
    if (name == jobSlotName((JobSlot)i)) {
      out = (JobSlot)i;
      return true;
    }
  return c.fail(at, "\"slot\" must be \"action\", \"reaction\", \"support\" or \"movement\"");
}

bool readLearn(const LoadCtx& c, const Database& db, const Json& list, const std::string& where, JobDef& job) {
  if ((int)list.size() > kMaxJobLearn) return c.fail(where, "\"learn\" lists at most " + std::to_string(kMaxJobLearn) + " entries");
  for (size_t i = 0; i < list.size(); ++i) {
    const Json& e = list[i];
    const std::string at = where + ", \"learn\" entry " + std::to_string(i + 1);
    if (!e.isObject()) return c.fail(at, "must be an object: { \"skill\": \"id\", \"cost\": 100, \"slot\": \"support\" }");
    if (!onlyThese(c, e, at, {"skill", "ability", "cost", "slot"})) return false;
    if (e.has("skill") == e.has("ability")) return c.fail(at, "give \"skill\" (a skill id) or \"ability\" (a menu ability id), one of them");
    JobLearnDef d;
    d.ability = e.has("ability");
    const char* key = d.ability ? "ability" : "skill";
    if (!c.str(e, key, at, d.refId, true)) return false;
    d.ref = d.ability ? db.findAbility(d.refId) : db.findSkill(d.refId);
    if (d.ref < 0)
      return c.fail(at, std::string("\"") + key + "\" names unknown " + key + " " + q(d.refId) + (d.ability ? " (data/abilities.json)" : " (data/skills.json)"));
    if (!c.integerIn(e, "cost", at, 0, kMaxJobJp, d.cost, true) || !readSlot(c, e, at, d.slot)) return false;
    job.learn.push_back(d);
  }
  return true;
}

bool readJob(const LoadCtx& c, const Database& db, const Json& e, size_t index, JobDef& job) {
  std::string where = "jobs[" + std::to_string(index) + "]";
  if (!e.isObject()) return c.fail(where, "must be an object");
  if (!onlyThese(c, e, where, {"id", "name", "description", "unitType", "bonus", "requires", "jpLevels", "learn"})) return false;
  if (!c.str(e, "id", where, job.id, true)) return false;
  if (!isStoryId(job.id)) return c.fail(where, "\"id\" must be letters, digits, \"_\" and \"-\" (got " + q(job.id) + ")");
  where = "job " + q(job.id);
  for (const JobDef& o : db.jobs)
    if (o.id == job.id) return c.fail(where, "duplicate id");
  job.name = job.id;
  if (!c.str(e, "name", where, job.name) || !c.str(e, "description", where, job.description) || !c.str(e, "unitType", where, job.unitTypeId)) return false;
  if (job.name.empty() || job.name.size() > 24) return c.fail(where, "\"name\" must be 1 to 24 characters");
  // No "unitType" = an overlay job: the unit keeps its own type, stats and promotion path (docs/JOBS.md "Overlay jobs").
  if (e.has("unitType")) {
    job.unitType = db.findUnitType(job.unitTypeId);
    if (job.unitType < 0) return c.fail(where, "\"unitType\" names unknown unit type " + q(job.unitTypeId) + " (data/units.json); leave it out for a job that keeps the unit's own class");
  }
  if (e.has("bonus")) {
    job.bonus.assign(db.stats.size(), 0);
    if (!c.statMap(db, e, "bonus", where, job.bonus)) return false;
  }
  if (!readLevelTable(c, e, where, job.jpLevels)) return false;
  if (e.has("requires")) {
    const Json* list;
    if (!c.array(e, "requires", where, list)) return false;
    for (size_t i = 0; i < list->size(); ++i) {
      const Json& r = (*list)[i];
      const std::string at = where + ", \"requires\" entry " + std::to_string(i + 1);
      if (!r.isObject()) return c.fail(at, "must be an object: { \"job\": \"squire\", \"level\": 2 }");
      if (!onlyThese(c, r, at, {"job", "level"})) return false;
      JobRequire req;
      if (!c.str(r, "job", at, req.jobId, true) || !c.integerIn(r, "level", at, 1, kMaxJobLevel, req.level, true)) return false;
      job.needs.push_back(req);
    }
  }
  const Json* learn;
  if (!c.array(e, "learn", where, learn)) return false;
  return readLearn(c, db, *learn, where, job);
}

// A requirement chain that comes back to its start can never be met: refuse the cycle by name.
bool checkCycles(const LoadCtx& c, const Database& db) {
  const size_t n = db.jobs.size();
  std::vector<int> state(n, 0);  // 0 unseen, 1 on the path, 2 done
  std::vector<int> path;
  struct Walk {
    const LoadCtx& c;
    const Database& db;
    std::vector<int>& state;
    std::vector<int>& path;
    bool visit(int j) {
      state[(size_t)j] = 1;
      path.push_back(j);
      for (const JobRequire& r : db.jobs[(size_t)j].needs) {
        if (state[(size_t)r.job] == 1) {
          std::string loop;
          auto from = std::find(path.begin(), path.end(), r.job);
          for (auto it = from; it != path.end(); ++it) loop += q(db.jobs[(size_t)*it].id) + " -> ";
          loop += q(db.jobs[(size_t)r.job].id);
          return c.fail("job " + q(db.jobs[(size_t)j].id), "\"requires\" goes round in a circle (" + loop + "): none of them could ever be unlocked");
        }
        if (state[(size_t)r.job] == 0 && !visit(r.job)) return false;
      }
      path.pop_back();
      state[(size_t)j] = 2;
      return true;
    }
  } walk{c, db, state, path};
  for (size_t j = 0; j < n; ++j)
    if (state[j] == 0 && !walk.visit((int)j)) return false;
  return true;
}

}  // namespace

bool loadJobRules(const LoadCtx& c, const Json& rules, Database& db) {
  db.rules.jobs = JobRules();
  if (!rules.has("jobs")) return true;
  const Json* blk;
  if (!c.object(rules, "jobs", "rules", blk)) return false;
  JobRules& r = db.rules.jobs;
  r.declared = true;
  const std::string at = "rules.jobs";
  if (!onlyThese(c, *blk, at, {"jp", "slots", "jpLevels", "changeIn"})) return false;
  if (blk->has("jp")) {
    const Json* jp;
    if (!c.object(*blk, "jp", at, jp)) return false;
    if (!onlyThese(c, *jp, at + ", \"jp\"", {"perAction", "perDefeat"})) return false;
    if (!c.integerIn(*jp, "perAction", at + ", \"jp\"", 0, kMaxJobJp, r.jpPerAction) || !c.integerIn(*jp, "perDefeat", at + ", \"jp\"", 0, kMaxJobJp, r.jpPerDefeat)) return false;
  }
  if (blk->has("slots")) {
    const Json* slots;
    if (!c.object(*blk, "slots", at, slots)) return false;
    if (!onlyThese(c, *slots, at + ", \"slots\"", {"action", "reaction", "support", "movement"})) return false;
    for (int i = 0; i < kJobSlotKinds; ++i)
      if (!c.integerIn(*slots, jobSlotName((JobSlot)i), at + ", \"slots\"", 0, kMaxSlotCount, r.slots[i])) return false;
  }
  if (!readLevelTable(c, *blk, at, r.jpLevels)) return false;
  if (blk->has("changeIn")) {
    std::string where;
    if (!c.str(*blk, "changeIn", at, where)) return false;
    if (where != "camp" && where != "camp+prep") return c.fail(at, "\"changeIn\" must be \"camp\" (between chapters) or \"camp+prep\" (also on the preparation screen)");
    r.changeInPrep = where == "camp+prep";
  }
  return true;
}

bool loadJobs(const std::string& path, Database& db, std::string& err) {
  db.jobs.clear();
  db.jobEntries.clear();
  const LoadCtx game(err, db.rootPath + "/game.json");
  if (!fileExists(path)) {
    if (db.rules.jobs.declared) return game.fail("rules.jobs", "needs data/jobs.json (the jobs themselves); the block does nothing without it");
    return true;
  }
  Json root;
  if (!loadObjectFile(path, root, err)) return false;
  LoadCtx c(err, path);
  const Json* list;
  if (!c.array(root, "jobs", "", list, true)) return false;
  if ((int)list->size() > kMaxJobs) return c.fail("", "too many jobs (at most " + std::to_string(kMaxJobs) + ")");
  if (list->size() == 0) {
    if (db.rules.jobs.declared) return game.fail("rules.jobs", "needs data/jobs.json to list at least one job");
    return true;  // the editor writes an empty list to remember that the maker switched the feature on: the same game as no file
  }
  for (size_t i = 0; i < list->size(); ++i) {
    JobDef job;
    if (!readJob(c, db, (*list)[i], i, job)) return false;
    db.jobs.push_back(job);
  }
  // Requirements name jobs, so they are resolved once every job is known.
  for (JobDef& job : db.jobs)
    for (JobRequire& r : job.needs) {
      const std::string at = "job " + q(job.id) + ", \"requires\"";
      r.job = db.findJob(r.jobId);
      if (r.job < 0) return c.fail(at, "names unknown job " + q(r.jobId));
      if (r.jobId == job.id) return c.fail(at, "a job cannot require itself");
      const int top = jobMaxLevel(db, r.job);
      if (r.level > top)
        return c.fail(at, q(r.jobId) + " only has " + std::to_string(top) + " job level" + (top == 1 ? "" : "s") + " (\"jpLevels\"), so level " + std::to_string(r.level) + " can never be reached");
    }
  if (!checkCycles(c, db)) return false;
  // One flat list of everything the jobs teach. A skill or ability is taught once in the whole game, and no skill shares its id with an ability that is
  // taught, so a plain id in a character's "learned" / "equipped" always means one entry.
  for (size_t j = 0; j < db.jobs.size(); ++j) {
    JobDef& job = db.jobs[j];
    job.firstFlat = (int)db.jobEntries.size();
    for (size_t i = 0; i < job.learn.size(); ++i) {
      JobLearnDef& d = job.learn[i];
      d.job = (int)j;
      d.flat = (int)db.jobEntries.size();
      const std::string at = "job " + q(job.id) + ", \"learn\" entry " + std::to_string(i + 1);
      for (const JobLearnDef& o : db.jobEntries) {
        if (o.refId != d.refId) continue;
        if (o.ability != d.ability) return c.fail(at, q(d.refId) + " is taught as a skill and as an ability (job " + q(db.jobs[(size_t)o.job].id) + "): a unit's \"learned\" and \"equipped\" lists could not tell them apart; give them different ids");
        return c.fail(at, q(d.refId) + " is listed twice (also by job " + q(db.jobs[(size_t)o.job].id) + "): a skill or ability is taught by one job only");
      }
      db.jobEntries.push_back(d);
    }
  }
  return true;
}

namespace {

// The flat index of what the job `job` teaches under the id `id` (-1 = not taught by it).
int entryOf(const Database& db, int job, const std::string& id) {
  for (const JobLearnDef& d : db.jobs[(size_t)job].learn)
    if (d.refId == id) return d.flat;
  return -1;
}

bool readIdArray(const LoadCtx& c, const Json& e, const char* key, const std::string& where, std::vector<std::string>& out) {
  if (!e.has(key)) return true;
  return c.stringList(e, key, where, out);
}

}  // namespace

bool isJobKey(const std::string& key) { return key == "job" || key == "jobs" || key == "equipped" || key == "learned"; }

bool readJobKeys(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, JobState& out, bool& given) {
  given = false;
  bool any = false;
  for (const Json::Member& m : e.members()) any = any || isJobKey(m.first);
  if (!any) return true;
  given = true;
  if (!db.jobsOn())
    return c.fail(where, "the job keys (\"job\", \"jobs\", \"equipped\", \"learned\") need jobs: data/jobs.json (docs/JOBS.md)");
  out = JobState();
  if (!e.has("job")) return c.fail(where, "\"jobs\", \"equipped\" and \"learned\" belong to a unit that has a job: add \"job\"");
  std::string jobId;
  if (!c.str(e, "job", where, jobId, true)) return false;
  out.job = db.findJob(jobId);
  if (out.job < 0) return c.fail(where, "\"job\" names unknown job " + q(jobId) + " (data/jobs.json)");
  // "jobs": { "squire": { "jp": 120, "learned": ["guard"] } }: progress in jobs; learned entries here cost nothing.
  if (e.has("jobs")) {
    const Json* jobs;
    if (!c.object(e, "jobs", where, jobs)) return false;
    for (const Json::Member& m : jobs->members()) {
      const std::string at = where + ", \"jobs\" " + q(m.first);
      const int j = db.findJob(m.first);
      if (j < 0) return c.fail(where, "\"jobs\" names unknown job " + q(m.first) + " (data/jobs.json)");
      if (!m.second.isObject()) return c.fail(at, "must be an object: { \"jp\": 120, \"learned\": [\"guard\"] }");
      if (!onlyThese(c, m.second, at, {"jp", "learned"})) return false;
      JobProgress& p = out.at(j);
      if (!c.integerIn(m.second, "jp", at, 0, kMaxJobTotal, p.earned)) return false;
      std::vector<std::string> ids;
      if (!readIdArray(c, m.second, "learned", at, ids)) return false;
      for (const std::string& id : ids) {
        const int flat = entryOf(db, j, id);
        if (flat < 0) return c.fail(at, "\"learned\" names " + q(id) + ", which job " + q(m.first) + " does not teach (its \"learn\" list in data/jobs.json)");
        if (std::find(p.learned.begin(), p.learned.end(), flat) != p.learned.end()) return c.fail(at, "\"learned\" lists " + q(id) + " twice");
        p.learned.push_back(flat);
      }
      std::sort(p.learned.begin(), p.learned.end());
    }
  }
  // "learned": the shorthand for the current job's learned entries.
  std::vector<std::string> own;
  if (!readIdArray(c, e, "learned", where, own)) return false;
  JobProgress& cur = out.at(out.job);
  for (const std::string& id : own) {
    const int flat = entryOf(db, out.job, id);
    if (flat < 0) return c.fail(where, "\"learned\" names " + q(id) + ", which the job " + q(jobId) + " does not teach (its \"learn\" list in data/jobs.json)");
    if (std::find(cur.learned.begin(), cur.learned.end(), flat) != cur.learned.end()) return c.fail(where, "\"learned\" lists " + q(id) + " twice");
    cur.learned.insert(std::upper_bound(cur.learned.begin(), cur.learned.end(), flat), flat);
  }
  std::vector<std::string> equipped;
  if (!readIdArray(c, e, "equipped", where, equipped)) return false;
  for (const std::string& id : equipped) {
    int flat = -1;
    for (const JobLearnDef& d : db.jobEntries)
      if (d.refId == id) flat = d.flat;
    if (flat < 0) return c.fail(where, "\"equipped\" names " + q(id) + ", which no job teaches (data/jobs.json)");
    out.equipped.push_back(flat);
  }
  // Drop empty progress entries a "jobs" key left behind? No: a job with 0 JP and nothing learned is simply not listed.
  out.progress.erase(std::remove_if(out.progress.begin(), out.progress.end(),
                                    [](const JobProgress& p) { return p.earned == 0 && p.spent == 0 && p.learned.empty(); }),
                     out.progress.end());
  // The base type (the unit's own class) is filled in by the caller, who knows the unit type; a stand-in lets the state check run on the rest.
  out.baseType = db.jobsOverlay() ? 0 : -1;
  const std::string problem = jobStateProblem(db, out);
  out.baseType = -1;
  if (!problem.empty()) return c.fail(where, problem);
  return true;
}

}  // namespace ote
