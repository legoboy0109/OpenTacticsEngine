// Jobs in a battle (docs/JOBS.md): what a unit's job gives it (skills and abilities), and the job points it earns. A unit's JobState lives on the Unit
// and is saved with it (save_jobs.cpp); everything here is a no-op in a game without data/jobs.json.
#include <algorithm>

#include "core/battle/battle.h"
#include "core/jobs/job_rules.h"

namespace ote {

int Battle::unitJobLevel(const Unit& u) const {
  if (!jobsGame() || !u.jobs.on()) return 0;
  return jobLevelOf(*db, u.jobs, u.jobs.job);
}

void Battle::jobEntriesOf(const Unit& u, std::vector<int>& out) const {
  out.clear();
  if (!jobsGame()) return;
  jobActiveEntries(*db, u.jobs, out);
}

bool Battle::jobGivesSkill(const Unit& u, int skill) const {
  std::vector<int> entries;
  jobEntriesOf(u, entries);
  for (int flat : entries) {
    const JobLearnDef& e = db->jobEntries[(size_t)flat];
    if (!e.ability && e.ref == skill) return true;
  }
  return false;
}

bool Battle::jobGivesAbility(const Unit& u, int ability) const {
  std::vector<int> entries;
  jobEntriesOf(u, entries);
  for (int flat : entries) {
    const JobLearnDef& e = db->jobEntries[(size_t)flat];
    if (e.ability && e.ref == ability) return true;
  }
  return false;
}

// JP goes to the job the unit uses now. One event per award: a=unit, b=JP, c=job, d=its job level now; kJobLevelUp when the level rose.
// With "jp": { "share": n } every other unit of its side that uses jobs and is on the board gets n percent of it in the SAME job (docs/JOBS.md
// "Sharing JP with allies"; flag kJobShared), however far away; shared JP is not shared again.
void Battle::awardJp(int unit, int amount, std::vector<Event>& events) {
  if (!jobsGame() || amount <= 0 || unit < 0 || unit >= (int)units.size()) return;
  Unit& u = units[(size_t)unit];
  if (!u.alive || !u.jobs.on()) return;
  const int job = u.jobs.job;
  auto give = [&](int who, int jp, int flags) {
    Unit& w = units[(size_t)who];
    int before = 1, after = 1;
    JobProgress& p = w.jobs.at(job);
    before = jobLevelAt(*db, job, p.earned);
    p.earned = std::min(kMaxJobTotal, p.earned + jp);
    after = jobLevelAt(*db, job, p.earned);
    Event e;
    e.type = EventType::JpGained;
    e.a = who;
    e.b = jp;
    e.c = job;
    e.d = after;
    e.flags = flags | (after > before ? kJobLevelUp : 0);
    events.push_back(e);
  };
  give(unit, amount, 0);
  const int share = amount * db->rules.jobs.jpShare / 100;
  if (share <= 0) return;
  const int side = u.side;
  for (size_t i = 0; i < units.size(); ++i) {
    const Unit& o = units[i];
    if ((int)i == unit || o.side != side || !o.alive || o.cell == kNoCell || !o.jobs.on()) continue;
    give((int)i, share, kJobShared);
  }
}

void Battle::awardDefeatJp(int killer, std::vector<Event>& events) {
  if (!jobsGame()) return;
  awardJp(killer, db->rules.jobs.jpPerDefeat, events);
}

}  // namespace ote
