// State types of the speed turn order and of charged actions (docs/TURN_ORDER.md). Included by battle.h before the Unit.
#pragma once
#include "core/base/types.h"

namespace ote {

// An action that was declared and goes off when its own counter fills (a unit's "chargeSpeed" action). It sits in the turn order beside
// the units; the caster's turn ended when it declared it.
struct PendingAction {
  int unit = -1;         // the caster
  bool weapon = false;   // false: an ability; true: an attack with a weapon
  int index = -1;        // the ability / weapon (index into Database::abilities / weapons)
  int speed = 0;         // the counter gains this much per tick (copied from the definition when declared)
  int counter = 0;       // starts at 0; the action goes off when it reaches the threshold
  bool follow = false;   // true: it follows the unit `target`; false: it hits the square `cell`, whoever stands there then
  int target = -1;       // follow: the unit it follows
  Cell cell = kNoCell;   // cell: the square (follow: the square the unit stood on when it was aimed at, for the markers)
  int slot = -1;         // a "steal": the foe's item slot that was chosen
  int serial = 0;        // declaration order: a tie between two counters goes to the older one
};

// One entry of the forecast of the order (Battle::turnPreview).
struct TurnEntry {
  int unit = -1;          // the unit whose turn it is, or the caster of the charged action
  bool charged = false;   // a pending action going off (not a turn)
  bool forecast = false;  // the "if declared now" action of chargeForecast, not a real one
  int pending = -1;       // charged: index into Battle::pending (-1 for the forecast one)
  int ticks = 0;          // ticks from now (0 = the unit that has the turn now)
};

}  // namespace ote
