// Guests (guests_api.h; docs/SRPG.md "Guests"): units on a side a person plays that the computer moves, that do not keep their side in the battle
// and never join the party unless a story "guestJoins" makes them. Every rule here is behind Unit::guest, which only a map or addUnit with
// "guest": true sets, so a game without guests plays exactly as before.
#include "core/battle/battle.h"

namespace ote {

int Battle::guestToAct() const {
  if (over || activeSide < 0 || activeSide >= (int)sides.size() || sides[(size_t)activeSide].ai) return -1;
  if (db->rules.speed.on) {  // one unit at a time: the guest whose turn it is
    if (activeUnit < 0 || activeUnit >= (int)units.size()) return -1;
    const Unit& u = units[(size_t)activeUnit];
    return u.guest && u.alive && u.cell != kNoCell ? activeUnit : -1;
  }
  for (const Unit& u : units)
    if (u.guest && u.alive && u.cell != kNoCell && u.side == activeSide && !u.done) return u.id;  // (a carried guest is off the board: it waits)
  return -1;
}

void Battle::cmdGuestJoins(const Command& c) {
  for (Unit& u : units) {
    if (!u.guest || !unitMatches(c.filter, u)) continue;
    // From now on a unit of its side like any other: the person moves it, it keeps its side in the battle and the party takes it in at the end.
    u.guest = false;
    if (u.alive) sides[(size_t)u.side].everHadUnits = true;
  }
}

}  // namespace ote
