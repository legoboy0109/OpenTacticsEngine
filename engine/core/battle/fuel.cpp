// Fuel (docs/ECONOMY.md "Repair and supply"). Moving spends the movement points of the path; the unit's
// reach is limited by what is left (Battle::reachable, movement.cpp, so the player, the path planner and the AI
// all see the same squares). A type with "fuelPerTurn" also burns that much at the start of its side's turn.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

int Battle::fuel(int unit) const {
  if (unit < 0 || unit >= (int)units.size()) return -1;
  return units[(size_t)unit].fuel;
}

int Battle::maxFuel(int unit) const {
  if (unit < 0 || unit >= (int)units.size()) return -1;
  const UnitTypeDef& t = typeOf(units[(size_t)unit]);
  return t.fuel > 0 ? t.fuel : -1;
}

void Battle::burnFuel(std::vector<Event>& events) {
  std::vector<int> dry;  // units that ran out of fuel and are destroyed
  for (Unit& u : units) {
    if (!u.alive || !ownsTurn(u) || u.fuel < 0) continue;
    int burn = typeOf(u).fuelPerTurn;
    if (burn <= 0) continue;
    u.fuel = std::max(0, u.fuel - burn);
    if (u.fuel == 0) dry.push_back(u.id);
  }
  if (dry.empty()) return;
  // A normal death: UnitDied, then the story's "defeated" events (nobody killed it), then the win checks. As in
  // checkDefeat the story runs after the sweep, because events may add or move units.
  for (int id : dry) killUnit(id, events);
  for (int id : dry) unitDefeated(id, -1, events);
  settle(events);
}

}  // namespace ote
