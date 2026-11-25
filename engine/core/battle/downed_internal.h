// PRIVATE members of class Battle for downed units and revive (docs/SRPG.md "Downed units and revive"). Included INSIDE the class body in battle.h
// (private section) and nowhere else. Definitions: core/battle/downed.cpp.

// killUnit, before anything changes: a unit that would be downed and has an "autoRevive" condition stands up at once instead (the condition
// ends with kConditionRevived, then UnitRevived with kAuto). True when it caught the fall.
bool fallCaught(int unit, std::vector<Event>& events);
// Stands the downed unit up with `hp` (UnitRevived). `by` = the reviver (-1 none), `what` = the ability or item used (-1 none), flags as the event's.
// A unit revived in its own turn has finished that turn; in a speed game its charge starts from 0.
void reviveUnit(int unit, int hp, int by, int what, int flags, std::vector<Event>& events);
// Takes a downed unit's body off the board for good (UnitRemoved with kDowned): its square is free, it is an ordinary fallen unit from now on.
void removeDownedBody(int unit, std::vector<Event>& events);
// The start of the active side's turn (a round's start in a speed game): its downed units count one more turn and, at rules.downed.removeAfter,
// their bodies are removed. Nothing without the rule.
void downedTurnStart(std::vector<Event>& events);
// A side that is defeated leaves the field: the bodies of its downed units go too (turn.cpp checkDefeat).
void removeDownedOfSide(int side, std::vector<Event>& events);
