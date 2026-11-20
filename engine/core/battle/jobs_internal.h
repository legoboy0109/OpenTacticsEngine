// PRIVATE members of class Battle for jobs (docs/JOBS.md). Included INSIDE the class body in battle.h (private section) and nowhere else.

// job_battle.cpp: job points for a unit that took part in something that earns experience (awardXp calls it first; nothing in a game without jobs).
void awardJp(int unit, int amount, std::vector<Event>& events);
// job_battle.cpp: the extra JP of defeating a unit (killUnit).
void awardDefeatJp(int killer, std::vector<Event>& events);
