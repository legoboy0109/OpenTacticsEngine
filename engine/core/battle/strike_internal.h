// PRIVATE members of class Battle for abilities that strike (docs/SRPG.md "Abilities that strike"). Included INSIDE the class body in battle.h (private
// section) and nowhere else. Definitions: core/battle/ability_strike.cpp.

// While a strike ability's attack is worked out or made: the weapon it strikes with (-1 = an ordinary attack). The attack pipeline asks it in a few
// places: attackWeaponsAt offers only this weapon, forecastFight turns the counter-attack off and ignores the weapon's "afterMove" (the ability's own
// counts), doStrike neither looks for the weapon in the inventory nor wears it, applyAttack / applyAreaAttack spend no ammunition, give no weapon rank
// and leave the end of the order to applyAbility. Set only through StrikeScope (strike_api.h); never saved (one action). Mutable: the forecasts are const.
mutable int strikeWeapon_ = -1;
// The range in force for strikeWeapon_ while a strike with "reach": "equipped" is worked out: the user's weapon in hand (-1 = the weapon's own range).
// Set only through StrikeScope, never saved.
mutable int strikeReachMin_ = -1, strikeReachMax_ = -1;
// The range a weapon reaches with now: its own, or for the strike weapon of a "reach": "equipped" strike, that of the user's weapon in hand. Every
// range test of the attack pipeline (weaponInRange, weaponReaches, aimCells, the height bonus) asks these.
int weaponMinRange(int weapon) const { return weapon == strikeWeapon_ && strikeReachMax_ >= 0 ? strikeReachMin_ : db->weapons[(size_t)weapon].minRange; }
int weaponMaxRange(int weapon) const { return weapon == strikeWeapon_ && strikeReachMax_ >= 0 ? strikeReachMax_ : db->weapons[(size_t)weapon].maxRange; }
// abilityTargetOk for a strike ability: a single-target weapon needs an enemy it may fight in reach; an area weapon a square it can aim at that would
// hit an enemy its team sees.
bool strikeTargetOk(const Unit& u, int ability, Cell from, int target, Cell cell, std::string* why) const;
// applyAbility for a strike ability: AbilityUsed, the attack, the ability's "inflict" on every unit it hit that still stands, its "xp", the cost.
void applyStrikeAbility(const Action& action, std::vector<Event>& events, bool resolving);
// How much a "heal" ability gives `target` (before the cap at its maximum): the number, "full", a percent of its maximum or the formula.
int abilityHealAmount(const AbilityDef& d, int user, int target) const;
