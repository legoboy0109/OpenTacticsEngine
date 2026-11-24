// PUBLIC members of class Battle for abilities that strike (contract: docs/SRPG.md "Abilities that strike (formula damage, areas, damage types)").
// Included INSIDE the class body in battle.h (public section) and nowhere else. Definitions: core/battle/ability_strike.cpp.
//
// An ability with "effect": { "strike": "<weapon>" } attacks with that weapons.json entry through the normal attack pipeline (damage and hit formulas,
// criticals, damage types, "versus", areas, line fire, the weapon's skills), as if the unit held it, except: nothing answers it (no counter-attack),
// it wears nothing, spends no ammunition and earns no weapon rank. The unit does not need to carry or be able to wield the weapon.

// While one lives, the attack questions (attackOptions, forecast, areaForecast) are asked for strike ability weapon `weapon` instead of the unit's
// own attacks (strikeWeapon_ in strike_internal.h). The API calls below use it; the AI uses it to score a strike with its attack scoring.
// The (unit, ability) form also sets the reach of a "reach": "equipped" strike (strikeReachOf); ability < 0 = no strike (weapon -1).
struct StrikeScope {
  const Battle& b;
  int prev, prevMin, prevMax;
  StrikeScope(const Battle& battle, int weapon)
      : b(battle), prev(battle.strikeWeapon_), prevMin(battle.strikeReachMin_), prevMax(battle.strikeReachMax_) {
    b.strikeWeapon_ = weapon;
    b.strikeReachMin_ = b.strikeReachMax_ = -1;
  }
  StrikeScope(const Battle& battle, int unit, int ability) : StrikeScope(battle, battle.abilityStrikeWeapon(ability)) {
    b.strikeReachOf(unit, ability, b.strikeReachMin_, b.strikeReachMax_);
  }
  ~StrikeScope() {
    b.strikeWeapon_ = prev;
    b.strikeReachMin_ = prevMin;
    b.strikeReachMax_ = prevMax;
  }
  StrikeScope(const StrikeScope&) = delete;
  StrikeScope& operator=(const StrikeScope&) = delete;
};
// The weapon a strike ability attacks with (-1 = the ability does not strike).
int abilityStrikeWeapon(int ability) const;
// A strike with "reach": "equipped" (docs/SRPG.md "Abilities that strike"): the min and max range of `unit`'s weapon in hand. Both -1 when the strike
// keeps its weapon's own range (no such option, no unit, or nothing in hand).
void strikeReachOf(int unit, int ability, int& minRange, int& maxRange) const;
// Where the unit could strike with the ability from `from`: the same options attackOptions gives for its weapon (an area weapon: squares to aim at;
// any other: enemy units it can see in reach). Empty for an ability that does not strike.
void abilityStrikeOptions(int unit, int ability, Cell from, std::vector<AttackOption>& out) const;
// The forecast of a strike on one unit (no counter side). False when it is not possible.
bool abilityStrikeForecast(int unit, int ability, int target, Cell from, Forecast& out) const;
// The forecast of a strike with an area weapon aimed at `target`.
bool abilityStrikeArea(int unit, int ability, Cell from, Cell target, AreaForecast& out) const;
