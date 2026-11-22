// PRIVATE members of class Battle for unit resources (docs/SRPG.md "Unit resources (MP and the like)"). Included INSIDE the class body in battle.h
// (private section) and nowhere else. Definitions: core/battle/unit_pools.cpp.

// Gives the unit a record of every pool (at what poolOf says it has), so later changes have something to change.
void notePools(Unit& u) const;
// Every unit on the board gets its record at the start of the battle (the start share of its maximum then).
void poolsBattleStart();
// Pays ability `ability`'s "spend": PoolChanged per unit resource, ResourceChanged per side resource. Checked before (abilityPriceOk).
void payAbilityPrice(int unit, int ability, std::vector<Event>& events);
// The start of the turn of the units that own it now (srpgTurnStart): each pool regenerates by its "regen", never above the maximum.
void poolsTurnStart(std::vector<Event>& events);
