// Game-wide settings of the computer opponent (game.json "rules.ai"; docs/AI.md "Scripted-style computer players"): the order its
// units act in, and the named attack-weight sets a unit's behaviour can pick. Absent = everything as it always was.
#pragma once
#include <string>
#include <utility>
#include <vector>

namespace ote {

// One named set of weights for choosing an attack (docs/AI.md "Attack weights"). Defaults here are the documented ones.
struct AiWeightSet {
  std::string name;
  // Weights, whole numbers 0..100.
  int dmg = 1, left = 0, friends = 0, cls = 0, turn = 0, counter = 0, danger = 0, wounds = 0;
  // Bases and caps.
  int killValue = 50, dmgCap = 40, leftBase = 20, friendsCap = 10, counterCap = 40, noCounterBonus = 10, dangerDiv = 8, dangerCap = 20;
  bool attackAtZero = false;                  // attack even when the best score is 0
  std::vector<std::pair<int, int>> priority;  // (unit type index, priority) for the `class` weight; a type not listed counts 0
  int priorityOf(int unitType) const {
    for (const std::pair<int, int>& p : priority)
      if (p.first == unitType) return p.second;
    return 0;
  }
};

struct AiRules {
  bool orderMode = false;  // "turnOrder": "order" (false = "best", the default)
  std::vector<AiWeightSet> weights;
  int findWeights(const std::string& name) const {
    for (size_t i = 0; i < weights.size(); ++i)
      if (weights[i].name == name) return (int)i;
    return -1;
  }
};

}  // namespace ote
