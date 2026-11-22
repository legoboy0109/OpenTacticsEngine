// The rules of the job system over a JobState (job_rules.h; docs/JOBS.md).
#include "core/jobs/job_rules.h"

#include <algorithm>

#include "core/jobs/mastery.h"

namespace ote {

namespace {

bool validJob(const Database& db, int job) { return job >= 0 && job < (int)db.jobs.size(); }
bool validEntry(const Database& db, int flat) { return flat >= 0 && flat < (int)db.jobEntries.size(); }

}  // namespace

const std::vector<int>& jobLevelTable(const Database& db, int job) {
  if (validJob(db, job) && !db.jobs[(size_t)job].jpLevels.empty()) return db.jobs[(size_t)job].jpLevels;
  return db.rules.jobs.jpLevels;
}

int jobMaxLevel(const Database& db, int job) { return 1 + (int)jobLevelTable(db, job).size(); }

int jobLevelAt(const Database& db, int job, int earned) {
  int level = 1;
  for (int at : jobLevelTable(db, job))
    if (earned >= at) ++level;
  return level;
}

int jobLevelOf(const Database& db, const JobState& s, int job) {
  const JobProgress* p = s.find(job);
  return jobLevelAt(db, job, p ? p->earned : 0);
}

int jobNextLevelAt(const Database& db, int job, int earned) {
  const std::vector<int>& t = jobLevelTable(db, job);
  const int level = jobLevelAt(db, job, earned);
  return level - 1 < (int)t.size() ? t[(size_t)level - 1] : -1;
}

int jobUnspent(const JobState& s, int job) {
  const JobProgress* p = s.find(job);
  return p ? p->earned - p->spent : 0;
}

int jobMasteredCount(const Database& db, const JobState& s, int job, int slot) {
  const JobProgress* p = s.find(job);
  if (!p) return 0;
  int n = 0;
  for (int flat : p->learned)
    if (validEntry(db, flat) && (slot < 0 || (int)db.jobEntries[(size_t)flat].slot == slot)) ++n;
  return n;
}

bool jobUnlocked(const Database& db, const JobState& s, int job, std::string* why, const std::vector<MasteryPoints>* mastery) {
  if (!validJob(db, job)) {
    if (why) *why = "there is no such job";
    return false;
  }
  for (const JobRequire& r : db.jobs[(size_t)job].needs) {
    switch (r.kind) {
      case JobRequire::Kind::Level:
        if (jobLevelOf(db, s, r.job) >= r.level) continue;
        if (why) *why = "needs " + db.jobs[(size_t)r.job].name + " level " + std::to_string(r.level);
        return false;
      case JobRequire::Kind::Count:  // docs/JOBS.md "Unlocking by mastered entries"
        if (jobMasteredCount(db, s, r.job, r.slot) >= r.count) continue;
        if (why)
          *why = "needs " + std::to_string(r.count) + " " + db.jobs[(size_t)r.job].name + (r.slot >= 0 ? std::string(" ") + jobSlotName((JobSlot)r.slot) : std::string()) +
                 (r.count == 1 ? " entry" : " entries") + " mastered";
        return false;
      case JobRequire::Kind::Entry: {
        const bool have = r.teach ? (mastery && teachMastered(db, s, *mastery, r.entry)) : jobHasLearned(s, r.entry);
        if (have) continue;
        if (why) *why = "needs " + (r.teach ? teachEntryName(db, r.entry) : jobEntryName(db, r.entry)) + " mastered";
        return false;
      }
    }
  }
  return true;
}

int jobSlotUsed(const Database& db, const JobState& s, JobSlot slot) {
  int n = 0;
  for (int flat : s.equipped)
    if (validEntry(db, flat) && db.jobEntries[(size_t)flat].slot == slot) ++n;
  return n;
}

bool jobHasLearned(const JobState& s, int flat) {
  for (const JobProgress& p : s.progress)
    if (std::find(p.learned.begin(), p.learned.end(), flat) != p.learned.end()) return true;
  return false;
}

void jobActiveEntries(const Database& db, const JobState& s, std::vector<int>& out) {
  out.clear();
  if (!s.on() || !validJob(db, s.job)) return;
  if (const JobProgress* p = s.find(s.job))
    for (int flat : p->learned) out.push_back(flat);
  for (int flat : s.equipped)
    if (std::find(out.begin(), out.end(), flat) == out.end()) out.push_back(flat);
}

bool jobSwitch(const Database& db, JobState& s, int job, std::string& why, const std::vector<MasteryPoints>* mastery) {
  if (!jobUnlocked(db, s, job, &why, mastery)) return false;
  s.job = job;
  s.equipped.erase(std::remove_if(s.equipped.begin(), s.equipped.end(),
                                  [&](int flat) { return validEntry(db, flat) && db.jobEntries[(size_t)flat].job == job; }),
                   s.equipped.end());
  return true;
}

bool jobLearnReason(const Database& db, const JobState& s, int flat, std::string& why, const std::vector<MasteryPoints>* mastery) {
  if (!validEntry(db, flat)) {
    why = "there is nothing like that to learn";
    return false;
  }
  const JobLearnDef& e = db.jobEntries[(size_t)flat];
  if (jobHasLearned(s, flat)) {
    why = "already learned";
    return false;
  }
  if (!db.rules.jobs.spendJp) {  // docs/JOBS.md "Learning from equipment": JP are never spent
    why = "learned only from equipment";
    return false;
  }
  std::string lock;
  if (!jobUnlocked(db, s, e.job, &lock, mastery)) {
    why = db.jobs[(size_t)e.job].name + " is locked: " + lock;
    return false;
  }
  const int left = jobUnspent(s, e.job);
  if (left < e.cost) {
    why = "costs " + std::to_string(e.cost) + " JP, " + std::to_string(left) + " left in " + db.jobs[(size_t)e.job].name;
    return false;
  }
  return true;
}

bool jobLearn(const Database& db, JobState& s, int flat, std::string& why, const std::vector<MasteryPoints>* mastery) {
  if (!jobLearnReason(db, s, flat, why, mastery)) return false;
  const JobLearnDef& e = db.jobEntries[(size_t)flat];
  JobProgress& p = s.at(e.job);
  p.spent += e.cost;
  p.learned.insert(std::upper_bound(p.learned.begin(), p.learned.end(), flat), flat);
  return true;
}

bool jobEquipReason(const Database& db, const JobState& s, int flat, std::string& why) {
  if (!validEntry(db, flat)) {
    why = "there is nothing like that to equip";
    return false;
  }
  const JobLearnDef& e = db.jobEntries[(size_t)flat];
  if (!jobHasLearned(s, flat)) {
    why = "not learned yet";
    return false;
  }
  if (e.job == s.job) {
    why = "already active: it belongs to the current job";
    return false;
  }
  if (std::find(s.equipped.begin(), s.equipped.end(), flat) != s.equipped.end()) {
    why = "already equipped";
    return false;
  }
  const int room = db.rules.jobs.slots[(int)e.slot];
  if (jobSlotUsed(db, s, e.slot) >= room) {
    why = room == 0 ? std::string("this game has no ") + jobSlotName(e.slot) + " slots"
                    : std::string("the ") + jobSlotName(e.slot) + " slots are full (" + std::to_string(room) + ")";
    return false;
  }
  return true;
}

bool jobEquip(const Database& db, JobState& s, int flat, std::string& why) {
  if (!jobEquipReason(db, s, flat, why)) return false;
  s.equipped.push_back(flat);
  return true;
}

bool jobUnequip(const Database& db, JobState& s, int flat, std::string& why) {
  (void)db;
  auto at = std::find(s.equipped.begin(), s.equipped.end(), flat);
  if (at == s.equipped.end()) {
    why = "not equipped";
    return false;
  }
  s.equipped.erase(at);
  return true;
}

int jobPlayType(const Database& db, const JobState& s, int job, int fallback) {
  if (!validJob(db, job)) return fallback;
  const JobDef& d = db.jobs[(size_t)job];
  if (!d.overlay()) return d.unitType;
  return s.baseType >= 0 ? s.baseType : fallback;
}

std::string jobTypeProblem(const Database& db, const JobState& s, int type) {
  if (!db.jobsOverlay() || !s.on() || !validJob(db, s.job)) return "";
  const int want = jobPlayType(db, s, s.job, type);
  if (want == type) return "";
  const JobDef& d = db.jobs[(size_t)s.job];
  return std::string("a unit in ") + (d.overlay() ? "the overlay job \"" : "the class job \"") + d.id + "\" must have the unit type \"" + db.unitTypes[(size_t)want].id + "\" (it has \"" +
         db.unitTypes[(size_t)type].id + "\")";
}

int jobBonusOf(const Database& db, const JobState& s, int stat) {
  if (!s.on() || !validJob(db, s.job) || stat < 0) return 0;
  const std::vector<int>& b = db.jobs[(size_t)s.job].bonus;
  return (size_t)stat < b.size() ? b[(size_t)stat] : 0;
}

std::string jobStateProblem(const Database& db, const JobState& s) {
  if (!db.jobsOn()) return s.on() || !s.progress.empty() || !s.equipped.empty() ? "this game has no jobs" : "";
  if (!s.on()) return s.progress.empty() && s.equipped.empty() ? "" : "has job progress but no job";
  if (!validJob(db, s.job)) return "\"job\" " + std::to_string(s.job) + " is not a job of this game";
  if (db.jobsOverlay()) {  // the unit's own class is kept so that an overlay job can play as it
    if (s.baseType < 0 || s.baseType >= (int)db.unitTypes.size()) return "\"baseType\" must be a unit type of this game (the game has overlay jobs: the unit's own class is kept)";
  } else if (s.baseType >= 0) {
    return "\"baseType\" belongs to games with overlay jobs (a job without \"unitType\")";
  }
  int last = -1;
  for (const JobProgress& p : s.progress) {
    const std::string at = "job " + std::to_string(p.job);
    if (!validJob(db, p.job)) return at + " is not a job of this game";
    if (p.job <= last) return at + " is listed twice or out of order";
    last = p.job;
    if (p.earned < 0 || p.spent < 0 || p.spent > p.earned) return "job \"" + db.jobs[(size_t)p.job].id + "\": spent JP must be from 0 to the JP earned";
    int prev = -1;
    for (int flat : p.learned) {
      if (!validEntry(db, flat) || db.jobEntries[(size_t)flat].job != p.job)
        return "job \"" + db.jobs[(size_t)p.job].id + "\": learned entry " + std::to_string(flat) + " is not taught by that job";
      if (flat <= prev) return "job \"" + db.jobs[(size_t)p.job].id + "\": learned entry " + std::to_string(flat) + " is listed twice or out of order";
      prev = flat;
    }
  }
  int used[kJobSlotKinds] = {0, 0, 0, 0};
  for (size_t i = 0; i < s.equipped.size(); ++i) {
    const int flat = s.equipped[i];
    if (!validEntry(db, flat)) return "equipped entry " + std::to_string(flat) + " is not a learn entry of this game";
    const JobLearnDef& e = db.jobEntries[(size_t)flat];
    if (!jobHasLearned(s, flat)) return "equipped \"" + e.refId + "\" has not been learned";
    if (e.job == s.job) return "equipped \"" + e.refId + "\" belongs to the current job (it is active anyway)";
    for (size_t o = 0; o < i; ++o)
      if (s.equipped[o] == flat) return "equipped \"" + e.refId + "\" is listed twice";
    if (++used[(int)e.slot] > db.rules.jobs.slots[(int)e.slot])
      return std::string("more equipped entries than ") + jobSlotName(e.slot) + " slots (" + std::to_string(db.rules.jobs.slots[(int)e.slot]) + ")";
  }
  return "";
}

std::string jobEntryName(const Database& db, int flat) {
  if (!validEntry(db, flat)) return "";
  const JobLearnDef& e = db.jobEntries[(size_t)flat];
  return e.ability ? db.abilities[(size_t)e.ref].name : db.skills[(size_t)e.ref].name;
}

std::string jobEntryKind(const Database& db, int flat) {
  if (!validEntry(db, flat)) return "";
  return db.jobEntries[(size_t)flat].ability ? "Ability" : "Skill";
}

}  // namespace ote
