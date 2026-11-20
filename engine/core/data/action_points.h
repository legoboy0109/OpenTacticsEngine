// The action-point turn system (docs/SRPG.md "Action points"): game.json "rules": { "actionPoints": ... }. When the block is present a side
// spends from one shared pool (a resource of the side) instead of each unit getting one move and one action. Rules live in
// core/battle/ap_rules.cpp; a game without the block never sees any of it.
#pragma once

namespace ote {

struct ActionPointRules {
  bool on = false;
  int resource = -1;              // the pool: an index into Database::resources (the side's amount of it)
  // "move": every MOVEMENT POINT a unit walks (a plain square 1, woods 2...) costs `stepCost`; the unit's Move stat is the most it walks per turn.
  int stepCost = 1;               // "costPerStep" (0 to 99)
  bool splitMoves = true;         // "splitMoves": several moves in a turn; false = one move, as in a game without action points
  int softCap = -1;               // "softCap": movement points a unit walks at the plain price each turn; -1 = no cap
  int softCapPercent = 100;       // "softCapPenaltyPercent" (0 to 1000): the price of every point beyond the cap rises by this much (100 = double)
  // "attack": what one attack costs (a weapon's "apCost" replaces it, an area weapon adds its "apExtra") and how many one unit makes per turn.
  int attackCost = 1;             // "cost" (0 to 99)
  int attacksPerTurn = 1;         // "attacksPerTurn" (1 to 99)
};

}  // namespace ote
