// PUBLIC members of class Battle for doors, chests and visiting (contract: docs/SRPG.md section 10). Included INSIDE the class
// body in battle.h (public section) and nowhere else. Definitions: places.cpp, places_apply.cpp, places_unlock.cpp.

// The structure with this map id (a map structure's "id"), -1 = none.
int placeByStoryId(const std::string& id) const;
// The structure's current kind's place block (kind None for an ordinary structure).
const PlaceDef& placeOf(int structure) const { return defOf(structures[(size_t)structure]).place; }
// A door that is shut / a chest that is shut (the structure's present kind is a door / chest kind).
bool isClosedDoor(int structure) const;
bool isClosedChest(int structure) const;
// Is `from` a square the kind lets a unit interact from: on one of its squares, or next to one ("from" of the kind)?
bool placeInReach(int structure, Cell from) const;
// Steps from `from` to the nearest square of the structure (0 = on it).
int placeDistance(int structure, Cell from) const;
// Every door, chest and place the unit could act on if it stood on `from`, with whether it can now and the plain reason if not.
// Menu mode lists them; unlock abilities and staves are in the abilities / staves lists, a door's attack in placeAttackOptions.
void placeOptions(int unit, Cell from, std::vector<PlaceOption>& out) const;
// The inventory slots of the unit's items that open this structure (door or chest), in inventory order.
void placeKeySlots(int unit, int structure, std::vector<int>& out) const;
// May this unit visit the place (and, if not, destroy it)? The rule: the placement's "visitSides" / "visitUnits" when it has any;
// otherwise a place with an owner is visited by the owner's team and destroyed by the others; a neutral place is anybody's.
bool visitAllowed(const Unit& u, int structure) const;
bool destroyAllowed(const Unit& u, int structure) const;
// A chest's items need room: the unit's inventory, or the side's storage. Without either the chest cannot be opened (drop something).
bool roomForLoot(const Unit& u, int structure) const;
// A shut door that can be attacked (a "break" way, hit points left).
bool doorBreakable(int structure) const;
// The doors the unit could attack with a weapon from `from` (breakable, seen, in weapon range, the weapon would do damage).
void placeAttackOptions(int unit, Cell from, std::vector<PlaceAttack>& out) const;
// What the attack would do (no randomness). False when it is not legal.
bool placeForecast(int unit, int structure, int weapon, Cell from, PlaceForecast& out) const;
// An unlock staff's squares: every square of a door / chest of a kind it names that is within its range of `from`. Ascending.
void staffUnlockCells(int unit, int slot, Cell from, std::vector<Cell>& out) const;
