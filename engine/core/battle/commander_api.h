// PUBLIC members of class Battle for commanders (contract: docs/COMMANDERS.md). Included INSIDE the class body in battle.h (public section) and
// nowhere else. Definitions: commander.cpp, commander_power.cpp. In a game without data/commanders.json (or for a side without a commander)
// every query answers as if the feature did not exist.

// The side's commander, or null.
const CommanderDef* commanderOf(int side) const;
// The side's power meter (0 without a commander) and its top.
int meterOf(int side) const;
int meterMax(int side) const;
// The power now active for the side (index into its commander's "powers"), -1 = none; and how many of the side's own turn starts (rounds in a
// speed game) it still has to see: it ends when that reaches 0, so 1 = "until the side's next turn starts".
int activePower(int side) const;
int powerTurnsLeft(int side) const;
// What using power `power` costs the side now: the base cost, plus costGrowthPercent of it (whole points) for every earlier use, never more than
// the meter's top. How often the side used it.
int powerCost(int side, int power) const;
int powerUses(int side, int power) const;
// May the side use the power now? Only on its turn (a speed game: while the active unit is its own), with no power active, and with the meter at
// the cost. `why` (optional) gets the plain reason when not; "needs N more" for a meter that is short.
bool canUsePower(int side, int power, std::string* why = nullptr) const;
// How many meter points the side lacks for the power (0 = it can pay).
int powerShortBy(int side, int power) const;
// The commander a "choose" side takes when nobody picks: a computer side by the seed, a person's side the first allowed one. `faction` (index or
// -1) limits it to that faction's "commanders" list. -1 when the game has no commanders.
static int defaultCommanderChoice(const Database& db, uint32_t seed, int side, bool ai, int faction);
