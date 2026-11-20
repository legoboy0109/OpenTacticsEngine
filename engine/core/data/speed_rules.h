// The speed turn order (docs/TURN_ORDER.md): game.json "rules": { "turnOrder": "speed" } or { "mode": "speed", ... }, and the "chargeSpeed" of an
// ability or a weapon. Rules live in core/battle/clock.cpp and charged_actions.cpp; a game without the block never sees any of it.
#pragma once
#include <string>
#include <vector>

namespace ote {

struct SpeedRules {
  bool on = false;
  int stat = -1;        // "stat": the stat every unit gains per tick of the clock (index into Database::stats)
  int threshold = 100;  // "threshold": the charge at which a unit (or a charged action) acts
  int preview = 8;      // "preview": how many entries of the order the player shows
  int roundTicks = 10;  // "roundTicks": a round passes every this many ticks (default: a tenth of the threshold)
  // "ctCost": what a turn costs the unit's charge, by how much of the turn it used (default 100 / 80 / 60).
  int costBoth = 100;   // it moved and acted
  int costOne = 80;     // only one of the two
  int costNone = 60;    // neither (it just waited)
};

// What an ability or a weapon says about being charged. speed 0 = it is not: it acts at once, as in any game.
struct ChargeDef {
  enum class Aim { Default, Unit, Cell };
  int speed = 0;                      // "chargeSpeed" (1 to 1000): the action's own counter gains this much per tick and it goes off at the threshold
  Aim aim = Aim::Default;             // "chargeTargets": follows the unit that was aimed at, or hits the square that was aimed at
  std::vector<std::string> interruptIds;  // "interruptedBy" as written
  std::vector<int> interrupt;             // the same as indices into Database::conditions: the caster getting one of these cancels the action
  bool on() const { return speed > 0; }
};

}  // namespace ote
