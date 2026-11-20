// PUBLIC members of class Battle for area attacks and line fire (contract: docs/SRPG.md "Area attacks and line fire"). Included INSIDE
// the class body in battle.h (public section) and nowhere else. Definitions: area_aim.cpp, area_forecast.cpp, area_attack.cpp.

// The weapon hits an area (weapons.json "area").
bool isAreaWeapon(int weapon) const { return weapon >= 0 && (size_t)weapon < db->weapons.size() && db->weapons[(size_t)weapon].area.on(); }
// Can a shot of `weapon` fired from `from` be aimed at `to`: inside its range, along a straight line of the grid when the weapon needs one
// ("fire": "line" or a line-shaped area), and, for "fire": "line", with nothing in between (a unit of any side, or a square that blocks
// sight, stops the shot). `ignoreUnit` is a unit that is not in the way (the shooter itself when it would stand somewhere else). For a
// weapon with neither rule this is only the range test.
bool weaponAims(int weapon, Cell from, Cell to, int ignoreUnit = -1) const;
// Every square `weapon` can be aimed at from `from` (see weaponAims), ascending. Nothing is said about what stands there.
void aimCells(int weapon, Cell from, int ignoreUnit, std::vector<Cell>& out) const;
// The square an Attack action aims at: Action::cell when it is set, else the square of the target unit (kNoCell when neither is valid).
Cell aimedCell(const Action& action) const;
// An area attack of `attacker` standing on `from`, aimed at the square `target`: who it hits and what each strike does. False when the
// attack is not possible (not an area weapon, out of range, not aimable, or no enemy the attacker's team can see would be hit).
bool areaForecast(int attacker, int weapon, Cell from, Cell target, AreaForecast& out) const;
