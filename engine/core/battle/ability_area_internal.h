// PRIVATE members of class Battle for abilities with an area (docs/SRPG.md "Areas and several effects for abilities"). Included INSIDE the class
// body in battle.h (private section) and nowhere else. Definitions: core/battle/ability_area.cpp.

// The units an area ability of `u` standing on `from` and aimed at `target` acts on, ascending unit id: in the area, accepted by the ability's
// "target" (ally = the user's team, the user included; enemy = the other teams; unit = everyone) and changed by it (abilityActsOn). `seenOnly`
// keeps only the units u's team sees (what a menu may count). `covered` (optional) gets the squares.
void abilityAreaUnits(const Unit& u, const AbilityDef& d, Cell from, Cell target, bool seenOnly, std::vector<int>& out,
                      std::vector<Cell>* covered = nullptr) const;
// abilityTargetOk for an area ability: a square in range whose area holds at least one unit the user's team sees that it would act on.
bool areaAbilityTargetOk(const Unit& u, int ability, Cell from, Cell cell, std::string* why) const;
// applyAbility for an area ability: AbilityUsed (kArea, data = the units), then for each unit in id order the heal, the cure and the inflicts.
void applyAreaAbility(const Action& action, std::vector<Event>& events, bool resolving);
// What one unit gets from a heal / cure / heal-and-cure ability (the heal first, then the cure); no "inflict" (abilityInflict does those).
void abilityHealCure(const AbilityDef& d, int user, int target, std::vector<Event>& events);
// The squares an area covers when aimed at `target` from `from` (grid/area.h), without those beyond its "vertical" (height rules).
void areaCoverOf(const AreaDef& area, Cell from, Cell target, std::vector<Cell>& out) const;
