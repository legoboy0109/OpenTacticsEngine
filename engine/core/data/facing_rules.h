// Facing (docs/SRPG.md "Facing"): game.json "rules": { "facing": ... }. When the block is present every unit has a facing (a direction of the grid),
// it turns with the unit's last step, toward its target when it attacks and by choice when it waits, and every attack has an arc (front, side or
// back) against the defender's facing that formulas read as `arc` and skills test with { "arc": "back" }. A game without the block is unchanged.
// Rules live in core/battle/facing_rules.cpp; the grid maths in core/grid/facing.h; the loader in core/data/loader_facing.cpp.
#pragma once
#include <string>

namespace ote {

struct FacingRules {
  bool on = false;
  // "arcs": { "front": { "name": "Front" }, "side": { ... }, "back": { ... } } (all optional): the words the forecast uses for an arc.
  std::string arcName[3] = {"Front", "Side", "Back"};
};

}  // namespace ote
