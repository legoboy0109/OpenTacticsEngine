// PRIVATE members of class Battle for the action-point turn system and resource refills (docs/SRPG.md "Action points", docs/ECONOMY.md
// "Refilling resources"). Included INSIDE the class body in battle.h (private section) and nowhere else.

// refill.cpp: at the start of the active side's turn each resource with a "refill" becomes min(what is left, carryOver) + perTurn.
void refillTurnStart(std::vector<Event>& events);
// ap_rules.cpp
// The movement points a unit may search with: `budget` (its Move stat, as reachFor has it) less what it walked this turn, cut to what the pool
// pays for when `limitByPool`. Only reachFor calls it, only in an action-point game.
int apMoveBudget(const Unit& u, int budget, bool limitByPool) const;
// The refusal of an Attack action (no attacks left, not enough AP); true = fine.
bool apAttackAllowed(const Unit& u, int weapon, std::string* why) const;
// The reason a Move to `cell` is refused when it is not among the unit's stops ("not enough AP" when only the pool is short).
const char* apMoveRefusal(int unit, Cell cell) const;
// Pays for an Attack / a Move about to be made (a ResourceChanged event, before the action's own events) and counts the attack.
void apPayAttack(const Action& action, std::vector<Event>& events);
void apPayMove(int unit, int points, std::vector<Event>& events);
// After an attack: the unit is done once it has no attack and no movement left; otherwise it stays ready.
void apAfterAttack(Unit& u);
