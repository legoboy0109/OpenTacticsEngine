// Downed units and revive (contract: docs/SRPG.md "Downed units and revive"): game.json "rules": { "downed": { ... } }. With the block a unit
// brought to 0 HP stays on its square, downed: it takes no turns, nobody can stand on its square, only a revive acts on it, and a revive (an
// ability, a usable item, a staff, or a condition's "autoRevive") stands it up again. A game without the block is unchanged.
// Rules live in core/battle/downed.cpp; the loader in core/data/loader_downed.cpp.
#pragma once

namespace ote {

struct DownedRules {
  bool on = false;
  // "afterBattle": "return" (the default: a unit still downed when the battle ends is back for the next one, as a survivor) or "fallen" (it
  // counts as fallen and the campaign's own "fallen" rule decides). A unit whose body was removed (removeAfter, its side defeated) is always fallen.
  bool fallenAfterBattle = false;
  // "removeAfter": the body is taken off the board when its side's turn has started this many times while it lay downed (1..99); 0 = never.
  int removeAfter = 0;
};

// How many hit points a revive gives (an ability keeps its own heal fields): a number, "full" (kHealFull), { "percent": n } or, where a reviver
// exists, a formula (a = the reviver, b = the unit revived). The result is at least 1 and at most the unit's maximum.
struct ReviveAmount {
  int fixed = 0;    // HP; kHealFull = all of it
  int percent = 0;  // 1..100 of the unit's maximum
  int expr = -1;    // index into Database::exprs
  bool on() const { return fixed > 0 || percent > 0 || expr >= 0; }
};

}  // namespace ote
