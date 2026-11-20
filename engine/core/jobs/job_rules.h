// The rules of the job system over a JobState (docs/JOBS.md): job levels, who may use which job, learning and equipping. All of it works on plain data
// (a Database and a JobState), so the same calls serve a unit in a battle, a roster entry in the camp and the loaders / save readers that check
// state. Every refusing call returns false with a plain sentence in `why` and changes nothing.
#pragma once
#include <string>
#include <vector>

#include "core/data/database.h"

namespace ote {

// ---- levels ----
// The table of total JP for levels 2, 3, ... of a job: its own, else the game's (possibly empty: the job has the one level).
const std::vector<int>& jobLevelTable(const Database& db, int job);
int jobMaxLevel(const Database& db, int job);
// The job level `earned` JP make in `job` (1 = nothing reached yet).
int jobLevelAt(const Database& db, int job, int earned);
int jobLevelOf(const Database& db, const JobState& s, int job);
// Total JP for the level above the one `earned` makes, or -1 at the top level.
int jobNextLevelAt(const Database& db, int job, int earned);
int jobUnspent(const JobState& s, int job);

// ---- who may use what ----
// May a unit in state `s` take `job` now (its requirements are met)? `why` = "needs Squire level 2" when not (the first unmet one).
bool jobUnlocked(const Database& db, const JobState& s, int job, std::string* why = nullptr);
// Index of `s`'s equipped entries judged by slot: how many entries of slot kind `slot` are equipped.
int jobSlotUsed(const Database& db, const JobState& s, JobSlot slot);
// The entries (flat indices) active for a unit in state `s`: the current job's learned ones, then the equipped ones. Empty without a job.
void jobActiveEntries(const Database& db, const JobState& s, std::vector<int>& out);
bool jobHasLearned(const JobState& s, int flat);

// ---- changing the state ----
// Takes the job: only an unlocked one. Equipped entries the new job teaches itself are dropped from the equipped list (they are active anyway).
// This changes the state only; the unit type and stats of whoever it belongs to are changed by the caller (job_camp.h).
bool jobSwitch(const Database& db, JobState& s, int job, std::string& why);
// Spends the JP of the entry's job to learn it (the job must be unlocked and the entry not yet learned).
bool jobLearn(const Database& db, JobState& s, int flat, std::string& why);
bool jobLearnReason(const Database& db, const JobState& s, int flat, std::string& why);  // the same test without changing anything
// Equip a learned entry of another job / take one off.
bool jobEquip(const Database& db, JobState& s, int flat, std::string& why);
bool jobEquipReason(const Database& db, const JobState& s, int flat, std::string& why);
bool jobUnequip(const Database& db, JobState& s, int flat, std::string& why);
// Adds earned JP to the current job (the total stops at kMaxJobTotal). Returns the job level before and after.
void jobAddJp(const Database& db, JobState& s, int amount, int& levelBefore, int& levelAfter);

// ---- checking ----
// "" when the state is consistent with the game (ids in range, learned entries belong to their job, spent within earned, equipped entries learned and
// within the slots), else the first problem in plain words (no path: the caller adds it). Used by the loaders and the save readers.
std::string jobStateProblem(const Database& db, const JobState& s);

// ---- words ----
std::string jobEntryName(const Database& db, int flat);  // the skill's / ability's name
std::string jobEntryKind(const Database& db, int flat);  // "Skill" / "Ability"

}  // namespace ote
