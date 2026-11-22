// The rules of learning from equipment over plain data (docs/JOBS.md "Learning from equipment (ability points)"): a unit's points in what items teach
// (a list of MasteryPoints) and, for an entry a job teaches too, its JobState. The same calls serve a unit in a battle, a roster entry and the save readers.
#pragma once
#include <string>
#include <vector>

#include "core/data/database.h"

namespace ote {

// The points the unit has in taught entry `entry` (0 = none).
int teachPoints(const std::vector<MasteryPoints>& m, int entry);
// Is taught entry `entry` mastered: for an entry a job teaches too, learned in that job; otherwise its points have reached the cost.
bool teachMastered(const Database& db, const JobState& s, const std::vector<MasteryPoints>& m, int entry);
// Adds points to an entry that is not mastered. When they reach the cost the entry is mastered: an entry a job teaches is then learned in that job (no JP
// spent) and its points record goes; any other keeps points == cost. Returns the points it has now (the cost once mastered); `mastered` = it became
// mastered with these points.
int addTeachPoints(const Database& db, JobState& s, std::vector<MasteryPoints>& m, int entry, int amount, bool& mastered);
// "" when the list fits the game (entries in range, ascending, each once, points 1..cost, no record for an entry already learned in its job), else the
// first problem in plain words (the save readers add the path).
std::string masteryProblem(const Database& db, const JobState& s, const std::vector<MasteryPoints>& m);
// The skill's / ability's name of a taught entry.
std::string teachEntryName(const Database& db, int entry);

}  // namespace ote
