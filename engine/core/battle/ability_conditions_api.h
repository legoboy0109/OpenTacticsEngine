// PUBLIC members of class Battle for the conditions an ability puts on (contract: docs/SRPG.md "Ability conditions").
// Included INSIDE the class body in battle.h (public section) and nowhere else. Definitions: core/battle/ability_conditions.cpp.

// The percent the ability's "inflict" takes hold on `target` when `user` uses it from `from`: 100 for an ability without "chance", else its
// number or its formula (a = the user, b = the target, rounded down, kept within 0..100). -1 when the ability inflicts nothing. Immunity and
// "has it already" are not part of it (inflictWhy says those).
int abilityInflictChance(int user, int ability, int target, Cell from) const;
// Does the ability roll for its condition at all (an "inflict" with a "chance")?
bool abilityInflictRolls(int ability) const;
