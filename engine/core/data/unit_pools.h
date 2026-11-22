// Unit resources (docs/SRPG.md "Unit resources (MP and the like)"): a pool each unit has for itself, the price abilities pay ("spend"). The loader is
// core/data/loader_unit_pools.cpp, the battle side core/battle/unit_pools.cpp. A game without "unitResources" has none.
#pragma once
#include <string>

namespace ote {

// game.json "unitResources" entry.
struct UnitResourceDef {
  std::string id, name;
  std::string statId;
  int stat = -1;         // index into Database::stats: the pool's maximum
  int start = 100;       // percent of the maximum a unit starts a battle with
  int regenPerTurn = 0;  // added at the start of each of the unit's own turns
  int regenPercent = 0;  // ... and this percent of the maximum
};

// One entry of an ability's "spend": a unit resource (index into Database::unitResources) or a side resource (index into Database::resources).
struct AbilitySpend {
  bool unit = true;
  int index = 0;
  int amount = 0;
};

const int kMaxUnitResources = 4;
const int kMaxSpendAmount = 999;

}  // namespace ote
