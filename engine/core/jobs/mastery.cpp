// Learning from equipment over plain data (mastery.h; docs/JOBS.md "Learning from equipment (ability points)").
#include "core/jobs/mastery.h"

#include <algorithm>

#include "core/jobs/job_rules.h"

namespace ote {

namespace {

bool validTeach(const Database& db, int entry) { return entry >= 0 && entry < (int)db.teachEntries.size(); }

}  // namespace

int teachPoints(const std::vector<MasteryPoints>& m, int entry) {
  for (const MasteryPoints& p : m)
    if (p.entry == entry) return p.points;
  return 0;
}

bool teachMastered(const Database& db, const JobState& s, const std::vector<MasteryPoints>& m, int entry) {
  if (!validTeach(db, entry)) return false;
  const TeachDef& t = db.teachEntries[(size_t)entry];
  if (t.jobEntry >= 0) return jobHasLearned(s, t.jobEntry);
  return teachPoints(m, entry) >= t.points;
}

int addTeachPoints(const Database& db, JobState& s, std::vector<MasteryPoints>& m, int entry, int amount, bool& mastered) {
  mastered = false;
  if (!validTeach(db, entry)) return 0;
  const TeachDef& t = db.teachEntries[(size_t)entry];
  if (teachMastered(db, s, m, entry)) return t.points;
  auto at = std::lower_bound(m.begin(), m.end(), entry, [](const MasteryPoints& p, int e) { return p.entry < e; });
  if (at == m.end() || at->entry != entry) {
    MasteryPoints p;
    p.entry = entry;
    at = m.insert(at, p);
  }
  at->points = std::min(t.points, at->points + std::max(0, amount));
  const int now = at->points;
  if (now < t.points) return now;
  mastered = true;
  if (t.jobEntry >= 0) {  // it belongs to a job: learned there for good, as if bought (no JP spent)
    const JobLearnDef& d = db.jobEntries[(size_t)t.jobEntry];
    JobProgress& p = s.at(d.job);
    p.learned.insert(std::upper_bound(p.learned.begin(), p.learned.end(), d.flat), d.flat);
    m.erase(at);
  }
  return t.points;
}

std::string masteryProblem(const Database& db, const JobState& s, const std::vector<MasteryPoints>& m) {
  if (m.empty()) return "";
  if (!db.teachingOn()) return "this game has no items that teach";
  int last = -1;
  for (const MasteryPoints& p : m) {
    if (!validTeach(db, p.entry)) return "entry " + std::to_string(p.entry) + " is not an entry items teach in this game";
    if (p.entry <= last) return "entry " + std::to_string(p.entry) + " is listed twice or out of order";
    last = p.entry;
    const TeachDef& t = db.teachEntries[(size_t)p.entry];
    if (p.points < 1 || p.points > t.points) return "\"" + t.refId + "\": points must be from 1 to " + std::to_string(t.points);
    if (t.jobEntry >= 0 && (p.points >= t.points || jobHasLearned(s, t.jobEntry)))
      return "\"" + t.refId + "\" is learned in its job: it keeps no points";
  }
  return "";
}

std::string teachEntryName(const Database& db, int entry) {
  if (!validTeach(db, entry)) return "";
  const TeachDef& t = db.teachEntries[(size_t)entry];
  return t.ability ? db.abilities[(size_t)t.ref].name : db.skills[(size_t)t.ref].name;
}

}  // namespace ote
