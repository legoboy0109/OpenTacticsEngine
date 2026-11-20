// The words and rows of the Jobs screen (docs/JOBS.md "Player UI"), free of any window so a unit test can read them: one list per tab (Jobs, Learn,
// Equip) for one roster character, plus the lines of the character list. The screen (jobs_screen.cpp) only shows them.
#pragma once
#include <string>
#include <vector>

#include "core/battle/carried.h"
#include "core/campaign/playthrough.h"
#include "core/data/database.h"
#include "ui/item_panel.h"

namespace ote {

struct JobRows {
  std::vector<PanelRow> rows;
  std::vector<int> ref;  // per row: the job index (Jobs tab), the flat learn entry (Learn / Equip tabs), -1 for a heading
};

enum class JobsTab { Jobs = 0, Learn = 1, Equip = 2 };
const char* jobsTabName(JobsTab t);

// "Squire  Lv 2  JP 40": the summary line over a character's lists (the job it uses, its job level, the JP left to spend in it).
std::string jobsSummary(const Database& db, const RosterUnit& r);
// The Jobs tab: every job; the current one marked, a locked one greyed with "needs Squire level 2" in its explanation.
JobRows jobsTabRows(const Database& db, const RosterUnit& r);
// The Learn tab: a heading per job (with its level and JP left), then what it teaches: "Guard  30 JP", "Learned", greyed when it cannot be afforded or the job is locked.
JobRows learnTabRows(const Database& db, const RosterUnit& r);
// The Equip tab: a heading per slot kind ("Support 1/2"), then the entries learned in OTHER jobs of that kind; the equipped ones marked.
JobRows equipTabRows(const Database& db, const RosterUnit& r);
// The character list: one row per roster character that uses jobs; `ids` gets their ids in step.
std::vector<PanelRow> jobsWhoRows(const Database& db, const Playthrough& pt, std::vector<std::string>& ids);

}  // namespace ote
