// PUBLIC members of class Battle for the action-point turn system (contract: docs/SRPG.md "Action points"). Included INSIDE the class body in
// battle.h (public section) and nowhere else. Definitions: ap_rules.cpp. In a game without "rules.actionPoints" every query answers as if the
// system did not exist (nothing to pay, nothing limited), so callers may use them without checking apGame() first.

// The game plays with a shared pool of action points.
bool apGame() const { return db->rules.ap.on; }
// The side's pool (0 in a game without action points).
int apPool(int side) const;
// What walking `points` more movement points costs the unit, counting the movement points it walked already this turn (`spentBefore`, -1 = the
// unit's own count). Every point costs the unit type's "step" (else the game's costPerStep); the points beyond the soft cap cost that plus the
// penalty percent, rounded up to a whole AP per point.
int apMoveCost(const Unit& u, int points, int spentBefore = -1) const;
// What one attack of `unit` with `weapon` costs: the weapon's "apCost" or the game's attack cost, plus an area weapon's "apExtra".
int apAttackCost(int unit, int weapon) const;
// Attacks the unit may still make this turn (the game's attacksPerTurn minus the ones made).
int apAttacksLeft(const Unit& u) const;
// The movement points the unit may still walk this turn: its Move stat minus what it walked, cut to what the side's pool pays for, and 0 when
// the game allows one move per turn and it made one.
int apMovePointsLeft(const Unit& u) const;
// May the unit still walk at least a point / still make an attack with some weapon of its own (attacks left, the pool pays the cheapest, the
// "afterMove" rule allows it)?
bool apMayMove(const Unit& u) const;
bool apMayAttack(const Unit& u) const;
// THE question "is this unit finished for the turn": it is `done`, or (action points) it can neither move nor attack any more. A game without
// action points: exactly `done`. The player dims, skips and counts units by this, never by `done` alone.
bool unitSpent(const Unit& u) const;
