// Learning from equipment (docs/JOBS.md "Learning from equipment (ability points)"): items teach skills and abilities; while equipped the unit may use
// them, and the ability points it earns fill each one until it is mastered. Data here; the loader is core/data/loader_teaching.cpp, the battle side
// core/battle/teaching.cpp. A game without any item "teaches" has none of this and plays, saves and simulates as before.
#pragma once
#include <string>
#include <vector>

namespace ote {

// One thing items teach. The game's list (Database::teachEntries) holds each skill / ability once, in the order items first name them.
struct TeachDef {
  bool ability = false;  // false = a skill, true = a menu ability
  std::string refId;     // the skill / ability id as written
  int ref = -1;          // index into Database::skills / Database::abilities
  int points = 1;        // what it costs to master
  int jobEntry = -1;     // the job learn entry (flat index into Database::jobEntries) with the same id, -1 = no job teaches it
};

// game.json "rules": { "abilityPoints": { ... } }.
struct AbilityPointRules {
  bool declared = false;  // the block was written (refused when no item teaches anything)
  std::string name = "AP";
  int perBattle = 10;  // to every unit of a human side alive on the board when its side wins
  int perAction = 0;   // wherever experience is awarded (as JP)
  int perDefeat = 0;   // for defeating a unit of another side
};

// A unit's points in one entry it has not mastered yet (or, for an entry no job teaches, also once mastered: points == cost).
struct MasteryPoints {
  int entry = 0;   // index into Database::teachEntries
  int points = 0;  // 1..the entry's cost
  bool operator==(const MasteryPoints& o) const { return entry == o.entry && points == o.points; }
};

const int kMaxTeachPoints = 9999;
const int kMaxItemTeaches = 8;

}  // namespace ote
