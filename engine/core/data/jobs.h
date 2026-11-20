// Jobs (docs/JOBS.md): data/jobs.json and game.json "rules": { "jobs": { ... } }. A character may change between jobs, earns job points (JP) in
// the job it is using, spends them to learn that job's skills and abilities, and equips a few learned ones from other jobs. A game without
// data/jobs.json has none of this and plays, saves and simulates as before. The rules over this data live in core/jobs/ (job_rules.cpp,
// job_camp.cpp), the loader in core/data/loader_jobs.cpp.
#pragma once
#include <string>
#include <vector>

namespace ote {

// The four kinds of slot a learned entry belongs to. Each kind has a number of places for entries learned in OTHER jobs
// (rules.jobs.slots); the current job's own learned entries are always active.
enum class JobSlot { Action = 0, Reaction = 1, Support = 2, Movement = 3 };
const int kJobSlotKinds = 4;
inline const char* jobSlotName(JobSlot s) {
  static const char* const names[] = {"action", "reaction", "support", "movement"};
  return names[(int)s];
}

// One thing a job teaches. `flat` is its position over all jobs in file order: saves and equipped lists use it.
struct JobLearnDef {
  bool ability = false;  // false = a skill, true = a menu ability
  std::string refId;     // the skill / ability id as written (resolved to `ref` at the end of loading)
  int ref = -1;          // index into Database::skills / Database::abilities
  int cost = 0;          // JP
  JobSlot slot = JobSlot::Action;
  int job = -1;          // the job that teaches it
  int flat = -1;
};

struct JobRequire {
  std::string jobId;
  int job = -1;
  int level = 1;  // the job level needed in that job
};

struct JobDef {
  std::string id, name, description;
  std::string unitTypeId;
  int unitType = -1;  // index into Database::unitTypes: what a unit plays as while it has this job; -1 = an OVERLAY job (no "unitType": the unit keeps its own type)
  std::vector<int> bonus;  // "bonus": per stat index (sized to the stat list) added in unitStat while this is the unit's current job; empty = none
  bool overlay() const { return unitType < 0; }
  std::vector<JobRequire> needs;  // the jobs.json "requires" list (a C++20 keyword, so not the member name)
  std::vector<int> jpLevels;  // total JP for job level 2, 3, ...; empty = the game's table
  std::vector<JobLearnDef> learn;
  int firstFlat = 0;  // `flat` of learn[0]
};

struct JobRules {
  bool declared = false;  // game.json wrote a "rules.jobs" block (refused without data/jobs.json)
  int jpPerAction = 10;  // JP earned wherever the unit is given experience (a fight it took part in, a staff it used)
  int jpPerDefeat = 0;   // extra JP for defeating a unit
  int slots[kJobSlotKinds] = {1, 1, 1, 1};
  std::vector<int> jpLevels;  // the game-wide table of total JP for job level 2, 3, ... (a job's own table replaces it)
  bool changeInPrep = false;  // "changeIn": "camp+prep": the preparation screen changes jobs too
};

// What a unit knows of the jobs. job < 0: it does not use the system (monsters, generic units).
struct JobProgress {
  int job = 0;
  int earned = 0;  // total JP earned in this job (the job level comes from it)
  int spent = 0;   // JP spent on learning; earned - spent is what is left to spend
  std::vector<int> learned;  // flat learn indices, ascending
};
struct JobState {
  int job = -1;                      // the job it uses now (index into Database::jobs)
  // The unit's OWN unit type (its character's / map entry's class, as promotion moved it): what an overlay job plays as. Recorded only in a game that has
  // an overlay job (Database::jobsOverlay()); -1 elsewhere, so the saves of every other game are unchanged. A class job plays as its own type instead.
  int baseType = -1;
  std::vector<JobProgress> progress;  // ascending job index; only jobs with something in them
  std::vector<int> equipped;          // flat learn indices learned in other jobs and equipped, in the order they were equipped
  bool on() const { return job >= 0; }
  const JobProgress* find(int j) const {
    for (const JobProgress& p : progress)
      if (p.job == j) return &p;
    return nullptr;
  }
  // The entry for job `j`, created (in order) when it is missing.
  JobProgress& at(int j) {
    size_t i = 0;
    for (; i < progress.size(); ++i) {
      if (progress[i].job == j) return progress[i];
      if (progress[i].job > j) break;
    }
    JobProgress p;
    p.job = j;
    return *progress.insert(progress.begin() + (long)i, p);
  }
  bool operator==(const JobState& o) const {
    if (job != o.job || baseType != o.baseType || equipped != o.equipped || progress.size() != o.progress.size()) return false;
    for (size_t i = 0; i < progress.size(); ++i) {
      const JobProgress &a = progress[i], &b = o.progress[i];
      if (a.job != b.job || a.earned != b.earned || a.spent != b.spent || a.learned != b.learned) return false;
    }
    return true;
  }
};

const int kMaxJobs = 40;
const int kMaxJobLearn = 40;
const int kMaxJobJp = 99999;  // one cost, one table step, one JP total
const int kMaxJobLevel = 20;
const int kMaxJobTotal = 9999999;  // total JP earned in one job

}  // namespace ote
