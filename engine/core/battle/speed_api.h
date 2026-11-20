// PUBLIC members of class Battle for the speed turn order and charged actions (contract: docs/TURN_ORDER.md). Included INSIDE the class body in
// battle.h (public section) and nowhere else. Definitions: clock.cpp, clock_round.cpp, clock_preview.cpp, charged_actions.cpp. In a game without
// "turnOrder": "speed" every query answers as if the system did not exist, so callers may use them without checking speedGame() first.

// ---- state (saved only in a speed game: save_clock.cpp) ----
int activeUnit = -1;      // the unit whose turn it is (-1 in a game that plays side by side, and between turns while the clock runs)
int clockTick = 0;        // ticks since the round began (a round passes every SpeedRules::roundTicks)
int turnNo = 0;           // unit turns begun so far; the computer opponent tells "still the same turn" by it
int chargeSerial = 0;     // numbers the pending actions in the order they were declared
std::vector<PendingAction> pending;  // charged actions on their way, oldest first

// The game plays with the speed turn order.
bool speedGame() const { return db->rules.speed.on; }
// THE question "whose turn is it" for per-unit rules: this unit's own turn. A side-by-side game: any unit of the active side.
bool ownsTurn(const Unit& u) const { return db->rules.speed.on ? u.id == activeUnit : u.side == activeSide; }
// THE question "may the person (or computer) move this unit now": it is alive, its turn, and it has not finished it (unitSpent).
bool mayActNow(const Unit& u) const { return u.alive && ownsTurn(u) && !over && !unitSpent(u); }
// How much charge the unit gains per tick: its speed stat; 0 when it is off the board or stopped by a condition that stops acting.
int speedOf(const Unit& u) const;
// What the unit's turn would cost its charge now, by how much of it was used (moved, acted).
int ctCostOf(const Unit& u) const;
// The unit has a charged action on its way (index into `pending`, -1 = none).
int pendingOf(int unit) const;
bool charging(int unit) const { return pendingOf(unit) >= 0; }
// An ability / a weapon that has a "chargeSpeed".
bool abilityCharges(int ability) const { return speedGame() && ability >= 0 && (size_t)ability < db->abilities.size() && db->abilities[(size_t)ability].charge.on(); }
bool weaponCharges(int weapon) const { return speedGame() && weapon >= 0 && (size_t)weapon < db->weapons.size() && db->weapons[(size_t)weapon].charge.on(); }
// The next `count` entries of the order, the turn now first: the clock run forward without changing anything, every unit assumed to use its whole
// turn (cost "both"). `extra` (optional) is an action the active unit would declare now: it is run with the rest and marked `forecast`.
void turnPreview(int count, std::vector<TurnEntry>& out, const PendingAction* extra = nullptr) const;
// Where a charged action of `speed` declared now by the active unit would go off in the order: the index into the first `count` entries of
// turnPreview, or -1 when it lands further away. `ticks` (optional) receives how many ticks from now it would take.
int chargeForecast(int speed, int count, int* ticks = nullptr) const;
// The pending action a declaration of this ability / weapon would make (follow or hit the square, by the definition's "chargeTargets"). `target` is the
// unit aimed at (-1 = none), `cell` the square (kNoCell = the target's square).
PendingAction chargeFor(int unit, bool weapon, int index, int target, Cell cell, int slot) const;
// The square a pending action marks on the map: the followed unit's square, else the aimed one (kNoCell when the followed unit is gone).
Cell chargeAimCell(const PendingAction& p) const;
