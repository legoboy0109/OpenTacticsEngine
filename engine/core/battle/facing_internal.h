// PRIVATE members of class Battle for facing (docs/SRPG.md "Facing"). Included INSIDE the class body in battle.h (private section) and nowhere else.

// facing_rules.cpp: turns the unit to look in direction `dir` (a UnitFaced event when that changes anything; nothing in a game without facing).
void faceUnit(int unit, int dir, std::vector<Event>& events);
// Turns the unit toward the square `target` (the attacker toward its target, the aimed square of an area attack).
void faceToward(int unit, Cell target, std::vector<Event>& events);
