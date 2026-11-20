// Where a unit can move and what it can hit from there: the squares the player's own selection paints (blue / red), and, for a
// hostile unit, the "danger zone" display (docs/SRPG.md "Enemy ranges"). One implementation of the reach and range rules so the
// two cannot drift: Battle::reachable / reachFor for the walk, Battle::attackWeaponsAt for the weapons (a structure's map weapon
// included), the weapon's own min / max range and its "afterMove" flag.
#pragma once
#include <cstdint>
#include <vector>

#include "core/battle/battle.h"

namespace ote {

// Per-cell masks (1 = yes), one entry per grid cell.
struct ThreatArea {
  std::vector<uint8_t> move;    // squares the unit can end a move on (its own included)
  std::vector<uint8_t> attack;  // squares one of its weapons reaches from one of those squares (move squares included)
  int moveCount = 0;            // set bits of `move`
  int attackOnlyCount = 0;      // set bits of `attack` that are not in `move`
};

// Marks in `attack` every square weapon `weapon` of `unit` reaches when fired from `s` (squares aimed at and, for an area weapon, covered). The
// player's own highlight and the enemy-range display both call it. `attack` must already have one entry per cell.
void markWeaponReach(const Battle& b, int unit, int weapon, Cell s, std::vector<uint8_t>& attack);

// Marks in `attack` every square the unit's weapons reach when it ends its move on one of `stops` (weapons without "afterMove" only
// from its own square). `attack` is resized and cleared first. Does not look at the unit's turn flags.
void attackArea(const Battle& b, const Unit& u, const std::vector<Cell>& stops, std::vector<uint8_t>& attack);

// The reach of `unit` on a fresh turn as the team `viewTeam` can know it (Battle::reachFor): nothing the team cannot see changes
// the result. A unit stopped by a condition that blocks acting has no attack squares. `scratch` is reused between calls.
void threatArea(const Battle& b, int unit, int viewTeam, Reach& scratch, ThreatArea& out);

}  // namespace ote
