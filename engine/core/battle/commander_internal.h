// PRIVATE members of class Battle for commanders (docs/COMMANDERS.md). Included INSIDE the class body in battle.h (private section) and nowhere else.

// commander.cpp. Battle::init: the side takes commander `c` (index, -1 none): its passive skills go in and the meter starts at 0.
void takeCommander(int side, int c);
// Start of the active side's turn (a round in a speed game; sideTurnEffects): counts the active power down and ends it at 0, then adds "perTurn".
void commanderTurnStart(std::vector<Event>& events);
// After one strike (combat.cpp): `damage` hit points were dealt by a unit of `attackerSide` to a unit of `defenderSide`; both meters fill (never
// while that side's power is active).
void commanderStrike(int attackerSide, int defenderSide, int damage);
// Adds `points` to the side's meter (cut at the top) unless its power is active.
void fillMeter(int side, long long points);
// The side's power ends: its timed skills go, PowerEnded.
void endPower(int side, std::vector<Event>& events);
// commander_power.cpp: pays, switches the power on, applies the instant effects.
void applyUsePower(const Action& action, std::vector<Event>& events);
void powerEffects(int side, const CommanderPower& p, std::vector<Event>& events);
