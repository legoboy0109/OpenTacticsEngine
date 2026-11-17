// The small types of map changes (docs/EVENTS.md "Changing the map") that battle.h needs before the class.
#pragma once
#include <vector>

namespace ote {

// A named change of the map that has been applied and not reverted. Applying records what the squares and structures held before,
// so reverting puts exactly that back (even when something else changed them in between).
struct AppliedChange {
  int change = -1;                  // index into the map's changes (MapDef::changes)
  std::vector<int> before;          // the terrain index each of the change's squares held, in the change's order
  std::vector<int> beforeKinds;     // the kind (index into db->structures) each of the change's structures was, in the change's order
};

}  // namespace ote
