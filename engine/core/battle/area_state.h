// What an area attack would do (Battle::areaForecast; contract: docs/SRPG.md "Area attacks and line fire"). Included by battle.h INSIDE
// namespace ote, right after Forecast (which these types contain), and nowhere else.

// One unit an area attack would hit.
struct AreaVictim {
  int unit = -1;
  bool enemy = true;    // false: an ally or a unit of the attacker's own side, hit only because the weapon has "friendlyFire"
  bool seen = true;     // the attacker's team can see it (an enemy hidden by fog is hit all the same, but never listed or counted)
  StrikeForecast hit;   // what one strike on it does (the attacker's numbers against this unit)
};

struct AreaForecast {
  Cell cell = kNoCell;               // the square aimed at
  std::vector<Cell> cells;           // every square the attack covers (grid/area.h), ascending
  std::vector<AreaVictim> victims;   // every unit it would hit, in unit-id order (the order they are struck in)
  int primary = -1;                  // index into `victims` of the unit on the aimed square, -1 = the square is empty or nobody there is hit
  Forecast fight;                    // the primary's whole exchange (the counter-attack included); only meaningful when primary >= 0
};
