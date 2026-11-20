// Named units carried from chapter to chapter by a campaign (docs/EVENTS.md "Campaign").
// The roster is data the campaign module owns; a Battle only gets a read-only snapshot of it
// (Battle::setStartingRoster) so it can dress the named units of human sides, both those the
// map places and those that addUnit events create later.
#pragma once
#include <string>
#include <vector>

#include "core/data/database.h"

namespace ote {

struct RosterUnit {
  std::string id;  // the story id (letters, digits, _ and -)
  int type = 0;    // index into Database::unitTypes
  std::string name;
  int level = 1, xp = 0;
  int merit = 0;                 // the rank counter of "rules.ranks" (Unit::merit); 0 in every other game
  std::vector<int> stats;        // per stat index (the unit's own values, without accessories)
  std::vector<ItemGrant> items;  // the inventory with the uses left
  std::string side;              // id of the side it fought on last (informational)
  bool gone = false;             // fell in an earlier chapter ("fallen": "gone"): never placed again
  std::vector<int> skills;       // skills learned for good (Unit::learned; docs/SRPG.md), empty in a game without skills
  JobState jobs;                 // the job it uses and its progress (docs/JOBS.md); `jobs.job` < 0 = none, always so in a game without jobs
  std::vector<int> weaponXp;     // weapon experience per RankRules::groups index (Unit::weaponXp); empty in a game without ranks
};

using Roster = std::vector<RosterUnit>;

}  // namespace ote
