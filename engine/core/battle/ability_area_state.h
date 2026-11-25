// What an area ability would do (Battle::abilityAreaForecast; contract: docs/SRPG.md "Areas and several effects for abilities"). Included by
// battle.h INSIDE namespace ote, next to area_state.h, and nowhere else.

// One unit an area ability would act on.
struct AbilityAreaHit {
  int unit = -1;
  bool seen = true;    // the user's team can see it (a unit hidden by fog is affected all the same, but never listed or counted)
  int heal = 0;        // HP it would get back (heal, capped at its maximum); 0 = none
  int revive = 0;      // HP it would stand up with (a revive: docs/SRPG.md "Downed units and revive"); 0 = none
  int cures = 0;       // conditions it would lose (cure); 0 = none
  int inflicts = 0;    // "inflict" entries that could take hold on it (not immune, not had already); each may still roll
};

struct AbilityAreaForecast {
  Cell cell = kNoCell;                // the square aimed at
  std::vector<Cell> cells;            // every square the area covers, ascending
  std::vector<AbilityAreaHit> hits;   // every unit it would act on, in unit-id order (the order it acts in)
};
