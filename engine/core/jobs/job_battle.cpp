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
void Battle::awardJp(int unit, int amount, std::vector<Event>& events) {
  if (!jobsGame() || amount <= 0 || unit < 0 || unit >= (int)units.size()) return;
  Unit& u = units[(size_t)unit];
  if (!u.alive || !u.jobs.on()) return;
  int before = 1, after = 1;
  jobAddJp(*db, u.jobs, amount, before, after);
  Event e;
  e.type = EventType::JpGained;
  e.a = unit;
  e.b = amount;
  e.c = u.jobs.job;
  e.d = after;
  if (after > before) e.flags |= kJobLevelUp;
  events.push_back(e);
}

void Battle::awardDefeatJp(int killer, std::vector<Event>& events) {
  if (!jobsGame()) return;
  awardJp(killer, db->rules.jobs.jpPerDefeat, events);
}

}  // namespace ote
