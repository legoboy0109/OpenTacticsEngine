// PUBLIC members of class Battle for map weapons, a ballista's weapon that a unit standing on the structure attacks with (contract:
// docs/SRPG.md "Map weapons"). Included INSIDE the class body in battle.h (public section) and nowhere else. Definitions: mapweapon.cpp.

// The weapon (index into db->weapons) the structure's present kind offers, -1 = none.
int mapWeaponOf(int structure) const;
// Its uses left: -1 = unlimited, 0 = spent, -2 = the structure's present kind has no weapon. Valid per kind: a structure that
// changed kind starts the new kind's weapon over (StructureState::weaponFor).
int mapWeaponUses(int structure) const;
// The structure under `cell` whose weapon `u` could fire if it stood there; -1 when there is none or the unit may not use it. `why`
// gets the plain reason when the square has a weapon the unit cannot use ("Only archers can use this", "spent", a rank).
int mapWeaponStructure(const Unit& u, Cell cell, std::string* why = nullptr) const;
// Every weapon the unit may attack with from `from`: weaponsOf plus the weapon of the structure there. A weapon the unit carries
// anyway is listed once and counts as the carried one. `mapStructure` (optional) gets the structure whose weapon was added, else -1.
void attackWeaponsAt(const Unit& u, Cell from, std::vector<int>& out, int* mapStructure = nullptr) const;
// The structure whose weapon `u` fires when it attacks with `weapon` standing on `from`; -1 for a carried or fixed weapon (or none).
int mapWeaponUsedBy(const Unit& u, Cell from, int weapon) const;
// A strike made with the structure's weapon: one use gone; at 0 it is spent, and a kind with "becomes" turns into that kind.
void spendMapWeapon(int structure, std::vector<Event>& events);
