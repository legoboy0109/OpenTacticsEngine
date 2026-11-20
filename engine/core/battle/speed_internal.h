// PRIVATE members of class Battle for the speed turn order and charged actions (docs/TURN_ORDER.md). Included INSIDE the class body in battle.h
// (private section) and nowhere else.

// clock.cpp
void speedStart(std::vector<Event>& events);              // before the first turn: round 1 begins, then the clock runs to the first unit
void runClock(std::vector<Event>& events);                // ticks until a unit's turn begins (resolving charged actions on the way)
void beginUnitTurn(int unit, std::vector<Event>& events);  // flags, per-unit start-of-turn effects, UnitTurnBegan
void closeUnitTurn(std::vector<Event>& events);           // the end of the active unit's turn: end effects, the charge it pays; no unit has the turn after it
void endUnitTurn(std::vector<Event>& events);             // closeUnitTurn, then the clock runs on
void speedAfterAction(std::vector<Event>& events);        // after every action: the turn of a finished unit ends by itself
bool inClock(const Unit& u) const { return u.alive && u.cell != kNoCell && !sides[(size_t)u.side].defeated; }
// clock_round.cpp
void beginRound(std::vector<Event>& events);              // the per-side start-of-turn effects, once for every side, and the round's events
void beginSideRound(int side, std::vector<Event>& events);
void tickStoppedConditions(std::vector<Event>& events);   // a stopped unit gets no turn: its conditions count down with the rounds instead
// charged_actions.cpp
bool chargeAllowed(const Action& action, std::string* why) const;  // a charging unit's other orders and a new declaration: false with the reason
void declareCharge(const Action& action, std::vector<Event>& events);
void resolveCharge(int index, std::vector<Event>& events);
void cancelCharge(int index, int reason, std::vector<Event>& events);  // reason: one of kCharge*
void chargeUnitGone(int unit, std::vector<Event>& events);          // the caster was defeated or taken off the map
void chargeCondition(int unit, int condition, std::vector<Event>& events);  // a condition was applied: "interruptedBy"
void dropStaleCharges(std::vector<Event>& events);                  // casters that left the board without a hook
int readyCharge() const;                                            // the full pending action that goes first, -1 = none
