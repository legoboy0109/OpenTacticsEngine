// PUBLIC members of class Battle for unit resources (contract: docs/SRPG.md "Unit resources (MP and the like)"). Included INSIDE the class body in
// battle.h (public section) and nowhere else. Definitions: core/battle/unit_pools.cpp. A game without "unitResources" has no pools.

// The pool's maximum for the unit: its stat as the rules see it (bonuses included), never below 0.
int poolMax(const Unit& u, int pool) const;
// What the unit has of pool `pool`: its record, or, before it has one (it has not spent or regenerated yet), the start share of the maximum; never above
// the maximum.
int poolOf(const Unit& u, int pool) const;
// Can the unit (and its side) pay ability `ability`'s "spend"? `why` = "needs 6 MP" / "not enough Gold" when not. True for an ability without a price.
bool abilityPriceOk(const Unit& u, int ability, std::string* why = nullptr) const;
