// PUBLIC members of class Battle for abilities with an area (contract: docs/SRPG.md "Areas and several effects for abilities"). Included INSIDE
// the class body in battle.h (public section) and nowhere else. Definitions: core/battle/ability_area.cpp.

// The area ability `ability` of `user`, standing on `from`, aimed at the square `target`: the squares it covers and every unit it would act on.
// False when it cannot be aimed there (not an area ability, out of range) or would act on no unit the user's team can see.
bool abilityAreaForecast(int user, int ability, Cell from, Cell target, AbilityAreaForecast& out) const;
// Does the ability's effect do anything to `t` if `u` used it from `from` (heal: hurt; cure: has something it cures; heal and cure: either;
// only "inflict": at least one entry could take hold)? The test a single target must pass and every unit of an area is put to. `why` says
// why not. Push, pull, swap and the like are not judged here (their target test is about squares).
bool abilityActsOn(const Unit& u, const AbilityDef& d, Cell from, const Unit& t, std::string* why) const;
