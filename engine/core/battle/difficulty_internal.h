// PRIVATE members of class Battle for difficulties (docs/DIFFICULTY.md, difficulty.cpp). Included INSIDE the class body in
// battle.h (private section) and nowhere else.

// Resolves `difficulty` and the numbers for it from the game's list and the map's own numbers (diffMap).
void resolveDifficulty();
// Is a unit of this side an "enemy" of the difficulty: a computer side with a human side of another team in the battle,
// that the map did not exempt?
bool difficultyEnemySide(int side) const;
// A placement (map or addUnit) that is limited to other difficulties does not appear.
bool appearsOnDifficulty(const MapUnitDef& def) const;
// The level a placed enemy is built at, and its stats after the shift (flat additions, percent), see difficulty.cpp.
int difficultyLevel(const MapUnitDef& def) const;
void difficultyStats(Unit& u, const MapUnitDef& def, int levelsShifted) const;
// A gain of resources for a side: scaled by the difficulty's resourcePercent for a human side (amounts above 0 only).
int resourceGain(int side, int amount) const;
// Experience a unit is given, scaled by xpPercent for a unit of a human side.
int xpGain(int unit, int amount) const;
// The shared "that many level-ups at the average growth" of one stat (a difficulty's level shift, the test level): growth% x
// levels rounded to the nearest (halves away from zero), through the stat caps.
void growStatByLevels(Unit& u, int stat, int levels) const;
// The test level (test_level.cpp): raises the units of human sides to testLevel.
void applyTestLevel();
