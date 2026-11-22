// PRIVATE members of class Battle for learning from equipment (docs/JOBS.md "Learning from equipment (ability points)"). Included INSIDE the class body
// in battle.h (private section) and nowhere else. Definitions: core/battle/teaching.cpp.

// Points for the unit's equipped items' entries (a unit of a human side; nothing in a game where no item teaches). One PointsGained per entry.
void awardPoints(int unit, int amount, std::vector<Event>& events);
// The battle is won by `team`: rules.abilityPoints.perBattle to each of its human sides' units alive on the board (objectives.cpp).
void awardBattlePoints(int team, std::vector<Event>& events);
