// PUBLIC members of class Battle for jobs (contract: docs/JOBS.md). Included INSIDE the class body in battle.h (public section) and nowhere else.
// Definitions: core/jobs/job_battle.cpp. In a game without data/jobs.json every unit has no job and every answer is "nothing".

// The game has jobs.
bool jobsGame() const { return db->jobsOn(); }
// The unit's job (index into db->jobs), -1 = it does not use the job system (monsters, generic units).
int jobOf(const Unit& u) const { return u.jobs.job; }
// Its job level in the job it uses now (1 = the first; 0 for a unit without a job).
int unitJobLevel(const Unit& u) const;
// The learn entries (indices into db->jobEntries) active for the unit: its job's learned ones, then the equipped ones from other jobs.
void jobEntriesOf(const Unit& u, std::vector<int>& out) const;
// Where a skill or ability the unit has comes from, for the unit details: true when a job (its own or an equipped one) gives it.
bool jobGivesSkill(const Unit& u, int skill) const;
bool jobGivesAbility(const Unit& u, int ability) const;
