// A memory of single-goal distance fields that outlives one ai::Context.
//
// A fresh Context is built for every AI step, so without this the same "how far is each square from capture target
// G" field was recomputed for every acting unit on every step. A field is a pure function of exactly three things:
// the grid's shape, the entry cost of every square for the move type (all distanceField reads from the Battle is
// Grid::neighbors and Battle::enterCost), and the goal. The cache is keyed on those by CONTENT: each entry stores
// the full per-square cost vector it was built for and is only reused when a new vector compares equal. So terrain
// edits, structure upgrades, weather and a different map or Battle all simply miss; nothing needs invalidating by hand.
// Unit positions, fog and doors are not inputs (distanceField ignores units), so they cannot make an entry stale.
#pragma once
#include <map>
#include <memory>
#include <vector>

#include "core/battle/battle.h"

namespace ote {
namespace ai {

// All the fields built for one (grid shape, move type, cost vector). Fields are only ever added, never removed or
// moved, so a reference to one stays valid for as long as the shared_ptr to its set is held.
struct GoalFieldSet {
  GridType gridType = GridType::Square;
  HexLayout layout = HexLayout::Pointy;
  int width = 0, height = 0, moveType = 0;
  std::vector<int> costs;                 // Battle::enterCost of every square for moveType
  std::map<Cell, std::vector<int>> fields;  // goal -> distanceField(goal)
};

// The set matching this Battle's current terrain for `moveType` (found, or created and remembered).
std::shared_ptr<GoalFieldSet> goalFieldSetFor(const Battle& b, int moveType);

// The field for `goal` within `set`, computing it on first use.
const std::vector<int>& goalFieldIn(const Battle& b, GoalFieldSet& set, Cell goal);

}  // namespace ai
}  // namespace ote
